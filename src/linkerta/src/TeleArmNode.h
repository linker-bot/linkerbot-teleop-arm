#pragma once
#include "LinkerArm.h"
#include "NodeConfig.h"
#include <condition_variable>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <memory>
#include <mutex>
#include <optional>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <thread>

namespace LinkerArm {
class TeleArmNode : public rclcpp::Node {
public:
    explicit TeleArmNode(const rclcpp::NodeOptions &options = rclcpp::NodeOptions(),
                         std::shared_ptr<LinkerArm> arm = nullptr);
    ~TeleArmNode() override;

private:
    void tick();
    void runWorker();
    void publishSnapshot(const Protocol::ArmSnapshot &snapshot);
    void publishDiagnostic(const std::optional<Protocol::ArmSnapshot> &snapshot,
                           const std::string &error);
    void calibrate(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                   std::shared_ptr<std_srvs::srv::Trigger::Response> response);
    NodeConfig config_;
    std::shared_ptr<LinkerArm> arm_;
    rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr joint_publisher_;
    rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr left_publisher_;
    rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr right_publisher_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr legacy_error_publisher_;
    rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diagnostic_publisher_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr calibration_service_;
    rclcpp::TimerBase::SharedPtr timer_;
    std::mutex worker_mutex_;
    std::condition_variable work_available_;
    bool stopping_ = false;
    bool request_pending_ = false;
    bool busy_ = false;
    bool calibration_pending_ = false;
    bool calibration_consumed_ = false;
    std::optional<Protocol::ArmSnapshot> completed_;
    std::optional<Protocol::ArmSnapshot> diagnostic_snapshot_;
    std::string worker_error_ = "Waiting for a complete snapshot";
    std::string calibration_status_ = "not requested";
    std::thread worker_;
};
} // namespace LinkerArm
