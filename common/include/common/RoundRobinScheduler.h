#pragma once

#include "IScheduler.h"

class RoundRobinScheduler : public IScheduler
{
public:
    WorkerHandle selectWorker(const std::vector<WorkerHandle> &available) override;

private:
    std::size_t nextIndex_ = 0;
};
