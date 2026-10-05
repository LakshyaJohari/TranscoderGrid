#include "../include/common/TimeBasedSplitStrategy.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <sstream>

static double probeDurationSeconds(const std::string &path)
{
    std::string cmd = "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"" + path + "\"";
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        return 0.0;
    }
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        result += buffer;
    }
    pclose(pipe);
    try
    {
        return std::stod(result);
    }
    catch (...)
    {
        return 0.0;
    }
}

std::vector<ChunkSpec> TimeBasedSplitStrategy::split(const JobSpec &spec)
{
    std::vector<ChunkSpec> chunks;
    double duration = probeDurationSeconds(spec.sourcePath);
    if (duration <= 0.0)
    {
        return chunks;
    }

    int n = std::max(1, spec.numChunks);
    double base = duration / n;

    std::filesystem::create_directories("work");
    for (int i = 0; i < n; ++i)
    {
        ChunkSpec c;
        c.index = i;
        c.jobId = spec.jobId;
        c.chunkId = spec.jobId + "_" + std::to_string(i);
        c.startTimeSec = i * base;
        if (i == n - 1)
        {
            c.durationSec = duration - c.startTimeSec;
        }
        else
        {
            c.durationSec = base;
        }
        c.sourceStorageKey = spec.sourcePath;
        c.outputStorageKey = (std::filesystem::path("work") / (c.chunkId + ".mp4")).string();
        chunks.push_back(c);
    }
    return chunks;
}
