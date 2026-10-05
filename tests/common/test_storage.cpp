#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>

#include "common/LocalFSStorageClient.h"
#include "common/MinIOStorageClient.h"

class StorageClientTest : public ::testing::Test
{
protected:
    const std::filesystem::path kTestStorageDir = "test_storage_dir";
    const std::string kTestUploadFile = "test_local_file.txt";

    void SetUp() override
    {
        std::error_code ec;
        std::filesystem::remove_all(kTestStorageDir, ec);

        // Create a dummy local file to upload
        std::ofstream out(kTestUploadFile);
        out << "TranscodeGrid distributed chunk payload 12345";
        out.close();
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove_all(kTestStorageDir, ec);
        std::filesystem::remove(kTestUploadFile, ec);
        std::filesystem::remove("test_downloaded_file.txt", ec);
    }
};

TEST_F(StorageClientTest, LocalFSUploadDownloadAndExists)
{
    LocalFSStorageClient storage(kTestStorageDir);

    EXPECT_FALSE(storage.exists("chunks/chunk-1.txt"));

    // Upload file
    EXPECT_NO_THROW(storage.upload(kTestUploadFile, "chunks/chunk-1.txt"));
    EXPECT_TRUE(storage.exists("chunks/chunk-1.txt"));

    // Download file
    std::string downloadPath = "test_downloaded_file.txt";
    EXPECT_NO_THROW(storage.download("chunks/chunk-1.txt", downloadPath));
    EXPECT_TRUE(std::filesystem::exists(downloadPath));

    // Verify content
    std::ifstream in(downloadPath);
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "TranscodeGrid distributed chunk payload 12345");
}

TEST_F(StorageClientTest, LocalFSThrowsOnNonExistentUploadSource)
{
    LocalFSStorageClient storage(kTestStorageDir);
    EXPECT_THROW(storage.upload("does_not_exist_file.txt", "remote_key.txt"), std::runtime_error);
}

TEST_F(StorageClientTest, LocalFSThrowsOnNonExistentDownloadKey)
{
    LocalFSStorageClient storage(kTestStorageDir);
    EXPECT_THROW(storage.download("non_existent_key.txt", "download_dest.txt"), std::runtime_error);
}

TEST_F(StorageClientTest, MinIOStorageClientFallbackToLocalCache)
{
    MinIOStorageClient minio("http://localhost:9999", "test-bucket");

    // Even if remote MinIO endpoint is down, cache storage succeeds
    EXPECT_NO_THROW(minio.upload(kTestUploadFile, "fallback-chunk.txt"));
    EXPECT_TRUE(minio.exists("fallback-chunk.txt"));

    std::string downloadPath = "test_downloaded_file.txt";
    EXPECT_NO_THROW(minio.download("fallback-chunk.txt", downloadPath));
    EXPECT_TRUE(std::filesystem::exists(downloadPath));
}
