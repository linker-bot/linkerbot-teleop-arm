#include "LinkerArm.h"
#include "SessionFactory.h"
#include <atomic>
#include <gtest/gtest.h>
#include <linkerbot/can/socketcan_transport.hpp>
#include <thread>

using namespace std::chrono_literals;
namespace {
class Emulator {
public:
    Emulator() : bus_(LINKERTA_TEST_VCAN_INTERFACE, false), worker_([this] { run(); }) {}
    ~Emulator() {
        stopped_ = true;
        worker_.join();
    }
    std::atomic<unsigned> queries{0};
    std::atomic<bool> respond{true};
    std::atomic<bool> failed{false};

private:
    void run() {
        try {
            while (!stopped_) {
                const auto request = bus_.read();
                if (!request) {
                    std::this_thread::sleep_for(1ms);
                    continue;
                }
                if (request->id != 4)
                    continue;
                ++queries;
                if (!respond)
                    continue;
                for (unsigned block = 0; block < 4; ++block) {
                    linkerbot::can::Message reply;
                    reply.id = 0x65 + block;
                    reply.dlc = block % 2 ? 7 : 8;
                    for (unsigned joint = 0; joint < (block % 2 ? 3u : 4u); ++joint) {
                        reply.data[2 * joint] = 0x03;
                        reply.data[2 * joint + 1] = 0x84;
                    }
                    bus_.write(reply);
                }
            }
        } catch (...) {
            failed = true;
        }
    }
    linkerbot::can::SocketCANTransport bus_;
    std::atomic<bool> stopped_{false};
    std::thread worker_;
};
TEST(Vcan, RealSocketRoutesCompleteSnapshotAndTimesOutWithoutReplay) {
    Emulator emulator;
    auto session = LinkerArm::makeSocketSession(LINKERTA_TEST_VCAN_INTERFACE, 1000000);
    {
        LinkerArm::LinkerArm arm(session);
        const auto snapshot = arm.readJointState(1s);
        ASSERT_TRUE(snapshot.valid);
        for (const auto position : snapshot.positions_degrees)
            EXPECT_FLOAT_EQ(position, 90.0f);
        emulator.respond = false;
        EXPECT_THROW(arm.readJointState(80ms), std::exception);
        EXPECT_FALSE(emulator.failed);
        EXPECT_GE(emulator.queries.load(), 2u);
    }
    session->close();
}
} // namespace
