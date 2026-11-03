#include "ArmProtocol.h"

#include <cmath>
#include <stdexcept>

namespace LinkerArm::Protocol {
namespace {

float wrapDegrees(float angle) noexcept {
    float wrapped = std::fmod(angle + 180.0f, 360.0f);
    if (wrapped < 0.0f) {
        wrapped += 360.0f;
    }
    return wrapped - 180.0f;
}

} // namespace

float decodeAngle(std::uint8_t high, std::uint8_t low) noexcept {
    const auto encoded = (static_cast<std::uint32_t>(high) << 8U) | low;
    // Do not rely on an out-of-range unsigned-to-signed conversion.
    const auto signed_value = encoded < 0x8000U ? static_cast<std::int32_t>(encoded)
                                                : static_cast<std::int32_t>(encoded) - 0x10000;
    return wrapDegrees(static_cast<float>(signed_value) / 10.0f);
}

float shortestAngularDelta(float previous, float current) noexcept {
    return wrapDegrees(current - previous);
}

DecodeResult decodeFrame(const Frame &frame) noexcept {
    if (frame.is_extended || frame.is_remote || frame.is_error || frame.is_fd ||
        frame.can_id > 0x7ffU) {
        return {DecodeError::invalid_flags, std::nullopt};
    }
    if (frame.dlc > frame.data.size()) {
        return {DecodeError::invalid_length, std::nullopt};
    }
    if (frame.can_id == 0x64U) {
        if (frame.dlc == 0) {
            return {DecodeError::invalid_length, std::nullopt};
        }
        DecodedFrame decoded;
        decoded.kind = FrameKind::version;
        decoded.version_bytes = frame.data;
        decoded.version_size = frame.dlc;
        return {DecodeError::none, decoded};
    }
    if (frame.can_id < 0x65U || frame.can_id > 0x68U) {
        return {DecodeError::unknown_id, std::nullopt};
    }
    const std::size_t block = frame.can_id - 0x65U;
    const bool four_joints = block % 2 == 0;
    if ((four_joints && frame.dlc != 8) || (!four_joints && frame.dlc != 6 && frame.dlc != 7)) {
        return {DecodeError::invalid_length, std::nullopt};
    }
    DecodedFrame decoded;
    decoded.block_index = block;
    decoded.count = four_joints ? 4 : 3;
    for (std::size_t i = 0; i < decoded.count; ++i) {
        decoded.positions_degrees[i] = decodeAngle(frame.data[2 * i], frame.data[2 * i + 1]);
    }
    if (!four_joints && frame.dlc == 7) {
        decoded.fault_bits = frame.data[6];
    }
    return {DecodeError::none, decoded};
}

bool ArmSnapshot::isFresh(TimePoint now, Clock::duration max_age) const noexcept {
    if (!valid || received_mask != kCompleteMask || max_age < Clock::duration::zero()) {
        return false;
    }
    for (const auto timestamp : received_at) {
        if (now < timestamp || now - timestamp > max_age) {
            return false;
        }
    }
    return true;
}

SnapshotAssembler::SnapshotAssembler(Clock::duration max_skew) : max_skew_(max_skew) {
    if (max_skew < Clock::duration::zero()) {
        throw std::invalid_argument("snapshot skew must not be negative");
    }
}

void SnapshotAssembler::beginCycle() noexcept {
    pending_ = {};
    first_at_.reset();
    last_at_.reset();
    not_before_.reset();
}

void SnapshotAssembler::beginCycle(TimePoint not_before) noexcept {
    beginCycle();
    not_before_ = not_before;
}

void SnapshotAssembler::reset(std::uint64_t generation) noexcept {
    beginCycle();
    generation_ = generation;
    latest_ = {};
    latest_.generation = generation_;
    latest_.sequence = sequence_;
}

bool SnapshotAssembler::ingest(const DecodedFrame &frame, TimePoint received_at) noexcept {
    if (frame.kind != FrameKind::joint || frame.block_index >= kBlockCount) {
        return false;
    }
    const auto block = frame.block_index;
    const std::size_t count = block % 2 == 0 ? 4 : 3;
    if (frame.count != count || (block % 2 == 0 && frame.fault_bits) ||
        (not_before_ && received_at < *not_before_) || (last_at_ && received_at < *last_at_)) {
        return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (!std::isfinite(frame.positions_degrees[i]) || frame.positions_degrees[i] < -180.0f ||
            frame.positions_degrees[i] >= 180.0f) {
            return false;
        }
    }
    const auto bit = static_cast<std::uint8_t>(1U << block);
    if ((pending_.received_mask & bit) != 0 ||
        (first_at_ && received_at - *first_at_ > max_skew_)) {
        // A repeated part is not merged into old parts. Preserve the explicit
        // request cutoff while discarding this partial candidate.
        pending_ = {};
        first_at_.reset();
    }
    if (!first_at_) {
        first_at_ = received_at;
    }
    last_at_ = received_at;
    constexpr std::array<std::size_t, kBlockCount> offsets{0, 4, 7, 11};
    for (std::size_t i = 0; i < count; ++i) {
        pending_.positions_degrees[offsets[block] + i] = frame.positions_degrees[i];
    }
    if (block % 2 != 0) {
        const std::size_t arm_base = block == 1 ? 0 : 7;
        for (std::size_t i = 0; i < 7; ++i) {
            pending_.fault_valid[arm_base + i] = frame.fault_bits.has_value();
            pending_.faults[arm_base + i] =
                frame.fault_bits && ((*frame.fault_bits & (1U << i)) != 0);
        }
    }
    pending_.received_at[block] = received_at;
    pending_.received_mask = static_cast<std::uint8_t>(pending_.received_mask | bit);
    if (pending_.received_mask != kCompleteMask) {
        return false;
    }
    pending_.valid = true;
    pending_.captured_at = received_at;
    pending_.sequence = ++sequence_;
    pending_.generation = generation_;
    latest_ = pending_;
    // The next received part belongs to a new candidate. Keep the request cutoff
    // and last timestamp so delayed/backwards timestamps still cannot enter it.
    pending_ = {};
    first_at_.reset();
    return true;
}

ArmSnapshot SnapshotAssembler::latest() const noexcept {
    return latest_;
}

std::uint8_t SnapshotAssembler::pendingMask() const noexcept {
    return pending_.received_mask;
}

} // namespace LinkerArm::Protocol
