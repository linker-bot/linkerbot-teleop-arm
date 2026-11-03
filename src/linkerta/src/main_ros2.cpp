#include "LinkerArm.h"
#include "NodeConfig.h"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/string.hpp"
#include <chrono>
#include <iostream>
#include <signal.h>
#include <sstream>

using namespace std::chrono_literals;

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);

    auto node = std::make_shared<rclcpp::Node>("linkerta_node");

    // uint32_t id = node->declare_parameter("arm_id", 0x123);
    // int baudrate = node->declare_parameter("arm_baudrate", 1000000);
    // std::string channel = node->declare_parameter<std::string>("arm_channel", "can0");
    int calibration = node->declare_parameter("calibration", 0);
    std::vector<int64_t> negation = node->declare_parameter<std::vector<int64_t>>(
        "negation", {-1, -1, -1, -1, -1, -1, -1, -1, 1, -1, 1, -1, -1, -1});

    std::cout << "calibration : " << calibration << std::endl;
    std::cout << "negation : ";
    for (auto n : negation) {
        std::cout << n << ",";
    }
    std::cout << std::endl;

    try {
        LinkerArm::validateNodeConfig(calibration, negation);
        LinkerArm::LinkerArm arm("master_arm");
        // LinkerArm::LinkerArm arm(id, channel, baudrate);
        std::this_thread::sleep_for(200ms);
        std::cout << arm.getVersion() << std::endl;

        auto pub_left_arm_control =
            node->create_publisher<sensor_msgs::msg::JointState>("/left_arm_joint_control", 10);
        auto pub_right_arm_control =
            node->create_publisher<sensor_msgs::msg::JointState>("/right_arm_joint_control", 10);
        auto pub_joint_error_code =
            node->create_publisher<std_msgs::msg::String>("/joint_error_code", 10);

        std_msgs::msg::String msg;
        sensor_msgs::msg::JointState left_joint_states;
        sensor_msgs::msg::JointState right_joint_states;
        // const float velocity = 10.0f;

        for (size_t i = 61; i <= 67; ++i) {
            left_joint_states.name.push_back("joint" + std::to_string(i));
            // left_joint_states.velocity.push_back(velocity);
            right_joint_states.name.push_back("joint" + std::to_string(i - 10));
            // right_joint_states.velocity.push_back(velocity);
        }

        rclcpp::WallRate loop(100);
        while (rclcpp::ok()) {
            const auto snapshot = arm.readJointState();
            const auto &position = snapshot.positions_degrees;
            const auto &error_code = snapshot.faults;
            // Legacy topics retain degrees until the explicit ROS2 units migration.
            // Both publications are derived from the same complete snapshot.
            left_joint_states.position.clear();
            right_joint_states.position.clear();
            for (size_t i = 0; i < position.size(); ++i) {
                if (i <= 6) {
                    left_joint_states.position.push_back(position[i] * negation[i]);
                } else {
                    right_joint_states.position.push_back(position[i] * negation[i]);
                }
            }

            left_joint_states.header.stamp = node->get_clock()->now();
            right_joint_states.header.stamp = node->get_clock()->now();

            pub_left_arm_control->publish(left_joint_states);
            pub_right_arm_control->publish(right_joint_states);

            std::stringstream ss;
            for (size_t i = 0; i < error_code.size(); ++i) {
                ss << "J" << i << ":";
                if (snapshot.fault_valid[i])
                    ss << error_code[i];
                else
                    ss << "unknown";
                ss << ", ";
            }
            msg.data = ss.str();
            pub_joint_error_code->publish(msg);
            rclcpp::spin_some(node);
            loop.sleep();
        }
    } catch (const std::exception &e) {
        RCLCPP_ERROR(node->get_logger(), "[ERROR] %s", e.what());
        rclcpp::shutdown();
        return -1;
    }

    rclcpp::shutdown();
    return 0;
}
