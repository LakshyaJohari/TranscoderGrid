#include "master/Master.h"

Master::Master(std::string listenAddress)
    : listenAddress_(std::move(listenAddress)) {}

const std::string &Master::listenAddress() const
{
    return listenAddress_;
}
