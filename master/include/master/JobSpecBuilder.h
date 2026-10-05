#pragma once

#include <string>

#include "common/JobSpec.h"

class JobSpecBuilder
{
public:
    JobSpecBuilder &withJobId(std::string jobId);
    JobSpecBuilder &withSourcePath(std::string sourcePath);
    JobSpecBuilder &withCodec(std::string codec);
    JobSpecBuilder &withResolution(std::string resolution);
    JobSpecBuilder &withCrf(int crf);
    JobSpecBuilder &withChunkCount(int numChunks);
    JobSpec build() const;

private:
    JobSpec spec_;
};
