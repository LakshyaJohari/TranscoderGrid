#include "master/JobDispatcher.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

#include <spdlog/spdlog.h>

#include "common/ISplitStrategy.h"
#include "common/IScheduler.h"
#include "master/WorkerProxy.h"
#include "master/WorkerRegistry.h"

TaskResult JobDispatcher::runJob(const JobSpec &spec, ISplitStrategy &splitStrategy,
                                  IScheduler &scheduler, WorkerRegistry &registry)
{
    TaskResult result;
    result.status = TaskStatus::Failed;

    spdlog::info("[JobDispatcher] Starting job: id={}, source={}, num_chunks={}", spec.jobId,
                 spec.sourcePath, spec.numChunks);

    // Step 1: Split the job into chunks
    auto chunks = splitStrategy.split(spec);
    if (chunks.empty())
    {
        result.message = "Split strategy produced no chunks";
        spdlog::error("[JobDispatcher] {}", result.message);
        return result;
    }
    spdlog::info("[JobDispatcher] Split job into {} chunks", chunks.size());

    // Step 2: M3 - Dispatch chunks in parallel (with M4 retry logic)
    std::vector<std::pair<ChunkSpec, TaskResult>> results;
    std::vector<std::future<std::pair<ChunkSpec, TaskResult>>> futures;

    spdlog::info("[JobDispatcher] Dispatching {} chunks in parallel (max {} concurrent)", chunks.size(),
                 MAX_PARALLEL_CHUNKS);

    for (const auto &chunk : chunks)
    {
        // M3: Limit concurrent dispatches
        while (futures.size() >= MAX_PARALLEL_CHUNKS)
        {
            // Wait for at least one future to complete
            auto it = std::find_if(futures.begin(), futures.end(),
                                   [](const auto &f)
                                   { return f.wait_for(std::chrono::milliseconds(0)) ==
                                            std::future_status::ready; });
            if (it != futures.end())
            {
                results.push_back(it->get());
                futures.erase(it);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }

        // Launch async dispatch for this chunk
        auto future = std::async(std::launch::async, [this, chunk, &spec, &scheduler, &registry]()
        {
            return dispatchChunkWithRetry(chunk, spec, scheduler, registry);
        });
        futures.push_back(std::move(future));
    }

    // Wait for all remaining futures to complete
    for (auto &future : futures)
    {
        results.push_back(future.get());
    }

    // Check for any failures
    for (const auto &[chunk, chunkResult] : results)
    {
        if (chunkResult.status != TaskStatus::Success)
        {
            result.message =
                "Chunk " + chunk.chunkId + " failed after retries: " + chunkResult.message;
            spdlog::error("[JobDispatcher] {}", result.message);
            return result;
        }
    }

    spdlog::info("[JobDispatcher] All {} chunks completed successfully", chunks.size());

    // Step 3: Reassemble chunks into final output
    spdlog::info("[JobDispatcher] Reassembling {} chunks...", chunks.size());

    std::filesystem::create_directories("work");

    // Write concat demuxer list file
    std::ofstream concat_list("work/concat_list.txt");
    for (const auto &[chunk, chunkResult] : results)
    {
        concat_list << "file '" << chunkResult.outputPath << "'\n";
    }
    concat_list.close();
    spdlog::debug("[JobDispatcher] Wrote concat list: work/concat_list.txt");

    // Run ffmpeg concat filter to reassemble
    std::string ffmpeg_cmd = "ffmpeg -y -f concat -safe 0 -i work/concat_list.txt -c copy \"" +
                             spec.sourcePath + ".output.mp4\" 2>/dev/null";

    spdlog::debug("[JobDispatcher] Running: {}", ffmpeg_cmd);
    int ret = std::system(ffmpeg_cmd.c_str());
    if (ret != 0)
    {
        result.message = "ffmpeg concat failed with exit code " + std::to_string(ret);
        spdlog::error("[JobDispatcher] {}", result.message);
        return result;
    }

    // Success
    result.status = TaskStatus::Success;
    result.message = "Job completed successfully";
    result.outputPath = spec.sourcePath + ".output.mp4";

    spdlog::info("[JobDispatcher] Job {} completed: output={}", spec.jobId, result.outputPath);
    return result;
}

TaskResult JobDispatcher::dispatchChunkWithRetry(const ChunkSpec &chunk, const JobSpec &spec,
                                                 IScheduler &scheduler, WorkerRegistry &registry)
{
    TaskResult result;
    result.status = TaskStatus::Failed;

    // M4: Retry loop
    for (int attempt = 0; attempt <= MAX_RETRIES; ++attempt)
    {
        // Get available workers
        auto available = registry.listAvailable();
        if (available.empty())
        {
            result.message = "No workers available";
            spdlog::error("[JobDispatcher] Chunk {}: {}", chunk.chunkId, result.message);
            if (attempt < MAX_RETRIES)
            {
                spdlog::info("[JobDispatcher] Retrying chunk {} ({}/{})", chunk.chunkId, attempt + 1,
                           MAX_RETRIES);
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                continue;
            }
            return result;
        }

        // Select a worker via the scheduler
        auto selected = scheduler.selectWorker(available);
        spdlog::info("[JobDispatcher] Dispatching chunk {}: id={}, worker={}, attempt {}/{}", 
                     chunk.chunkId, chunk.chunkId, selected.id, attempt + 1, MAX_RETRIES + 1);

        try
        {
            // Create proxy and dispatch to the selected worker
            WorkerProxy proxy(selected.address);
            result = proxy.dispatch(chunk, spec);

            if (result.status == TaskStatus::Success)
            {
                spdlog::info("[JobDispatcher] Chunk {} completed successfully on worker {}", 
                           chunk.chunkId, selected.id);
                return result;
            }
            else
            {
                // Check if this is a retryable error (RPC failure) vs a real transcode failure
                bool isRetryable = result.message.find("RPC") != std::string::npos ||
                                  result.message.find("timeout") != std::string::npos ||
                                  result.message.find("connection") != std::string::npos;

                if (isRetryable && attempt < MAX_RETRIES)
                {
                    spdlog::warn("[JobDispatcher] Chunk {} failed with retryable error: {}. "
                               "Retrying ({}/{})",
                               chunk.chunkId, result.message, attempt + 1, MAX_RETRIES);
                    std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                    continue;
                }
                else
                {
                    spdlog::error("[JobDispatcher] Chunk {} failed ({}): {}", chunk.chunkId,
                                isRetryable ? "retryable but out of attempts" : "not retryable",
                                result.message);
                    return result;
                }
            }
        }
        catch (const std::exception &e)
        {
            result.message = std::string("Exception: ") + e.what();
            spdlog::error("[JobDispatcher] Chunk {} exception: {}", chunk.chunkId, result.message);

            if (attempt < MAX_RETRIES)
            {
                spdlog::info("[JobDispatcher] Retrying chunk {} ({}/{})", chunk.chunkId, 
                           attempt + 1, MAX_RETRIES);
                std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
                continue;
            }
            return result;
        }
    }

    // Should not reach here, but just in case
    result.status = TaskStatus::Failed;
    result.message = "Chunk dispatch failed after all retries";
    return result;
}
