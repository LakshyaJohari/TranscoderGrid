#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "common/IScheduler.h"
#include "common/RoundRobinScheduler.h"
#include "common/LeastLoadedScheduler.h"

// ==========================================
// RoundRobinScheduler Tests
// ==========================================

TEST(RoundRobinSchedulerTest, ReturnsEmptyHandleWhenNoWorkersAvailable)
{
    RoundRobinScheduler scheduler;
    std::vector<WorkerHandle> available = {};

    WorkerHandle selected = scheduler.selectWorker(available);
    EXPECT_TRUE(selected.id.empty());
    EXPECT_TRUE(selected.address.empty());
}

TEST(RoundRobinSchedulerTest, AlwaysSelectsSingleWorker)
{
    RoundRobinScheduler scheduler;
    std::vector<WorkerHandle> available = {
        {"worker-1", "127.0.0.1:50052", 0, false}
    };

    for (int i = 0; i < 5; ++i)
    {
        WorkerHandle selected = scheduler.selectWorker(available);
        EXPECT_EQ(selected.id, "worker-1");
        EXPECT_EQ(selected.address, "127.0.0.1:50052");
    }
}

TEST(RoundRobinSchedulerTest, CyclesThroughWorkersInStrictOrder)
{
    RoundRobinScheduler scheduler;
    std::vector<WorkerHandle> available = {
        {"worker-1", "127.0.0.1:50052", 0, false},
        {"worker-2", "127.0.0.1:50053", 0, false},
        {"worker-3", "127.0.0.1:50054", 0, false}
    };

    std::vector<std::string> expectedOrder = {
        "worker-1", "worker-2", "worker-3",
        "worker-1", "worker-2", "worker-3",
        "worker-1"
    };

    for (const auto &expectedId : expectedOrder)
    {
        WorkerHandle selected = scheduler.selectWorker(available);
        EXPECT_EQ(selected.id, expectedId);
    }
}

TEST(RoundRobinSchedulerTest, HandlesDynamicListResizingSafely)
{
    RoundRobinScheduler scheduler;
    std::vector<WorkerHandle> initial = {
        {"worker-1", "127.0.0.1:50052", 0, false},
        {"worker-2", "127.0.0.1:50053", 0, false},
        {"worker-3", "127.0.0.1:50054", 0, false},
        {"worker-4", "127.0.0.1:50055", 0, false}
    };

    // Cycle through 3 workers (nextIndex_ is now 3)
    scheduler.selectWorker(initial); // 1
    scheduler.selectWorker(initial); // 2
    scheduler.selectWorker(initial); // 3

    // Shrink list to 2 workers: index 3 would be out of bounds if not handled
    std::vector<WorkerHandle> shrunk = {
        {"worker-1", "127.0.0.1:50052", 0, false},
        {"worker-2", "127.0.0.1:50053", 0, false}
    };

    WorkerHandle selected = scheduler.selectWorker(shrunk);
    EXPECT_EQ(selected.id, "worker-1") << "Scheduler should safely reset index when worker list shrinks";
}

// ==========================================
// LeastLoadedScheduler Tests
// ==========================================

TEST(LeastLoadedSchedulerTest, ReturnsEmptyHandleWhenNoWorkersAvailable)
{
    LeastLoadedScheduler scheduler;
    std::vector<WorkerHandle> available = {};

    WorkerHandle selected = scheduler.selectWorker(available);
    EXPECT_TRUE(selected.id.empty());
}

TEST(LeastLoadedSchedulerTest, SelectsWorkerWithLowestActiveJobs)
{
    LeastLoadedScheduler scheduler;
    std::vector<WorkerHandle> available = {
        {"worker-busy-1", "127.0.0.1:50052", 5, false},
        {"worker-idle",   "127.0.0.1:50053", 1, false},
        {"worker-busy-2", "127.0.0.1:50054", 4, false}
    };

    WorkerHandle selected = scheduler.selectWorker(available);
    EXPECT_EQ(selected.id, "worker-idle");
    EXPECT_EQ(selected.activeJobs, 1);
}

TEST(LeastLoadedSchedulerTest, BreaksTiesWithFirstEncounteredWorker)
{
    LeastLoadedScheduler scheduler;
    std::vector<WorkerHandle> available = {
        {"worker-first",  "127.0.0.1:50052", 2, false},
        {"worker-second", "127.0.0.1:50053", 2, false},
        {"worker-busy",   "127.0.0.1:50054", 6, false}
    };

    WorkerHandle selected = scheduler.selectWorker(available);
    EXPECT_EQ(selected.id, "worker-first");
}

TEST(LeastLoadedSchedulerTest, AdaptsToDynamicLoadChanges)
{
    LeastLoadedScheduler scheduler;
    std::vector<WorkerHandle> available = {
        {"worker-1", "127.0.0.1:50052", 3, false},
        {"worker-2", "127.0.0.1:50053", 1, false}
    };

    // Initially worker-2 is least loaded
    EXPECT_EQ(scheduler.selectWorker(available).id, "worker-2");

    // Simulate worker-2 taking on jobs and worker-1 becoming idle
    available[0].activeJobs = 0;
    available[1].activeJobs = 4;

    // Now worker-1 should be selected
    EXPECT_EQ(scheduler.selectWorker(available).id, "worker-1");
}
