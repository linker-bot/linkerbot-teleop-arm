#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace LinkerArm::Benchmark {
struct Options {
    bool simulate = false;
    bool help = false;
    std::string interface_name;
    std::string output;
    std::string device_label;
    std::string firmware;
    std::uint64_t count = 100;
    double rate_hz = 10.0;
    std::chrono::milliseconds timeout{250};
    std::uint64_t drop_every = 0;
};
Options parseOptions(const std::vector<std::string> &args);
const char *usage() noexcept;
} // namespace LinkerArm::Benchmark
