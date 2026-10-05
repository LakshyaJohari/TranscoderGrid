#pragma once
#include <string>

struct ChunkSpec
{
    std::string chunkId;
    std::string jobId;
    int index = 0;
    double startTimeSec = 0.0;
    double durationSec = 0.0;
    std::string sourceStorageKey;
    std::string outputStorageKey;
};
