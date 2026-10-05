#include "master/WorkerProxy.h"

#include <chrono>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <spdlog/spdlog.h>

#include "common/ChunkSpec.h"
#include "common/JobSpec.h"

// Forward declare namespace to avoid full include of generated proto headers here
namespace rffmpeg {
class WorkerService;
}

WorkerProxy::WorkerProxy(std::string address) : address_(std::move(address))
{
    // Create an insecure channel (for local/internal networks only)
    channel_ = grpc::CreateChannel(address_, grpc::InsecureChannelCredentials());

    // Create a stub from the generated WorkerService code
    // This will work once protobuf generation produces the full stub definition
    // stub_ = rffmpeg::WorkerService::NewStub(channel_);
}

WorkerProxy::~WorkerProxy() = default;

TaskResult WorkerProxy::dispatch(const ChunkSpec &chunk, const JobSpec &spec)
{
    TaskResult result;
    result.status = TaskStatus::Failed;

    if (!stub_)
    {
        result.message = "gRPC stub not initialized";
        spdlog::error("[WorkerProxy {}] {}", address_, result.message);
        return result;
    }

    spdlog::info("[WorkerProxy {}] Dispatching chunk: id={}, duration={:.2f}s", address_,
                 chunk.chunkId, chunk.durationSec);

    try
    {
        // Build the gRPC request from the chunk spec
        rffmpeg::ChunkRequest request;
        request.set_chunk_id(chunk.chunkId);
        request.set_job_id(chunk.jobId);
        request.set_source_storage_key(chunk.sourceStorageKey);
        request.set_output_storage_key(chunk.outputStorageKey);
        request.set_start_time_sec(chunk.startTimeSec);
        request.set_duration_sec(chunk.durationSec);
        request.set_output_codec(spec.outputCodec);
        request.set_resolution(spec.resolution);
        request.set_bitrate_kbps(spec.bitrateKbps);
        request.set_crf(spec.crf);
        for (const auto &filter : spec.extraFilters)
        {
            request.add_extra_filters(filter);
        }

        // Setup context with deadline
        grpc::ClientContext context;
        context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(300));

        // Call the streaming RPC
        auto stream = stub_->TranscodeChunk(&context, request);
        if (!stream)
        {
            result.message = "Failed to create RPC stream";
            spdlog::error("[WorkerProxy {}] {}", address_, result.message);
            return result;
        }

        // Read progress updates from stream
        rffmpeg::ProgressUpdate update;
        while (stream->Read(&update))
        {
            spdlog::debug("[WorkerProxy {}] chunk_id={}, progress={:.1f}%, status={}", address_,
                          update.chunk_id(), update.percent_complete(), update.status());
        }

        // Get final RPC status
        grpc::Status rpc_status = stream->Finish();
        if (!rpc_status.ok())
        {
            result.status = TaskStatus::Failed;
            result.message = "RPC failed: " + rpc_status.error_message();
            spdlog::error("[WorkerProxy {}] {}", address_, result.message);
            return result;
        }

        // Check final update status
        if (update.status() == "success")
        {
            result.status = TaskStatus::Success;
            result.outputPath = chunk.outputStorageKey;
            result.message = update.message();
            result.durationMs = 0; // TODO: track actual transcode time
            spdlog::info("[WorkerProxy {}] Chunk completed successfully", address_);
        }
        else
        {
            result.status = TaskStatus::Failed;
            result.message = "Worker transcode failed: " + update.message();
            spdlog::error("[WorkerProxy {}] {}", address_, result.message);
        }
    }
    catch (const std::exception &e)
    {
        result.status = TaskStatus::Failed;
        result.message = std::string("Exception during dispatch: ") + e.what();
        spdlog::error("[WorkerProxy {}] {}", address_, result.message);
    }

    return result;
}

std::string WorkerProxy::address() const
{
    return address_;
}
