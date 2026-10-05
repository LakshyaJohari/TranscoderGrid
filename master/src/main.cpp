#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#include "common/FFmpegExecutor.h"
#include "common/LeastLoadedScheduler.h"
#include "common/RoundRobinScheduler.h"
#include "common/TimeBasedSplitStrategy.h"
#include "master/JobDispatcher.h"
#include "master/WorkerRegistry.h"

int main(int argc, char **argv)
{
    CLI::App app{"Distributed FFmpeg Master"};

    std::string sourcePath;
    std::string outputPath;
    int numChunks = 4;
    std::vector<std::string> workerAddresses;
    std::string scheduler = "least_loaded";
    std::string jobId = "job-1";

    app.add_option("source", sourcePath, "Source video file path")->required();
    app.add_option("output", outputPath, "Output video file path")->required();
    app.add_option("-n,--chunks", numChunks, "Number of chunks (default: 4)");
    app.add_option("--worker-address", workerAddresses, "Worker addresses (host:port) — repeatable")->required();
    app.add_option("--scheduler", scheduler, "Scheduler: least_loaded|round_robin (default: least_loaded)");
    app.add_option("--job-id", jobId, "Job ID (default: job-1)");

    CLI11_PARSE(app, argc, argv);

    // Initialize logging
    spdlog::set_level(spdlog::level::info);
    spdlog::info("Master starting: source={}, output={}, chunks={}, job_id={}", sourcePath, outputPath, numChunks, jobId);

    // Check source exists
    if (!std::filesystem::exists(sourcePath))
    {
        spdlog::error("Source file not found: {}", sourcePath);
        return 1;
    }

    // Create registry and register workers
    auto registry = std::make_unique<WorkerRegistry>();
    for (const auto &addr : workerAddresses)
    {
        WorkerHandle handle;
        handle.id = "worker-" + std::to_string(registry->workerCount());
        handle.address = addr;
        handle.activeJobs = 0;
        handle.hasGpu = false;
        registry->registerWorker(handle);
        spdlog::info("Registered worker: id={}, address={}", handle.id, addr);
    }

    // M3: Start heartbeat monitoring for worker health tracking
    registry->startHeartbeatMonitoring();

    // Create job spec
    JobSpec spec;
    spec.jobId = jobId;
    spec.sourcePath = sourcePath;
    spec.outputCodec = "libx264";
    spec.crf = 23;
    spec.numChunks = numChunks;

    // Create strategy
    auto splitStrategy = std::make_unique<TimeBasedSplitStrategy>();

    // Create scheduler
    std::unique_ptr<IScheduler> schedulerImpl;
    if (scheduler == "round_robin")
    {
        spdlog::info("Using RoundRobinScheduler");
        schedulerImpl = std::make_unique<RoundRobinScheduler>();
    }
    else
    {
        spdlog::info("Using LeastLoadedScheduler");
        schedulerImpl = std::make_unique<LeastLoadedScheduler>();
    }

    // Dispatch job
    JobDispatcher dispatcher;
    auto result = dispatcher.runJob(spec, *splitStrategy, *schedulerImpl, *registry);

    if (result.status != TaskStatus::Success)
    {
        spdlog::error("Job failed: {}", result.message);
        return 2;
    }

    // Copy output to requested location
    spdlog::info("Copying output from {} to {}", result.outputPath, outputPath);
    try
    {
        std::filesystem::copy_file(result.outputPath, outputPath,
                                    std::filesystem::copy_options::overwrite_existing);
    }
    catch (const std::exception &e)
    {
        spdlog::error("Failed to copy output file: {}", e.what());
        return 3;
    }

    spdlog::info("Job completed successfully");
    std::cout << "Output written to " << outputPath << std::endl;

    return 0;
}
