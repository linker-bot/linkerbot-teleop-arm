#pragma once
#include "ArmProtocol.h"
#include <atomic>
#include <condition_variable>
#include <linkerbot/transport/session.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace LinkerArm {
enum FRAME_PROPERTY {
    GET_JOINT_POSITION_1 = 2,
    GET_JOINT_POSITION_2 = 3,
    GET_JOINT_POSITION = 4,
    LEFT_JOINT_POSITION_1 = 0x65,
    LEFT_JOINT_POSITION_2 = 0x66,
    RIGHT_JOINT_POSITION_1 = 0x67,
    RIGHT_JOINT_POSITION_2 = 0x68,
    RESET_ZERO_COMMAND_1 = 0xC0,
    RESET_ZERO_COMMAND_2 = 0xC1,
    LINKER_HAND_VERSION = 0x64
};

struct SessionOptions {
    std::size_t receive_capacity = 64;
    std::string endpoint;
    std::chrono::milliseconds command_timeout{250};
    std::chrono::microseconds transmit_gap{5000};
    // Explicit ownership transfer: only an owner closes or physically recovers the Session.
    bool own_session = false;
};

class LinkerArm {
public:
    explicit LinkerArm(const std::string &device);
    LinkerArm(const std::string &interface_name, int bitrate);
    explicit LinkerArm(std::shared_ptr<linkerbot::transport::Session> session);
    LinkerArm(std::shared_ptr<linkerbot::transport::Session> session, SessionOptions options);
    ~LinkerArm();
    std::vector<float> getJointPosition();
    std::vector<bool> getJointErrorCode();
    Protocol::ArmSnapshot latestSnapshot();
    Protocol::ArmSnapshot readJointState(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(250));
    std::string getVersion();
    std::string lastError();
    void resetZero(uint8_t joint = 0xFF);
    void printJointInfo();
    // Global physical recovery is permitted only for explicitly owned Sessions.
    // Re-registers routes, clears cached data and waits for a valid version reply.
    std::uint64_t recover(std::chrono::milliseconds timeout = std::chrono::milliseconds(250));
    linkerbot::transport::SessionHealth sessionHealth() const;
    linkerbot::transport::RouteHealth routeHealth() const;

private:
    class RequestLease;
    void receiveResponse();
    void stopReader() noexcept;
    void sendFrame(std::uint32_t id, const std::vector<std::uint8_t> &data,
                   Protocol::TimePoint deadline);
    void refreshSessionState();
    void observeOverflow();
    void invalidate(const std::string &message, std::uint64_t generation);
    std::shared_ptr<linkerbot::transport::Session> session_;
    std::shared_ptr<linkerbot::transport::RouteInbox> inbox_;
    SessionOptions options_;
    std::atomic<bool> running_{false};
    std::thread reader_;
    mutable std::mutex state_mutex_;
    std::mutex request_mutex_;
    std::condition_variable request_available_;
    bool request_active_ = false;
    std::condition_variable updated_;
    Protocol::SnapshotAssembler assembler_;
    std::vector<uint8_t> version_;
    std::string error_;
    bool pending_query_ = false;
    bool identity_required_ = false;
    std::uint64_t generation_ = 0;
    std::uint64_t observed_overflow_ = 0;
    Protocol::TimePoint query_deadline_{};
};
} // namespace LinkerArm
