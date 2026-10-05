#include "common/MinIOStorageClient.h"

#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <filesystem>

MinIOStorageClient::MinIOStorageClient(std::string endpoint,
                                       std::string bucket,
                                       std::string accessKey,
                                       std::string secretKey)
    : endpoint_(std::move(endpoint)),
      bucket_(std::move(bucket)),
      accessKey_(std::move(accessKey)),
      secretKey_(std::move(secretKey)),
      cacheStorage_("storage/" + bucket_)
{
}

std::string MinIOStorageClient::buildObjectUrl(const std::string &remoteKey) const
{
    std::string base = endpoint_;
    if (!base.empty() && base.back() == '/')
    {
        base.pop_back();
    }
    return base + "/" + bucket_ + "/" + remoteKey;
}

bool MinIOStorageClient::executeCurlCommand(const std::string &cmd) const
{
    int rc = std::system((cmd + " >nul 2>nul").c_str());
    return rc == 0;
}

void MinIOStorageClient::upload(const std::string &localPath, const std::string &remoteKey)
{
    if (!std::filesystem::exists(localPath))
    {
        throw std::runtime_error("Local source file does not exist: " + localPath);
    }

    // Always mirror to cache for resilient local access
    cacheStorage_.upload(localPath, remoteKey);

    // Attempt S3 REST upload via curl if endpoint is reachable
    std::string url = buildObjectUrl(remoteKey);
    std::ostringstream cmd;
    cmd << "curl -s -f -X PUT -T \"" << localPath << "\" \"" << url << "\"";
    executeCurlCommand(cmd.str());
}

void MinIOStorageClient::download(const std::string &remoteKey, const std::string &localPath)
{
    // First try downloading from MinIO endpoint if available
    std::string url = buildObjectUrl(remoteKey);
    std::ostringstream cmd;
    cmd << "curl -s -f -o \"" << localPath << "\" \"" << url << "\"";

    if (executeCurlCommand(cmd.str()) && std::filesystem::exists(localPath))
    {
        return;
    }

    // Fallback to cache storage
    if (cacheStorage_.exists(remoteKey))
    {
        cacheStorage_.download(remoteKey, localPath);
        return;
    }

    throw std::runtime_error("Object not found in MinIO or cache: " + remoteKey);
}

bool MinIOStorageClient::exists(const std::string &remoteKey)
{
    std::string url = buildObjectUrl(remoteKey);
    std::ostringstream cmd;
    cmd << "curl -s -f -I \"" << url << "\"";
    if (executeCurlCommand(cmd.str()))
    {
        return true;
    }
    return cacheStorage_.exists(remoteKey);
}

const std::string &MinIOStorageClient::endpoint() const
{
    return endpoint_;
}

const std::string &MinIOStorageClient::bucket() const
{
    return bucket_;
}
