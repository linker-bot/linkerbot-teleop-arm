#include "CanBus.h"
#include <algorithm>
#include <chrono>
#include <thread>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#ifdef __linux__
#include <ifaddrs.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <set>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
namespace Communication {
#ifdef __linux__
namespace {
[[noreturn]] void fail(const char *operation) {
    throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}
} // namespace
CanBus::CanBus(std::string interface, int bitrate) {
    if (interface.empty() || interface.size() >= IFNAMSIZ || bitrate <= 0)
        throw std::invalid_argument("invalid CAN interface or bitrate");
    int opened = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (opened < 0)
        fail("socket");
    try {
        ifreq request{};
        std::memcpy(request.ifr_name, interface.c_str(), interface.size());
        if (ioctl(opened, SIOCGIFINDEX, &request) < 0)
            fail("interface index");
        sockaddr_can address{};
        address.can_family = AF_CAN;
        address.can_ifindex = request.ifr_ifindex;
        if (bind(opened, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
            fail("bind");
        timeval timeout{0, 50000};
        if (setsockopt(opened, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
            fail("receive timeout");
        if (setsockopt(opened, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0)
            fail("send timeout");
        fd_ = opened;
    } catch (...) {
        ::close(opened);
        throw;
    }
}
CanBus::CanBus(AdoptFd handle) {
    if (handle.fd < 0)
        throw std::invalid_argument("invalid adopted fd");
    const timeval timeout{0, 50000};
    if (setsockopt(handle.fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(handle.fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        const int saved = errno;
        ::close(handle.fd);
        errno = saved;
        fail("configure adopted socket");
    }
    fd_ = handle.fd;
}
CanBus::CanBus(std::string type) {
    if (type != "master_arm")
        throw std::invalid_argument("unknown device type");
    ifaddrs *addresses = nullptr;
    if (getifaddrs(&addresses) < 0)
        fail("enumerate interfaces");
    std::set<std::string> names;
    for (auto p = addresses; p; p = p->ifa_next)
        if (p->ifa_name && (std::string(p->ifa_name).rfind("can", 0) == 0 ||
                            std::string(p->ifa_name).rfind("vcan", 0) == 0))
            names.insert(p->ifa_name);
    freeifaddrs(addresses);
    std::unique_ptr<CanBus> selected;
    for (const auto &name : names) {
        std::unique_ptr<CanBus> candidate;
        try {
            candidate = std::make_unique<CanBus>(name, 1000000);
            candidate->send({}, 0x64);
            auto frame = candidate->recv();
            if (frame.can_id != 0x64 || frame.can_dlc == 0)
                continue;
        } catch (const std::runtime_error &) {
            continue;
        }
        if (selected)
            throw std::runtime_error("multiple arms found; specify CAN interface explicitly");
        selected = std::move(candidate);
    }
    if (!selected)
        throw std::runtime_error("no arm found; configure CAN externally and specify interface");
    fd_ = selected->fd_;
    selected->fd_ = -1;
}
CanBus::~CanBus() {
    if (fd_ >= 0)
        ::close(fd_);
}
void CanBus::send(const std::vector<uint8_t> &data, uint32_t id, bool) {
    if (data.size() > 8 || id > CAN_SFF_MASK)
        throw std::invalid_argument("invalid classic CAN frame");
    can_frame frame{};
    frame.can_id = id;
    frame.can_dlc = static_cast<uint8_t>(data.size());
    std::copy(data.begin(), data.end(), frame.data);
    std::lock_guard<std::mutex> lock(send_mutex_);
    const auto count = ::write(fd_, &frame, sizeof(frame));
    if (count < 0)
        fail("CAN write");
    if (count != sizeof(frame))
        throw std::runtime_error("incomplete CAN write");
    // Preserve the legacy device pacing until real hardware measurements justify changing it.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}
CANFrame CanBus::recv(uint32_t id) {
    can_frame frame{};
    auto count = ::read(fd_, &frame, sizeof(frame));
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
        return {};
    if (count < 0)
        fail("CAN read");
    if (count != sizeof(frame))
        throw std::runtime_error("short CAN read");
    if (frame.can_dlc > 8 || (frame.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) ||
        (id != 0xFF && frame.can_id != id))
        return {};
    CANFrame result{};
    result.can_id = frame.can_id;
    result.can_dlc = frame.can_dlc;
    std::copy_n(frame.data, frame.can_dlc, result.data);
    return result;
}
void CanBus::setReceiveTimeout(int seconds, int micros) {
    if (seconds < 0 || micros < 0 || micros >= 1000000 || (seconds == 0 && micros == 0))
        throw std::invalid_argument("invalid timeout");
    timeval t{seconds, micros};
    if (setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) < 0)
        fail("timeout");
}
#else
CanBus::CanBus(std::string) {
    throw std::runtime_error("SocketCAN requires Linux");
}
CanBus::CanBus(std::string, int) {
    throw std::runtime_error("SocketCAN requires Linux");
}
CanBus::CanBus(AdoptFd) {
    throw std::runtime_error("SocketCAN requires Linux");
}
CanBus::~CanBus() = default;
void CanBus::send(const std::vector<uint8_t> &, uint32_t, bool) {
    throw std::runtime_error("SocketCAN requires Linux");
}
CANFrame CanBus::recv(uint32_t) {
    throw std::runtime_error("SocketCAN requires Linux");
}
void CanBus::setReceiveTimeout(int, int) {
    throw std::runtime_error("SocketCAN requires Linux");
}
#endif
} // namespace Communication
