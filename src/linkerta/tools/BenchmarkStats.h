#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace LinkerArm::Benchmark {
// Nearest-rank percentile over successful request durations, in microseconds.
// Missing samples yield no value; zero is not used to imply a successful
// measurement.
inline std::optional<std::uint64_t> percentile(std::vector<std::uint64_t> samples, double p) {
    if (!std::isfinite(p) || p <= 0.0 || p > 1.0)
        throw std::invalid_argument("percentile must be in (0, 1]");
    if (samples.empty())
        return std::nullopt;
    std::sort(samples.begin(), samples.end());
    const auto rank = static_cast<std::size_t>(std::ceil(p * samples.size()));
    return samples[rank - 1];
}
} // namespace LinkerArm::Benchmark
