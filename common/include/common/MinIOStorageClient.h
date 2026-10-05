#pragma once

#include <string>
#include <memory>

#include "common/IStorageClient.h"
#include "common/LocalFSStorageClient.h"

class MinIOStorageClient : public IStorageClient
{
public:
    MinIOStorageClient(std::string endpoint,
                       std::string bucket,
                       std::string accessKey = "minioadmin",
                       std::string secretKey = "minioadmin");

    void upload(const std::string &localPath, const std::string &remoteKey) override;
    void download(const std::string &remoteKey, const std::string &localPath) override;
    bool exists(const std::string &remoteKey) override;

    const std::string &endpoint() const;
    const std::string &bucket() const;

private:
    std::string endpoint_;
    std::string bucket_;
    std::string accessKey_;
    std::string secretKey_;
    LocalFSStorageClient cacheStorage_;

    std::string buildObjectUrl(const std::string &remoteKey) const;
    bool executeCurlCommand(const std::string &cmd) const;
};
