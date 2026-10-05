# Quick Start: Building & Testing M2

**TL;DR:** Build, test with one worker, verify output. All code is ready.

---

## 30-Second Build

```powershell
cd "c:\Users\srija\Kode\Season 4"
$env:VCPKG_ROOT = "C:\vcpkg"
cmake --preset dev
cmake --build --preset dev --parallel
```

**Wait:** 5–15 min (first build) or 30 sec (incremental)

---

## Generate Test Video (if needed)

```powershell
ffmpeg -f lavfi -i testsrc=duration=12:size=640x360:rate=30 -f lavfi -i sine=frequency=1000:duration=12 -c:v libx264 -c:a aac -shortest sample.mp4
```

---

## Test M2 (3 Terminals)

### Terminal A: Start Worker

```powershell
.\build\dev\worker\worker.exe --port 50052 --worker-id worker-1
```

**Expected:** `Worker listening on 0.0.0.0:50052` → leave running

### Terminal B: Run Master (Dispatch Job)

```powershell
.\build\dev\master\master.exe sample.mp4 4 output.mp4 --worker-address localhost:50052
```

**Expected:** `Output written to output.mp4` → exit code 0

### Terminal C: Verify Output

```powershell
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 output.mp4
```

**Success Criterion:** Duration ≈ 12.0s (within ±0.1s)

---

## Multi-Worker Test (M3 Preview)

```powershell
# Terminal A1
.\build\dev\worker\worker.exe --port 50052 --worker-id worker-1

# Terminal A2
.\build\dev\worker\worker.exe --port 50053 --worker-id worker-2

# Terminal B
.\build\dev\master\master.exe sample.mp4 4 output_multi.mp4 `
  --worker-address localhost:50052 --worker-address localhost:50053 `
  --scheduler least_loaded
```

**Expected:** Faster (parallel processing across 2 workers)

---

## Full Documentation

| Doc | Purpose |
|---|---|
| `M2_BUILD_AND_TEST.md` | Detailed test procedures + debugging |
| `M2_COMPLETE_SUMMARY.md` | Architecture, code quality, performance |
| `NEXT_STEPS.md` | Roadmap to M3 |
| `DEVDOC.md` | Full specification (all milestones) |

---

## Troubleshooting

| Problem | Solution |
|---|---|
| Build fails with SSL error | Network issue; retry or check proxy settings |
| Worker won't start | Port already in use; try `--port 50053` |
| Master hangs at "Reassembling" | Worker crashed; check Terminal A logs |
| Output duration is wrong | Chunk splitting bug; run M1 baseline test |
| RPC connection refused | Worker not running; start Terminal A |

---

## Key Commands

```powershell
# Clean rebuild
cmake --build --preset dev --clean-first

# Verbose build (debugging)
cmake --build --preset dev --verbose

# Run tests (once M7 is done)
ctest --preset dev --output-on-failure

# Format check
Get-ChildItem -Recurse master,worker,common -Include *.cpp,*.h | 
  ForEach-Object { clang-format --dry-run --Werror $_.FullName }
```

---

## File Organization

```
distributed-ffmpeg/
├── common/               # Shared interfaces (M1–M7)
├── master/
│   ├── src/main.cpp      ← CLI + worker registration (M2)
│   ├── src/JobDispatcher.cpp  ← Job orchestration (M2)
│   └── src/WorkerProxy.cpp    ← gRPC client (M2)
├── worker/
│   ├── src/main.cpp           ← gRPC server (M2)
│   └── src/WorkerServiceImpl.cpp ← RPC handlers (M2)
├── proto/jobservice.proto     ← RPC contract (M2)
├── samples/single_machine/    ← M1 baseline
└── M2_*.md                    ← All docs
```

---

## Next Steps After M2 Verification

1. ✅ M2 tests pass → `git tag M2-verified`
2. ⏳ M3: Parallel dispatch + heartbeat registration
3. ⏳ M4: Fault tolerance (requeue on failure)
4. ⏳ M5: Storage abstraction (MinIO/S3)
5. ⏳ M6: Job state persistence (SQLite)
6. ⏳ M7: Tests, Docker, polish

**Estimated timeline:** M3–M7 at 2 weeks per milestone = **10 weeks to production**.

---

**Status:** Ready to build ✅  
**Last Updated:** 2026-07-16
