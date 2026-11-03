#include <gtest/gtest.h>
#ifdef __linux__
#include <cerrno>
#include <linkerbot/can/socketcan_transport.hpp>
#include <linux/can.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {
using linkerbot::can::Message;
using linkerbot::can::SocketCANTransport;
class SocketCanIo : public testing::Test {
protected:
    int peer = -1;
    std::unique_ptr<SocketCANTransport> bus;
    void SetUp() override {
        int sockets[2];
        ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets), 0);
        peer = sockets[1];
        bus = std::make_unique<SocketCANTransport>(SocketCANTransport::AdoptFd{sockets[0]});
    }
    void TearDown() override {
        close(peer);
    }
};
TEST_F(SocketCanIo, RejectsOversizedPayloadBeforeWriting) {
    Message message;
    message.id = 4;
    message.dlc = 9;
    EXPECT_THROW(bus->write(message), linkerbot::can::ValidationError);
    message.id = 0x800;
    message.dlc = 0;
    EXPECT_THROW(bus->write(message), linkerbot::can::ValidationError);
    can_frame frame{};
    EXPECT_EQ(recv(peer, &frame, sizeof(frame), MSG_DONTWAIT), -1);
    EXPECT_EQ(errno, EAGAIN);
    message.id = 4;
    message.dlc = 8;
    std::fill_n(message.data.begin(), 8, 0xab);
    bus->write(message);
    ASSERT_EQ(read(peer, &frame, sizeof(frame)), sizeof(frame));
    EXPECT_EQ(frame.can_dlc, 8);
    EXPECT_EQ(frame.data[7], 0xab);
}
TEST_F(SocketCanIo, EmptyFrameIsInitializedAndLengthBounded) {
    Message message;
    message.id = 4;
    bus->write(message);
    can_frame wire{};
    ASSERT_EQ(read(peer, &wire, sizeof(wire)), sizeof(wire));
    EXPECT_EQ(wire.can_dlc, 0);
    for (const auto byte : wire.data)
        EXPECT_EQ(byte, 0);
    wire.can_id = 0x65;
    wire.can_dlc = 9;
    ASSERT_EQ(write(peer, &wire, sizeof(wire)), sizeof(wire));
    EXPECT_THROW((void)bus->read(), linkerbot::can::ValidationError);
}
TEST_F(SocketCanIo, FiltersRemoteErrorAndPreservesExtendedIdentity) {
    for (const uint32_t flag : {CAN_RTR_FLAG, CAN_ERR_FLAG}) {
        can_frame wire{};
        wire.can_id = 0x65 | flag;
        wire.can_dlc = 8;
        ASSERT_EQ(write(peer, &wire, sizeof(wire)), sizeof(wire));
        EXPECT_FALSE(bus->read());
    }
    can_frame wire{};
    wire.can_id = 0x65 | CAN_EFF_FLAG;
    wire.can_dlc = 8;
    ASSERT_EQ(write(peer, &wire, sizeof(wire)), sizeof(wire));
    const auto message = bus->read();
    ASSERT_TRUE(message);
    EXPECT_TRUE(message->is_extended);
    EXPECT_EQ(message->id, 0x65);
}
TEST_F(SocketCanIo, ShortDatagramIsAnErrorAndIdleIsNot) {
    const uint8_t byte = 1;
    ASSERT_EQ(write(peer, &byte, 1), 1);
    EXPECT_THROW((void)bus->read(), linkerbot::can::TransportIOError);
    EXPECT_FALSE(bus->read());
    bus->close();
    EXPECT_THROW((void)bus->read(), linkerbot::can::ClosedError);
}
TEST(CanBusValidation, RejectsInvalidInterfaceBeforeSocketAccess) {
    EXPECT_THROW(SocketCANTransport(""), linkerbot::can::ValidationError);
    EXPECT_THROW(SocketCANTransport(std::string(128, 'x')), linkerbot::can::ValidationError);
    EXPECT_THROW(SocketCANTransport("nonexistent-can"), linkerbot::can::TransportIOError);
}
} // namespace
#endif
