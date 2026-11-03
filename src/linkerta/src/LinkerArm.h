#pragma once
#include "ArmProtocol.h"
#include "CanBus.h"
#include <atomic>
#include <condition_variable>
#include <thread>
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
class LinkerArm {
public:
    explicit LinkerArm(std::string device);
    LinkerArm(const std::string &, int);
    explicit LinkerArm(std::unique_ptr<Communication::ICanBus>);
    ~LinkerArm();
    std::vector<float> getJointPosition();
    std::vector<bool> getJointErrorCode();
    // Cached data can be invalid or stale. Check valid/isFresh() before use.
    Protocol::ArmSnapshot latestSnapshot();
    // One in-flight request; throws on invalid timeout, send/receive failure or
    // deadline expiry. Driver calls themselves cannot be forcibly interrupted.
    Protocol::ArmSnapshot readJointState(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(250));
    std::string getVersion();
    std::string lastError();
    void resetZero(uint8_t joint = 0xFF);
    void printJointInfo();

private:
    class RequestLease;
    void receiveResponse();
    std::unique_ptr<Communication::ICanBus> bus_;
    std::atomic<bool> running_{true};
    std::thread reader_;
    std::mutex state_mutex_;
    std::mutex request_mutex_;
    std::condition_variable request_available_;
    bool request_active_ = false;
    std::condition_variable updated_;
    Protocol::SnapshotAssembler assembler_;
    std::vector<uint8_t> version_;
    std::string error_;
    bool pending_query_ = false;
    Protocol::TimePoint query_deadline_{};
};
} // namespace LinkerArm
