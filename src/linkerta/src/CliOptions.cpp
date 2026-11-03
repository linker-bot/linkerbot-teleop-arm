#include "CliOptions.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

namespace LinkerArm {
namespace {
std::uint64_t unsignedNumber(const std::string &text, const std::string &name) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument(name + " must be an unsigned decimal integer");
    return result;
}

void validateInterface(const std::string &name) {
    if (name.empty() || name.size() > 15 || name.find('\0') != std::string::npos ||
        name.find_first_of(" /\\\t\n\r\v\f") != std::string::npos)
        throw std::invalid_argument(
            "interface must contain 1..15 characters without whitespace or slashes");
}
} // namespace

CliOptions parseCliOptions(const std::vector<std::string> &args) {
    CliOptions options;
    std::set<std::string> seen;
    std::size_t positional = 0;
    for (std::size_t i = 0; i < args.size(); ++i) {
        auto key = args[i];
        if (key == "--help" || key == "-h") {
            options.help = true;
            continue;
        }
        std::string value;
        if (key.rfind("--", 0) == 0) {
            const auto equal = key.find('=');
            if (equal != std::string::npos) {
                value = key.substr(equal + 1);
                key.resize(equal);
            } else {
                if (i + 1 == args.size())
                    throw std::invalid_argument("missing value for " + key);
                value = args[++i];
            }
        } else {
            if (!key.empty() && key.front() == '-')
                throw std::invalid_argument("unknown option " + key);
            if (positional > 1)
                throw std::invalid_argument(
                    "expected at most interface and bitrate positional arguments");
            value = key;
            key = positional++ == 0 ? "--interface" : "--bitrate";
        }
        if (key != "--interface" && key != "--bitrate" && key != "--rate-hz" &&
            key != "--timeout-ms" && key != "--count")
            throw std::invalid_argument("unknown option " + key);
        if (!seen.insert(key).second)
            throw std::invalid_argument("duplicate option " + key);
        if (key == "--interface") {
            validateInterface(value);
            options.interface_name = value;
        } else if (key == "--rate-hz") {
            std::size_t consumed = 0;
            try {
                options.rate_hz = std::stod(value, &consumed);
            } catch (const std::exception &) {
                throw std::invalid_argument("rate-hz must be finite and between 0.1 and 200");
            }
            if (consumed != value.size() || value.empty() ||
                value.find_first_of(" \t\r\n\v\f") != std::string::npos ||
                !std::isfinite(options.rate_hz) || options.rate_hz < 0.1 || options.rate_hz > 200.0)
                throw std::invalid_argument("rate-hz must be finite and between 0.1 and 200");
        } else {
            const auto number = unsignedNumber(value, key);
            if (key == "--bitrate") {
                if (number == 0 || number > 10000000)
                    throw std::invalid_argument("bitrate must be between 1 and 10000000");
                options.bitrate = static_cast<int>(number);
            } else if (key == "--timeout-ms") {
                if (number == 0 || number > 10000)
                    throw std::invalid_argument("timeout-ms must be between 1 and 10000");
                options.timeout = std::chrono::milliseconds(number);
            } else {
                options.count = number;
            }
        }
    }
    return options;
}

const char *cliUsage() noexcept {
    return "Usage: linkerta [interface [bitrate]] [options]\n"
           "  --help, -h             Show help without opening hardware\n"
           "  --interface NAME       Configured SocketCAN interface (default can0)\n"
           "  --bitrate N            Expected bitrate, 1..10000000 (default 1000000)\n"
           "  --rate-hz HZ           Maximum query rate, 0.1..200 (default 1)\n"
           "  --timeout-ms MS        Complete-snapshot deadline, 1..10000 (default 250)\n"
           "  --count N              Stop after N complete snapshots; 0 runs until signal\n"
           "Output: fourteen space-separated joint positions in degrees per snapshot.\n"
           "CAN must be configured externally; bitrate is not applied by this process.\n"
           "No calibration is performed. Any query failure exits without publishing cached data.\n";
}
} // namespace LinkerArm
