// Test-only transport fixture. Production nodes never expose fake transport controls.
#include "TeleArmNode.h"
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <linkerbot/can/fake_transport.hpp>
#include <linkerbot/transport/session.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/u_int32.hpp>
#include <std_srvs/srv/set_bool.hpp>

using namespace std::chrono_literals;
namespace {
int runFixture() {
    std::atomic<bool> respond{true};
    std::atomic<unsigned> calibrations{0};
    auto fake = std::make_unique<linkerbot::can::FakeTransport>();
    fake->on_request(0x04, [&](const linkerbot::can::Message &) {
        std::vector<linkerbot::can::Message> replies;
        if (!respond.load())
            return replies;
        for (unsigned block = 0; block < 4; ++block) {
            linkerbot::can::Message frame;
            frame.id = 0x65 + block;
            frame.dlc = block % 2 ? 7 : 8;
            for (std::size_t joint = 0; joint < (block % 2 ? 3u : 4u); ++joint) {
                frame.data[joint * 2] = 0x03;
                frame.data[joint * 2 + 1] = 0x84;
            }
            replies.push_back(frame);
        }
        return replies;
    });
    fake->on_request(0xc1, [&](const linkerbot::can::Message &) {
        ++calibrations;
        return std::vector<linkerbot::can::Message>{};
    });
    auto session = linkerbot::transport::Session::own(std::move(fake));
    LinkerArm::SessionOptions session_options;
    session_options.transmit_gap = 0us;
    auto arm = std::make_shared<LinkerArm::LinkerArm>(session, session_options);
    auto driver = std::make_shared<LinkerArm::TeleArmNode>(rclcpp::NodeOptions(), arm);
    auto control = std::make_shared<rclcpp::Node>("test_transport_control");
    auto service = control->create_service<std_srvs::srv::SetBool>(
        "/test_respond", [&](const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
                             std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
            respond = request->data;
            response->success = true;
        });
    auto publisher =
        control->create_publisher<std_msgs::msg::UInt32>("/test_calibration_count", 10);
    auto timer = control->create_wall_timer(20ms, [&] {
        std_msgs::msg::UInt32 message;
        message.data = calibrations.load();
        publisher->publish(message);
    });
    rclcpp::executors::SingleThreadedExecutor executor;
    executor.add_node(driver);
    executor.add_node(control);
    executor.spin();
    executor.remove_node(driver);
    executor.remove_node(control);
    driver.reset();
    arm.reset();
    session->close();
    return 0;
}
} // namespace

int main(int argc, char **argv) {
    int result = 0;
    try {
        rclcpp::init(argc, argv);
        result = runFixture();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "ros2_test_fixture: %s\n", error.what());
        result = 1;
    } catch (...) {
        std::fprintf(stderr, "ros2_test_fixture: unknown exception\n");
        result = 1;
    }
    // runFixture's stack has released its executor, nodes and owned Session.
    // Also shut down ROS after partial initialization or exceptional teardown.
    try {
        rclcpp::shutdown();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "ros2_test_fixture shutdown: %s\n", error.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "ros2_test_fixture shutdown: unknown exception\n");
        return 1;
    }
    return result;
}
