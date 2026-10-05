# Distributed FFmpeg Render Farm — Development Document

**Purpose of this document:** This is a build specification intended to be consumed by an AI coding assistant (or a human developer) to scaffold and implement the project end-to-end. It defines scope, architecture, module boundaries, exact dependencies, build tooling, coding standards, and milestone acceptance criteria so that generated code is consistent, buildable, and testable at every step.

---

## 1. Project Summary

A distributed video transcoding system written in C++20. A **Master** node accepts a transcode job (source video + target spec), splits it into chunks, dispatches chunks to registered **Worker** nodes over gRPC, each worker runs FFmpeg locally on its chunk, uploads the result to shared/object storage, and the Master reassembles the final output. The system follows SOLID principles and uses well-known design patterns (Strategy, Factory, Command, Observer, State, Proxy, Decorator, Adapter, Builder) to keep components swappable and testable.

### Non-goals (explicitly out of scope for v1)
- No web UI (CLI + simple status endpoint only).
- No authentication/authorization layer (assume trusted internal network for v1).
- No live/real-time streaming transcode (batch VOD only).
- No automatic worker auto-scaling/provisioning (workers are manually started; only registration/discovery is automatic).

---

## 2. Target Environment

| Item | Value |
|---|---|
| OS (dev/build) | Windows 11 (or Windows 10 21H2+) |
| Language standard | C++20 |
| Compiler | MSVC v143 (Visual Studio 2022, 17.9+) — primary. Clang-cl via VS is acceptable as alternate toolset. |
| Architecture | x64 (primary), arm64 (best-effort via VS ARM64 toolset) |
| Shell | PowerShell 7+ (pwsh) for all scripts — avoid bash-only syntax |
| Containerization | Docker Desktop for Windows with WSL2 backend, Docker Compose v2 (bundled with Docker Desktop) |
| WSL2 | Required for running Linux-based worker containers (`master`/`worker` containers run as Linux containers under WSL2, even though native dev builds target native Windows) |

> **Two build targets to keep straight:** (1) **Native Windows build** — MSVC-compiled `master.exe`/`worker.exe`, useful for local debugging in Visual Studio; (2) **Linux containers via Docker Desktop/WSL2** — used for actual deployment/CI parity, since the FFmpeg + gRPC + vcpkg toolchain is far more battle-tested on Linux. Both are supported; native Windows is for dev-loop speed, containers are for the "real" deployable artifact.

---

## 3. Full Dependency List

### 3.1 System / Native Libraries

| Dependency | Version (min) | Purpose | Install method (Windows) |
|---|---|---|---|
| FFmpeg (CLI binaries: `ffmpeg.exe`, `ffprobe.exe`) | 6.0+ | Actual transcoding, keyframe/metadata probing | Download official builds from gyan.dev (`https://www.gyan.dev/ffmpeg/builds/`) or BtbN builds; add folder to `PATH`, or via `winget install Gyan.FFmpeg` / `choco install ffmpeg` |
| gRPC (`grpc`, `grpc++`) | 1.60+ | Master↔Worker RPC | vcpkg (`vcpkg install grpc:x64-windows`) |
| Protocol Buffers (`protobuf`) | 3.21+ | RPC message serialization | vcpkg (bundled with gRPC port) |
| Boost (`boost-asio`, `boost-filesystem`, `boost-system`) | 1.82+ | Networking fallback utilities, filesystem ops | vcpkg |
| spdlog | 1.13+ | Structured logging | vcpkg |
| nlohmann/json | 3.11+ | JSON config parsing, JSON-based job specs | vcpkg |
| GoogleTest + GoogleMock | 1.14+ | Unit testing / mocking interfaces | vcpkg |
| AWS SDK for C++ (`aws-sdk-cpp`, S3 module only) OR libcurl + minimal S3 client | latest / 8.x | Object storage client (MinIO/S3-compatible) | vcpkg (`aws-sdk-cpp[s3]:x64-windows`) — note: long build time on Windows (30-60 min first build), see §7.6 |
| CLI11 | 2.4+ | Command-line argument parsing for `master`/`worker` binaries | vcpkg |
| SQLite3 | 3.45+ | Lightweight job/task state persistence (avoids needing external DB for v1) | vcpkg |

> All C++ dependencies are managed through **vcpkg in manifest mode** with triplet `x64-windows` (or `x64-windows-static` if you want fully static binaries with no runtime DLL dependencies — recommended for easier distribution of `.exe` files). FFmpeg itself is **not** built from source or fetched via vcpkg — use prebuilt official Windows binaries and shell out to them as subprocesses, same as on Linux.

### 3.2 Build Tooling

| Tool | Version (min) | Purpose | Windows notes |
|---|---|---|---|
| Visual Studio 2022 | 17.9+ (Community/Pro/Enterprise) | IDE + MSVC compiler + Windows SDK | Install workload **"Desktop development with C++"**; this also brings a bundled CMake/Ninja and a bundled copy of vcpkg (VS 2022 17.6+ has vcpkg built in, but a standalone vcpkg clone is still recommended for manifest-mode control) |
| CMake | 3.26+ | Primary build system, target-based | Use the one bundled with VS, or install standalone via `winget install Kitware.CMake` |
| Ninja | 1.11+ | Fast build backend (used via `-G Ninja`) | Bundled with VS "Desktop development with C++" workload, or `winget install Ninja-build.Ninja` |
| vcpkg | latest (manifest mode) | Dependency package manager | Clone standalone (see §7.1) rather than relying solely on the VS-bundled copy, for reproducibility across machines/CI |
| protoc + grpc_cpp_plugin | matching protobuf/grpc version | Codegen from `.proto` files (invoked automatically via CMake `protobuf_generate`) | Comes from the vcpkg `grpc`/`protobuf` ports; `.exe` tools land in `vcpkg_installed/x64-windows/tools/` |
| ccache / sccache | latest | Compilation caching | **sccache** is preferred over ccache on Windows (better MSVC support): `winget install Mozilla.Sccache` or `cargo install sccache` |
| clang-format | 17+ | Code formatting (config in `.clang-format`) | Bundled with VS "C++ Clang tools for Windows" optional component, or `winget install LLVM.LLVM` |
| clang-tidy | 17+ | Static analysis / lint | Same LLVM install as above |
| Git for Windows | 2.44+ | Version control, needed for vcpkg bootstrap | `winget install Git.Git` |
| Docker Desktop | 4.28+ | Containerized build & runtime (via WSL2 backend) | `winget install Docker.DockerDesktop`; enable WSL2 integration during setup |
| WSL2 + a Linux distro (Ubuntu) | latest | Backend for Docker Desktop, and optional Linux-side testing | `wsl --install -d Ubuntu` from an elevated PowerShell |
| Docker Compose | v2 (plugin) | Local multi-node orchestration (1 master + N workers + MinIO) | Bundled with Docker Desktop |
| GitHub Actions | n/a | CI: build + unit test + lint on every PR | Use `windows-latest` runner for the native build job, `ubuntu-latest` for the container build job (see §9) |
| PowerShell 7 | 7.4+ | Script execution (`.ps1` replacing `.sh`) | `winget install Microsoft.PowerShell` |

### 3.3 Optional / Nice-to-have

| Tool | Purpose |
|---|---|
| MinIO (Docker image `minio/minio`) | Local S3-compatible object storage for dev/testing |
| grpcurl | Manual gRPC endpoint testing from CLI |
| Wireshark / tcpdump | Debugging network issues between master/worker |
| perf / gprof | Profiling FFmpeg subprocess and scheduler performance |

---

## 4. Repository Layout

```
distributed-ffmpeg/
├── CMakeLists.txt                 # top-level, adds subdirectories
├── vcpkg.json                     # manifest: pins all dependency versions
├── vcpkg-configuration.json       # registry/baseline pin
├── .clang-format
├── .clang-tidy
├── CMakePresets.json              # dev/release/docker presets
├── proto/
│   └── jobservice.proto
├── common/                        # shared interfaces + value types, no I/O
│   ├── CMakeLists.txt
│   ├── include/common/
│   │   ├── JobSpec.h
│   │   ├── ChunkSpec.h
│   │   ├── TaskResult.h
│   │   ├── ISplitStrategy.h
│   │   ├── IFFmpegExecutor.h
│   │   ├── IStorageClient.h
│   │   ├── IScheduler.h
│   │   ├── Job.h                  # State pattern
│   │   └── Observer.h             # Observer pattern base
│   └── src/
│       ├── KeyframeSplitStrategy.cpp
│       ├── TimeBasedSplitStrategy.cpp
│       ├── FFmpegExecutor.cpp
│       ├── RetryExecutorDecorator.cpp
│       ├── LoggingExecutorDecorator.cpp
│       ├── LocalFSStorageClient.cpp
│       ├── MinIOStorageClient.cpp
│       ├── LeastLoadedScheduler.cpp
│       ├── RoundRobinScheduler.cpp
│       └── Job.cpp
├── master/
│   ├── CMakeLists.txt
│   ├── include/master/
│   │   ├── Master.h
│   │   ├── WorkerProxy.h          # Proxy pattern
│   │   ├── JobDispatcher.h
│   │   ├── WorkerRegistry.h
│   │   └── JobSpecBuilder.h       # Builder pattern
│   └── src/
│       ├── main.cpp
│       ├── Master.cpp
│       ├── WorkerProxy.cpp
│       ├── JobDispatcher.cpp
│       └── WorkerRegistry.cpp
├── worker/
│   ├── CMakeLists.txt
│   ├── include/worker/
│   │   └── WorkerServiceImpl.h
│   └── src/
│       ├── main.cpp
│       └── WorkerServiceImpl.cpp
├── tests/
│   ├── CMakeLists.txt
│   ├── common/
│   │   ├── test_split_strategy.cpp
│   │   ├── test_ffmpeg_executor.cpp   # uses GoogleMock on IFFmpegExecutor
│   │   ├── test_scheduler.cpp
│   │   └── test_job_state_machine.cpp
│   ├── master/
│   │   └── test_job_dispatcher.cpp
│   └── worker/
│       └── test_worker_service.cpp
├── docker/
│   ├── Dockerfile.master
│   ├── Dockerfile.worker
│   ├── Dockerfile.base            # shared base image w/ ffmpeg + built deps
│   └── docker-compose.yml         # 1 master + 3 workers + minio
├── scripts/
│   ├── setup_vcpkg.ps1
│   ├── run_local_demo.ps1
│   └── gen_proto.ps1
├── .github/workflows/
│   └── ci.yml            # matrix: windows-latest (native build+test), ubuntu-latest (container build)
└── README.md
```

---

## 5. Interface Contracts (build these first, exactly as specified)

> AI implementation note: build `common/` before `master/` or `worker/`. Every class in `master`/`worker` must depend only on these interfaces (Dependency Inversion), never on concrete implementations directly — concrete types are wired in `main.cpp` via constructor injection.

```cpp
// common/include/common/JobSpec.h
struct JobSpec {
    std::string jobId;
    std::string sourcePath;      // local path or storage key
    std::string outputCodec;     // e.g. "libx264", "libx265", "libaom-av1"
    std::string resolution;      // e.g. "1920x1080", empty = keep source
    int bitrateKbps = 0;         // 0 = use CRF instead
    int crf = 23;
    std::vector<std::string> extraFilters;
    int numChunks = 4;
};

// common/include/common/ChunkSpec.h
struct ChunkSpec {
    std::string chunkId;
    std::string jobId;
    int index;
    double startTimeSec;
    double durationSec;
    std::string sourceStorageKey;
    std::string outputStorageKey;
};

// common/include/common/TaskResult.h
enum class TaskStatus { Pending, Running, Success, Failed, Retrying };
struct TaskResult {
    TaskStatus status;
    std::string message;
    std::string outputPath;
    double durationMs = 0;
};

// common/include/common/ISplitStrategy.h
class ISplitStrategy {
public:
    virtual std::vector<ChunkSpec> split(const JobSpec& spec) = 0;
    virtual ~ISplitStrategy() = default;
};

// common/include/common/IFFmpegExecutor.h
class IFFmpegExecutor {
public:
    virtual TaskResult run(const ChunkSpec& chunk, const JobSpec& spec) = 0;
    virtual ~IFFmpegExecutor() = default;
};

// common/include/common/IStorageClient.h
class IStorageClient {
public:
    virtual void upload(const std::string& localPath, const std::string& remoteKey) = 0;
    virtual void download(const std::string& remoteKey, const std::string& localPath) = 0;
    virtual bool exists(const std::string& remoteKey) = 0;
    virtual ~IStorageClient() = default;
};

// common/include/common/IScheduler.h
struct WorkerHandle {
    std::string id;
    std::string address; // host:port
    int activeJobs = 0;
    bool hasGpu = false;
};
class IScheduler {
public:
    virtual WorkerHandle selectWorker(const std::vector<WorkerHandle>& available) = 0;
    virtual ~IScheduler() = default;
};
```

### 5.1 gRPC Service Contract

```protobuf
// proto/jobservice.proto
syntax = "proto3";
package rffmpeg;

service WorkerService {
  rpc TranscodeChunk (ChunkRequest) returns (stream ProgressUpdate);
  rpc Heartbeat (Empty) returns (WorkerStatus);
  rpc CancelJob (JobIdRequest) returns (Empty);
}

message ChunkRequest {
  string chunk_id = 1;
  string job_id = 2;
  string source_storage_key = 3;
  string output_storage_key = 4;
  double start_time_sec = 5;
  double duration_sec = 6;
  string output_codec = 7;
  string resolution = 8;
  int32 bitrate_kbps = 9;
  int32 crf = 10;
  repeated string extra_filters = 11;
}

message ProgressUpdate {
  string chunk_id = 1;
  double percent_complete = 2;
  string status = 3; // "running" | "success" | "failed"
  string message = 4;
}

message Empty {}

message WorkerStatus {
  string worker_id = 1;
  int32 active_jobs = 2;
  bool has_gpu = 3;
  double cpu_load = 4;
}

message JobIdRequest {
  string job_id = 1;
}
```

---

## 6. Design Pattern → File Mapping (for AI traceability)

| Pattern | File(s) | Notes |
|---|---|---|
| Strategy | `ISplitStrategy` + `KeyframeSplitStrategy`/`TimeBasedSplitStrategy`; `IScheduler` + `LeastLoadedScheduler`/`RoundRobinScheduler` | Swappable at construction time via CLI flag |
| Factory | `FFmpegCommandFactory.h/.cpp` | Builds `std::vector<std::string>` of ffmpeg CLI args from `JobSpec`+`ChunkSpec` |
| Command | `ITask` interface, `TranscodeCommand` | Encapsulates one chunk job; supports `execute()`, `undo()` is N/A but supports `cancel()` |
| Observer | `Observer.h`, `Job::attach/notify` | Master/Dashboard subscribes to job state changes |
| State | `Job.h/.cpp`, `enum class JobState` | Enforce legal transitions only (e.g. can't go Done→Running) |
| Proxy | `WorkerProxy.h/.cpp` | Wraps gRPC stub; local-feeling `.dispatch()` call |
| Decorator | `RetryExecutorDecorator`, `LoggingExecutorDecorator` | Wrap `IFFmpegExecutor`; chainable |
| Adapter | `LocalFSStorageClient`, `MinIOStorageClient` | Both implement `IStorageClient` |
| Builder | `JobSpecBuilder.h/.cpp` | Fluent API: `.withCodec().withResolution().withCRF().build()` |
| Singleton (used sparingly) | `Logger` wraps spdlog global logger behind `ILogger` interface — inject, don't call statically, to keep testability |

---

## 7. Build Instructions (Windows)

### 7.1 One-time setup (PowerShell, run as Administrator where noted)
```powershell
# Install core tooling via winget
winget install Microsoft.VisualStudio.2022.Community  # or use existing VS install; ensure "Desktop development with C++" workload is checked
winget install Git.Git
winget install Kitware.CMake
winget install Ninja-build.Ninja
winget install Microsoft.PowerShell
winget install Docker.DockerDesktop
winget install Gyan.FFmpeg

# Enable WSL2 (elevated PowerShell, then reboot if prompted)
wsl --install -d Ubuntu

# Clone and bootstrap vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# Set VCPKG_ROOT permanently (User scope) so CMake presets can find it
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")

# Verify ffmpeg is on PATH
ffmpeg -version
```
> After installing FFmpeg via winget/choco, confirm `ffmpeg.exe`/`ffprobe.exe` are resolvable from a fresh terminal (`Get-Command ffmpeg`). If not, add the install directory to the User `PATH` environment variable manually via System Properties → Environment Variables.

> Open a **"x64 Native Tools Command Prompt for VS 2022"** or use the VS Developer PowerShell profile when building outside the IDE, so `cl.exe` and the Windows SDK are on `PATH`.

### 7.2 vcpkg.json (manifest — place at repo root)
```json
{
  "name": "distributed-ffmpeg",
  "version": "0.1.0",
  "dependencies": [
    "grpc",
    "protobuf",
    { "name": "boost-asio", "version>=": "1.82.0" },
    "boost-filesystem",
    "spdlog",
    "nlohmann-json",
    { "name": "gtest", "version>=": "1.14.0" },
    { "name": "aws-sdk-cpp", "features": ["s3"] },
    "cli11",
    "sqlite3"
  ]
}
```

### 7.3 Configure & build
```powershell
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev --output-on-failure
```
Or simply open the repo root folder in **Visual Studio 2022** (`File → Open → Folder`) — VS auto-detects `CMakePresets.json` and `vcpkg.json`, restores dependencies, and exposes `master.exe`/`worker.exe`/test targets directly in the Solution Explorer / Debug dropdown for one-click build & debug (breakpoints, watch windows, etc. work out of the box).

### 7.4 CMakePresets.json (expected shape)
```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "dev",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/dev",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      },
      "environment": {
        "CMAKE_C_COMPILER_LAUNCHER": "sccache",
        "CMAKE_CXX_COMPILER_LAUNCHER": "sccache"
      }
    },
    {
      "name": "release",
      "inherits": "dev",
      "binaryDir": "${sourceDir}/build/release",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
    },
    {
      "name": "dev-static",
      "inherits": "dev",
      "binaryDir": "${sourceDir}/build/dev-static",
      "cacheVariables": { "VCPKG_TARGET_TRIPLET": "x64-windows-static" }
    }
  ],
  "buildPresets": [
    { "name": "dev", "configurePreset": "dev" },
    { "name": "release", "configurePreset": "release" }
  ],
  "testPresets": [
    { "name": "dev", "configurePreset": "dev", "output": {"outputOnFailure": true} }
  ]
}
```
> `dev` preset produces `.exe` + dynamically-linked vcpkg DLLs (fastest to build/iterate). `dev-static` produces statically-linked binaries with no external DLL dependencies (slower build, but the resulting `master.exe`/`worker.exe` are trivially copyable/deployable — useful once you also containerize, or when handing a binary to someone without the vcpkg runtime DLLs installed).

### 7.5 Docker (recommended path for AI to validate builds reproducibly, and for the actual deployment artifact)
Docker Desktop on Windows runs **Linux containers via the WSL2 backend** by default — the `master`/`worker` containers themselves are built from a Linux (Ubuntu) base image, same Dockerfiles as a Linux dev would use. This is intentional: it gives you a clean, reproducible Linux build/runtime target regardless of the Windows host, and matches whatever Linux environment you'd deploy to in production (e.g. Kubernetes).
```powershell
docker compose -f docker/docker-compose.yml build
docker compose -f docker/docker-compose.yml up
```
`docker/Dockerfile.base` should: start from `ubuntu:24.04`, install FFmpeg via apt (`apt-get install -y ffmpeg`), install build-essential/cmake/ninja/ccache, bootstrap vcpkg, and build+cache the vcpkg dependency set as a separate layer so app-code changes don't re-trigger dependency rebuilds.

> Ensure Docker Desktop → Settings → Resources → WSL Integration has your Ubuntu distro enabled, and that file sharing/mounts for the repo drive (e.g. `C:\`) are enabled under Settings → Resources → File Sharing, otherwise bind-mounts in `docker-compose.yml` will fail silently.

### 7.6 Known Windows-specific build friction (flag for AI/dev awareness)
- **`aws-sdk-cpp` build time:** first `vcpkg install` of `aws-sdk-cpp[s3]:x64-windows` can take 30–60+ minutes on Windows due to lack of prebuilt binary caching unless you configure a **vcpkg binary cache** (`$env:VCPKG_BINARY_SOURCES = "clear;x-gha,readwrite"` in CI, or a local `nuget`/`files` cache locally). Set this up early to avoid repeated long rebuilds.
- **Path length limits:** vcpkg + deep header trees (esp. gRPC/protobuf/AWS SDK) can hit Windows `MAX_PATH` (260 char) issues. Mitigate by: enabling long paths (`Enable-WindowsOptionalFeature` / registry `LongPathsEnabled=1`), and keeping the repo close to drive root (e.g. `C:\dev\distributed-ffmpeg` rather than deeply nested user folders).
- **Line endings:** set `git config --global core.autocrlf true` (or add a repo `.gitattributes` forcing LF for `.proto`/`.cpp`/`.h`/`.cmake`) so CI (which may run on Linux runners for container jobs) doesn't choke on CRLF in build scripts.
- **Subprocess spawning of `ffmpeg.exe`:** use `CreateProcessW`/`_wspawnv` semantics (or a wrapper like `boost::process` which handles this portably) rather than POSIX `fork`/`exec` — Windows has no `fork()`. `boost::process` (header-only, ships with Boost) is the recommended abstraction so the same `IFFmpegExecutor` implementation code compiles on both Windows and Linux with minimal `#ifdef`s.
- **Antivirus/Defender overhead:** add the repo and `build/` directory to Windows Defender exclusions to meaningfully speed up incremental builds.

---

## 8. Coding Standards

- **Style:** Google C++ Style Guide baseline, enforced via `.clang-format` (LLVM-based, 100 col width, 2-space indent — adjust as preferred but keep consistent).
- **Naming:** `PascalCase` for types/classes, `camelCase` for methods/variables, `kConstantName` for constants, `ISomething` prefix for pure interfaces.
- **Memory:** prefer `std::unique_ptr`/`std::shared_ptr` over raw `new`/`delete`; use `std::make_unique`/`std::make_shared`. No raw owning pointers.
- **Error handling:** use exceptions for unrecoverable construction errors; use `TaskResult`/`std::expected`-style return values (or `absl::StatusOr` if abseil is added) for recoverable runtime failures inside hot paths (avoid exceptions in the chunk-processing loop for performance/clarity).
- **Concurrency:** use `std::jthread` (C++20) with `std::stop_token` for cancellable worker threads; guard shared state with `std::mutex` + `std::lock_guard`/`std::scoped_lock`; prefer message-passing (queues) over shared mutable state where possible.
- **Process spawning:** use `boost::process` (not raw Win32 `CreateProcess` or POSIX `fork`/`exec`) to launch `ffmpeg.exe`/`ffprobe.exe` as subprocesses, so `FFmpegExecutor` is portable across Windows and Linux from the same source without `#ifdef` branches.
- **Every public interface must have a corresponding GoogleMock-friendly abstract base** (already reflected in Section 5) so unit tests never spawn real `ffmpeg` processes or hit real storage.
- **No global mutable state** except behind an injected `ILogger`/`IConfig` interface.

---

## 9. Testing Strategy

| Layer | Approach |
|---|---|
| `common/` classes | Pure unit tests, GoogleTest. Mock `IFFmpegExecutor`/`IStorageClient` with GoogleMock where a class depends on them. |
| `JobDispatcher` (master) | Unit test with mocked `IScheduler` + mocked `WorkerProxy` (inject a fake). |
| `WorkerServiceImpl` | Unit test the gRPC handler logic with a mocked `IFFmpegExecutor`, no real subprocess. |
| Integration | `scripts/run_local_demo.ps1` spins up docker-compose stack (1 master, 2 workers, MinIO) and runs a real small sample `.mp4` through the full pipeline end-to-end; assert output file exists, duration matches source (via `ffprobe`), and checksums of reassembled output are stable across repeated runs. |
| CI | GitHub Actions, two jobs on every PR: (1) `windows-latest` — native `cmake --build`/`ctest` for fast unit-test signal; (2) `ubuntu-latest` — `docker compose build` + integration test, for deployment-parity signal. Both run `clang-tidy` (warnings-as-errors on new code) and `clang-format --dry-run --Werror`. |

Sample test video for integration tests: generate synthetically at test-time with FFmpeg itself (don't commit binary video assets to the repo):
```powershell
ffmpeg -f lavfi -i testsrc=duration=30:size=1280x720:rate=30 -f lavfi -i sine=frequency=1000:duration=30 -c:v libx264 -c:a aac sample.mp4
```

---

## 10. Milestones — Exact Build & Test Procedure

> **How to use this section:** each milestone lists (a) exact preconditions, (b) exact files to create/edit with what must go in them, (c) exact commands to build, (d) exact commands to run, (e) exact test steps with the specific expected output/exit code to check against. Do not skip steps, do not add functionality from a later milestone while implementing an earlier one, and do not proceed to the next milestone until every item in that milestone's "Definition of Done Checklist" is checked off. Commit to git with message `M<N>: <short description>` immediately after a milestone's checklist passes, before starting the next milestone.

---

### M1 — Single-machine chunk / transcode / reassemble (no networking)

**Preconditions**
- `ffmpeg -version` and `ffprobe -version` both succeed in the terminal you will build/run from.
- `cmake --version` reports ≥ 3.26, `ninja --version` succeeds.
- `$env:VCPKG_ROOT` is set and `$env:VCPKG_ROOT\vcpkg.exe` exists.

**Step 1 — Repo skeleton**
Create the following empty/near-empty files exactly as named (paths relative to repo root):
```
CMakeLists.txt
vcpkg.json
CMakePresets.json
common/CMakeLists.txt
common/include/common/JobSpec.h
common/include/common/ChunkSpec.h
common/include/common/TaskResult.h
common/include/common/ISplitStrategy.h
common/include/common/IFFmpegExecutor.h
common/include/common/TimeBasedSplitStrategy.h
common/include/common/FFmpegExecutor.h
common/src/TimeBasedSplitStrategy.cpp
common/src/FFmpegExecutor.cpp
samples/single_machine/CMakeLists.txt
samples/single_machine/src/main.cpp
```
Do **not** create `master/`, `worker/`, `proto/`, `docker/`, or `tests/` yet — those belong to later milestones. Keeping M1 scoped to only these files is itself part of the acceptance check.

**Step 2 — Populate the value types and interfaces**
Copy `JobSpec`, `ChunkSpec`, `TaskResult`, `ISplitStrategy`, `IFFmpegExecutor` verbatim from Section 5 of this document into their matching header files. Do not add fields not listed there at this stage — extra fields belong to later milestones (e.g. do not add gRPC-related fields yet).

**Step 3 — Implement `TimeBasedSplitStrategy`**
In `common/include/common/TimeBasedSplitStrategy.h`, declare a class `TimeBasedSplitStrategy : public ISplitStrategy` with `std::vector<ChunkSpec> split(const JobSpec& spec) override;`.
In `common/src/TimeBasedSplitStrategy.cpp`:
1. Implement a private helper that calls `ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 <source>` via `boost::process::child` with `boost::process::std_out > pipe_stream`, **not** `popen`/`std::system` (avoid shell string concatenation — pass the source path as a separate argv element to `boost::process::child`, never interpolated into a shell string, so paths containing spaces or quotes cannot break or inject into the command).
2. Parse the duration as a `double`.
3. Divide by `spec.numChunks`, produce that many `ChunkSpec` entries with `startTimeSec`/`durationSec` set so consecutive chunks are contiguous and the last chunk absorbs any rounding remainder (`durationSec = totalDuration - startTimeSec` for the final chunk only).
4. Populate `chunkId` as `spec.jobId + "_" + index`, `sourceStorageKey` as `spec.sourcePath` (local path is used directly as the storage key at this milestone since there is no real storage layer yet), and `outputStorageKey` as a path under a local `work/` directory.

**Step 4 — Implement `FFmpegExecutor`**
In `common/include/common/FFmpegExecutor.h`, declare `class FFmpegExecutor : public IFFmpegExecutor` with `TaskResult run(const ChunkSpec& chunk, const JobSpec& spec) override;`.
In `common/src/FFmpegExecutor.cpp`:
1. Build the argv vector explicitly, e.g. `{"-y", "-ss", std::to_string(chunk.startTimeSec), "-t", std::to_string(chunk.durationSec), "-i", chunk.sourceStorageKey, "-c:v", codec, "-preset", "veryfast", "-crf", std::to_string(spec.crf), outPath}` — each element a separate argv entry, never concatenated into one shell string.
2. Launch via `boost::process::child` pointed at the ffmpeg binary resolved with `boost::process::search_path("ffmpeg")`.
3. Wait for exit, capture exit code.
4. On exit code 0, set `TaskResult.status = TaskStatus::Success` and `outputPath` to the produced file; otherwise `TaskStatus::Failed` with the exit code in `message`.
5. Record wall-clock duration in `durationMs` using `std::chrono::steady_clock`.

**Step 5 — Implement the single-machine demo**
In `samples/single_machine/src/main.cpp`:
1. Parse 3 required CLI args: source path, number of chunks, output path. Print a usage string and exit code `1` if fewer than 3 args are given.
2. Build a `JobSpec`, call `TimeBasedSplitStrategy::split`.
3. For each `ChunkSpec`, call `FFmpegExecutor::run`; if any chunk's `TaskResult.status != TaskStatus::Success`, print the error and exit code `2` immediately (do not attempt reassembly with missing chunks).
4. Write an ffmpeg concat-demuxer list file (`file '<path>'` per line) into `work/concat_list.txt`.
5. Invoke `ffmpeg -y -f concat -safe 0 -i work/concat_list.txt -c copy <output>` via `boost::process::child` (again, argv array, not a shell string) to reassemble. Exit code `3` if this fails.
6. On success print `Output written to <output>` and exit code `0`.

**Step 6 — Wire up CMake**
- `common/CMakeLists.txt`: define a static library target `common` from the files in Step 2–4; `find_package(Boost REQUIRED COMPONENTS filesystem)` and link `Boost::process` (header-only, but link `Boost::filesystem` which it depends on).
- `samples/single_machine/CMakeLists.txt`: define an executable target `single_machine_demo` linking against `common`.
- Top-level `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.26)`, `project(distributed-ffmpeg CXX)`, `set(CMAKE_CXX_STANDARD 20)`, `add_subdirectory(common)`, `add_subdirectory(samples/single_machine)`.
- `vcpkg.json`: at this milestone only needs `boost-process`, `boost-filesystem`. Do not add `grpc`, `protobuf`, `aws-sdk-cpp`, `gtest`, etc. yet — pull those in at the milestones that actually need them, so build times stay short while M1 is being iterated on.
- `CMakePresets.json`: copy the `dev` preset from Section 7.4 verbatim.

**Step 7 — Build**
```powershell
cmake --preset dev
cmake --build --preset dev --parallel
```
Expected result: build succeeds with zero errors; `build/dev/samples/single_machine/single_machine_demo.exe` exists.

**Step 8 — Generate a test video**
```powershell
ffmpeg -f lavfi -i testsrc=duration=12:size=640x360:rate=30 -f lavfi -i sine=frequency=1000:duration=12 -c:v libx264 -c:a aac -shortest sample.mp4
```
Expected result: `sample.mp4` exists, and `ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 sample.mp4` prints a value close to `12.0`.

**Step 9 — Run the demo**
```powershell
.\build\dev\samples\single_machine\single_machine_demo.exe sample.mp4 4 output.mp4
```
Expected result: exit code `0`; console prints `Output written to output.mp4`; four intermediate chunk files exist under `work/`.

**Step 10 — Verify correctness**
```powershell
ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 output.mp4
```
Expected result: printed duration is within **0.1 seconds** of the source's `12.0`. Also visually confirm (open `output.mp4` in a player) that the video plays start-to-finish with no visible seams/black frames at the 3 chunk boundaries (~3s, 6s, 9s marks).

**Step 11 — Negative-path test**
```powershell
.\build\dev\samples\single_machine\single_machine_demo.exe nonexistent.mp4 4 output.mp4
```
Expected result: non-zero exit code (`2`), and a clear error message referencing the failed chunk — **not** a crash/segfault and **not** a hang.

**Definition of Done Checklist (M1)**
- [ ] `cmake --build --preset dev` exits 0 with no warnings treated as errors disabled (warnings are OK for now, errors are not).
- [ ] Step 9 produces `output.mp4` with duration matching Step 10's tolerance.
- [ ] Step 11 fails cleanly with a non-zero exit code and readable error text.
- [ ] No `std::system(...)` or `popen(...)` calls anywhere in `common/` (grep to confirm: `Select-String -Path common\src\*.cpp -Pattern "std::system|popen"` returns no matches).
- [ ] `git commit -m "M1: single-machine chunk/transcode/reassemble scaffold"`.

---

### M2 — gRPC wiring with one hardcoded worker

**Preconditions:** M1 checklist fully passed and committed.

**Step 1 — Add gRPC/protobuf to the manifest**
Edit `vcpkg.json`, add `"grpc"` and `"protobuf"` to `dependencies`. Re-run `cmake --preset dev` (this triggers a fresh vcpkg install; expect several minutes the first time).

**Step 2 — Add the proto file**
Create `proto/jobservice.proto` with the exact contents from Section 5.1 of this document. Do not add extra RPCs or fields beyond what's specified there yet.

**Step 3 — Wire proto codegen into CMake**
In the top-level `CMakeLists.txt`, add `find_package(protobuf CONFIG REQUIRED)` and `find_package(gRPC CONFIG REQUIRED)`. Create a new target, e.g. `add_library(jobservice_proto proto/jobservice.proto)`, and use `protobuf_generate(TARGET jobservice_proto LANGUAGE cpp)` plus the gRPC-specific generation call (`protobuf_generate(... PLUGIN protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin> ...)`) so `.pb.cc`/`.pb.h`/`.grpc.pb.cc`/`.grpc.pb.h` are generated automatically on every build, not run manually.

**Step 4 — Create the worker skeleton**
Create:
```
worker/CMakeLists.txt
worker/include/worker/WorkerServiceImpl.h
worker/src/WorkerServiceImpl.cpp
worker/src/main.cpp
```
`WorkerServiceImpl` inherits the generated `rffmpeg::WorkerService::Service` base. Implement:
- `Heartbeat`: return a `WorkerStatus` with a hardcoded `worker_id` (e.g. read from a `--worker-id` CLI flag, default `"worker-1"`), `active_jobs = 0`, `has_gpu = false`, `cpu_load = 0.0` for now (real load tracking comes in M3).
- `TranscodeChunk`: for now, internally construct a `ChunkSpec`/`JobSpec` from the incoming `ChunkRequest` fields, call the M1 `FFmpegExecutor::run(...)` synchronously, and stream back exactly one `ProgressUpdate` with `percent_complete = 100.0` and `status = "success"` (or `"failed"`) once it's done — real incremental progress streaming is out of scope for M2.
`worker/src/main.cpp`: parse `--port` (default `50052`) and `--worker-id` via CLI11, build a `grpc::ServerBuilder`, register `WorkerServiceImpl`, call `BuildAndStart()`, then `server->Wait()`.

**Step 5 — Create the master skeleton**
Create:
```
master/CMakeLists.txt
master/include/master/WorkerProxy.h
master/src/WorkerProxy.cpp
master/src/main.cpp
```
`WorkerProxy` wraps a `rffmpeg::WorkerService::Stub` created from `grpc::CreateChannel(address, grpc::InsecureChannelCredentials())`. Implement `WorkerProxy::dispatch(const ChunkSpec&, const JobSpec&) -> TaskResult` which builds a `ChunkRequest`, calls the streaming RPC, reads all `ProgressUpdate`s off the stream, logs each one, and returns a `TaskResult` built from the final update.
`master/src/main.cpp`: parse source path, chunk count, output path, and a hardcoded `--worker-address` (default `localhost:50052`) via CLI11. Reuse M1's `TimeBasedSplitStrategy` to produce chunks, then for each chunk call `WorkerProxy::dispatch` **sequentially** (parallel dispatch is M3), collect outputs, and reassemble exactly as the M1 demo did (you can call the same reassembly code — refactor it into a shared free function in `common/` if not already, e.g. `common::reassembleChunks(...)`, so it isn't duplicated between `samples/single_machine` and `master`).

**Step 6 — Build**
```powershell
cmake --preset dev
cmake --build --preset dev --parallel
```
Expected result: `build/dev/worker/worker.exe` and `build/dev/master/master.exe` both exist, zero build errors.

**Step 7 — Run worker and master locally**
Open two separate terminals.
Terminal A:
```powershell
.\build\dev\worker\worker.exe --port 50052 --worker-id worker-1
```
Expected result: process starts and blocks (no crash, no immediate exit); no console output is required beyond a startup log line.

Terminal B:
```powershell
.\build\dev\master\master.exe sample.mp4 4 output.mp4 --worker-address localhost:50052
```
Expected result: master process runs to completion, printing one progress log line per chunk (4 total) followed by `Output written to output.mp4`; exit code `0`.

**Step 8 — Verify correctness**
Same as M1 Step 10 — check `output.mp4` duration against source via `ffprobe`, and visually confirm no seams.

**Step 9 — Verify Heartbeat RPC independently**
```powershell
grpcurl -plaintext -d "{}" localhost:50052 rffmpeg.WorkerService/Heartbeat
```
Expected result: JSON response with `"workerId": "worker-1"`, `"activeJobs": 0`, `"hasGpu": false`.

**Step 10 — Negative-path test: worker not running**
Stop Terminal A's worker process (Ctrl+C), then re-run Terminal B's master command.
Expected result: master exits with a non-zero code and a clear "failed to connect" / RPC error message — not a silent hang. If it hangs, add an explicit gRPC deadline (e.g. `context.set_deadline(...)` a few seconds out) to `WorkerProxy::dispatch` and re-test until this passes.

**Definition of Done Checklist (M2)**
- [ ] Step 7 completes end-to-end with correct output.
- [ ] Step 9's `grpcurl` call returns a well-formed response.
- [ ] Step 10 fails within a bounded time (a few seconds), not indefinitely.
- [ ] `git commit -m "M2: gRPC wiring, single hardcoded worker"`.

---

### M3 — Multi-worker registry + least-loaded scheduling

**Preconditions:** M2 checklist fully passed and committed.

**Step 1 — Add scheduler interface and implementations**
Create `common/include/common/IScheduler.h` (copy `WorkerHandle`/`IScheduler` verbatim from Section 5), then:
```
common/src/RoundRobinScheduler.cpp   (+ matching header)
common/src/LeastLoadedScheduler.cpp  (+ matching header)
```
`RoundRobinScheduler::selectWorker` cycles through the `available` vector using an internal counter mod `available.size()`. `LeastLoadedScheduler::selectWorker` returns the entry with the lowest `activeJobs` (ties broken by first-in-list).

**Step 2 — Add `WorkerRegistry` to master**
Create `master/include/master/WorkerRegistry.h` / `master/src/WorkerRegistry.cpp`. Responsibilities:
- `registerWorker(WorkerHandle)` — add/update an entry keyed by `id`.
- `listAvailable() -> std::vector<WorkerHandle>` — return workers whose last-seen heartbeat is within `heartbeatTimeoutMs` (from config, default `5000`).
- A background `std::jthread` that, every 2 seconds, calls `Heartbeat` (via `WorkerProxy`) on every registered worker, updates `activeJobs`/`hasGpu`/last-seen timestamp, and drops workers that fail 3 consecutive heartbeats from the "available" set (but keep the entry around, just excluded from scheduling — this groundwork is what M4 builds fault-tolerance on top of).

**Step 3 — Update master startup to accept multiple workers**
Change `master/src/main.cpp`'s CLI to accept a **repeatable** `--worker-address` flag (CLI11 supports multi-value options), register each one into `WorkerRegistry` at startup, then start the heartbeat thread.

**Step 4 — Wire scheduler into `JobDispatcher`**
Create `master/include/master/JobDispatcher.h` / `master/src/JobDispatcher.cpp`. Move the per-chunk dispatch loop out of `main.cpp` into `JobDispatcher::runJob(const JobSpec&, ISplitStrategy&, IScheduler&, WorkerRegistry&) -> TaskResult`. For each chunk: call `registry.listAvailable()`, call `scheduler.selectWorker(available)`, dispatch via that worker's `WorkerProxy`. Dispatch chunks **concurrently** this milestone (e.g. one `std::jthread` or `std::async` per chunk, joined/waited before reassembly) — this is the first point where dispatch stops being purely sequential.
Default scheduler = `LeastLoadedScheduler`, selectable via a `--scheduler round_robin|least_loaded` CLI flag.

**Step 5 — Docker Compose for local multi-worker testing**
Create `docker/Dockerfile.base`, `docker/Dockerfile.worker`, `docker/Dockerfile.master`, `docker/docker-compose.yml` defining `master` + `worker1` + `worker2` + `worker3` services on a shared bridge network, each worker listening on port `50052` inside its own container (mapped differently on the host if you need host access, e.g. `50052:50052`, `50053:50052`, `50054:50052`).

**Step 6 — Build containers**
```powershell
docker compose -f docker/docker-compose.yml build
```
Expected result: all 4 images build with exit code 0.

**Step 7 — Run the stack**
```powershell
docker compose -f docker/docker-compose.yml up -d worker1 worker2 worker3
docker compose -f docker/docker-compose.yml logs -f worker1 worker2 worker3
```
Expected result: all three worker containers report "listening" in their logs and stay running (`docker compose ps` shows `Up` for all three, not `Restarting` or `Exited`).

**Step 8 — Run a 6-chunk job against the 3-worker stack**
```powershell
docker compose -f docker/docker-compose.yml run --rm master `
  /app/master sample.mp4 6 output.mp4 `
  --worker-address worker1:50052 --worker-address worker2:50052 --worker-address worker3:50052 `
  --scheduler least_loaded
```
Expected result: exit code `0`, `output.mp4` produced correctly (verify duration via `ffprobe` as in M1 Step 10).

**Step 9 — Verify non-trivial distribution**
Capture the per-chunk "dispatched to worker X" log lines the master prints (add this log line in `JobDispatcher` if not already present — it is required for this test to be checkable) and count how many chunks went to each of the 3 workers.
Expected result: **no single worker received all 6 chunks** — with `LeastLoadedScheduler` and 3 equally-idle workers you should see roughly 2 chunks per worker (exact split may vary slightly due to timing, but a 6/0/0 split is a failure and indicates the scheduler isn't actually consulting `activeJobs`).

**Step 10 — Verify round-robin as an alternate**
Re-run Step 8 with `--scheduler round_robin` instead. Expected result: chunk-to-worker assignment log follows the exact repeating pattern worker1, worker2, worker3, worker1, worker2, worker3.

**Definition of Done Checklist (M3)**
- [ ] `docker compose build` succeeds for all services.
- [ ] Step 8 produces a correct `output.mp4`.
- [ ] Step 9 shows a non-trivial (not all-on-one-worker) distribution.
- [ ] Step 10's round-robin ordering matches exactly.
- [ ] `git commit -m "M3: multi-worker registry + scheduler"`.

---

### M4 — Fault tolerance (worker failure mid-job)

**Preconditions:** M3 checklist fully passed and committed.

**Step 1 — Add retry/requeue logic to `JobDispatcher`**
Wrap each chunk's dispatch call: if `WorkerProxy::dispatch` throws/returns a failed `TaskResult` due to an RPC error (connection refused, deadline exceeded — distinguish this from an *ffmpeg* failure, which should NOT be retried against a different worker since the input chunk itself may be bad), re-select a worker via `scheduler.selectWorker` **excluding** the worker that just failed, and retry up to a configurable `maxRetries` (default `2`) before giving up and failing the whole job.

**Step 2 — Ensure heartbeat-based eviction actually removes dead workers from scheduling**
Confirm (re-read if needed) that `WorkerRegistry::listAvailable()` from M3 Step 2 excludes workers that have missed 3 consecutive heartbeats — this is the mechanism M4's test relies on. If it doesn't yet correctly exclude them, fix it now before proceeding.

**Step 3 — Use a longer job for a visible failure window**
```powershell
ffmpeg -f lavfi -i testsrc=duration=40:size=640x360:rate=30 -f lavfi -i sine=frequency=1000:duration=40 -c:v libx264 -c:a aac -shortest longsample.mp4
```

**Step 4 — Start the stack and kick off a job with more chunks than workers can instantly finish**
```powershell
docker compose -f docker/docker-compose.yml up -d worker1 worker2 worker3
docker compose -f docker/docker-compose.yml run --rm master `
  /app/master longsample.mp4 9 output.mp4 `
  --worker-address worker1:50052 --worker-address worker2:50052 --worker-address worker3:50052
```
While this is running (it should take at least ~20–30 seconds wall-clock with 3 workers on 9 chunks — adjust chunk count upward if it finishes too fast to intervene), proceed immediately to Step 5.

**Step 5 — Kill a worker mid-job**
In a separate terminal, while the master run from Step 4 is still in progress:
```powershell
docker compose -f docker/docker-compose.yml stop worker2
```

**Step 6 — Observe master behavior**
Expected result in the master's console/log output:
- A log line indicating an RPC failure/timeout for the chunk(s) that were in flight on `worker2`.
- A subsequent log line indicating that chunk was re-dispatched to a different worker (`worker1` or `worker3`).
- The job still completes with exit code `0` and produces a correct `output.mp4` (verify via `ffprobe` duration check as before).

**Step 7 — Negative-path test: kill enough workers that the job cannot complete**
Repeat Steps 3–5 but stop **all three** workers mid-job.
Expected result: the job fails with a clear, non-hanging error (bounded by `maxRetries` × per-RPC deadline, not an indefinite hang) and a non-zero exit code — this proves the retry logic has a real ceiling and doesn't spin forever.

**Definition of Done Checklist (M4)**
- [ ] Step 6 shows the exact requeue behavior described (failure log → reassignment log → successful completion).
- [ ] Output file from Step 6 passes the duration check.
- [ ] Step 7 fails within a bounded time, not indefinitely.
- [ ] `git commit -m "M4: fault tolerance via heartbeat eviction + chunk requeue"`.

---

### M5 — Storage abstraction (swap Local FS ↔ MinIO with no code changes)

**Preconditions:** M4 checklist fully passed and committed.

**Step 1 — Add `IStorageClient` and both adapters**
Create `common/include/common/IStorageClient.h` (copy verbatim from Section 5), then:
```
common/include/common/LocalFSStorageClient.h + common/src/LocalFSStorageClient.cpp
common/include/common/MinIOStorageClient.h + common/src/MinIOStorageClient.cpp
```
`LocalFSStorageClient::upload`/`download` are plain `std::filesystem::copy_file` calls between a configured root directory and the given key. `MinIOStorageClient` uses `aws-sdk-cpp`'s S3 client configured with a custom endpoint (MinIO is S3-API-compatible) — `upload` = `PutObject`, `download` = `GetObject`, `exists` = `HeadObject`.

**Step 2 — Add `aws-sdk-cpp` to the manifest**
Edit `vcpkg.json`, add `{ "name": "aws-sdk-cpp", "features": ["s3"] }`. Re-run `cmake --preset dev` and budget 30–60 minutes for the first build per the known-friction note in Section 7.6 — do not interrupt this build.

**Step 3 — Inject storage client into worker and master**
Update `WorkerServiceImpl` and `JobDispatcher`/`main.cpp` (both master and worker) to take an `IStorageClient&` (or `std::unique_ptr<IStorageClient>`) via constructor injection, selected at startup based on a `--storage local|minio` CLI flag (default `local`), rather than either hardcoding a concrete type or constructing storage clients ad hoc inside business logic.
Update the transcode flow: worker now `download`s the source chunk's `sourceStorageKey` from `IStorageClient` before running ffmpeg (instead of assuming a locally-shared path), and `upload`s the resulting chunk to `outputStorageKey` afterward. Master `download`s all chunk outputs before reassembly, and `upload`s the final reassembled file.

**Step 4 — Add MinIO to docker-compose**
Add a `minio` service to `docker/docker-compose.yml` using image `minio/minio`, command `server /data --console-address ":9001"`, with a healthcheck, and environment variables `MINIO_ROOT_USER`/`MINIO_ROOT_PASSWORD` (use `minioadmin`/`minioadmin` for local dev only — never for anything internet-reachable). Add a one-shot `mc` (MinIO client) service or an entrypoint script that creates the `rffmpeg-jobs` bucket on stack startup if it doesn't already exist.

**Step 5 — Test path A: Local FS storage (regression check)**
```powershell
docker compose -f docker/docker-compose.yml up -d worker1 worker2 worker3
docker compose -f docker/docker-compose.yml run --rm master `
  /app/master sample.mp4 4 output.mp4 --storage local `
  --worker-address worker1:50052 --worker-address worker2:50052 --worker-address worker3:50052
```
Expected result: identical behavior to M3 — exit `0`, correct `output.mp4` (duration check).

**Step 6 — Test path B: MinIO storage**
```powershell
docker compose -f docker/docker-compose.yml up -d minio
# wait for minio healthcheck to pass, then:
docker compose -f docker/docker-compose.yml run --rm master `
  /app/master sample.mp4 4 output.mp4 --storage minio `
  --worker-address worker1:50052 --worker-address worker2:50052 --worker-address worker3:50052
```
Expected result: exit `0`, correct `output.mp4` — **same acceptance check as Step 5, only the `--storage` flag changed.**

**Step 7 — Verify objects actually landed in MinIO (not silently falling back to local disk)**
```powershell
docker compose -f docker/docker-compose.yml exec minio mc ls local/rffmpeg-jobs/
```
(or open the MinIO console at `http://localhost:9001` with the root credentials). Expected result: chunk and final-output object keys are listed — if this is empty while Step 6 still reported success, the storage client is silently no-op'ing and must be fixed before this milestone is considered done.

**Step 8 — Confirm zero source changes between Steps 5 and 6**
```powershell
git status
```
Expected result: clean working tree between the two test runs — the only difference was the `--storage` CLI flag, no code edits, no rebuild required to switch backends.

**Definition of Done Checklist (M5)**
- [ ] Step 5 (local) passes.
- [ ] Step 6 (MinIO) passes with the identical acceptance check.
- [ ] Step 7 confirms real objects in MinIO, not a silent no-op.
- [ ] Step 8 confirms the switch is config-only.
- [ ] `git commit -m "M5: storage abstraction, LocalFS + MinIO adapters"`.

---

### M6 — Observability (job/chunk status CLI)

**Preconditions:** M5 checklist fully passed and committed.

**Step 1 — Add the `Job` state machine and `Observer` pattern**
Create `common/include/common/Job.h` / `common/src/Job.cpp`:
- `enum class JobState { Queued, Splitting, Dispatched, Running, Merging, Done, Failed };`
- `Job::transitionTo(JobState next)` must reject illegal transitions (e.g. `Done → Running` throws or returns false) — write this as an explicit allow-list, e.g. `Queued→Splitting→Dispatched→Running→Merging→Done`, with `Failed` reachable from any non-terminal state.
- `Job` also tracks per-chunk state (`std::map<std::string /*chunkId*/, TaskStatus>` plus a `double percentComplete` per chunk).
- `Observer` base class with `virtual void onJobStateChanged(const Job&) = 0;`; `Job::attach(Observer*)` / `Job::notify()`.

**Step 2 — Persist job/chunk state to SQLite**
Add `sqlite3` to `vcpkg.json` if not already present. Create `master/include/master/JobStore.h` / `master/src/JobStore.cpp` implementing an `Observer` that, on every notification, upserts the job's current state and per-chunk progress into a local SQLite file (`jobs.db`) — this is what a separate CLI invocation queries, since the running `master` process and the `status` query happen in different process invocations.

**Step 3 — Add a `master status <jobId>` subcommand**
Extend `master/src/main.cpp` with a CLI11 subcommand `status` taking a `jobId` argument, that opens `jobs.db` read-only and prints a table: chunk ID, state, percent complete, and an overall job state/percent line. This must work while a job is still running in another process (SQLite supports concurrent readers), not only after completion.

**Step 4 — Wire `JobDispatcher` to update `Job` state at each phase**
Ensure `JobDispatcher::runJob` transitions the `Job` through `Queued → Splitting → Dispatched → Running → Merging → Done` (or `→ Failed`) and updates each chunk's `TaskStatus`/percent as `WorkerProxy::dispatch` streams `ProgressUpdate`s back — this is the piece that actually feeds Step 3's data.

**Step 5 — Run a job and poll status concurrently**
Terminal A:
```powershell
docker compose -f docker/docker-compose.yml up -d worker1 worker2 worker3
docker compose -f docker/docker-compose.yml run --rm master `
  /app/master longsample.mp4 9 output.mp4 --job-id demo-job-1 `
  --worker-address worker1:50052 --worker-address worker2:50052 --worker-address worker3:50052
```
Terminal B, while Terminal A is still running (repeat every couple seconds):
```powershell
docker compose -f docker/docker-compose.yml run --rm master /app/master status demo-job-1
```
Expected result: successive calls in Terminal B show chunk states progressing (`Pending`→`Running`→`Success`) and the overall job percent-complete increasing monotonically across polls, eventually reaching `Done`/100% once Terminal A finishes.

**Step 6 — Negative-path test: unknown job id**
```powershell
docker compose -f docker/docker-compose.yml run --rm master /app/master status not-a-real-job
```
Expected result: a clear "job not found" message and non-zero exit code — not a crash, not an empty silent success.

**Definition of Done Checklist (M6)**
- [ ] Step 5 shows monotonically progressing status across at least 3 distinct polls.
- [ ] Final poll shows `Done` / 100% matching Terminal A's own completion.
- [ ] Step 6 fails cleanly.
- [ ] `git commit -m "M6: job state machine, SQLite-backed status CLI"`.

---

### M7 — Polish (tests, lint, docs, one-command demo)

**Preconditions:** M6 checklist fully passed and committed.

**Step 1 — Backfill unit tests**
Add `gtest` to `vcpkg.json` if not already present. Create `tests/CMakeLists.txt` plus, at minimum, one test file per interface implementation built so far:
```
tests/common/test_split_strategy.cpp        # TimeBasedSplitStrategy: chunk count, total duration conservation, last-chunk remainder handling
tests/common/test_ffmpeg_executor.cpp       # mock the subprocess boundary or run against a tiny generated clip; assert Success/Failed paths
tests/common/test_scheduler.cpp             # RoundRobinScheduler ordering, LeastLoadedScheduler tie-breaking
tests/common/test_job_state_machine.cpp     # legal transitions succeed, illegal transitions (e.g. Done->Running) are rejected
tests/master/test_job_dispatcher.cpp        # inject a fake IScheduler + fake WorkerProxy-equivalent, assert requeue-on-failure behavior from M4
```
Wire `tests/CMakeLists.txt` into the top-level `CMakeLists.txt` via `enable_testing()` + `add_subdirectory(tests)`, with each test file registered via `gtest_discover_tests`.

**Step 2 — Run the full test suite**
```powershell
cmake --build --preset dev --parallel
ctest --preset dev --output-on-failure
```
Expected result: every test passes, exit code `0`. If any fail, fix the underlying code (not the test) unless the test itself is verifiably wrong.

**Step 3 — Static analysis pass**
```powershell
Get-ChildItem -Recurse common,master,worker -Include *.cpp,*.h |
  ForEach-Object { clang-tidy $_.FullName -- -std=c++20 }
```
Expected result: no errors. Warnings should be triaged — either fixed or explicitly suppressed with a comment explaining why, not silently ignored.

**Step 4 — Format check**
```powershell
Get-ChildItem -Recurse common,master,worker,tests -Include *.cpp,*.h |
  ForEach-Object { clang-format --dry-run --Werror $_.FullName }
```
Expected result: no diffs reported. If diffs exist, run the same command without `--dry-run --Werror` (i.e. `clang-format -i <file>`) to apply formatting in place, then re-run the check.

**Step 5 — Write the one-command demo script**
`scripts/run_local_demo.ps1` must, with zero arguments and zero manual steps:
1. Generate `sample.mp4` if it doesn't already exist (Step 8 of M1).
2. `docker compose build`.
3. `docker compose up -d` the full stack (3 workers + minio).
4. Wait for all health checks to pass (poll, don't sleep-and-hope).
5. Run a master job end-to-end against the MinIO backend.
6. Run the `ffprobe` duration check against the output and print `PASS`/`FAIL`.
7. Tear the stack down (`docker compose down`) regardless of pass/fail, so re-runs start clean.

**Step 6 — Time the demo**
```powershell
Measure-Command { .\scripts\run_local_demo.ps1 }
```
Expected result: printed `PASS`, and total elapsed time under 5 minutes on a typical laptop (excluding the one-time Docker image build/vcpkg dependency build on the very first run — time only a second, warm-cache run).

**Step 7 — README pass**
Update `README.md` with: one-paragraph project description, the architecture diagram (ASCII is fine, referencing Section 1's diagram), a "Quickstart" section that is just `./scripts/run_local_demo.ps1`, and a link to this DEVDOC.md for anyone extending the system.

**Step 8 — Final full-repo sanity sweep**
```powershell
git status              # expect: clean, nothing untracked that should be committed
ctest --preset dev --output-on-failure
.\scripts\run_local_demo.ps1
```
All three must succeed with no manual intervention.

**Definition of Done Checklist (M7)**
- [ ] Step 2: all unit tests pass.
- [ ] Step 3: clang-tidy clean (or warnings explicitly justified).
- [ ] Step 4: clang-format clean.
- [ ] Step 6: one-command demo passes in under 5 minutes (warm cache).
- [ ] Step 7: README updated and accurate.
- [ ] `git commit -m "M7: tests, lint, docs, one-command demo"`.

---

## 11. Configuration

Both `master` and `worker` binaries read a JSON config (via `nlohmann::json`) at startup, with CLI11 flags to override. Note: when the worker runs inside a Linux container (§7.5), the config instead uses `/usr/bin/ffmpeg` / `/usr/bin/ffprobe` — use a separate `config.linux.json` / `config.windows.json` pair, or better, resolve the binary path at runtime via `boost::process::search_path("ffmpeg")` so the same config works on both without hardcoding OS-specific paths:

```json
{
  "master": {
    "listenPort": 50051,
    "scheduler": "least_loaded",
    "splitStrategy": "keyframe",
    "numChunksDefault": 4,
    "heartbeatTimeoutMs": 5000
  },
  "worker": {
    "listenPort": 50052,
    "ffmpegBinaryPath": "C:\\ffmpeg\\bin\\ffmpeg.exe",
    "ffprobeBinaryPath": "C:\\ffmpeg\\bin\\ffprobe.exe",
    "maxConcurrentChunks": 2,
    "hasGpu": false
  },
  "storage": {
    "type": "minio",
    "endpoint": "http://minio:9000",
    "bucket": "rffmpeg-jobs",
    "accessKey": "ENV:MINIO_ACCESS_KEY",
    "secretKey": "ENV:MINIO_SECRET_KEY"
  }
}
```

---

## 12. Open Questions / Decisions for Implementer

1. Keyframe-based splitting requires re-encoding at boundaries (can't always cut on exact GOP boundary if user requests arbitrary chunk count) — decide whether to snap chunk count to nearest keyframe or force GOP-aligned encode on source ingest. **Recommendation:** snap to nearest keyframe, document the tradeoff in README.
2. Whether to support resumable uploads to storage for very large chunks — defer to post-M7 backlog.
3. Whether `master` itself needs HA (multiple master replicas) — explicitly out of scope for v1, note as future work with e.g. Raft-based leader election.

---

## 13. Reference Commands for AI Agent (quick sanity checks while building, PowerShell)

```powershell
# verify ffmpeg/ffprobe present
ffmpeg -version; ffprobe -version

# locate vcpkg-installed proto tools
$grpcPlugin = "C:\vcpkg\installed\x64-windows\tools\grpc\grpc_cpp_plugin.exe"
$protoc = "C:\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe"

# generate proto stubs manually (also wired into CMake via protobuf_generate)
& $protoc -I proto --cpp_out=build/gen --grpc_out=build/gen `
  --plugin=protoc-gen-grpc=$grpcPlugin proto/jobservice.proto

# run a single unit test target
ctest --preset dev -R test_split_strategy --output-on-failure

# format check
Get-ChildItem -Recurse common,master,worker -Include *.cpp,*.h | ForEach-Object {
  clang-format --dry-run --Werror $_.FullName
}

# check for long-path issues after a fresh vcpkg install
Get-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled"
```

---

*End of devdoc.md — implementer should proceed milestone by milestone (Section 10), committing after each milestone's acceptance criteria pass.*
