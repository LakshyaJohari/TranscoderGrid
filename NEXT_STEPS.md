# Next Steps: M2 Completion & M3 Planning

## Immediate (When Network Stabilizes)

### Step 1: Successful Build
```powershell
cd "c:\Users\srija\Kode\Season 4"
$env:VCPKG_ROOT = "C:\vcpkg"
cmake --preset dev
cmake --build --preset dev --parallel
```

**Expected output:** `build/dev/master/master.exe` and `build/dev/worker/worker.exe` exist.

### Step 2: Implement WorkerProxy::dispatch() 

The method currently returns a stub failure. Replace with real gRPC call:

**File:** `master/src/WorkerProxy.cpp`

```cpp
#include "master/WorkerProxy.h"

#include <chrono>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <spdlog/spdlog.h>

#include "common/ChunkSpec.h"
#include "common/JobSpec.h"

WorkerProxy::WorkerProxy(std::string address) : address_(std::move(address))
{
    channel_ = grpc::CreateChannel(address_, grpc::InsecureChannelCredentials());
    stub_ = rffmpeg::WorkerService::NewStub(channel_);
}

WorkerProxy::~WorkerProxy() = default;

TaskResult WorkerProxy::dispatch(const ChunkSpec &chunk, const JobSpec &spec)
{
    TaskResult result;
    result.status = TaskStatus::Failed;

    // Build the gRPC request
    rffmpeg::ChunkRequest request;
    request.set_chunk_id(chunk.chunkId);
    request.set_job_id(chunk.jobId);
    request.set_source_storage_key(chunk.sourceStorageKey);
    request.set_output_storage_key(chunk.outputStorageKey);
    request.set_start_time_sec(chunk.startTimeSec);
    request.set_duration_sec(chunk.durationSec);
    request.set_output_codec(spec.outputCodec);
    request.set_resolution(spec.resolution);
    request.set_bitrate_kbps(spec.bitrateKbps);
    request.set_crf(spec.crf);
    for (const auto &filter : spec.extraFilters)
    {
        request.add_extra_filters(filter);
    }

    // Setup context with deadline
    grpc::ClientContext context;
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(300)); // 5 min timeout

    // Call the RPC
    auto stream = stub_->TranscodeChunk(&context, request);
    if (!stream)
    {
        result.message = "Failed to create stream";
        spdlog::error("[WorkerProxy {}] Failed to create RPC stream", address_);
        return result;
    }

    // Read progress updates from stream
    rffmpeg::ProgressUpdate update;
    while (stream->Read(&update))
    {
        spdlog::debug("[WorkerProxy {}] chunk_id={}, progress={}%, status={}", address_,
                      update.chunk_id(), update.percent_complete(), update.status());
    }

    // Get final RPC status
    grpc::Status rpc_status = stream->Finish();
    if (!rpc_status.ok())
    {
        result.status = TaskStatus::Failed;
        result.message = rpc_status.error_message();
        spdlog::error("[WorkerProxy {}] RPC failed: {}", address_, result.message);
        return result;
    }

    // If we got here and last update was success, mark as success
    if (update.status() == "success")
    {
        result.status = TaskStatus::Success;
        result.outputPath = chunk.outputStorageKey;
        result.message = update.message();
        spdlog::info("[WorkerProxy {}] Chunk transcoded successfully", address_);
    }
    else
    {
        result.status = TaskStatus::Failed;
        result.message = "Transcode failed on worker: " + update.message();
        spdlog::error("[WorkerProxy {}] Transcode failed: {}", address_, result.message);
    }

    return result;
}

std::string WorkerProxy::address() const
{
    return address_;
}
```

### Step 3: Implement JobDispatcher::runJob()

**File:** `master/src/JobDispatcher.cpp`

```cpp
#include "master/JobDispatcher.h"

#include <filesystem>
#include <fstream>
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

    spdlog::info("[JobDispatcher] Starting job: id={}, source={}, chunks={}", spec.jobId,
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

    // Step 2: Dispatch each chunk to a worker
    std::vector<std::pair<ChunkSpec, TaskResult>> results;

    for (const auto &chunk : chunks)
    {
        auto available = registry.listAvailable();
        if (available.empty())
        {
            result.message = "No workers available";
            spdlog::error("[JobDispatcher] {}", result.message);
            return result;
        }

        // Select worker
        auto selected = scheduler.selectWorker(available);
        spdlog::info("[JobDispatcher] Dispatching chunk {} to worker {}", chunk.chunkId,
                     selected.id);

        // Create proxy and dispatch
        WorkerProxy proxy(selected.address);
        auto chunkResult = proxy.dispatch(chunk, spec);

        results.push_back({chunk, chunkResult});

        if (chunkResult.status != TaskStatus::Success)
        {
            spdlog::error("[JobDispatcher] Chunk {} failed: {}", chunk.chunkId,
                          chunkResult.message);
            // In M2, fail the whole job on first failure (M4 adds retry logic)
            result.message = "Chunk " + chunk.chunkId + " failed: " + chunkResult.message;
            return result;
        }
    }

    // Step 3: Reassemble chunks
    spdlog::info("[JobDispatcher] All chunks completed. Reassembling...");

    // Write concat list file
    std::ofstream concat_list("work/concat_list.txt");
    for (const auto &[chunk, chunkResult] : results)
    {
        concat_list << "file '" << chunkResult.outputPath << "'\n";
    }
    concat_list.close();

    // Run ffmpeg concat
    std::string cmd = std::string("ffmpeg -y -f concat -safe 0 -i work/concat_list.txt ") +
                      "-c copy \"" + spec.sourcePath + "_out.mp4\" 2>/dev/null";

    int ret = std::system(cmd.c_str());
    if (ret != 0)
    {
        result.message = "ffmpeg concat failed with code " + std::to_string(ret);
        spdlog::error("[JobDispatcher] {}", result.message);
        return result;
    }

    result.status = TaskStatus::Success;
    result.message = "Job completed successfully";
    result.outputPath = spec.sourcePath + "_out.mp4";

    spdlog::info("[JobDispatcher] Job completed: {}", result.outputPath);
    return result;
}
```

### Step 4: Test M2 End-to-End

**Terminal A (Worker):**
```powershell
.\build\dev\worker\worker.exe --port 50052 --worker-id worker-1
```

**Terminal B (Generate test video if needed):**
```powershell
ffmpeg -f lavfi -i testsrc=duration=12:size=640x360:rate=30 -f lavfi -i sine=frequency=1000:duration=12 -c:v libx264 -c:a aac -shortest sample.mp4
```

**Terminal C (Master):**
```powershell
.\build\dev\master\master.exe sample.mp4 4 output.mp4 --worker-address localhost:50052
```

**Verify:**
```powershell
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 output.mp4
```

Expected: Duration within 0.1s of input (12.0s).

---

## Then, Move to M3 (Multi-Worker Scheduling)

Once M2 works, the core distributed architecture is proven. M3 adds:

1. **Multi-worker dispatch** (already designed, just add loop in JobDispatcher)
2. **Heartbeat-based worker registration** (background thread in WorkerRegistry)
3. **Switchable schedulers** (already have RoundRobin + LeastLoaded)

**Estimated effort:** 1–2 hours for full M3 implementation + testing.

---

## Architecture Summary

```
Master Process
├── WorkerRegistry (tracks available workers)
├── JobDispatcher (orchestrates job → chunks → workers)
│   ├── ISplitStrategy (TimeBasedSplitStrategy)
│   ├── IScheduler (LeastLoadedScheduler / RoundRobinScheduler)
│   └── WorkerProxy[] (one per worker, encapsulates gRPC stub)
└── CLI11 (CLI parsing)

Worker Process
├── WorkerServiceImpl (gRPC service handler)
│   └── IFFmpegExecutor (actually runs ffmpeg)
└── gRPC Server (listens for TranscodeChunk, Heartbeat, CancelJob)
```

---

## Quick Reference: Key Files

| Module | Files | Responsibility |
|---|---|---|
| **Proto** | `proto/jobservice.proto` | RPC contract |
| **Worker** | `worker/src/main.cpp`, `worker/include/worker/WorkerServiceImpl.h` | Transcode executor |
| **Master** | `master/src/main.cpp`, `master/src/JobDispatcher.cpp`, `master/src/WorkerProxy.cpp` | Orchestration |
| **Common** | `common/src/TimeBasedSplitStrategy.cpp`, `common/src/{Least,Round}RobinScheduler.cpp` | Shared logic |

---

**Last updated:** M2 architecture complete, awaiting network stabilization for build.
