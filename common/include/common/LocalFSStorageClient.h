#pragma once

#include <filesystem>
#include <string>

#include "common/IStorageClient.h"

class LocalFSStorageClient : public IStorageClient
{
public:
    explicit LocalFSStorageClient(std::filesystem::path rootDirectory = "storage");

    void upload(const std::string &localPath, const std::string &remoteKey) override;
    void download(const std::string &remoteKey, const std::string &localPath) override;
    bool exists(const std::string &remoteKey) override;

    const std::filesystem::path &rootDirectory() const;

private:
    std::filesystem::path rootDirectory_;
    std::filesystem::path resolvePath(const std::string &key) const;
};
