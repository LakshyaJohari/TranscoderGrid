#pragma once
#include "ISplitStrategy.h"

class TimeBasedSplitStrategy : public ISplitStrategy
{
public:
    std::vector<ChunkSpec> split(const JobSpec &spec) override;
};
