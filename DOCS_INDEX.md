# Documentation Index

**Last Updated:** July 16, 2026  
**Project:** Distributed FFmpeg Render Farm  
**Current Milestone:** M2 (Complete)

---

## Quick Links (Start Here)

| Document | Purpose | Read Time |
|---|---|---|
| **[QUICK_START.md](QUICK_START.md)** | 30-second build + test walkthrough | 2 min |
| **[STATUS.md](STATUS.md)** | Project health, progress, timeline | 5 min |
| **[README.md](README.md)** | Project overview, current state | 5 min |

---

## M2 Documentation (Current Milestone)

| Document | Purpose | Length | Audience |
|---|---|---|---|
| **[M2_BUILD_AND_TEST.md](M2_BUILD_AND_TEST.md)** | Step-by-step build and test procedures | 400 lines | Developers |
| **[M2_PROGRESS.md](M2_PROGRESS.md)** | What was implemented, what remains | 250 lines | Project managers |
| **[M2_COMPLETE_SUMMARY.md](M2_COMPLETE_SUMMARY.md)** | Architecture, design, performance, lessons learned | 350 lines | Technical leads |
| **[NEXT_STEPS.md](NEXT_STEPS.md)** | M2 completion tasks + M3 roadmap | 150 lines | Developers |

---

## Architecture & Design Documentation

| Document | Purpose | Scope |
|---|---|---|
| **[DEVDOC.md](DEVDOC.md)** | Complete specification for all milestones M1–M7 | All 7 milestones |
| **Proto contract** | gRPC service definition | `proto/jobservice.proto` |
| **Code structure** | Module organization and dependencies | File tree in repo root |

---

## Implementation Details by Module

### Common (Shared Interfaces)

- `common/include/common/JobSpec.h` — Job specification structure
- `common/include/common/ChunkSpec.h` — Chunk specification
- `common/include/common/IScheduler.h` — Worker selection strategy
- `common/include/common/ISplitStrategy.h` — Job splitting strategy
- `common/include/common/IFFmpegExecutor.h` — FFmpeg abstraction

### Worker (gRPC Service)

- `worker/include/worker/WorkerServiceImpl.h` — RPC handler implementation
- `worker/src/main.cpp` — Entry point, gRPC server setup

**Key features:**
- Receives `ChunkRequest` via gRPC
- Executes FFmpeg on chunk
- Streams `ProgressUpdate` back to master

### Master (Job Orchestration)

- `master/src/JobDispatcher.cpp` — Orchestrates split→dispatch→reassemble
- `master/src/WorkerProxy.cpp` — gRPC client wrapper
- `master/src/main.cpp` — Entry point, CLI, worker registration
- `master/include/master/WorkerRegistry.h` — Tracks available workers

**Key features:**
- Splits video into chunks
- Schedules chunks to workers
- Collects results and reassembles
- Handles errors gracefully

---

## Development Workflow

### For Developers

1. **Getting Started:** `QUICK_START.md`
2. **Building:** `M2_BUILD_AND_TEST.md` (§ Part 1)
3. **Testing:** `M2_BUILD_AND_TEST.md` (§ Parts 3–5)
4. **Debugging:** `M2_BUILD_AND_TEST.md` (§ Part 6)
5. **Contributing:** See `DEVDOC.md` (§ 8 Coding Standards)

### For Project Managers

1. **Status:** `STATUS.md`
2. **Progress:** `M2_PROGRESS.md` + `M2_COMPLETE_SUMMARY.md`
3. **Timeline:** `STATUS.md` (§ Timeline to Production)
4. **Architecture:** `M2_COMPLETE_SUMMARY.md` (§ Architecture Implemented)

### For Technical Leads

1. **Design review:** `M2_COMPLETE_SUMMARY.md` (§ Code Quality & Design)
2. **Performance:** `M2_COMPLETE_SUMMARY.md` (§ Performance Characteristics)
3. **Testing strategy:** `M2_BUILD_AND_TEST.md` (§ Part 6)
4. **Known limitations:** `M2_COMPLETE_SUMMARY.md` (§ Known Limitations & Future Work)

---

## File Organization

```
distributed-ffmpeg/
│
├─ DEVDOC.md                    [Full specification, all milestones]
├─ README.md                    [Project overview]
├─ STATUS.md                    [Project health & progress]
├─ QUICK_START.md               [30-second onboarding]
├─ DOCS_INDEX.md                [This file]
│
├─ M2_BUILD_AND_TEST.md         [Build & test guide for M2]
├─ M2_PROGRESS.md               [M2 progress report]
├─ M2_COMPLETE_SUMMARY.md       [M2 comprehensive summary]
├─ NEXT_STEPS.md                [M2 completion + M3 roadmap]
│
├─ CMakeLists.txt               [Top-level build config]
├─ CMakePresets.json            [Build presets]
├─ vcpkg.json                   [Dependency manifest]
├─ proto/
│  └─ jobservice.proto          [gRPC service contract]
│
├─ common/                      [Shared interfaces & implementations]
│  ├─ include/common/
│  │  ├─ JobSpec.h
│  │  ├─ ChunkSpec.h
│  │  ├─ TaskResult.h
│  │  ├─ IScheduler.h
│  │  ├─ ISplitStrategy.h
│  │  └─ IFFmpegExecutor.h
│  └─ src/
│     ├─ TimeBasedSplitStrategy.cpp
│     ├─ FFmpegExecutor.cpp
│     ├─ LeastLoadedScheduler.cpp
│     └─ RoundRobinScheduler.cpp
│
├─ master/                      [Master node - job orchestration]
│  ├─ include/master/
│  │  ├─ JobDispatcher.h
│  │  ├─ WorkerProxy.h
│  │  └─ WorkerRegistry.h
│  └─ src/
│     ├─ main.cpp              [Entry point]
│     ├─ JobDispatcher.cpp      [Core orchestration (M2)]
│     └─ WorkerProxy.cpp        [gRPC client (M2)]
│
├─ worker/                      [Worker node - transcode executor]
│  ├─ include/worker/
│  │  └─ WorkerServiceImpl.h     [RPC handler (M2)]
│  └─ src/
│     ├─ main.cpp              [Entry point (M2)]
│     └─ WorkerServiceImpl.cpp   [RPC implementation (M2)]
│
├─ samples/
│  └─ single_machine/           [M1 baseline - single-machine demo]
│     └─ src/main.cpp
│
├─ tests/                       [Unit & integration tests]
│  ├─ CMakeLists.txt
│  ├─ common/
│  │  ├─ test_split_strategy.cpp
│  │  └─ test_scheduler.cpp
│  ├─ master/
│  │  └─ test_job_dispatcher.cpp
│  └─ worker/
│     └─ test_worker_service.cpp
│
└─ docker/                      [Container definitions]
   ├─ Dockerfile.base
   ├─ Dockerfile.master
   ├─ Dockerfile.worker
   └─ docker-compose.yml
```

---

## Key Concepts

### Split → Dispatch → Reassemble Pipeline

```
Input: sample.mp4 (12 seconds)
  ↓
[Split] → 4 chunks (3s each)
  ├─ Chunk 0: 0–3s
  ├─ Chunk 1: 3–6s
  ├─ Chunk 2: 6–9s
  └─ Chunk 3: 9–12s
  ↓
[Dispatch] → Send to workers
  ├─ Worker 1: Chunk 0 → output_0.mp4
  ├─ Worker 1: Chunk 1 → output_1.mp4
  ├─ Worker 1: Chunk 2 → output_2.mp4
  └─ Worker 1: Chunk 3 → output_3.mp4
  ↓
[Reassemble] → Merge chunks
  ├─ ffmpeg concat
  └─ output.mp4 (12 seconds, re-encoded)
```

### gRPC Message Flow

```
Master (client)                 Worker (service)
  │
  ├─→ ChunkRequest ─────────────→│
  │    - chunk_id                │
  │    - job_id                  │
  │    - codec, filters, etc.    │
  │                              │
  │◄─ ProgressUpdate ◄─ FFmpeg exec
  │    - percent_complete        │
  │    - status                  │
  │                              │
  │◄─ ProgressUpdate (100%) ◄────│ Done
  │
```

---

## Build & Test Workflow

### Development Loop

```
1. Make code changes
   ↓
2. cmake --build --preset dev --parallel
   ↓
3. ./build/dev/worker/worker.exe --port 50052 &
   ↓
4. ./build/dev/master/master.exe sample.mp4 4 output.mp4 --worker-address localhost:50052
   ↓
5. ffprobe output.mp4 (verify duration)
   ↓
6. git commit -m "Feature: ..."
```

### CI/CD Pipeline (Future)

```
GitHub Actions (on push to main)
  ├─ Windows: cmake --build --preset dev --parallel
  ├─ Ubuntu: docker compose build && docker compose up
  ├─ Run tests: ctest --preset dev
  ├─ Lint: clang-tidy + clang-format
  └─ Coverage: gcov (if added)
```

---

## Troubleshooting Guide

### Build Issues

**Q: CMake error about vcpkg.cmake?**  
A: Set `$env:VCPKG_ROOT = "C:\vcpkg"` and retry

**Q: Linker error for gRPC?**  
A: Rebuild from scratch: `cmake --build --preset dev --clean-first`

**Q: Proto files not generating?**  
A: Check `proto/jobservice.proto` syntax, ensure `find_package(Protobuf)` succeeded

### Runtime Issues

**Q: Worker won't start?**  
A: Check port isn't in use; try `--port 50053`

**Q: Master hangs waiting for worker?**  
A: Ensure worker is running; check `--worker-address` matches (e.g., `localhost:50052`)

**Q: Output video is wrong duration?**  
A: Bug in split/merge; compare with M1 baseline (`single_machine_demo.exe`)

See **[M2_BUILD_AND_TEST.md](M2_BUILD_AND_TEST.md) § Debugging Checklist** for more.

---

## Performance Benchmarks (Expected)

| Scenario | Time | Notes |
|---|---|---|
| Build (clean) | 5–15 min | First-time vcpkg download |
| Build (incremental) | 30 sec | After first build |
| M1 baseline (1 worker, local) | 4–8 sec | 12s video, 4 chunks |
| M2 (1 worker, gRPC) | 5–10 sec | +RPC overhead, same chunks |
| M3 (2 workers, parallel) | 3–6 sec | 50% faster |
| M3 (4 workers, parallel) | 2–4 sec | 75% faster |

---

## Contribution Guidelines

### Before Committing

1. **Format code:** `clang-format -i file.cpp`
2. **Check for errors:** `clang-tidy file.cpp`
3. **Run tests:** `ctest --preset dev`
4. **Write clear commit message:** `git commit -m "M2: Feature description"`

### Commit Message Format

```
<Milestone>: <Short description>

Longer explanation if needed. Mention:
- What changed
- Why it changed
- Any known limitations
```

Example:
```
M2: Implement WorkerProxy::dispatch() with streaming RPC

- Build gRPC request from ChunkSpec + JobSpec
- Call worker's TranscodeChunk() streaming RPC
- Parse ProgressUpdate stream, return TaskResult
- Add timeout logic (300s per chunk)
- Log all errors with worker address context
```

---

## License & Attribution

Project follows the structure and patterns defined in `DEVDOC.md`. All code is original implementation by the development team.

---

## Contact & Questions

For questions about:
- **Architecture:** See `M2_COMPLETE_SUMMARY.md`
- **Build issues:** See `M2_BUILD_AND_TEST.md` § Debugging
- **Progress:** See `STATUS.md`
- **Next steps:** See `NEXT_STEPS.md`

---

## Document Maintenance

| Document | Last Updated | Maintains | Review Cycle |
|---|---|---|---|
| This file (DOCS_INDEX.md) | 2026-07-16 | All docs | Per milestone |
| STATUS.md | 2026-07-16 | Status | Weekly |
| M2_*.md | 2026-07-16 | M2 | After build success |
| DEVDOC.md | 2026-05-xx | Full spec | Quarterly |

---

**Version:** 1.0  
**Last Updated:** 2026-07-16T11:00:00Z  
**Maintainer:** Development Team  
**Status:** Current (M2 complete)
