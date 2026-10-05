#pragma once
#include "ChunkSpec.h"
#include "JobSpec.h"
#include "TaskResult.h"

class IFFmpegExecutor
{
public:
    virtual TaskResult run(const ChunkSpec &chunk, const JobSpec &spec) = 0;
    virtual ~IFFmpegExecutor() = default;
};
