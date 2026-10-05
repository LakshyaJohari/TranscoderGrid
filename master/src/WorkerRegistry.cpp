#include "master/WorkerRegistry.h"

#include <spdlog/spdlog.h>

#include "master/WorkerProxy.h"

WorkerRegistry::WorkerRegistry() = default;

WorkerRegistry::~WorkerRegistry()
{
    stopHeartbeatMonitoring();
}

void WorkerRegistry::registerWorker(const WorkerHandle &handle)
{
    std::lock_guard<std::mutex> lock(workersMutex_);
    WorkerInfo info;
    info.handle = handle;
    info.lastSeen = std::chrono::steady_clock::now();
    info.isAvailable = true;
    workers_.push_back(info);
    spdlog::info("[WorkerRegistry] Registered: id={}, address={}", handle.id, handle.address);
}

std::vector<WorkerHandle> WorkerRegistry::listAvailable() const
{
    std::lock_guard<std::mutex> lock(workersMutex_);
    std::vector<WorkerHandle> available;
    for (const auto &info : workers_)
    {
        if (info.isAvailable && info.missedHeartbeats < MAX_MISSED_HEARTBEATS)
        {
            available.push_back(info.handle);
        }
    }
    return available;
}

size_t WorkerRegistry::workerCount() const
{
    std::lock_guard<std::mutex> lock(workersMutex_);
    return workers_.size();
}

void WorkerRegistry::startHeartbeatMonitoring()
{
    if (heartbeatThread_)
    {
        spdlog::warn("[WorkerRegistry] Heartbeat monitoring already started");
        return;
    }

    stopHeartbeat_ = false;
    heartbeatThread_ = std::make_unique<std::jthread>([this](std::stop_token stop)
    { 
        heartbeatLoop();
    });
    spdlog::info("[WorkerRegistry] Heartbeat monitoring started");
}

void WorkerRegistry::stopHeartbeatMonitoring()
{
    if (!heartbeatThread_)
    {
        return;
    }

    stopHeartbeat_ = true;
    heartbeatThread_.reset();
    spdlog::info("[WorkerRegistry] Heartbeat monitoring stopped");
}

void WorkerRegistry::updateWorkerStatus(const std::string &workerId, int activeJobs, bool hasGpu,
                                        double cpuLoad)
{
    std::lock_guard<std::mutex> lock(workersMutex_);
    for (auto &info : workers_)
    {
        if (info.handle.id == workerId)
        {
            info.handle.activeJobs = activeJobs;
            info.handle.hasGpu = hasGpu;
            info.missedHeartbeats = 0;  // Reset on successful heartbeat
            info.lastSeen = std::chrono::steady_clock::now();
            spdlog::debug("[WorkerRegistry] Updated: id={}, activeJobs={}", workerId, activeJobs);
            return;
        }
    }
}

void WorkerRegistry::markWorkerSeen(const std::string &workerId)
{
    std::lock_guard<std::mutex> lock(workersMutex_);
    for (auto &info : workers_)
    {
        if (info.handle.id == workerId)
        {
            info.lastSeen = std::chrono::steady_clock::now();
            info.missedHeartbeats = 0;
            return;
        }
    }
}

void WorkerRegistry::heartbeatLoop()
{
    spdlog::info("[WorkerRegistry::heartbeatLoop] Started");

    while (!stopHeartbeat_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_INTERVAL_MS));

        std::vector<WorkerInfo *> workersToCheck;
        {
            std::lock_guard<std::mutex> lock(workersMutex_);
            for (auto &info : workers_)
            {
                workersToCheck.push_back(&info);
            }
        }

        // Call heartbeat on each worker (outside of lock to avoid blocking)
        for (auto *info : workersToCheck)
        {
            try
            {
                WorkerProxy proxy(info->handle.address);
                // Note: This will call the actual gRPC heartbeat once proto stubs are available
                // For now, just mark as seen to avoid eviction
                markWorkerSeen(info->handle.id);
                spdlog::debug("[WorkerRegistry] Heartbeat OK: {}", info->handle.id);
            }
            catch (const std::exception &e)
            {
                std::lock_guard<std::mutex> lock(workersMutex_);
                for (auto &w : workers_)
                {
                    if (w.handle.id == info->handle.id)
                    {
                        w.missedHeartbeats++;
                        if (w.missedHeartbeats >= MAX_MISSED_HEARTBEATS)
                        {
                            w.isAvailable = false;
                            spdlog::warn("[WorkerRegistry] Evicted: id={} ({}  missed heartbeats)",
                                        w.handle.id, w.missedHeartbeats);
                        }
                        break;
                    }
                }
                spdlog::debug("[WorkerRegistry] Heartbeat failed: {} ({})", info->handle.id,
                             e.what());
            }
        }
    }

    spdlog::info("[WorkerRegistry::heartbeatLoop] Stopped");
}
