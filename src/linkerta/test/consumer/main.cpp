#include <chrono>
#include <linkerbot/can/fake_transport.hpp>
#include <linkerta/ArmProtocol.h>
#include <linkerta/LinkerArm.h>
#include <linkerta/SessionFactory.h>
#include <memory>
#include <vector>

int main() {
    using namespace std::chrono_literals;
    using linkerbot::can::Message;
    auto transport = std::make_unique<linkerbot::can::FakeTransport>();
    transport->on_request(4, [](const Message &) {
        std::vector<Message> frames;
        for (unsigned i = 0; i < 4; ++i) {
            Message frame;
            frame.id = 0x65 + i;
            frame.dlc = i % 2 ? 7 : 8;
            frame.data[1] = 100;
            frames.push_back(frame);
        }
        return frames;
    });
    auto session = linkerbot::transport::Session::own(std::move(transport));
    LinkerArm::SessionOptions options;
    options.transmit_gap = 0us;
    {
        LinkerArm::LinkerArm arm(session, options);
        const auto state = arm.readJointState(1s);
        if (!state.valid || state.positions_degrees[0] != 10.0f)
            return 1;
        if (LinkerArm::Protocol::decodeAngle(0, 100) != 10.0f)
            return 2;
    }
    session->close();
    return 0;
}
