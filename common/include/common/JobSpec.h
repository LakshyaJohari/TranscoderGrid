#pragma once
#include <string>
#include <vector>

struct JobSpec
{
    std::string jobId;
    std::string sourcePath;
    std::string outputCodec; // e.g. libx264
    std::string resolution;  // e.g. 1920x1080
    int bitrateKbps = 0;
    int crf = 23;
    std::vector<std::string> extraFilters;
    int numChunks = 4;
};
