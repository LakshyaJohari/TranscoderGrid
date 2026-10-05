#pragma once

#include <memory>
#include <string>

#include <grpcpp/server_context.h>

#include "common/IFFmpegExecutor.h"

// Forward declare the generated proto service and messages
namespace rffmpeg {
class WorkerService;
class ChunkRequest;
class ProgressUpdate;
class Empty;
class WorkerStatus;
class JobIdRequest;
}

namespace grpc {
class Status;
template <typename T>
class ServerWriter;
}

class WorkerServiceImpl : public rffmpeg::WorkerService::Service
{
public:
    explicit WorkerServiceImpl(std::string workerId, std::unique_ptr<IFFmpegExecutor> executor);

    const std::string &workerId() const;

    // gRPC service methods
    grpc::Status TranscodeChunk(grpc::ServerContext *context, const rffmpeg::ChunkRequest *request,
                                grpc::ServerWriter<rffmpeg::ProgressUpdate> *writer) override;

    grpc::Status Heartbeat(grpc::ServerContext *context, const rffmpeg::Empty *request,
                           rffmpeg::WorkerStatus *response) override;

    grpc::Status CancelJob(grpc::ServerContext *context, const rffmpeg::JobIdRequest *request,
                           rffmpeg::Empty *response) override;

private:
    std::string workerId_;
    std::unique_ptr<IFFmpegExecutor> executor_;
};
