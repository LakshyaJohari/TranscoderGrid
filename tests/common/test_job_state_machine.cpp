#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "common/JobSpec.h"
#include "common/ChunkSpec.h"
#include "common/TaskResult.h"

TEST(JobSpecModelTest, DefaultValuesAreSensible)
{
    JobSpec spec;
    EXPECT_TRUE(spec.jobId.empty());
    EXPECT_TRUE(spec.sourcePath.empty());
    EXPECT_TRUE(spec.outputCodec.empty());
    EXPECT_TRUE(spec.resolution.empty());
    EXPECT_EQ(spec.bitrateKbps, 0);
    EXPECT_EQ(spec.crf, 23); // Default high-quality x264 CRF
    EXPECT_EQ(spec.numChunks, 4);
    EXPECT_TRUE(spec.extraFilters.empty());
}

TEST(JobSpecModelTest, CustomConfigurationPreserved)
{
    JobSpec spec;
    spec.jobId = "transcode-4k-hevc";
    spec.sourcePath = "/storage/raw_master.mov";
    spec.outputCodec = "libx265";
    spec.resolution = "3840x2160";
    spec.bitrateKbps = 15000;
    spec.crf = 18;
    spec.numChunks = 16;
    spec.extraFilters = {"fps=60", "hqdn3d=1.5:1.5:6:6"};

    EXPECT_EQ(spec.jobId, "transcode-4k-hevc");
    EXPECT_EQ(spec.sourcePath, "/storage/raw_master.mov");
    EXPECT_EQ(spec.outputCodec, "libx265");
    EXPECT_EQ(spec.resolution, "3840x2160");
    EXPECT_EQ(spec.bitrateKbps, 15000);
    EXPECT_EQ(spec.crf, 18);
    EXPECT_EQ(spec.numChunks, 16);
    ASSERT_EQ(spec.extraFilters.size(), 2u);
    EXPECT_EQ(spec.extraFilters[0], "fps=60");
}

TEST(ChunkSpecModelTest, FieldsStoreChunkCoordinatesCorrectly)
{
    ChunkSpec chunk;
    chunk.chunkId = "job-42_3";
    chunk.jobId = "job-42";
    chunk.index = 3;
    chunk.startTimeSec = 9.0;
    chunk.durationSec = 3.0;
    chunk.sourceStorageKey = "s3://media/input.mp4";
    chunk.outputStorageKey = "s3://media/chunks/job-42_3.mp4";

    EXPECT_EQ(chunk.chunkId, "job-42_3");
    EXPECT_EQ(chunk.jobId, "job-42");
    EXPECT_EQ(chunk.index, 3);
    EXPECT_DOUBLE_EQ(chunk.startTimeSec, 9.0);
    EXPECT_DOUBLE_EQ(chunk.durationSec, 3.0);
    EXPECT_EQ(chunk.sourceStorageKey, "s3://media/input.mp4");
    EXPECT_EQ(chunk.outputStorageKey, "s3://media/chunks/job-42_3.mp4");
}

TEST(TaskResultModelTest, StatusRepresentationsAreAccurate)
{
    TaskResult pending;
    EXPECT_EQ(pending.status, TaskStatus::Pending);
    EXPECT_DOUBLE_EQ(pending.durationMs, 0.0);
    EXPECT_TRUE(pending.message.empty());

    TaskResult success;
    success.status = TaskStatus::Success;
    success.outputPath = "work/chunk_0.mp4";
    success.durationMs = 1245.5;
    success.message = "ok";
    EXPECT_EQ(success.status, TaskStatus::Success);
    EXPECT_EQ(success.outputPath, "work/chunk_0.mp4");
    EXPECT_GT(success.durationMs, 0.0);

    TaskResult failed;
    failed.status = TaskStatus::Failed;
    failed.message = "RPC deadline exceeded";
    EXPECT_EQ(failed.status, TaskStatus::Failed);
    EXPECT_FALSE(failed.message.empty());
}
