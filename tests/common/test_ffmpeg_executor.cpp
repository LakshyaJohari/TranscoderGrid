#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <filesystem>
#include <string>

#include "common/IFFmpegExecutor.h"
#include "common/FFmpegExecutor.h"
#include "common/JobSpec.h"
#include "common/ChunkSpec.h"

// GoogleMock definition for IFFmpegExecutor
class MockFFmpegExecutor : public IFFmpegExecutor
{
public:
    MOCK_METHOD(TaskResult, run, (const ChunkSpec &chunk, const JobSpec &spec), (override));
};

using ::testing::_;
using ::testing::Return;
using ::testing::Field;

TEST(MockFFmpegExecutorTest, SimulatesSuccessfulTranscodeChunk)
{
    MockFFmpegExecutor mockExecutor;

    ChunkSpec chunk;
    chunk.chunkId = "chunk-0";
    chunk.startTimeSec = 0.0;
    chunk.durationSec = 5.0;

    JobSpec spec;
    spec.jobId = "test-job";
    spec.outputCodec = "libx264";

    TaskResult expectedSuccess;
    expectedSuccess.status = TaskStatus::Success;
    expectedSuccess.outputPath = "work/chunk-0.mp4";
    expectedSuccess.durationMs = 850.0;
    expectedSuccess.message = "ok";

    EXPECT_CALL(mockExecutor, run(_, _))
        .Times(1)
        .WillOnce(Return(expectedSuccess));

    TaskResult result = mockExecutor.run(chunk, spec);
    EXPECT_EQ(result.status, TaskStatus::Success);
    EXPECT_EQ(result.outputPath, "work/chunk-0.mp4");
    EXPECT_DOUBLE_EQ(result.durationMs, 850.0);
    EXPECT_EQ(result.message, "ok");
}

TEST(MockFFmpegExecutorTest, SimulatesWorkerFailure)
{
    MockFFmpegExecutor mockExecutor;

    ChunkSpec chunk;
    chunk.chunkId = "chunk-corrupt";

    JobSpec spec;
    spec.jobId = "corrupt-job";

    TaskResult expectedFailure;
    expectedFailure.status = TaskStatus::Failed;
    expectedFailure.message = "ffmpeg failed (exit=1)";

    EXPECT_CALL(mockExecutor, run(_, _))
        .Times(1)
        .WillOnce(Return(expectedFailure));

    TaskResult result = mockExecutor.run(chunk, spec);
    EXPECT_EQ(result.status, TaskStatus::Failed);
    EXPECT_EQ(result.message, "ffmpeg failed (exit=1)");
}

TEST(FFmpegExecutorRealTest, HandlesInvalidSourcePathGracefully)
{
    FFmpegExecutor realExecutor;

    ChunkSpec chunk;
    chunk.chunkId = "chunk_non_existent";
    chunk.sourceStorageKey = "invalid_non_existent_source_9999.mp4";
    chunk.startTimeSec = 0.0;
    chunk.durationSec = 2.0;

    JobSpec spec;
    spec.jobId = "real_fail_job";
    spec.outputCodec = "libx264";

    TaskResult result = realExecutor.run(chunk, spec);
    EXPECT_EQ(result.status, TaskStatus::Failed);
    EXPECT_FALSE(result.message.empty());
    EXPECT_GE(result.durationMs, 0.0);
}
