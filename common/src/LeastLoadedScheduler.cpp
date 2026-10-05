#include "../include/common/LeastLoadedScheduler.h"

#include <algorithm>

WorkerHandle LeastLoadedScheduler::selectWorker(const std::vector<WorkerHandle> &available)
{
    if (available.empty())
    {
        return {};
    }
    return *std::min_element(
        available.begin(),
        available.end(),
        [](const WorkerHandle &left, const WorkerHandle &right)
        {
            return left.activeJobs < right.activeJobs;
        });
}
