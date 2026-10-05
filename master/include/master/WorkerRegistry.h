#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "common/IScheduler.h"

class WorkerRegistry
{
public:
    WorkerRegistry();
    ~WorkerRegistry();

    void registerWorker(const WorkerHandle &handle);
    std::vector<WorkerHandle> listAvailable() const;
    size_t workerCount() const;

    // Start heartbeat monitoring (M3)
    void startHeartbeatMonitoring();
    void stopHeartbeatMonitoring();

    // Update worker status from heartbeat (M3)
    void updateWorkerStatus(const std::string &workerId, int activeJobs, bool hasGpu, double cpuLoad);

    // Mark worker as seen (M3)
    void markWorkerSeen(const std::string &workerId);

private:
    struct WorkerInfo
    {
        WorkerHandle handle;
        std::chrono::steady_clock::time_point lastSeen;
        int missedHeartbeats = 0;
        bool isAvailable = true;
    };

    std::vector<WorkerInfo> workers_;
    mutable std::mutex workersMutex_;

    std::unique_ptr<std::jthread> heartbeatThread_;
    std::atomic<bool> stopHeartbeat_{false};

    void heartbeatLoop();
    static constexpr int HEARTBEAT_INTERVAL_MS = 2000;
    static constexpr int HEARTBEAT_TIMEOUT_MS = 5000;
    static constexpr int MAX_MISSED_HEARTBEATS = 3;
};
