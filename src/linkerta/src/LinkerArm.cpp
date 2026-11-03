#include "LinkerArm.h"
#include <algorithm>
#include <iostream>
#include <optional>
#include <sstream>
namespace LinkerArm {
// Keep the queue wait bounded without a platform-specific timed-mutex path.
// The gate mutex protects only ownership bookkeeping, never transport I/O.
class LinkerArm::RequestLease {
public:
    explicit RequestLease(LinkerArm &arm,
                          std::optional<Protocol::TimePoint> deadline = std::nullopt)
        : arm_(arm) {
        std::unique_lock<std::mutex> lock(arm_.request_mutex_);
        const auto available = [&] { return !arm_.request_active_; };
        if (deadline) {
            if (!arm_.request_available_.wait_until(lock, *deadline, available))
                throw std::runtime_error("joint request queue timeout");
        } else {
            arm_.request_available_.wait(lock, available);
        }
        arm_.request_active_ = true;
    }
    ~RequestLease() {
        {
            std::lock_guard<std::mutex> lock(arm_.request_mutex_);
            arm_.request_active_ = false;
        }
        arm_.request_available_.notify_one();
    }
    RequestLease(const RequestLease &) = delete;
    RequestLease &operator=(const RequestLease &) = delete;

private:
    LinkerArm &arm_;
};

LinkerArm::LinkerArm(std::string device)
    : LinkerArm(std::make_unique<Communication::CanBus>(device)) {}
LinkerArm::LinkerArm(const std::string &channel, int bitrate)
    : LinkerArm(std::make_unique<Communication::CanBus>(channel, bitrate)) {}
LinkerArm::LinkerArm(std::unique_ptr<Communication::ICanBus> bus) : bus_(std::move(bus)) {
    if (!bus_)
        throw std::invalid_argument("null transport");
    bus_->send({}, LINKER_HAND_VERSION);
    reader_ = std::thread(&LinkerArm::receiveResponse, this);
}
LinkerArm::~LinkerArm() {
    running_ = false;
    updated_.notify_all();
    if (reader_.joinable())
        reader_.join();
}
Protocol::ArmSnapshot LinkerArm::latestSnapshot() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return assembler_.latest();
}
Protocol::ArmSnapshot LinkerArm::readJointState(std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0)
        throw std::invalid_argument("timeout must be positive");
    const auto now = Protocol::Clock::now();
    // Reject before converting milliseconds to the clock's finer tick unit.
    const auto largest_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(Protocol::Clock::duration::max());
    if (timeout >= largest_ms)
        throw std::invalid_argument("timeout exceeds the clock range");
    const auto wait = std::chrono::duration_cast<Protocol::Clock::duration>(timeout);
    if (now.time_since_epoch() > Protocol::Clock::duration::max() - wait)
        throw std::invalid_argument("timeout exceeds the deadline range");
    const auto deadline = now + wait;
    RequestLease request(*this, deadline);
    std::unique_lock<std::mutex> lock(state_mutex_);
    auto sequence = assembler_.latest().sequence;
    assembler_.beginCycle(std::chrono::steady_clock::now());
    error_.clear();
    if (std::chrono::steady_clock::now() >= deadline)
        throw std::runtime_error("joint request queue timeout");
    query_deadline_ = deadline;
    pending_query_ = true;
    lock.unlock();
    try {
        bus_->send({}, GET_JOINT_POSITION);
    } catch (...) {
        std::lock_guard<std::mutex> guard(state_mutex_);
        pending_query_ = false;
        throw;
    }
    lock.lock();
    if (std::chrono::steady_clock::now() >= deadline) {
        pending_query_ = false;
        throw std::runtime_error("joint response timeout");
    }
    if (!updated_.wait_until(lock, deadline, [&] {
            return (assembler_.latest().valid && assembler_.latest().sequence > sequence) ||
                   !error_.empty() || !running_;
        })) {
        pending_query_ = false;
        throw std::runtime_error("joint response timeout");
    }
    pending_query_ = false;
    if (!error_.empty())
        throw std::runtime_error(error_);
    if (!running_)
        throw std::runtime_error("arm stopped");
    return assembler_.latest();
}
std::vector<float> LinkerArm::getJointPosition() {
    auto snapshot = readJointState();
    return {snapshot.positions_degrees.begin(), snapshot.positions_degrees.end()};
}
std::vector<bool> LinkerArm::getJointErrorCode() {
    auto snapshot = latestSnapshot();
    return std::vector<bool>(snapshot.faults.begin(), snapshot.faults.end());
}
std::string LinkerArm::getVersion() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (version_.empty())
        return {};
    std::ostringstream out;
    out << "Hardware Version: v" << int(version_[0]) / 100 << '.' << (int(version_[0]) % 100) / 10
        << '.' << int(version_[0]) % 10;
    return out.str();
}
std::string LinkerArm::lastError() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return error_;
}
void LinkerArm::resetZero(uint8_t joint) {
    if (joint != 0xFF && joint >= 14)
        throw std::invalid_argument("joint must be 0..13 or 255");
    RequestLease request(*this);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        pending_query_ = false;
        assembler_.reset(assembler_.latest().generation);
    }
    bus_->send(joint == 0xFF ? std::vector<uint8_t>{} : std::vector<uint8_t>{joint},
               joint == 0xFF ? RESET_ZERO_COMMAND_2 : RESET_ZERO_COMMAND_1);
}
void LinkerArm::printJointInfo() {
    auto s = latestSnapshot();
    if (!s.valid)
        return;
    for (size_t i = 0; i < 14; ++i)
        std::cout << i << " : " << s.positions_degrees[i] << " | "
                  << (s.fault_valid[i] ? (s.faults[i] ? "FAULT" : "OK") : "UNKNOWN") << '\n';
}
void LinkerArm::receiveResponse() {
    while (running_) {
        try {
            auto received = bus_->recv();
            auto received_at = std::chrono::steady_clock::now();
            if (!received.can_dlc)
                continue;
            Protocol::Frame frame{};
            frame.can_id = received.can_id;
            frame.dlc = received.can_dlc;
            std::copy_n(received.data, 8, frame.data.begin());
            auto decoded = Protocol::decodeFrame(frame);
            if (!decoded.frame)
                continue;
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (decoded.frame->kind == Protocol::FrameKind::version) {
                version_.assign(decoded.frame->version_bytes.begin(),
                                decoded.frame->version_bytes.begin() + decoded.frame->version_size);
            } else if (pending_query_ && received_at <= query_deadline_ &&
                       assembler_.ingest(*decoded.frame, received_at)) {
                pending_query_ = false;
                updated_.notify_all();
            }
        } catch (const std::exception &e) {
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                error_ = e.what();
                assembler_.reset(assembler_.latest().generation);
            }
            updated_.notify_all();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
}
} // namespace LinkerArm
