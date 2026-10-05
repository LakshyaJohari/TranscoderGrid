#include "../include/common/RoundRobinScheduler.h"

WorkerHandle RoundRobinScheduler::selectWorker(const std::vector<WorkerHandle> &available)
{
    if (available.empty())
    {
        return {};
    }
    if (nextIndex_ >= available.size())
    {
        nextIndex_ = 0;
    }
    WorkerHandle selected = available[nextIndex_];
    nextIndex_ = (nextIndex_ + 1) % available.size();
    return selected;
}
