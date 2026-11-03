#include "TeleArmNode.h"

#include <atomic>
#include <cmath>
#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <functional>
#include <future>
#include <gtest/gtest.h>
#include <limits>
#include <linkerbot/can/fake_transport.hpp>
#include <linkerbot/transport/session.hpp>
#include <memory>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/joint_state.hpp>
#include <std_srvs/srv/trigger.hpp>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
namespace {
using linkerbot::can::FakeTransport;
using linkerbot::can::Message;
using linkerbot::transport::Session;

struct RosArmRig {
    std::atomic<bool> respond{true};
    std::atomic<bool> fail_calibration{false};
    std::atomic<unsigned> calibrations{0};
    std::shared_ptr<Session> session;
    std::shared_ptr<LinkerArm::LinkerArm> arm;

    RosArmRig() {
        auto fake = std::make_unique<FakeTransport>();
        fake->on_request(0x04, [this](const Message &) {
            std::vector<Message> replies;
            if (!respond.load())
                return replies;
            for (unsigned block = 0; block < 4; ++block) {
                Message frame;
                frame.id = 0x65 + block;
                frame.dlc = block % 2 ? 7 : 8;
                // Every joint is +90 degrees in the wire format.
                for (unsigned joint = 0; joint < (block % 2 ? 3u : 4u); ++joint) {
                    frame.data[joint * 2] = 0x03;
                    frame.data[joint * 2 + 1] = 0x84;
                }
                replies.push_back(frame);
            }
            return replies;
        });
        fake->on_request(0xc1, [this](const Message &) {
            ++calibrations;
            if (fail_calibration.load())
                throw linkerbot::can::TransportIOError("injected calibration failure");
            return std::vector<Message>{};
        });
        session = Session::own(std::move(fake));
        LinkerArm::SessionOptions options;
        options.transmit_gap = 0us;
        options.command_timeout = 50ms;
        arm = std::make_shared<LinkerArm::LinkerArm>(session, options);
    }
    ~RosArmRig() {
        arm.reset();
        session->close();
    }
};

class RosNodeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!rclcpp::ok())
            rclcpp::init(0, nullptr);
    }
    static void TearDownTestSuite() {
        rclcpp::shutdown();
    }
    rclcpp::NodeOptions options(std::vector<rclcpp::Parameter> extra = {}) {
        std::vector<rclcpp::Parameter> params{{"poll_rate_hz", 50.0},
                                              {"request_timeout_ms", 30},
                                              {"max_snapshot_age_ms", 150},
                                              {"negation", std::vector<std::int64_t>(14, 1)}};
        params.insert(params.end(), extra.begin(), extra.end());
        rclcpp::NodeOptions result;
        result.parameter_overrides(params);
        return result;
    }
    bool spinUntil(const std::function<bool()> &predicate, std::chrono::milliseconds timeout = 3s) {
        const auto until = std::chrono::steady_clock::now() + timeout;
        while (!predicate() && std::chrono::steady_clock::now() < until) {
            executor.spin_some();
            std::this_thread::sleep_for(2ms);
        }
        executor.spin_some();
        return predicate();
    }
    void spinFor(std::chrono::milliseconds duration) {
        const auto until = std::chrono::steady_clock::now() + duration;
        while (std::chrono::steady_clock::now() < until) {
            executor.spin_some();
            std::this_thread::sleep_for(2ms);
        }
    }
    rclcpp::executors::SingleThreadedExecutor executor;
};

TEST_F(RosNodeTest, PublishesRadiansAndCompatibleDegreesWithOneTimestamp) {
    RosArmRig rig;
    auto driver = std::make_shared<LinkerArm::TeleArmNode>(options(), rig.arm);
    auto observer = std::make_shared<rclcpp::Node>("units_observer");
    std::vector<sensor_msgs::msg::JointState> standard, left, right;
    auto s = observer->create_subscription<sensor_msgs::msg::JointState>(
        "/joint_states", 100,
        [&](sensor_msgs::msg::JointState::ConstSharedPtr msg) { standard.push_back(*msg); });
    auto l = observer->create_subscription<sensor_msgs::msg::JointState>(
        "/left_arm_joint_control", 100,
        [&](sensor_msgs::msg::JointState::ConstSharedPtr msg) { left.push_back(*msg); });
    auto r = observer->create_subscription<sensor_msgs::msg::JointState>(
        "/right_arm_joint_control", 100,
        [&](sensor_msgs::msg::JointState::ConstSharedPtr msg) { right.push_back(*msg); });
    executor.add_node(driver);
    executor.add_node(observer);
    ASSERT_TRUE(
        spinUntil([&] { return standard.size() >= 8 && left.size() >= 8 && right.size() >= 8; }));
    bool matched = false;
    for (const auto &whole : standard) {
        for (const auto &lhs : left) {
            for (const auto &rhs : right) {
                if (whole.header.stamp != lhs.header.stamp ||
                    whole.header.stamp != rhs.header.stamp)
                    continue;
                ASSERT_EQ(whole.position.size(), 14u);
                ASSERT_EQ(whole.name.size(), 14u);
                ASSERT_EQ(lhs.position.size(), 7u);
                ASSERT_EQ(rhs.position.size(), 7u);
                for (double value : whole.position)
                    EXPECT_NEAR(value, std::acos(-1.0) / 2.0, 1e-6);
                for (double value : lhs.position)
                    EXPECT_DOUBLE_EQ(value, 90.0);
                for (double value : rhs.position)
                    EXPECT_DOUBLE_EQ(value, 90.0);
                EXPECT_GT(rclcpp::Time(whole.header.stamp).nanoseconds(), 0);
                matched = true;
                break;
            }
            if (matched)
                break;
        }
        if (matched)
            break;
    }
    EXPECT_TRUE(matched);
    // A bounded timing check catches an unthrottled worker/timer without assuming
    // exact scheduling from shared CI runners.
    const auto before = standard.size();
    spinFor(200ms);
    EXPECT_GT(standard.size(), before);
    EXPECT_LE(standard.size() - before, 25u);
    executor.remove_node(driver);
    executor.remove_node(observer);
}

TEST_F(RosNodeTest, TimeoutReportsErrorAndNeverRepublishesCachedPositions) {
    RosArmRig rig;
    auto driver = std::make_shared<LinkerArm::TeleArmNode>(options(), rig.arm);
    auto observer = std::make_shared<rclcpp::Node>("timeout_observer");
    unsigned positions = 0;
    bool failed = false;
    auto s = observer->create_subscription<sensor_msgs::msg::JointState>(
        "/joint_states", 100, [&](sensor_msgs::msg::JointState::ConstSharedPtr) { ++positions; });
    auto d = observer->create_subscription<diagnostic_msgs::msg::DiagnosticArray>(
        "/diagnostics", 100, [&](diagnostic_msgs::msg::DiagnosticArray::ConstSharedPtr msg) {
            for (const auto &status : msg->status)
                failed = failed || status.level == diagnostic_msgs::msg::DiagnosticStatus::ERROR;
        });
    executor.add_node(driver);
    executor.add_node(observer);
    ASSERT_TRUE(spinUntil([&] { return positions >= 3; }));
    rig.respond = false;
    ASSERT_TRUE(spinUntil([&] { return failed; }));
    // Drain any previously queued successful publication before observing the
    // no-stale-data invariant over several request deadlines.
    spinFor(100ms);
    const auto after_timeout = positions;
    spinFor(200ms);
    EXPECT_EQ(positions, after_timeout);
    rig.respond = true;
    ASSERT_TRUE(spinUntil([&] { return positions > after_timeout; }));
    executor.remove_node(driver);
    executor.remove_node(observer);
}

TEST_F(RosNodeTest, DelayedExecutorDiscardsAnExpiredCompletedSnapshot) {
    RosArmRig rig;
    auto driver =
        std::make_shared<LinkerArm::TeleArmNode>(options({{"max_snapshot_age_ms", 20}}), rig.arm);
    auto observer = std::make_shared<rclcpp::Node>("stale_observer");
    unsigned published = 0;
    auto subscription = observer->create_subscription<sensor_msgs::msg::JointState>(
        "/joint_states", 100, [&](sensor_msgs::msg::JointState::ConstSharedPtr) { ++published; });
    executor.add_node(driver);
    executor.add_node(observer);
    // The first timer invocation only asks the worker for a sample. Stop the
    // executor while that sample completes, then let it age before publication.
    std::this_thread::sleep_for(30ms);
    executor.spin_some();
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (!rig.arm->latestSnapshot().valid && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    ASSERT_TRUE(rig.arm->latestSnapshot().valid);
    std::this_thread::sleep_for(60ms);
    rig.respond = false;
    executor.spin_some();
    spinFor(70ms); // Process any publication queued by the stale-result timer.
    EXPECT_EQ(published, 0u);
    executor.remove_node(driver);
    executor.remove_node(observer);
}

TEST_F(RosNodeTest, LegacyPublishersCanBeDisabled) {
    RosArmRig rig;
    auto driver = std::make_shared<LinkerArm::TeleArmNode>(
        options({{"publish_legacy_topics", false}}), rig.arm);
    EXPECT_EQ(driver->count_publishers("/left_arm_joint_control"), 0u);
    EXPECT_EQ(driver->count_publishers("/right_arm_joint_control"), 0u);
    EXPECT_EQ(driver->count_publishers("/joint_states"), 1u);
}

TEST_F(RosNodeTest, InvalidParametersAreRejectedBeforeOpeningHardware) {
    const std::vector<rclcpp::Parameter> invalid{
        {"poll_rate_hz", 0.0},
        {"poll_rate_hz", std::numeric_limits<double>::infinity()},
        {"request_timeout_ms", 0},
        {"max_snapshot_age_ms", -1},
        {"calibration", 1},
        {"calibration_joint", 14},
        {"negation", std::vector<std::int64_t>(13, 1)},
        {"negation", std::vector<std::int64_t>(14, 0)}};
    for (const auto &parameter : invalid) {
        SCOPED_TRACE(parameter.get_name());
        EXPECT_THROW(std::make_shared<LinkerArm::TeleArmNode>(options({parameter})),
                     std::exception);
    }
}

TEST_F(RosNodeTest, HardwareConfigurationCannotChangeWhileRunning) {
    RosArmRig rig;
    auto driver = std::make_shared<LinkerArm::TeleArmNode>(options(), rig.arm);
    const auto result = driver->set_parameter(rclcpp::Parameter("poll_rate_hz", 20.0));
    EXPECT_FALSE(result.successful);
    EXPECT_DOUBLE_EQ(driver->get_parameter("poll_rate_hz").as_double(), 50.0);
}

TEST_F(RosNodeTest, CalibrationIsDisabledByDefaultAndNeverRunsAtStartup) {
    RosArmRig rig;
    auto driver = std::make_shared<LinkerArm::TeleArmNode>(options(), rig.arm);
    auto caller = std::make_shared<rclcpp::Node>("disabled_calibration_caller");
    auto client = caller->create_client<std_srvs::srv::Trigger>("/linkerta_node/calibrate_once");
    executor.add_node(driver);
    executor.add_node(caller);
    ASSERT_TRUE(spinUntil([&] { return client->service_is_ready(); }));
    EXPECT_EQ(rig.calibrations.load(), 0u);
    auto future = client->async_send_request(std::make_shared<std_srvs::srv::Trigger::Request>());
    ASSERT_TRUE(spinUntil([&] { return future.wait_for(0ms) == std::future_status::ready; }));
    EXPECT_FALSE(future.get()->success);
    spinFor(100ms);
    EXPECT_EQ(rig.calibrations.load(), 0u);
    executor.remove_node(driver);
    executor.remove_node(caller);
}

TEST_F(RosNodeTest, AcceptedCalibrationDoesNotClaimDeviceSuccessOrReplay) {
    RosArmRig rig;
    auto driver =
        std::make_shared<LinkerArm::TeleArmNode>(options({{"allow_calibration", true}}), rig.arm);
    auto caller = std::make_shared<rclcpp::Node>("accepted_calibration_caller");
    auto client = caller->create_client<std_srvs::srv::Trigger>("/linkerta_node/calibrate_once");
    executor.add_node(driver);
    executor.add_node(caller);
    ASSERT_TRUE(spinUntil([&] { return client->service_is_ready(); }));
    auto first = client->async_send_request(std::make_shared<std_srvs::srv::Trigger::Request>());
    ASSERT_TRUE(spinUntil([&] { return first.wait_for(0ms) == std::future_status::ready; }));
    EXPECT_FALSE(first.get()->success);
    ASSERT_TRUE(spinUntil([&] { return rig.calibrations.load() == 1; }));
    auto second = client->async_send_request(std::make_shared<std_srvs::srv::Trigger::Request>());
    ASSERT_TRUE(spinUntil([&] { return second.wait_for(0ms) == std::future_status::ready; }));
    EXPECT_FALSE(second.get()->success);
    spinFor(150ms);
    EXPECT_EQ(rig.calibrations.load(), 1u);
    executor.remove_node(driver);
    executor.remove_node(caller);
}

TEST_F(RosNodeTest, ExplicitCalibrationIsOneShotEvenIfTransportFails) {
    RosArmRig rig;
    rig.fail_calibration = true;
    auto driver =
        std::make_shared<LinkerArm::TeleArmNode>(options({{"allow_calibration", true}}), rig.arm);
    auto caller = std::make_shared<rclcpp::Node>("calibration_caller");
    auto client = caller->create_client<std_srvs::srv::Trigger>("/linkerta_node/calibrate_once");
    executor.add_node(driver);
    executor.add_node(caller);
    ASSERT_TRUE(spinUntil([&] { return client->service_is_ready(); }));
    EXPECT_EQ(rig.calibrations.load(), 0u);
    auto first = client->async_send_request(std::make_shared<std_srvs::srv::Trigger::Request>());
    ASSERT_TRUE(spinUntil([&] { return first.wait_for(0ms) == std::future_status::ready; }));
    (void)first.get(); // An accepted enqueue must not be mistaken for device completion.
    ASSERT_TRUE(spinUntil([&] { return rig.calibrations.load() == 1; }));
    auto second = client->async_send_request(std::make_shared<std_srvs::srv::Trigger::Request>());
    ASSERT_TRUE(spinUntil([&] { return second.wait_for(0ms) == std::future_status::ready; }));
    EXPECT_FALSE(second.get()->success);
    spinFor(200ms);
    EXPECT_EQ(rig.calibrations.load(), 1u);
    executor.remove_node(driver);
    executor.remove_node(caller);
}
} // namespace
