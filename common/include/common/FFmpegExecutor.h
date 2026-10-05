#pragma once
#include "IFFmpegExecutor.h"

class FFmpegExecutor : public IFFmpegExecutor
{
public:
    TaskResult run(const ChunkSpec &chunk, const JobSpec &spec) override;
};
