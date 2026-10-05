#pragma once

#include <string>

class Master
{
public:
    explicit Master(std::string listenAddress);

    const std::string &listenAddress() const;

private:
    std::string listenAddress_;
};
