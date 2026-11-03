#include "CliOptions.h"
#include "LinkerArm.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <vector>

namespace {
volatile std::sig_atomic_t running = 1;
void signalHandler(int) {
    running = 0;
}

void waitUntil(LinkerArm::Protocol::TimePoint deadline) {
    while (running) {
        const auto remaining = deadline - LinkerArm::Protocol::Clock::now();
        if (remaining <= LinkerArm::Protocol::Clock::duration::zero())
            break;
        std::this_thread::sleep_for(
            std::min(remaining, std::chrono::duration_cast<LinkerArm::Protocol::Clock::duration>(
                                    std::chrono::milliseconds(20))));
    }
}
} // namespace

int main(int argc, char *argv[]) {
    try {
        const auto options =
            LinkerArm::parseCliOptions(std::vector<std::string>(argv + 1, argv + argc));
        if (options.help) {
            std::cout << LinkerArm::cliUsage();
            return 0;
        }
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        LinkerArm::LinkerArm arm(options.interface_name, options.bitrate);
        const auto period = std::chrono::duration_cast<LinkerArm::Protocol::Clock::duration>(
            std::chrono::duration<double>(1.0 / options.rate_hz));
        std::uint64_t emitted = 0;
        while (running && (options.count == 0 || emitted < options.count)) {
            const auto started = LinkerArm::Protocol::Clock::now();
            const auto snapshot = arm.readJointState(options.timeout);
            if (!running)
                break;
            for (std::size_t i = 0; i < snapshot.positions_degrees.size(); ++i) {
                if (i)
                    std::cout << ' ';
                std::cout << snapshot.positions_degrees[i];
            }
            std::cout << '\n' << std::flush;
            ++emitted;
            if (options.count != 0 && emitted == options.count)
                break;
            // A slow query does not create a catch-up burst.
            waitUntil(started + period);
        }
    } catch (const std::exception &error) {
        if (!running)
            return 0;
        std::cerr << "linkerta: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
