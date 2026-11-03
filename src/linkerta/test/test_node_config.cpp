#include "NodeConfig.h"
#include <gtest/gtest.h>

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
