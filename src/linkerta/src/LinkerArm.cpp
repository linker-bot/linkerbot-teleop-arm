#include "LinkerArm.h"
#include "SessionFactory.h"
#include <cstdio>

#include <algorithm>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace LinkerArm {
namespace transport = linkerbot::transport;
namespace {
Protocol::TimePoint deadlineAfter(std::chrono::milliseconds timeout) {
    if (timeout.count() <= 0)
        throw std::invalid_argument("timeout must be positive");
    const auto now = Protocol::Clock::now();
    const auto maximum =
        std::chrono::duration_cast<std::chrono::milliseconds>(Protocol::Clock::duration::max());
    if (timeout >= maximum)
        throw std::invalid_argument("timeout exceeds clock range");
    const auto wait = std::chrono::duration_cast<Protocol::Clock::duration>(timeout);
    if (now.time_since_epoch() > Protocol::Clock::duration::max() - wait)
        throw std::invalid_argument("timeout exceeds deadline range");
    return now + wait;
}
SessionOptions ownedOptions() {
    SessionOptions options;
    options.own_session = true;
    return options;
}
std::string newEndpoint() {
    static std::atomic<std::uint64_t> next{0};
    return "tele-arm-" + std::to_string(next.fetch_add(1));
}
const std::vector<transport::RouteKey> routes{
    {0x64, false}, {0x65, false}, {0x66, false}, {0x67, false}, {0x68, false}};
} // namespace

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

LinkerArm::LinkerArm(const std::string &device)
    : LinkerArm(discoverArmSession(device), ownedOptions()) {}
LinkerArm::LinkerArm(const std::string &name, int bitrate)
    : LinkerArm(makeSocketSession(name, bitrate), ownedOptions()) {}
LinkerArm::LinkerArm(std::shared_ptr<transport::Session> session)
    : LinkerArm(std::move(session), SessionOptions{}) {}
LinkerArm::LinkerArm(std::shared_ptr<transport::Session> session, SessionOptions options)
    : session_(std::move(session)), options_(std::move(options)) {
    if (!session_)
        throw std::invalid_argument("null Session");
    if (!options_.receive_capacity || options_.transmit_gap.count() < 0)
        throw std::invalid_argument("invalid Session queue capacity or transmit pacing");
    const auto deadline = deadlineAfter(options_.command_timeout);
    if (options_.transmit_gap >
        std::chrono::duration_cast<std::chrono::microseconds>(options_.command_timeout))
        throw std::invalid_argument("transmit pacing exceeds command timeout");
    if (options_.endpoint.empty())
        options_.endpoint = newEndpoint();
    generation_ = session_->health().generation;
    assembler_.reset(generation_);
    // Route conflicts fail before any command or worker is created. In particular,
    // failure must never cancel a pre-existing client's work on a shared Session.
    inbox_ = session_->subscribe_routes(routes, options_.receive_capacity);
    running_ = true;
    try {
        reader_ = std::thread(&LinkerArm::receiveResponse, this);
        sendFrame(LINKER_HAND_VERSION, {}, deadline);
    } catch (...) {
        stopReader();
        if (options_.own_session) {
            try {
                session_->close();
            } catch (...) {
                std::fputs("LinkerArm: failed to close Session after construction failure\n",
                           stderr);
            }
        }
        throw;
    }
}

LinkerArm::~LinkerArm() {
    stopReader();
    try {
        session_->cancel_endpoint(options_.endpoint);
    } catch (...) {
        std::fputs("LinkerArm: failed to cancel endpoint during destruction\n", stderr);
    }
    if (options_.own_session) {
        try {
            session_->close();
        } catch (...) {
            std::fputs("LinkerArm: failed to close owned Session during destruction\n", stderr);
        }
    }
}
void LinkerArm::stopReader() noexcept {
    running_ = false;
    std::shared_ptr<transport::RouteInbox> inbox;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        inbox = inbox_;
        pending_query_ = false;
    }
    if (inbox)
        inbox->close();
    updated_.notify_all();
    if (reader_.joinable())
        reader_.join();
}

void LinkerArm::sendFrame(std::uint32_t id, const std::vector<std::uint8_t> &data,
                          Protocol::TimePoint deadline) {
    if (id > 0x7ff || data.size() > 8)
        throw std::invalid_argument("invalid classic CAN command");
    linkerbot::can::Message message;
    message.id = id;
    message.dlc = static_cast<std::uint8_t>(data.size());
    std::copy(data.begin(), data.end(), message.data.begin());
    transport::MessageBatch batch;
    batch.frames.push_back(message);
    // A single-frame batch has no inter-frame gap. Pacing is deliberately before
    // each command so the Session's writer enforces the legacy minimum interval.
    batch.initial_delay = options_.transmit_gap;
    transport::CancellationSource cancellation;
    transport::SendOptions options;
    options.deadline = deadline;
    options.cancellation = cancellation.token();
    options.endpoint = options_.endpoint;
    options.family = std::to_string(id);
    options.policy = transport::SendPolicy::fifo;
    try {
        const auto receipt = session_->submit(std::move(batch), std::move(options));
        const auto result = receipt.wait_until(deadline);
        if (result.outcome != transport::SendOutcome::dispatched ||
            Protocol::Clock::now() >= deadline)
            throw linkerbot::can::TimeoutError("command did not dispatch within deadline");
    } catch (...) {
        // A wait timeout alone does not cancel Session work. This token cancels
        // only this command; an in-flight backend write may still have happened.
        cancellation.cancel();
        throw;
    }
}

void LinkerArm::invalidate(const std::string &message, std::uint64_t generation) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    generation_ = generation;
    assembler_.reset(generation);
    version_.clear();
    error_ = message;
    identity_required_ = true;
    pending_query_ = false;
    updated_.notify_all();
}
void LinkerArm::refreshSessionState() {
    // Keep the health observation and generation update ordered against an
    // owner's concurrent recovery. A stale pre-lock observation must not roll
    // a freshly recovered generation back.
    std::lock_guard<std::mutex> lock(state_mutex_);
    const auto health = session_->health();
    const auto physical = session_->transport_health();
    const bool changed = health.generation != generation_;
    if (changed || !health.open || physical.state == linkerbot::can::TransportState::kFaulted) {
        generation_ = health.generation;
        assembler_.reset(generation_);
        version_.clear();
        error_ = changed ? "Session generation changed; reconstruct or recover the arm client"
                         : "Session is closed or faulted";
        identity_required_ = true;
        pending_query_ = false;
        updated_.notify_all();
    }
}
void LinkerArm::observeOverflow() {
    // Keep the inbox identity and overflow counter from the same generation.
    // Otherwise a late observation from a closed route could invalidate a
    // newly recovered snapshot.
    std::lock_guard<std::mutex> lock(state_mutex_);
    const auto health = inbox_->health();
    if (health.overflow != observed_overflow_) {
        observed_overflow_ = health.overflow;
        assembler_.reset(generation_);
        error_ = "Session receive route overflow; incomplete snapshot discarded";
        pending_query_ = false;
        updated_.notify_all();
    }
}
Protocol::ArmSnapshot LinkerArm::latestSnapshot() {
    refreshSessionState();
    observeOverflow();
    std::lock_guard<std::mutex> lock(state_mutex_);
    return assembler_.latest();
}
Protocol::ArmSnapshot LinkerArm::readJointState(std::chrono::milliseconds timeout) {
    const auto deadline = deadlineAfter(timeout);
    RequestLease request(*this, deadline);
    refreshSessionState();
    observeOverflow();
    std::unique_lock<std::mutex> lock(state_mutex_);
    if (!running_ || identity_required_)
        throw std::runtime_error(error_.empty() ? "arm identity requires revalidation" : error_);
    if (Protocol::Clock::now() >= deadline)
        throw linkerbot::can::TimeoutError("joint request queue timeout");
    const auto sequence = assembler_.latest().sequence;
    const auto generation = generation_;
    assembler_.beginCycle(Protocol::Clock::now());
    error_.clear();
    query_deadline_ = deadline;
    pending_query_ = true;
    lock.unlock();
    try {
        sendFrame(GET_JOINT_POSITION, {}, deadline);
    } catch (const std::exception &error) {
        lock.lock();
        pending_query_ = false;
        assembler_.reset(generation_);
        error_ = error.what();
        throw;
    }
    lock.lock();
    if (!updated_.wait_until(lock, deadline, [&] {
            const auto snapshot = assembler_.latest();
            return (snapshot.valid && snapshot.sequence > sequence &&
                    snapshot.generation == generation) ||
                   !error_.empty() || !running_;
        })) {
        pending_query_ = false;
        throw linkerbot::can::TimeoutError("joint response timeout");
    }
    pending_query_ = false;
    if (!error_.empty())
        throw std::runtime_error(error_);
    if (!running_)
        throw std::runtime_error("arm stopped");
    return assembler_.latest();
}
std::vector<float> LinkerArm::getJointPosition() {
    const auto snapshot = readJointState();
    return {snapshot.positions_degrees.begin(), snapshot.positions_degrees.end()};
}
std::vector<bool> LinkerArm::getJointErrorCode() {
    const auto snapshot = latestSnapshot();
    return std::vector<bool>(snapshot.faults.begin(), snapshot.faults.end());
}
std::string LinkerArm::getVersion() {
    refreshSessionState();
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (version_.empty())
        return {};
    std::ostringstream out;
    out << "Hardware Version: v" << int(version_[0]) / 100 << '.' << (int(version_[0]) % 100) / 10
        << '.' << int(version_[0]) % 10;
    return out.str();
}
std::string LinkerArm::lastError() {
    refreshSessionState();
    std::lock_guard<std::mutex> lock(state_mutex_);
    return error_;
}
void LinkerArm::resetZero(std::uint8_t joint) {
    if (joint != 0xff && joint >= 14)
        throw std::invalid_argument("joint must be 0..13 or 255");
    const auto deadline = deadlineAfter(options_.command_timeout);
    RequestLease request(*this, deadline);
    refreshSessionState();
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (!running_ || identity_required_)
            throw std::runtime_error("arm identity requires revalidation");
        pending_query_ = false;
        assembler_.reset(generation_);
    }
    sendFrame(joint == 0xff ? RESET_ZERO_COMMAND_2 : RESET_ZERO_COMMAND_1,
              joint == 0xff ? std::vector<std::uint8_t>{} : std::vector<std::uint8_t>{joint},
              deadline);
}
void LinkerArm::printJointInfo() {
    const auto snapshot = latestSnapshot();
    if (!snapshot.valid)
        return;
    for (std::size_t i = 0; i < snapshot.positions_degrees.size(); ++i)
        std::cout << i << " : " << snapshot.positions_degrees[i] << " | "
                  << (snapshot.fault_valid[i] ? (snapshot.faults[i] ? "FAULT" : "OK") : "UNKNOWN")
                  << '\n';
}

std::uint64_t LinkerArm::recover(std::chrono::milliseconds timeout) {
    if (!options_.own_session)
        throw std::logic_error(
            "only the Session owner may recover; reconstruct after external recovery");
    const auto deadline = deadlineAfter(timeout);
    RequestLease request(*this, deadline);
    if (Protocol::Clock::now() >= deadline)
        throw linkerbot::can::TimeoutError("recovery request queue timeout");
    const auto observed = session_->health().generation;
    stopReader();
    session_->cancel_endpoint(options_.endpoint);
    invalidate("arm identity requires revalidation", observed);
    const auto generation = session_->recover(observed);
    auto inbox = session_->subscribe_routes(routes, options_.receive_capacity);
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        inbox_ = std::move(inbox);
        observed_overflow_ = 0;
        generation_ = generation;
        assembler_.reset(generation);
        error_.clear();
        version_.clear();
        identity_required_ = true;
    }
    running_ = true;
    reader_ = std::thread(&LinkerArm::receiveResponse, this);
    sendFrame(LINKER_HAND_VERSION, {}, deadline);
    std::unique_lock<std::mutex> lock(state_mutex_);
    if (!updated_.wait_until(lock, deadline,
                             [&] { return !identity_required_ || !error_.empty() || !running_; }))
        throw linkerbot::can::TimeoutError("recovered arm did not confirm its identity");
    if (identity_required_)
        throw std::runtime_error(error_.empty() ? "recovered arm unavailable" : error_);
    return generation;
}
transport::SessionHealth LinkerArm::sessionHealth() const {
    return session_->health();
}
transport::RouteHealth LinkerArm::routeHealth() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return inbox_->health();
}

void LinkerArm::receiveResponse() {
    while (running_) {
        try {
            refreshSessionState();
            observeOverflow();
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                if (identity_required_ && !error_.empty())
                    break;
            }
            const auto received =
                inbox_->receive_until(Protocol::Clock::now() + std::chrono::milliseconds(20));
            if (!received)
                continue;
            Protocol::Frame frame;
            frame.can_id = received->message.id;
            frame.dlc = received->message.dlc;
            frame.is_extended = received->message.is_extended;
            frame.is_fd = received->message.is_fd;
            std::copy_n(received->message.data.begin(), frame.data.size(), frame.data.begin());
            const auto decoded = Protocol::decodeFrame(frame);
            if (!decoded.frame)
                continue;
            observeOverflow();
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (received->generation != generation_)
                continue;
            if (decoded.frame->kind == Protocol::FrameKind::version) {
                version_.assign(decoded.frame->version_bytes.begin(),
                                decoded.frame->version_bytes.begin() + decoded.frame->version_size);
                identity_required_ = false;
                updated_.notify_all();
            } else if (!identity_required_ && pending_query_ &&
                       received->received_at <= query_deadline_ &&
                       assembler_.ingest(*decoded.frame, received->received_at)) {
                pending_query_ = false;
                updated_.notify_all();
            }
        } catch (const std::exception &error) {
            if (running_) {
                invalidate(error.what(), session_->health().generation);
                break;
            }
        }
    }
    running_ = false;
    updated_.notify_all();
}
} // namespace LinkerArm
