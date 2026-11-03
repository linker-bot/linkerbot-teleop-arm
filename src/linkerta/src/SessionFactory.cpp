#include "SessionFactory.h"
#include "ArmProtocol.h"

#include <algorithm>
#include <chrono>
#include <set>
#include <stdexcept>
#ifdef __linux__
#include <ifaddrs.h>
#include <linkerbot/can/socketcan_transport.hpp>
#endif

namespace LinkerArm {
namespace transport = linkerbot::transport;

std::shared_ptr<transport::Session> makeSocketSession(const std::string &interface_name,
                                                      int bitrate) {
    if (bitrate <= 0)
        throw std::invalid_argument("bitrate must be positive; configure CAN outside this process");
#ifdef __linux__
    return transport::Session::create(
        [interface_name] {
            return std::make_unique<linkerbot::can::SocketCANTransport>(interface_name, false);
        },
        32);
#else
    (void)interface_name;
    throw std::runtime_error("SocketCAN requires Linux");
#endif
}

std::shared_ptr<transport::Session> discoverArmSession(const std::string &device_type) {
    if (device_type != "master_arm")
        throw std::invalid_argument("unknown device type");
#ifdef __linux__
    ifaddrs *addresses = nullptr;
    if (getifaddrs(&addresses) != 0)
        throw std::runtime_error("cannot enumerate CAN interfaces");
    const std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> holder(addresses, &freeifaddrs);
    std::set<std::string> interfaces;
    for (auto *address = addresses; address; address = address->ifa_next) {
        if (!address->ifa_name)
            continue;
        const std::string name(address->ifa_name);
        if (name.rfind("can", 0) == 0 || name.rfind("vcan", 0) == 0)
            interfaces.insert(name);
    }
    std::shared_ptr<transport::Session> selected;
    for (const auto &name : interfaces) {
        std::shared_ptr<transport::Session> candidate;
        bool matched = false;
        try {
            candidate = makeSocketSession(name, 1000000);
            auto inbox = candidate->subscribe_routes({{0x64, false}}, 8);
            const auto deadline = transport::Clock::now() + std::chrono::milliseconds(150);
            linkerbot::can::Message query;
            query.id = 0x64;
            transport::SendOptions options;
            options.deadline = deadline;
            options.endpoint = "tele-arm-discovery";
            transport::CancellationSource cancellation;
            options.cancellation = cancellation.token();
            try {
                candidate->send({{query}}, options);
                while (const auto reply = inbox->receive_until(deadline)) {
                    Protocol::Frame frame;
                    frame.can_id = reply->message.id;
                    frame.dlc = reply->message.dlc;
                    frame.is_extended = reply->message.is_extended;
                    frame.is_fd = reply->message.is_fd;
                    std::copy_n(reply->message.data.begin(), frame.data.size(), frame.data.begin());
                    const auto decoded = Protocol::decodeFrame(frame);
                    if (decoded.frame && decoded.frame->kind == Protocol::FrameKind::version) {
                        matched = true;
                        break;
                    }
                }
            } catch (...) {
                cancellation.cancel();
                throw;
            }
            inbox->close();
        } catch (const std::exception &) {
            matched = false;
        }
        if (!matched)
            continue;
        if (selected)
            throw std::runtime_error("multiple arms found; specify CAN interface explicitly");
        selected = std::move(candidate);
    }
    if (!selected)
        throw std::runtime_error("no arm found; configure CAN externally and specify interface");
    return selected;
#else
    throw std::runtime_error("SocketCAN requires Linux");
#endif
}
} // namespace LinkerArm
