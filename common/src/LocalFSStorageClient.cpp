#include "common/LocalFSStorageClient.h"

#include <stdexcept>
#include <system_error>

LocalFSStorageClient::LocalFSStorageClient(std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory))
{
    std::error_code ec;
    std::filesystem::create_directories(rootDirectory_, ec);
}

std::filesystem::path LocalFSStorageClient::resolvePath(const std::string &key) const
{
    std::filesystem::path keyPath(key);
    if (keyPath.is_absolute())
    {
        return keyPath;
    }
    return rootDirectory_ / keyPath;
}

void LocalFSStorageClient::upload(const std::string &localPath, const std::string &remoteKey)
{
    std::filesystem::path src(localPath);
    if (!std::filesystem::exists(src))
    {
        throw std::runtime_error("Local file does not exist: " + localPath);
    }

    std::filesystem::path dest = resolvePath(remoteKey);
    if (dest.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(dest.parent_path(), ec);
    }

    std::error_code ec;
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        throw std::runtime_error("Failed to upload " + localPath + " to " + dest.string() + ": " + ec.message());
    }
}

void LocalFSStorageClient::download(const std::string &remoteKey, const std::string &localPath)
{
    std::filesystem::path src = resolvePath(remoteKey);
    if (!std::filesystem::exists(src))
    {
        throw std::runtime_error("Remote key not found in storage: " + remoteKey);
    }

    std::filesystem::path dest(localPath);
    if (dest.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(dest.parent_path(), ec);
    }

    std::error_code ec;
    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        throw std::runtime_error("Failed to download " + src.string() + " to " + localPath + ": " + ec.message());
    }
}

bool LocalFSStorageClient::exists(const std::string &remoteKey)
{
    return std::filesystem::exists(resolvePath(remoteKey));
}

const std::filesystem::path &LocalFSStorageClient::rootDirectory() const
{
    return rootDirectory_;
}
