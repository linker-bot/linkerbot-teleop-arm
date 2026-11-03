#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
struct CANFrame {
    uint32_t can_id{};
    uint8_t can_dlc{};
    uint8_t data[8]{};
};
struct Job {
    const char *name;
    uint32_t id;
    bool is_ext;
    uint8_t data0;
};
namespace Communication {
class ICanBus {
public:
    virtual ~ICanBus() = default;
    virtual void send(const std::vector<uint8_t> &data, uint32_t id, bool wait = false) = 0;
    // Implementations must return within a finite bounded interval (production: 50 ms).
    // Empty frame means bounded idle timeout, never a transport failure.
    virtual CANFrame recv(uint32_t id = 0xFF) = 0;
};
class CanBus : public ICanBus {
public:
    explicit CanBus(std::string device_type);
    CanBus(std::string interface, int bitrate);
    // Test seam: takes ownership of a configured datagram socket. Production
    // callers should use the interface constructor.
    struct AdoptFd {
        int fd;
    };
    explicit CanBus(AdoptFd);
    ~CanBus() override;
    CanBus(const CanBus &) = delete;
    CanBus &operator=(const CanBus &) = delete;
    void send(const std::vector<uint8_t> &, uint32_t, bool = false) override;
    CANFrame recv(uint32_t = 0xFF) override;
    void setReceiveTimeout(int, int);

private:
    [[maybe_unused]] int fd_{-1};
    std::mutex send_mutex_;
};
} // namespace Communication
