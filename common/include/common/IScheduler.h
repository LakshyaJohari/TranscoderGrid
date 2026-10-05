#pragma once
#include <string>
#include <vector>

struct WorkerHandle
{
    std::string id;
    std::string address;
    int activeJobs = 0;
    bool hasGpu = false;
};

class IScheduler
{
public:
    virtual WorkerHandle selectWorker(const std::vector<WorkerHandle> &available) = 0;
    virtual ~IScheduler() = default;
};
