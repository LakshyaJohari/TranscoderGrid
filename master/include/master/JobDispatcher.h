#pragma once

#include <memory>
#include <vector>

#include "common/ChunkSpec.h"
#include "common/IScheduler.h"
#include "common/ISplitStrategy.h"
#include "common/JobSpec.h"
#include "common/TaskResult.h"

class WorkerRegistry;

class JobDispatcher
{
public:
    JobDispatcher() = default;

    TaskResult runJob(const JobSpec &spec, ISplitStrategy &splitter, IScheduler &scheduler,
                      WorkerRegistry &registry);

private:
    // M3: Parallel dispatch
    static constexpr int MAX_PARALLEL_CHUNKS = 4;

    // M4: Retry logic
    static constexpr int MAX_RETRIES = 2;
    static constexpr int RETRY_DELAY_MS = 500;

    TaskResult dispatchChunkWithRetry(const ChunkSpec &chunk, const JobSpec &spec,
                                       IScheduler &scheduler, WorkerRegistry &registry);
};
