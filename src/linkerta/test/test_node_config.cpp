#include "NodeConfig.h"
#include <gtest/gtest.h>
#include <limits>

TEST(NodeConfig, AcceptsFourteenDirections) {
    EXPECT_NO_THROW(LinkerArm::validateNodeConfig(0, std::vector<std::int64_t>(14, 1)));
    EXPECT_NO_THROW(LinkerArm::validateNodeConfig(0, std::vector<std::int64_t>(14, -1)));
}

TEST(NodeConfig, RejectsMalformedMapsBeforeHardwareAccess) {
    for (const auto size : {0, 13, 15}) {
        EXPECT_THROW(LinkerArm::validateNodeConfig(0, std::vector<std::int64_t>(size, 1)),
                     std::invalid_argument);
    }
    for (const auto value : {0, 2, -2}) {
        auto directions = std::vector<std::int64_t>(14, 1);
        directions.back() = value;
        EXPECT_THROW(LinkerArm::validateNodeConfig(0, directions), std::invalid_argument);
    }
}

TEST(NodeConfig, NeverReplaysStartupCalibration) {
    for (const auto calibration : {-1, 1, 2}) {
        EXPECT_THROW(LinkerArm::validateNodeConfig(calibration, std::vector<std::int64_t>(14, 1)),
                     std::invalid_argument);
    }
}

TEST(NodeConfig, RejectsInvalidRuntimeParameters) {
    LinkerArm::NodeConfig config;
    EXPECT_NO_THROW(LinkerArm::validateNodeConfig(config));
    for (double rate : {0.0, -1.0, 201.0, std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()}) {
        config.poll_rate_hz = rate;
        EXPECT_THROW(LinkerArm::validateNodeConfig(config), std::invalid_argument);
    }
    config = {};
    for (auto timeout : {0, -1, 10001}) {
        config.request_timeout_ms = timeout;
        EXPECT_THROW(LinkerArm::validateNodeConfig(config), std::invalid_argument);
    }
    config = {};
    config.max_snapshot_age_ms = 0;
    EXPECT_THROW(LinkerArm::validateNodeConfig(config), std::invalid_argument);
    config = {};
    for (auto joint : {-1, 14, 254, 256}) {
        config.calibration_joint = joint;
        EXPECT_THROW(LinkerArm::validateNodeConfig(config), std::invalid_argument);
    }
    config = {};
    for (const auto &name : {"can interface", "1234567890123456", "/can0"}) {
        config.can_interface = name;
        EXPECT_THROW(LinkerArm::validateNodeConfig(config), std::invalid_argument);
    }
}
