#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace LinkerArm {

struct NodeConfig {
    int calibration = 0;
    std::vector<std::int64_t> negation{-1, -1, -1, -1, -1, -1, -1, -1, 1, -1, 1, -1, -1, -1};
    std::string device = "master_arm";
    std::string can_interface;
    double poll_rate_hz = 100.0;
    std::int64_t request_timeout_ms = 250;
    bool publish_legacy_topics = true;
    bool allow_calibration = false;
    std::int64_t max_snapshot_age_ms = 250;
    std::int64_t calibration_joint = 255;
};

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

inline void validateNodeConfig(const NodeConfig &config) {
    validateNodeConfig(config.calibration, config.negation);
    if (!std::isfinite(config.poll_rate_hz) || config.poll_rate_hz < 1.0 ||
        config.poll_rate_hz > 200.0) {
        throw std::invalid_argument("poll_rate_hz must be finite and between 1 and 200");
    }
    if (config.request_timeout_ms < 1 || config.request_timeout_ms > 10000) {
        throw std::invalid_argument("request_timeout_ms must be between 1 and 10000");
    }
    if (config.max_snapshot_age_ms < 1 || config.max_snapshot_age_ms > 10000) {
        throw std::invalid_argument("max_snapshot_age_ms must be between 1 and 10000");
    }
    if (config.calibration_joint < 0 ||
        (config.calibration_joint > 13 && config.calibration_joint != 255)) {
        throw std::invalid_argument("calibration_joint must be 0..13 or 255");
    }
    if (config.can_interface.empty() && config.device.empty()) {
        throw std::invalid_argument("device must not be empty when can_interface is unset");
    }
    if (config.can_interface.size() > 15 ||
        config.can_interface.find_first_of(" /\t\n\r") != std::string::npos) {
        throw std::invalid_argument("can_interface must be a valid short interface name");
    }
}

} // namespace LinkerArm
