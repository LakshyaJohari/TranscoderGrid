#pragma once

#include <memory>
#include <string>

#include "common/ChunkSpec.h"
#include "common/JobSpec.h"
#include "common/TaskResult.h"

namespace grpc {
class Channel;
}

namespace rffmpeg {
class WorkerService;
}

class WorkerProxy
{
public:
    explicit WorkerProxy(std::string address);
    ~WorkerProxy();

    TaskResult dispatch(const ChunkSpec &chunk, const JobSpec &spec);
    std::string address() const;

private:
    std::string address_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<rffmpeg::WorkerService::Stub> stub_;
};
