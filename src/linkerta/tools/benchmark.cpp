#include "BenchmarkOptions.h"
#include "BenchmarkStats.h"
#include "LinkerArm.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <linkerbot/can/fake_transport.hpp>
#include <memory>
#include <sstream>
#include <thread>

#ifndef LINKERTA_BUILD_REVISION
#define LINKERTA_BUILD_REVISION "unknown"
#endif

namespace {
namespace B = LinkerArm::Benchmark;
using Clock = LinkerArm::Protocol::Clock;
volatile std::sig_atomic_t stop_signal = 0;
void stop(int signal) {
    stop_signal = signal;
}
std::string json(const std::string &text) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\')
            out << '\\' << static_cast<char>(c);
        else if (c < 0x20)
            out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << unsigned(c);
        else
            out << static_cast<char>(c);
    }
    out << '"';
    return out.str();
}
std::string csv(const std::string &text) {
    std::string result = "\"";
    for (const char c : text) {
        if (c == '"')
            result += '"';
        result += c;
    }
    return result + '"';
}
std::string utcNow() {
    const auto time = std::time(nullptr);
    std::ostringstream out;
    out << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}
void metric(std::ostream &out, const std::optional<std::uint64_t> &value) {
    if (value)
        out << *value;
    else
        out << "null";
}
void writeMetadata(const B::Options &options, const std::string &started,
                   const std::string &version, std::uint64_t performed,
                   const std::vector<std::uint64_t> &durations, bool complete) {
    std::ofstream out(options.output + ".metadata.json");
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "{\n  \"schema_version\": 1,\n  \"synthetic\": " << std::boolalpha << options.simulate
        << ",\n  \"complete\": " << complete << ",\n  \"started_utc\": " << json(started)
        << ",\n  \"driver_revision\": " << json(LINKERTA_BUILD_REVISION)
        << ",\n  \"device_label\": "
        << json(options.simulate ? "FakeTransport" : options.device_label)
        << ",\n  \"firmware_supplied\": " << json(options.simulate ? "synthetic" : options.firmware)
        << ",\n  \"observed_version\": " << json(version)
        << ",\n  \"interface\": " << json(options.interface_name)
        << ",\n  \"requested_count\": " << options.count
        << ",\n  \"performed_count\": " << performed
        << ",\n  \"successful_count\": " << durations.size()
        << ",\n  \"failed_count\": " << performed - durations.size() << ",\n  \"success_ratio\": "
        << (performed ? static_cast<double>(durations.size()) / performed : 0.0)
        << ",\n  \"rate_hz\": " << options.rate_hz
        << ",\n  \"timeout_ms\": " << options.timeout.count()
        << ",\n  \"transmit_gap_us\": 5000,\n  \"simulation_drop_every\": " << options.drop_every
        << ",\n  \"latency_population\": \"successful requests only; host steady "
           "clock\","
        << "\n  \"percentile_method\": \"nearest rank\",\n  \"p50_us\": ";
    metric(out, B::percentile(durations, 0.50));
    out << ",\n  \"p95_us\": ";
    metric(out, B::percentile(durations, 0.95));
    out << ",\n  \"p99_us\": ";
    metric(out, B::percentile(durations, 0.99));
    out << "\n}\n";
}
std::shared_ptr<LinkerArm::LinkerArm> makeArm(const B::Options &options) {
    if (!options.simulate)
        return std::make_shared<LinkerArm::LinkerArm>(options.interface_name, 1000000);
    auto fake = std::make_unique<linkerbot::can::FakeTransport>();
    fake->on_request(0x64, [](const linkerbot::can::Message &) {
        linkerbot::can::Message version;
        version.id = 0x64;
        version.dlc = 1;
        version.data[0] = 123;
        return std::vector<linkerbot::can::Message>{version};
    });
    fake->on_request(0x04, [count = std::uint64_t{0},
                            drop = options.drop_every](const linkerbot::can::Message &) mutable {
        ++count;
        std::vector<linkerbot::can::Message> replies;
        if (drop && count % drop == 0)
            return replies;
        for (unsigned block = 0; block < 4; ++block) {
            linkerbot::can::Message frame;
            frame.id = 0x65 + block;
            frame.dlc = block % 2 ? 7 : 8;
            for (unsigned joint = 0; joint < (block % 2 ? 3u : 4u); ++joint) {
                frame.data[2 * joint] = 0x03;
                frame.data[2 * joint + 1] = 0x84;
            }
            replies.push_back(frame);
        }
        return replies;
    });
    auto session = linkerbot::transport::Session::own(std::move(fake));
    LinkerArm::SessionOptions session_options;
    session_options.own_session = true;
    return std::make_shared<LinkerArm::LinkerArm>(session, session_options);
}
int run(const B::Options &options) {
    if (std::filesystem::exists(options.output) ||
        std::filesystem::exists(options.output + ".metadata.json"))
        throw std::invalid_argument("refusing to overwrite an existing CSV or metadata file");
    auto arm = makeArm(options);
    std::ofstream out(options.output);
    out.exceptions(std::ios::badbit | std::ios::failbit);
    out << "request,elapsed_us,duration_us,success,sequence,generation,oldest_"
           "age_us,skew_us,"
           "route_pending,route_high_water,route_overflow,tx_pending,tx_high_"
           "water,session_overflows,error\n";
    const auto started_utc = utcNow();
    const auto started = Clock::now();
    std::uint64_t performed = 0;
    std::vector<std::uint64_t> successful_durations;
    successful_durations.reserve(static_cast<std::size_t>(options.count));
    writeMetadata(options, started_utc, "not yet observed", 0, successful_durations, false);
    const auto period = std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double>(1.0 / options.rate_hz));
    for (std::uint64_t request = 0; request < options.count && !stop_signal; ++request) {
        const auto begin = Clock::now();
        std::optional<LinkerArm::Protocol::ArmSnapshot> snapshot;
        std::string error;
        try {
            snapshot = arm->readJointState(options.timeout);
        } catch (const std::exception &exception) {
            error = exception.what();
        }
        const auto end = Clock::now();
        const auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
        if (snapshot)
            successful_durations.push_back(static_cast<std::uint64_t>(duration));
        ++performed;
        const auto route = arm->routeHealth();
        const auto session = arm->sessionHealth();
        out << request + 1 << ','
            << std::chrono::duration_cast<std::chrono::microseconds>(begin - started).count() << ','
            << duration << ',' << (snapshot ? 1 : 0) << ',';
        if (snapshot) {
            const auto limits =
                std::minmax_element(snapshot->received_at.begin(), snapshot->received_at.end());
            out << snapshot->sequence << ',' << snapshot->generation << ','
                << std::chrono::duration_cast<std::chrono::microseconds>(end - *limits.first)
                       .count()
                << ','
                << std::chrono::duration_cast<std::chrono::microseconds>(*limits.second -
                                                                         *limits.first)
                       .count();
        } else {
            out << ',' << session.generation << ",,";
        }
        out << ',' << route.pending << ',' << route.high_water << ',' << route.overflow << ','
            << session.pending_tx << ',' << session.queue_high_water << ',' << session.overflows
            << ',' << csv(error) << '\n';
        out.flush();
        if (request + 1 == options.count)
            break;
        const auto deadline = begin + period;
        while (!stop_signal && Clock::now() < deadline) {
            const auto remaining = deadline - Clock::now();
            if (remaining > Clock::duration::zero())
                std::this_thread::sleep_for(std::min(
                    remaining,
                    std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(20))));
        }
    }
    std::string version;
    try {
        version = arm->getVersion();
    } catch (const std::exception &) {
        version = "unavailable";
    }
    writeMetadata(options, started_utc, version, performed, successful_durations,
                  performed == options.count);
    std::cout << (options.simulate ? "SYNTHETIC" : "HARDWARE") << " requests=" << performed
              << " successes=" << successful_durations.size()
              << " failures=" << performed - successful_durations.size() << " success_ratio="
              << (performed ? static_cast<double>(successful_durations.size()) / performed : 0.0)
              << " p50_us=";
    metric(std::cout, B::percentile(successful_durations, 0.50));
    std::cout << " p95_us=";
    metric(std::cout, B::percentile(successful_durations, 0.95));
    std::cout << " p99_us=";
    metric(std::cout, B::percentile(successful_durations, 0.99));
    std::cout << '\n';
    if (stop_signal)
        return 128 + stop_signal;
    return successful_durations.size() == options.count ? 0 : 1;
}
} // namespace
int main(int argc, char **argv) {
    try {
        const auto options = B::parseOptions(std::vector<std::string>(argv + 1, argv + argc));
        if (options.help) {
            std::cout << B::usage();
            return 0;
        }
        std::signal(SIGINT, stop);
        std::signal(SIGTERM, stop);
        return run(options);
    } catch (const std::exception &exception) {
        std::cerr << "linkerta_benchmark: " << exception.what() << '\n';
        return 2;
    }
}
