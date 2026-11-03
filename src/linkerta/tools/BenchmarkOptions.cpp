#include "BenchmarkOptions.h"
#include <charconv>
#include <cmath>
#include <set>
#include <stdexcept>

namespace LinkerArm::Benchmark {
namespace {
std::uint64_t number(const std::string &text, const std::string &key) {
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (text.empty() || parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument(key + " requires an unsigned decimal integer");
    return result;
}
bool blank(const std::string &text) {
    return text.find_first_not_of(" \t\r\n") == std::string::npos;
}
} // namespace
Options parseOptions(const std::vector<std::string> &args) {
    Options options;
    std::set<std::string> seen;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto &key = args[i];
        if (!seen.insert(key).second)
            throw std::invalid_argument("duplicate option " + key);
        if (key == "--help") {
            options.help = true;
            continue;
        }
        if (key == "--simulate") {
            options.simulate = true;
            continue;
        }
        if (i + 1 == args.size())
            throw std::invalid_argument("missing value for " + key);
        const auto &value = args[++i];
        if (key == "--interface")
            options.interface_name = value;
        else if (key == "--output")
            options.output = value;
        else if (key == "--device-label")
            options.device_label = value;
        else if (key == "--firmware")
            options.firmware = value;
        else if (key == "--count")
            options.count = number(value, key);
        else if (key == "--timeout-ms") {
            const auto n = number(value, key);
            if (n < 1 || n > 10000)
                throw std::invalid_argument("timeout-ms must be 1..10000");
            options.timeout = std::chrono::milliseconds(n);
        } else if (key == "--drop-every")
            options.drop_every = number(value, key);
        else if (key == "--rate-hz") {
            std::size_t parsed = 0;
            try {
                options.rate_hz = std::stod(value, &parsed);
            } catch (const std::exception &) {
                throw std::invalid_argument("rate-hz must be finite and 0.1..200");
            }
            if (parsed != value.size() || value.find_first_of(" \t\r\n") != std::string::npos)
                throw std::invalid_argument("invalid rate-hz");
        } else
            throw std::invalid_argument("unknown option " + key);
    }
    if (options.help)
        return options;
    if (options.simulate == !options.interface_name.empty())
        throw std::invalid_argument("select exactly one of --simulate and --interface");
    if (options.count == 0 || options.count > 1000000)
        throw std::invalid_argument("count must be 1..1000000");
    if (!std::isfinite(options.rate_hz) || options.rate_hz < 0.1 || options.rate_hz > 200.0)
        throw std::invalid_argument("rate-hz must be finite and 0.1..200");
    if (blank(options.output) || options.output.find('\0') != std::string::npos)
        throw std::invalid_argument("an explicit --output CSV path is required");
    if (!options.simulate) {
        if (options.interface_name.size() > 15 ||
            options.interface_name.find_first_of(" /\\\t\n\r\v\f") != std::string::npos ||
            options.interface_name.find('\0') != std::string::npos)
            throw std::invalid_argument("invalid SocketCAN interface name");
        if (blank(options.device_label) || blank(options.firmware))
            throw std::invalid_argument("hardware runs require --device-label and --firmware");
        if (options.drop_every)
            throw std::invalid_argument("--drop-every is only available with --simulate");
    }
    return options;
}
const char *usage() noexcept {
    return "Usage: linkerta_benchmark (--simulate | --interface NAME) --output "
           "FILE [options]\n"
           "  --device-label TEXT --firmware TEXT  Required for hardware runs\n"
           "  --count N          Requests, 1..1000000 (default 100)\n"
           "  --rate-hz HZ       Maximum request rate, 0.1..200 (default 10)\n"
           "  --timeout-ms MS    Request deadline, 1..10000 (default 250)\n"
           "  --drop-every N     Simulation-only: drop every Nth response "
           "(default 0)\n"
           "  --help             Show help without opening hardware\n"
           "Writes CSV plus FILE.metadata.json. Existing files are rejected.\n"
           "Simulation measurements are synthetic, never physical-device SLO "
           "evidence.\n"
           "No calibration, interface configuration, or automatic recovery is "
           "performed.\n";
}
} // namespace LinkerArm::Benchmark
