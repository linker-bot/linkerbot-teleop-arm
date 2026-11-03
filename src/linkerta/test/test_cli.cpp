#include "CliOptions.h"

#include <gtest/gtest.h>
#include <stdexcept>

TEST(CliOptions, DefaultsAndHelpRequireNoHardware) {
    const auto defaults = LinkerArm::parseCliOptions({});
    EXPECT_EQ(defaults.interface_name, "can0");
    EXPECT_EQ(defaults.bitrate, 1000000);
    EXPECT_DOUBLE_EQ(defaults.rate_hz, 1.0);
    EXPECT_EQ(defaults.timeout.count(), 250);
    EXPECT_EQ(defaults.count, 0u);
    EXPECT_FALSE(defaults.help);
    EXPECT_TRUE(LinkerArm::parseCliOptions({"--help"}).help);
    EXPECT_TRUE(LinkerArm::parseCliOptions({"-h"}).help);
    EXPECT_NE(std::string(LinkerArm::cliUsage()).find("degrees"), std::string::npos);
}

TEST(CliOptions, PreservesPositionalCompatibilityAndNamedParameters) {
    const auto old = LinkerArm::parseCliOptions({"vcan0", "500000"});
    EXPECT_EQ(old.interface_name, "vcan0");
    EXPECT_EQ(old.bitrate, 500000);
    const auto named =
        LinkerArm::parseCliOptions({"--interface=can1", "--bitrate", "250000", "--rate-hz", "12.5",
                                    "--timeout-ms=500", "--count", "3"});
    EXPECT_EQ(named.interface_name, "can1");
    EXPECT_EQ(named.bitrate, 250000);
    EXPECT_DOUBLE_EQ(named.rate_hz, 12.5);
    EXPECT_EQ(named.timeout.count(), 500);
    EXPECT_EQ(named.count, 3u);
}

TEST(CliOptions, RejectsMalformedArgumentsBeforeHardwareAccess) {
    const std::vector<std::vector<std::string>> invalid{{"--unknown", "1"},
                                                        {"--interface"},
                                                        {"--interface="},
                                                        {"--interface", "a/b"},
                                                        {"--interface", "can 0"},
                                                        {"--interface", "abcdefghijklmnop"},
                                                        {"can0", "1000000", "extra"},
                                                        {"--bitrate", "-1"},
                                                        {"--bitrate", "+1"},
                                                        {"--bitrate", "1x"},
                                                        {"--bitrate", "0"},
                                                        {"--bitrate", "10000001"},
                                                        {"--timeout-ms", "0"},
                                                        {"--timeout-ms", "10001"},
                                                        {"--count", "-1"},
                                                        {"--count", "18446744073709551616"},
                                                        {"--rate-hz", "NaN"},
                                                        {"--rate-hz", "inf"},
                                                        {"--rate-hz", "0.09"},
                                                        {"--rate-hz", "200.1"},
                                                        {"--rate-hz", " 1"},
                                                        {"--rate-hz", "1x"},
                                                        {"--count", "1", "--count", "2"},
                                                        {"can0", "--interface", "can1"},
                                                        {"-x"}};
    for (const auto &args : invalid) {
        SCOPED_TRACE(args.front());
        EXPECT_THROW(LinkerArm::parseCliOptions(args), std::invalid_argument);
    }
}

TEST(CliOptions, AcceptsBoundaryValuesWithoutNarrowingOverflow) {
    const auto options =
        LinkerArm::parseCliOptions({"--bitrate", "10000000", "--timeout-ms", "10000", "--count",
                                    "18446744073709551615", "--rate-hz", "0.1"});
    EXPECT_EQ(options.count, UINT64_MAX);
    EXPECT_DOUBLE_EQ(options.rate_hz, 0.1);
    EXPECT_DOUBLE_EQ(LinkerArm::parseCliOptions({"--rate-hz", "200"}).rate_hz, 200.0);
}
