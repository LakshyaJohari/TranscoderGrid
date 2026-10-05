#pragma once

#include "IScheduler.h"

class LeastLoadedScheduler : public IScheduler
{
public:
    WorkerHandle selectWorker(const std::vector<WorkerHandle> &available) override;
};
