#include "master/JobSpecBuilder.h"

JobSpecBuilder &JobSpecBuilder::withJobId(std::string jobId)
{
    spec_.jobId = std::move(jobId);
    return *this;
}

JobSpecBuilder &JobSpecBuilder::withSourcePath(std::string sourcePath)
{
    spec_.sourcePath = std::move(sourcePath);
    return *this;
}

JobSpecBuilder &JobSpecBuilder::withCodec(std::string codec)
{
    spec_.outputCodec = std::move(codec);
    return *this;
}

JobSpecBuilder &JobSpecBuilder::withResolution(std::string resolution)
{
    spec_.resolution = std::move(resolution);
    return *this;
}

JobSpecBuilder &JobSpecBuilder::withCrf(int crf)
{
    spec_.crf = crf;
    return *this;
}

JobSpecBuilder &JobSpecBuilder::withChunkCount(int numChunks)
{
    spec_.numChunks = numChunks;
    return *this;
}

JobSpec JobSpecBuilder::build() const
{
    return spec_;
}
