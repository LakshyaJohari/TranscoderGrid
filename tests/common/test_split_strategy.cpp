#include <gtest/gtest.h>
#include <filesystem>
#include <cstdlib>
#include <cmath>

#include "common/JobSpec.h"
#include "common/ChunkSpec.h"
#include "common/TimeBasedSplitStrategy.h"

class SplitStrategyTest : public ::testing::Test
{
protected:
    static inline const std::string kTestVideoPath = "test_synth_10s.mp4";

    static void SetUpTestSuite()
    {
        // Generate a 10-second synthetic test video if ffmpeg is available
        std::string cmd = "ffmpeg -y -f lavfi -i testsrc=duration=10:size=320x240:rate=30 "
                          "-f lavfi -i sine=frequency=1000:duration=10 "
                          "-c:v libx264 -c:a aac -shortest " + kTestVideoPath + " 2>nul";
        std::system(cmd.c_str());
    }

    static void TearDownTestSuite()
    {
        if (std::filesystem::exists(kTestVideoPath))
        {
            std::filesystem::remove(kTestVideoPath);
        }
    }
};

TEST_F(SplitStrategyTest, HandlesNonExistentFileGracefully)
{
    TimeBasedSplitStrategy strategy;
    JobSpec spec;
    spec.jobId = "missing_file_job";
    spec.sourcePath = "this_file_does_not_exist_9999.mp4";
    spec.numChunks = 4;

    auto chunks = strategy.split(spec);
    EXPECT_TRUE(chunks.empty()) << "Splitting a non-existent file should return empty chunks";
}

TEST_F(SplitStrategyTest, SplitsIntoRequestedChunkCount)
{
    if (!std::filesystem::exists(kTestVideoPath))
    {
        GTEST_SKIP() << "FFmpeg test video not available on test machine";
    }

    TimeBasedSplitStrategy strategy;
    JobSpec spec;
    spec.jobId = "job_4chunks";
    spec.sourcePath = kTestVideoPath;
    spec.numChunks = 4;

    auto chunks = strategy.split(spec);
    ASSERT_EQ(chunks.size(), 4u);

    // Verify chunk 0 starts at 0.0
    EXPECT_DOUBLE_EQ(chunks[0].startTimeSec, 0.0);
    EXPECT_EQ(chunks[0].index, 0);
    EXPECT_EQ(chunks[0].jobId, "job_4chunks");
    EXPECT_EQ(chunks[0].chunkId, "job_4chunks_0");
    EXPECT_EQ(chunks[0].sourceStorageKey, kTestVideoPath);

    // Verify chunk contiguity (each chunk starts where previous ended)
    for (size_t i = 0; i < chunks.size() - 1; ++i)
    {
        double currentEnd = chunks[i].startTimeSec + chunks[i].durationSec;
        double nextStart = chunks[i + 1].startTimeSec;
        EXPECT_NEAR(currentEnd, nextStart, 1e-4)
            << "Chunk " << i << " end should match Chunk " << i + 1 << " start";
        EXPECT_EQ(chunks[i].index, static_cast<int>(i));
    }

    // Verify last chunk covers until total duration (~10.0s)
    double totalDuration = chunks.back().startTimeSec + chunks.back().durationSec;
    EXPECT_NEAR(totalDuration, 10.0, 0.5);
}

TEST_F(SplitStrategyTest, HandlesOddChunkCountAndRemainderAbsorption)
{
    if (!std::filesystem::exists(kTestVideoPath))
    {
        GTEST_SKIP() << "FFmpeg test video not available on test machine";
    }

    TimeBasedSplitStrategy strategy;
    JobSpec spec;
    spec.jobId = "job_3chunks";
    spec.sourcePath = kTestVideoPath;
    spec.numChunks = 3;

    auto chunks = strategy.split(spec);
    ASSERT_EQ(chunks.size(), 3u);

    // Each chunk should be approximately 3.333s
    EXPECT_NEAR(chunks[0].durationSec, 10.0 / 3.0, 0.5);
    EXPECT_NEAR(chunks[1].durationSec, 10.0 / 3.0, 0.5);

    // Total duration should still match 10.0s
    double totalCovered = chunks[2].startTimeSec + chunks[2].durationSec;
    EXPECT_NEAR(totalCovered, 10.0, 0.5);
}

TEST_F(SplitStrategyTest, HandlesSingleChunkBoundary)
{
    if (!std::filesystem::exists(kTestVideoPath))
    {
        GTEST_SKIP() << "FFmpeg test video not available on test machine";
    }

    TimeBasedSplitStrategy strategy;
    JobSpec spec;
    spec.jobId = "job_single";
    spec.sourcePath = kTestVideoPath;
    spec.numChunks = 1;

    auto chunks = strategy.split(spec);
    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_DOUBLE_EQ(chunks[0].startTimeSec, 0.0);
    EXPECT_NEAR(chunks[0].durationSec, 10.0, 0.5);
    EXPECT_EQ(chunks[0].chunkId, "job_single_0");
}
