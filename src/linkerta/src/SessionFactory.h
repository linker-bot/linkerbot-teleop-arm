#pragma once

#include <linkerbot/transport/session.hpp>
#include <memory>
#include <string>

namespace LinkerArm {
std::shared_ptr<linkerbot::transport::Session> makeSocketSession(const std::string &interface_name,
                                                                 int bitrate);
std::shared_ptr<linkerbot::transport::Session> discoverArmSession(const std::string &device_type);
} // namespace LinkerArm
