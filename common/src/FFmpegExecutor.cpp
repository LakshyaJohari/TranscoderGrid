#include "../include/common/FFmpegExecutor.h"
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <filesystem>

TaskResult FFmpegExecutor::run(const ChunkSpec &chunk, const JobSpec &spec)
{
    TaskResult res;
    res.status = TaskStatus::Running;
    auto start = std::chrono::steady_clock::now();

    std::filesystem::path outPath = std::filesystem::path("work") / (chunk.chunkId + ".mp4");
    std::filesystem::create_directories(outPath.parent_path());

    std::ostringstream cmd;
    std::string codec = spec.outputCodec.empty() ? "libx264" : spec.outputCodec;
    cmd << "ffmpeg -y -ss " << chunk.startTimeSec << " -t " << chunk.durationSec
        << " -i \"" << chunk.sourceStorageKey << "\""
        << " -c:v " << codec << " -preset veryfast -crf " << spec.crf;
    if (!spec.resolution.empty())
    {
        cmd << " -s " << spec.resolution;
    }
    cmd << " \"" << outPath.string() << "\"";

    int rc = std::system(cmd.str().c_str());

    auto end = std::chrono::steady_clock::now();
    res.durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    if (rc == 0)
    {
        res.status = TaskStatus::Success;
        res.outputPath = outPath.string();
        res.message = "ok";
    }
    else
    {
        res.status = TaskStatus::Failed;
        res.message = "ffmpeg failed (exit=" + std::to_string(rc) + ")";
    }
    return res;
}
