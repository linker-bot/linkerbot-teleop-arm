#include "BenchmarkOptions.h"
#include "BenchmarkStats.h"
#include <gtest/gtest.h>
#include <limits>

namespace B = LinkerArm::Benchmark;
TEST(Benchmark, RequiresExplicitModeAndHardwareIdentity) {
    EXPECT_THROW(B::parseOptions({"--output", "run.csv"}), std::invalid_argument);
    EXPECT_THROW(B::parseOptions({"--simulate", "--interface", "can0", "--output", "run.csv"}),
                 std::invalid_argument);
    EXPECT_THROW(B::parseOptions({"--interface", "can0", "--output", "run.csv"}),
                 std::invalid_argument);
    const auto real = B::parseOptions({"--interface", "can0", "--output", "run.csv",
                                       "--device-label", "arm-A", "--firmware", "measured-v1"});
    EXPECT_FALSE(real.simulate);
    EXPECT_EQ(real.count, 100u);
    EXPECT_TRUE(B::parseOptions({"--help"}).help);
}
TEST(Benchmark, RejectsUnboundedAndInvalidRuns) {
    const std::vector<std::vector<std::string>> bad{{"--count", "0"},
                                                    {"--count", "1000001"},
                                                    {"--count", "-1"},
                                                    {"--count", "18446744073709551616"},
                                                    {"--timeout-ms", "0"},
                                                    {"--timeout-ms", "10001"},
                                                    {"--rate-hz", "NaN"},
                                                    {"--rate-hz", "inf"},
                                                    {"--rate-hz", "0.01"},
                                                    {"--rate-hz", "201"},
                                                    {"--rate-hz", "1x"},
                                                    {"--unknown", "1"},
                                                    {"--count", "2", "--count", "3"}};
    for (auto args : bad) {
        args.insert(args.begin(), {"--simulate", "--output", "run.csv"});
        EXPECT_THROW(B::parseOptions(args), std::invalid_argument);
    }
    EXPECT_THROW(B::parseOptions({"--simulate"}), std::invalid_argument);
    EXPECT_THROW(B::parseOptions({"--interface", "can0", "--output", "x", "--device-label", "arm",
                                  "--firmware", "v1", "--drop-every", "2"}),
                 std::invalid_argument);
}
TEST(Benchmark, NearestRankPreservesTailAndEmptyIsUnknown) {
    EXPECT_FALSE(B::percentile({}, 0.99));
    EXPECT_EQ(B::percentile({42}, 0.99), 42u);
    EXPECT_EQ(B::percentile({5, 1, 4, 2, 3}, 0.5), 3u);
    EXPECT_EQ(B::percentile({5, 1, 4, 2, 3}, 0.95), 5u);
    EXPECT_EQ(B::percentile({5, 1, 4, 2, 3}, 0.99), 5u);
    EXPECT_THROW(B::percentile({1}, 0.0), std::invalid_argument);
    EXPECT_THROW(B::percentile({1}, std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
}
