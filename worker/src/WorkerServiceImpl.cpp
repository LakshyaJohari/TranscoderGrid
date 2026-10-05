#include "worker/WorkerServiceImpl.h"

#include <spdlog/spdlog.h>

#include "common/ChunkSpec.h"
#include "common/JobSpec.h"

WorkerServiceImpl::WorkerServiceImpl(std::string workerId, std::unique_ptr<IFFmpegExecutor> executor)
    : workerId_(std::move(workerId)), executor_(std::move(executor))
{
}

const std::string &WorkerServiceImpl::workerId() const
{
    return workerId_;
}

grpc::Status WorkerServiceImpl::TranscodeChunk(grpc::ServerContext *context, const rffmpeg::ChunkRequest *request,
                                              grpc::ServerWriter<rffmpeg::ProgressUpdate> *writer)
{
    spdlog::info("[Worker {}] TranscodeChunk request: chunk_id={}, job_id={}", workerId_, request->chunk_id(),
                 request->job_id());

    // Build ChunkSpec from the request
    ChunkSpec chunk;
    chunk.chunkId = request->chunk_id();
    chunk.jobId = request->job_id();
    chunk.sourceStorageKey = request->source_storage_key();
    chunk.outputStorageKey = request->output_storage_key();
    chunk.startTimeSec = request->start_time_sec();
    chunk.durationSec = request->duration_sec();

    // Build JobSpec from the request
    JobSpec spec;
    spec.jobId = request->job_id();
    spec.sourcePath = request->source_storage_key();
    spec.outputCodec = request->output_codec();
    spec.resolution = request->resolution();
    spec.bitrateKbps = request->bitrate_kbps();
    spec.crf = request->crf();
    for (const auto &filter : request->extra_filters())
    {
        spec.extraFilters.push_back(filter);
    }

    // Run the transcode
    auto result = executor_->run(chunk, spec);

    // Send final progress update
    rffmpeg::ProgressUpdate update;
    update.set_chunk_id(request->chunk_id());
    update.set_percent_complete(100.0);
    update.set_status(result.status == TaskStatus::Success ? "success" : "failed");
    update.set_message(result.message);

    writer->Write(update);
    spdlog::info("[Worker {}] TranscodeChunk completed: status={}", workerId_,
                 result.status == TaskStatus::Success ? "success" : "failed");

    return grpc::Status::OK;
}

grpc::Status WorkerServiceImpl::Heartbeat(grpc::ServerContext *context, const rffmpeg::Empty *request,
                                          rffmpeg::WorkerStatus *response)
{
    response->set_worker_id(workerId_);
    response->set_active_jobs(0);    // TODO: track actual active jobs
    response->set_has_gpu(false);    // TODO: detect GPU if available
    response->set_cpu_load(0.0);     // TODO: measure CPU load

    return grpc::Status::OK;
}

grpc::Status WorkerServiceImpl::CancelJob(grpc::ServerContext *context, const rffmpeg::JobIdRequest *request,
                                          rffmpeg::Empty *response)
{
    spdlog::warn("[Worker {}] CancelJob not yet implemented for job_id={}", workerId_, request->job_id());
    return grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "CancelJob not yet implemented");
}
