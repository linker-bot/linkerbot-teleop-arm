#include "LinkerArm.h"
#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <linkerbot/can/fake_transport.hpp>
#include <thread>

using namespace std::chrono_literals;
namespace {
void exerciseLifetime() {
    auto session = linkerbot::transport::Session::create([] {
        auto fake = std::make_unique<linkerbot::can::FakeTransport>();
        fake->on_request(4, [](const linkerbot::can::Message &) {
            std::vector<linkerbot::can::Message> replies;
            for (unsigned i = 0; i < 4; ++i) {
                linkerbot::can::Message frame;
                frame.id = 0x65 + i;
                frame.dlc = i % 2 ? 7 : 8;
                replies.push_back(frame);
            }
            return replies;
        });
        return fake;
    });
    {
        LinkerArm::SessionOptions options;
        options.transmit_gap = 0us;
        LinkerArm::LinkerArm arm(session, options);
        const auto snapshot = arm.readJointState(1s);
        EXPECT_TRUE(snapshot.valid);
    }
    session->close();
}
#ifdef __linux__
std::size_t entries(const char *path) {
    std::size_t count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
        (void)entry;
        ++count;
    }
    return count;
}
// Wait for procfs task teardown to settle before taking the baseline.
std::size_t stableThreadCount() {
    auto previous = entries("/proc/self/task");
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    unsigned equal_samples = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(2ms);
        const auto current = entries("/proc/self/task");
        equal_samples = current == previous ? equal_samples + 1 : 0;
        previous = current;
        if (equal_samples >= 10)
            return current;
    }
    ADD_FAILURE() << "Thread count did not settle before lifecycle baseline";
    return previous;
}
#endif
TEST(Lifecycle, RepeatedCreateReadCloseReleasesResources) {
    exerciseLifetime(); // Warm up any runtime lazy initialization before
                        // counting.
#ifdef __linux__
    const auto threads = stableThreadCount();
    const auto descriptors = entries("/proc/self/fd");
#endif
    for (unsigned cycle = 0; cycle < 40; ++cycle) {
        SCOPED_TRACE(cycle);
        exerciseLifetime();
    }
#ifdef __linux__
    // A joined thread can briefly remain visible in procfs while the kernel
    // completes teardown. Wait for equality, never hide leakage with tolerance.
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (entries("/proc/self/task") != threads && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    EXPECT_EQ(entries("/proc/self/task"), threads);
    EXPECT_EQ(entries("/proc/self/fd"), descriptors);
#endif
}
} // namespace
