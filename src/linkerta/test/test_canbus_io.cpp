#include "CanBus.h"
#include <gtest/gtest.h>

#ifdef __linux__
#include <cerrno>
#include <fcntl.h>
#include <linux/can.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
class SocketCanIo : public testing::Test {
protected:
    int peer = -1;
    std::unique_ptr<Communication::CanBus> bus;
    void SetUp() override {
        int sockets[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets), 0);
        peer = sockets[1];
        bus = std::make_unique<Communication::CanBus>(Communication::CanBus::AdoptFd{sockets[0]});
    }
    void TearDown() override {
        close(peer);
    }
};

TEST_F(SocketCanIo, RejectsOversizedPayloadBeforeWriting) {
    EXPECT_THROW(bus->send(std::vector<uint8_t>(9, 0xff), 0x04), std::invalid_argument);
    EXPECT_THROW(bus->send({}, 0x800), std::invalid_argument);
    can_frame frame{};
    EXPECT_EQ(recv(peer, &frame, sizeof(frame), MSG_DONTWAIT), -1);
    EXPECT_EQ(errno, EAGAIN);
    bus->send(std::vector<uint8_t>(8, 0xab), 0x04);
    ASSERT_EQ(read(peer, &frame, sizeof(frame)), sizeof(frame));
    EXPECT_EQ(frame.can_dlc, 8);
    EXPECT_EQ(frame.data[7], 0xab);
}

TEST_F(SocketCanIo, EmptyFrameIsInitializedAndLengthBounded) {
    bus->send({}, 0x04);
    can_frame wire{};
    ASSERT_EQ(read(peer, &wire, sizeof(wire)), sizeof(wire));
    EXPECT_EQ(wire.can_dlc, 0);
    for (auto byte : wire.data)
        EXPECT_EQ(byte, 0);
    wire.can_id = 0x65;
    wire.can_dlc = 9;
    ASSERT_EQ(write(peer, &wire, sizeof(wire)), sizeof(wire));
    EXPECT_EQ(bus->recv().can_dlc, 0);
}

TEST_F(SocketCanIo, FiltersRemoteErrorAndExtendedFrames) {
    for (const uint32_t flag : {CAN_RTR_FLAG, CAN_ERR_FLAG, CAN_EFF_FLAG}) {
        can_frame wire{};
        wire.can_id = 0x65 | flag;
        wire.can_dlc = 8;
        ASSERT_EQ(write(peer, &wire, sizeof(wire)), sizeof(wire));
        EXPECT_EQ(bus->recv().can_dlc, 0);
    }
}

TEST_F(SocketCanIo, ShortDatagramIsAnErrorAndIdleIsNot) {
    const uint8_t byte = 1;
    ASSERT_EQ(write(peer, &byte, 1), 1);
    EXPECT_THROW(bus->recv(), std::runtime_error);
    EXPECT_EQ(bus->recv().can_dlc, 0);
    EXPECT_THROW(bus->setReceiveTimeout(0, 0), std::invalid_argument);
}

TEST(CanBusValidation, RejectsInvalidInterfaceBeforeSocketAccess) {
    EXPECT_THROW(Communication::CanBus("", 1000000), std::invalid_argument);
    EXPECT_THROW(Communication::CanBus(std::string(128, 'x'), 1000000), std::invalid_argument);
    EXPECT_THROW(Communication::CanBus("can0", 0), std::invalid_argument);
    EXPECT_THROW(Communication::CanBus("nonexistent-can", 1000000), std::runtime_error);
}
} // namespace
#endif
