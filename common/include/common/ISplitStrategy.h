#pragma once
#include "JobSpec.h"
#include "ChunkSpec.h"
#include <vector>

class ISplitStrategy
{
public:
    virtual std::vector<ChunkSpec> split(const JobSpec &spec) = 0;
    virtual ~ISplitStrategy() = default;
};
