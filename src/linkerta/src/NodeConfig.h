#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace LinkerArm {

// Validate before opening hardware or indexing a caller-provided direction map.
inline void validateNodeConfig(int calibration, const std::vector<std::int64_t> &negation) {
    if (calibration != 0) {
        throw std::invalid_argument(
            "startup calibration is disabled; use an explicit calibration operation");
    }
    if (negation.size() != 14) {
        throw std::invalid_argument("negation must contain exactly 14 directions");
    }
    for (const auto direction : negation) {
        if (direction != -1 && direction != 1) {
            throw std::invalid_argument("each negation direction must be -1 or 1");
        }
    }
}

} // namespace LinkerArm
