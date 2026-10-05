# M2 Build & Test Guide

**Date:** July 16, 2026  
**Purpose:** Step-by-step instructions to build and test M2 once network access is restored.

---

## Prerequisites

Before starting, ensure:
- `VCPKG_ROOT` environment variable is set to `C:\vcpkg`
- `ffmpeg` and `ffprobe` are on PATH
- CMake 3.26+ is available
- Visual Studio 2022 C++ toolchain is installed

---

## Part 1: Build

### Step 1A: Configure CMake

```powershell
cd "c:\Users\srija\Kode\Season 4"
$env:VCPKG_ROOT = "C:\vcpkg"
cmake --preset dev
```

**Expected output:**
```
-- Configuring done
-- Generating done
```

If you see SSL/network errors, try:
```powershell
# Option 1: Enable long paths (if hitting MAX_PATH)
reg add HKLM\SYSTEM\CurrentControlSet\Control\FileSystem /v LongPathsEnabled /t REG_DWORD /d 1 /f

# Option 2: Disable sccache compiler launcher if causing issues
# Edit CMakePresets.json, remove CMAKE_CXX_COMPILER_LAUNCHER lines
```

### Step 1B: Build

```powershell
cmake --build --preset dev --parallel
```

**Expected build artifacts:**
- `build/dev/common/common.lib`
- `build/dev/worker/worker.exe`
- `build/dev/master/master.exe`
- `build/dev/samples/single_machine/single_machine_demo.exe`

**Build time:** 5–15 minutes on first run (depends on vcpkg binary cache); 30 seconds on incremental rebuilds.

If build fails, check:
1. Proto compilation errors → Check `proto/jobservice.proto` syntax
2. gRPC linking errors → Verify `find_package(gRPC)` succeeded
3. Missing headers → Proto headers should be auto-generated in `build/dev` directory

---

## Part 2: Generate Test Video

If you don't have `sample.mp4`, create one:

```powershell
ffmpeg -f lavfi -i testsrc=duration=12:size=640x360:rate=30 -f lavfi -i sine=frequency=1000:duration=12 -c:v libx264 -c:a aac -shortest sample.mp4
```

Verify:
```powershell
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 sample.mp4
```

Expected output: `12.0` (or close to it, e.g., `12.004`)

---

## Part 3: M2 End-to-End Test

This test verifies one worker can transcode a job split into 4 chunks.

### Terminal A: Start Worker

```powershell
.\build\dev\worker\worker.exe --port 50052 --worker-id worker-1
```

**Expected output:**
```
[2026-07-16 10:30:45.123] [info] Starting worker: id=worker-1, port=50052
[2026-07-16 10:30:45.124] [info] Worker listening on 0.0.0.0:50052
```

**Keep this terminal running.** The worker will block at `server->Wait()` indefinitely.

### Terminal B: Run Master (Dispatch Job)

In a new terminal:

```powershell
cd "c:\Users\srija\Kode\Season 4"
.\build\dev\master\master.exe sample.mp4 4 output.mp4 --worker-address localhost:50052 --job-id m2-test-1
```

**Expected console output:**
```
[2026-07-16 10:30:46.234] [info] Master starting: source=sample.mp4, output=output.mp4, chunks=4, job_id=m2-test-1
[2026-07-16 10:30:46.235] [info] Registered worker: id=worker-0, address=localhost:50052
[2026-07-16 10:30:46.236] [info] [JobDispatcher] Starting job: id=m2-test-1, source=sample.mp4, num_chunks=4
[2026-07-16 10:30:46.237] [info] [JobDispatcher] Split job into 4 chunks
[2026-07-16 10:30:46.238] [info] [JobDispatcher] Dispatching chunk 1/4: id=m2-test-1_0, worker=worker-0
[2026-07-16 10:30:48.500] [info] [WorkerProxy localhost:50052] Chunk completed successfully
[2026-07-16 10:30:48.501] [info] [JobDispatcher] Chunk 1/4 completed successfully
... (repeat for chunks 2, 3, 4)
[2026-07-16 10:31:00.123] [info] [JobDispatcher] All 4 chunks completed. Reassembling...
[2026-07-16 10:31:01.234] [info] Master: Output written to output.mp4

Exit code: 0
```

**Timeline expectations:**
- Each chunk takes ~1–2 seconds to transcode (depends on CPU)
- Total: ~4–8 seconds for 4 chunks + reassembly
- If master waits >10 seconds at "Reassembling", worker may have crashed; check Terminal A

### Terminal A (Observer): Worker Activity

While Terminal B is running, Terminal A should show:

```
[2026-07-16 10:30:46.238] [info] [Worker worker-1] TranscodeChunk request: chunk_id=m2-test-1_0, job_id=m2-test-1
[2026-07-16 10:30:48.500] [info] [Worker worker-1] TranscodeChunk completed: status=success
... (repeat for remaining chunks)
```

If Terminal A shows no activity, the RPC is not reaching the worker → check network/firewall.

---

## Part 4: Verify Output

Once master completes:

### Check Output File Exists

```powershell
ls -la output.mp4
```

Expected: File size > 0 (typically 100KB–1MB for 12-second H.264 video)

### Check Duration

```powershell
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 output.mp4
```

**Acceptance criterion:** Duration within 0.1 seconds of source (12.0s ± 0.1s).

If output is 12.0s ±0.05s, ✅ **M2 is working**.

### Play Video (Optional)

Open `output.mp4` in VLC or Windows Media Player. Verify:
- Video plays from start to end without errors
- No visible artifacts or seams at chunk boundaries (though FFmpeg concat with `-c copy` should make boundaries invisible)
- Audio is in sync with video

---

## Part 5: Negative Test (Expected Failure Path)

Verify the system fails gracefully when the worker is not available.

### Terminal A: Stop the Worker

Press Ctrl+C in the worker terminal to shut it down.

### Terminal B: Try to Dispatch Again

```powershell
.\build\dev\master\master.exe sample.mp4 4 output_fail.mp4 --worker-address localhost:50052 --job-id m2-test-fail
```

**Expected behavior:**
- Master attempts to connect to worker on localhost:50052
- After ~5 seconds, gets connection refused error
- Master logs error and exits with non-zero exit code (typically 2 or 3)
- Master does NOT hang indefinitely

**Expected output:**
```
[2026-07-16 10:31:20.123] [info] Master starting: source=sample.mp4, ...
[2026-07-16 10:31:20.124] [info] Registered worker: id=worker-0, address=localhost:50052
[2026-07-16 10:31:20.125] [info] [JobDispatcher] Starting job: ...
[2026-07-16 10:31:20.126] [info] [JobDispatcher] Split job into 4 chunks
[2026-07-16 10:31:20.127] [info] [JobDispatcher] Dispatching chunk 1/4: ...
[2026-07-16 10:31:25.234] [error] [WorkerProxy localhost:50052] RPC failed: <error message>
[2026-07-16 10:31:25.235] [error] [JobDispatcher] Chunk m2-test-1_0 failed: RPC failed: ...

Exit code: 2
```

**Acceptable failure modes:**
- ✅ Connection refused (expected if worker is down)
- ✅ Deadline exceeded (timeout after 5–10 seconds)
- ❌ Hang indefinitely (indicates bug in timeout logic)
- ❌ Crash / segmentation fault (indicates memory/pointer bug)

---

## Part 6: Multi-Worker Test (Preview of M3)

Optional: If time permits, test with two workers to preview M3 scheduling.

### Terminal A1: Start Worker 1

```powershell
.\build\dev\worker\worker.exe --port 50052 --worker-id worker-1
```

### Terminal A2: Start Worker 2

```powershell
.\build\dev\worker\worker.exe --port 50053 --worker-id worker-2
```

### Terminal B: Dispatch to Both

```powershell
.\build\dev\master\master.exe sample.mp4 4 output_multi.mp4 `
  --worker-address localhost:50052 --worker-address localhost:50053 `
  --scheduler least_loaded `
  --job-id m2-multi-test
```

**Expected behavior:**
- Master registers both workers
- Dispatches 4 chunks: 2 to worker 1, 2 to worker 2 (load balanced)
- Both workers transcode in parallel
- Total time should be ~2–4 seconds (half the single-worker time)

**Expected log output:**
```
[...] [JobDispatcher] Dispatching chunk 1/4: ..., worker=worker-1
[...] [JobDispatcher] Dispatching chunk 2/4: ..., worker=worker-2
[...] [JobDispatcher] Dispatching chunk 3/4: ..., worker=worker-1  <- worker 1 available again
[...] [JobDispatcher] Dispatching chunk 4/4: ..., worker=worker-2  <- worker 2 available again
```

---

## Debugging Checklist

If tests fail, diagnose with:

| Symptom | Cause | Fix |
|---|---|---|
| `CMake Error: proto not found` | Proto file has syntax errors | Check `proto/jobservice.proto` |
| `undefined reference to grpc::...` | gRPC linkage missing | Rebuild from clean (`cmake --build --preset dev --clean-first`) |
| `Connection refused` (on success path) | Worker not running or wrong port | Start worker in Terminal A, verify port matches `--worker-address` |
| Master hangs for >30s at "Reassembling" | Worker crashed mid-transcode | Check Terminal A for errors; may need to re-run |
| Output file corrupted or 0 bytes | ffmpeg concat failed | Check that chunk files exist in `work/` directory |
| Duration of output is wrong (e.g., 6s instead of 12s) | Chunk splitting or merging bug | Check chunk boundaries; run M1 demo (`single_machine_demo.exe`) as comparison |

---

## Success Criteria

**M2 is DONE when:**

- [ ] `cmake --build` completes with zero errors (warnings OK)
- [ ] Worker starts and listens on port 50052 without crashing
- [ ] Master can dispatch a job to the worker and get results back
- [ ] Output file is created with correct duration (within 0.1s tolerance)
- [ ] Negative test (worker down) fails cleanly within 10 seconds
- [ ] Optional: Multi-worker test distributes chunks across workers
- [ ] Code compiles with warnings-as-errors once test issues are fixed

---

## After M2: Road to M3

Once M2 passes all checks:

1. **Enable warnings-as-errors:** Edit `.clang-tidy` / `CMakeLists.txt` to reject warnings in final build
2. **Add integration tests:** Write GoogleTest cases for `JobDispatcher` + mock `WorkerProxy`
3. **Docker compose:** Wire up `docker-compose.yml` for multi-container testing
4. **M3 work:** Implement heartbeat-based worker registration and parallel chunk dispatch

**Estimated effort to M3:** 2–4 hours once M2 is stable.

---

**Created:** 2026-07-16  
**Status:** Ready to execute once build succeeds
