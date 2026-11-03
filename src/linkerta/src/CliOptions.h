#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace LinkerArm {

struct CliOptions {
    std::string interface_name = "can0";
    int bitrate = 1000000;
    double rate_hz = 1.0;
    std::chrono::milliseconds timeout{250};
    std::uint64_t count = 0; // Zero means run until SIGINT or SIGTERM.
    bool help = false;
};

// args excludes argv[0]. Parsing never opens or configures hardware.
CliOptions parseCliOptions(const std::vector<std::string> &args);
const char *cliUsage() noexcept;

} // namespace LinkerArm
