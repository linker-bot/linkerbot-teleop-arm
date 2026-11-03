#include "TeleArmNode.h"
#include <cstdio>
#include <exception>
#include <rclcpp/rclcpp.hpp>
int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    int result = 0;
    try {
        auto node = std::make_shared<LinkerArm::TeleArmNode>();
        rclcpp::executors::SingleThreadedExecutor executor;
        executor.add_node(node);
        executor.spin();
        executor.remove_node(node);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "linkerta_node: %s\n", error.what());
        result = 1;
    }
    rclcpp::shutdown();
    return result;
}
