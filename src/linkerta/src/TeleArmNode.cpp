#include "TeleArmNode.h"
#include <algorithm>
#include <chrono>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <diagnostic_msgs/msg/key_value.hpp>
#include <functional>
#include <sstream>
#include <utility>

namespace LinkerArm {
namespace {
constexpr double kRadiansPerDegree = 3.14159265358979323846 / 180.0;
rcl_interfaces::msg::ParameterDescriptor readOnly() {
    rcl_interfaces::msg::ParameterDescriptor descriptor;
    descriptor.read_only = true;
    return descriptor;
}
diagnostic_msgs::msg::KeyValue value(std::string key, std::string data) {
    diagnostic_msgs::msg::KeyValue result;
    result.key = std::move(key);
    result.value = std::move(data);
    return result;
}
} // namespace

TeleArmNode::TeleArmNode(const rclcpp::NodeOptions &options, std::shared_ptr<LinkerArm> arm)
    : Node("linkerta_node", options) {
    const auto descriptor = readOnly();
    // Validate the full-width parameter before narrowing the legacy field.
    const auto calibration = declare_parameter<std::int64_t>("calibration", 0, descriptor);
    if (calibration != 0)
        throw std::invalid_argument("startup calibration is disabled");
    config_.negation = declare_parameter<std::vector<std::int64_t>>(
        "negation", {-1, -1, -1, -1, -1, -1, -1, -1, 1, -1, 1, -1, -1, -1}, descriptor);
    config_.device = declare_parameter<std::string>("device", "master_arm", descriptor);
    config_.can_interface = declare_parameter<std::string>("can_interface", "", descriptor);
    config_.poll_rate_hz = declare_parameter<double>("poll_rate_hz", 100.0, descriptor);
    config_.request_timeout_ms =
        declare_parameter<std::int64_t>("request_timeout_ms", 250, descriptor);
    config_.max_snapshot_age_ms =
        declare_parameter<std::int64_t>("max_snapshot_age_ms", 250, descriptor);
    config_.publish_legacy_topics =
        declare_parameter<bool>("publish_legacy_topics", true, descriptor);
    config_.allow_calibration = declare_parameter<bool>("allow_calibration", false, descriptor);
    config_.calibration_joint =
        declare_parameter<std::int64_t>("calibration_joint", 255, descriptor);
    validateNodeConfig(config_);
    arm_ = std::move(arm);
    if (!arm_) {
        if (config_.can_interface.empty())
            arm_ = std::make_shared<LinkerArm>(config_.device);
        else
            arm_ = std::make_shared<LinkerArm>(config_.can_interface, 1000000);
    }
    joint_publisher_ = create_publisher<sensor_msgs::msg::JointState>("/joint_states", 10);
    diagnostic_publisher_ =
        create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);
    if (config_.publish_legacy_topics) {
        left_publisher_ =
            create_publisher<sensor_msgs::msg::JointState>("/left_arm_joint_control", 10);
        right_publisher_ =
            create_publisher<sensor_msgs::msg::JointState>("/right_arm_joint_control", 10);
        legacy_error_publisher_ = create_publisher<std_msgs::msg::String>("/joint_error_code", 10);
    }
    calibration_service_ = create_service<std_srvs::srv::Trigger>(
        "~/calibrate_once",
        std::bind(&TeleArmNode::calibrate, this, std::placeholders::_1, std::placeholders::_2));
    const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::duration<double>(1.0 / config_.poll_rate_hz));
    timer_ = create_wall_timer(period, std::bind(&TeleArmNode::tick, this));
    worker_ = std::thread(&TeleArmNode::runWorker, this);
}
TeleArmNode::~TeleArmNode() {
    if (timer_)
        timer_->cancel();
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        stopping_ = true;
    }
    work_available_.notify_all();
    if (worker_.joinable())
        worker_.join();
}
void TeleArmNode::runWorker() {
    std::unique_lock<std::mutex> lock(worker_mutex_);
    while (true) {
        work_available_.wait(lock,
                             [&] { return stopping_ || request_pending_ || calibration_pending_; });
        if (stopping_)
            return;
        const bool calibration = calibration_pending_;
        calibration_pending_ = false;
        request_pending_ = false;
        busy_ = true;
        lock.unlock();
        std::optional<Protocol::ArmSnapshot> snapshot;
        std::string error;
        std::string calibration_status;
        try {
            if (calibration) {
                arm_->resetZero(static_cast<std::uint8_t>(config_.calibration_joint));
                calibration_status = "command dispatched once; device completion unconfirmed";
            } else {
                snapshot =
                    arm_->readJointState(std::chrono::milliseconds(config_.request_timeout_ms));
            }
        } catch (const std::exception &exception) {
            error = exception.what();
            if (calibration)
                calibration_status = "command attempt failed or uncertain; not retried: " + error;
        }
        lock.lock();
        busy_ = false;
        if (stopping_)
            return;
        if (calibration) {
            completed_.reset();
            worker_error_ = "Calibration requested; waiting for a new snapshot";
            calibration_status_ = std::move(calibration_status);
        } else {
            // A service may have arrived during the blocking read. Never publish
            // that pre-calibration result after accepting the command.
            if (!calibration_pending_)
                completed_ = std::move(snapshot);
            worker_error_ = std::move(error);
        }
    }
}
void TeleArmNode::tick() {
    std::optional<Protocol::ArmSnapshot> snapshot;
    std::string error;
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        snapshot = std::move(completed_);
        completed_.reset();
        error = worker_error_;
        if (!busy_ && !calibration_pending_) {
            request_pending_ = true;
            work_available_.notify_one();
        }
    }
    if (snapshot) {
        if (snapshot->isFresh(Protocol::Clock::now(),
                              std::chrono::milliseconds(config_.max_snapshot_age_ms))) {
            publishSnapshot(*snapshot);
            diagnostic_snapshot_ = snapshot;
        } else {
            error = "Discarded stale or incomplete snapshot";
            snapshot.reset();
        }
    }
    if (!error.empty())
        diagnostic_snapshot_.reset();
    if (diagnostic_snapshot_ &&
        !diagnostic_snapshot_->isFresh(Protocol::Clock::now(),
                                       std::chrono::milliseconds(config_.max_snapshot_age_ms))) {
        diagnostic_snapshot_.reset();
        error = "Last complete snapshot is stale";
    }
    publishDiagnostic(diagnostic_snapshot_, error);
}
void TeleArmNode::publishSnapshot(const Protocol::ArmSnapshot &snapshot) {
    sensor_msgs::msg::JointState standard, left, right;
    // Approximate host arrival in the current ROS clock domain, not device sample time.
    // Every message derived from this snapshot uses exactly the same stamp.
    const auto age = Protocol::Clock::now() - snapshot.captured_at;
    const auto current = now();
    const auto age_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(age).count();
    // A simulated ROS clock can start at zero or jump backwards. Clamp at zero
    // rather than constructing an invalid negative ROS timestamp.
    const rclcpp::Time stamp(std::max<std::int64_t>(0, current.nanoseconds() - age_ns),
                             current.get_clock_type());
    standard.header.stamp = stamp;
    left.header.stamp = stamp;
    right.header.stamp = stamp;
    for (std::size_t i = 0; i < Protocol::kJointCount; ++i) {
        const auto name = "joint" + std::to_string(i < 7 ? 61 + i : 51 + i - 7);
        const double degrees = static_cast<double>(snapshot.positions_degrees[i]) *
                               static_cast<double>(config_.negation[i]);
        standard.name.push_back(name);
        standard.position.push_back(degrees * kRadiansPerDegree);
        auto &legacy = i < 7 ? left : right;
        legacy.name.push_back(name);
        legacy.position.push_back(degrees);
    }
    joint_publisher_->publish(standard);
    if (config_.publish_legacy_topics) {
        left_publisher_->publish(left);
        right_publisher_->publish(right);
        std_msgs::msg::String faults;
        std::ostringstream text;
        for (std::size_t i = 0; i < Protocol::kJointCount; ++i) {
            text << "J" << i << ":";
            if (snapshot.fault_valid[i])
                text << snapshot.faults[i];
            else
                text << "unknown";
            text << ", ";
        }
        faults.data = text.str();
        legacy_error_publisher_->publish(faults);
    }
}
void TeleArmNode::publishDiagnostic(const std::optional<Protocol::ArmSnapshot> &snapshot,
                                    const std::string &error) {
    diagnostic_msgs::msg::DiagnosticArray message;
    message.header.stamp = now();
    diagnostic_msgs::msg::DiagnosticStatus status;
    status.name = "linkerta/arm";
    status.hardware_id = config_.can_interface.empty() ? config_.device : config_.can_interface;
    status.level = error.empty() ? diagnostic_msgs::msg::DiagnosticStatus::WARN
                                 : diagnostic_msgs::msg::DiagnosticStatus::ERROR;
    status.message = error.empty() ? "Waiting for the next complete snapshot" : error;
    if (snapshot) {
        const bool faulty =
            std::any_of(snapshot->faults.begin(), snapshot->faults.end(), [](bool v) { return v; });
        const bool known = std::all_of(snapshot->fault_valid.begin(), snapshot->fault_valid.end(),
                                       [](bool v) { return v; });
        status.level = faulty  ? diagnostic_msgs::msg::DiagnosticStatus::ERROR
                       : known ? diagnostic_msgs::msg::DiagnosticStatus::OK
                               : diagnostic_msgs::msg::DiagnosticStatus::WARN;
        status.message = faulty  ? "Joint fault reported"
                         : known ? "Complete fresh snapshot"
                                 : "Joint fault state unknown";
        status.values.push_back(value("sequence", std::to_string(snapshot->sequence)));
        status.values.push_back(value("generation", std::to_string(snapshot->generation)));
    }
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        status.values.push_back(value("calibration", calibration_status_));
    }
    message.status.push_back(std::move(status));
    diagnostic_publisher_->publish(message);
}
void TeleArmNode::calibrate(const std::shared_ptr<std_srvs::srv::Trigger::Request>,
                            std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
    std::lock_guard<std::mutex> lock(worker_mutex_);
    if (!config_.allow_calibration) {
        response->success = false;
        response->message = "Calibration disabled; set allow_calibration at startup to opt in";
    } else if (calibration_consumed_) {
        response->success = false;
        response->message = "Calibration attempt already consumed; no automatic replay";
    } else {
        calibration_consumed_ = true;
        calibration_pending_ = true;
        request_pending_ = false;
        completed_.reset();
        calibration_status_ = "one command attempt queued; device completion unconfirmed";
        response->success = false;
        response->message =
            "One command attempt queued; success cannot be confirmed without device ACK; "
            "inspect diagnostics; this process will not retry";
        work_available_.notify_one();
    }
}
} // namespace LinkerArm
