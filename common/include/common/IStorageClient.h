#pragma once
#include <string>

class IStorageClient
{
public:
    virtual void upload(const std::string &localPath, const std::string &remoteKey) = 0;
    virtual void download(const std::string &remoteKey, const std::string &localPath) = 0;
    virtual bool exists(const std::string &remoteKey) = 0;
    virtual ~IStorageClient() = default;
};
