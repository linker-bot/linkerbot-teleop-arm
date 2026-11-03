#ifndef LINKER_ARM_PROTOCOL_H
#define LINKER_ARM_PROTOCOL_H

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace LinkerArm::Protocol {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
constexpr std::size_t kJointCount = 14;
constexpr std::size_t kBlockCount = 4;
constexpr std::uint8_t kCompleteMask = 0x0f;

// Portable wire representation. can_id is the arbitration ID, not a SocketCAN
// ID with flags embedded. Transport adapters must extract flags separately.
struct Frame {
    std::uint32_t can_id = 0;
    std::uint8_t dlc = 0;
    std::array<std::uint8_t, 8> data{};
    bool is_extended = false;
    bool is_remote = false;
    bool is_error = false;
    bool is_fd = false;
};

enum class DecodeError { none, unknown_id, invalid_flags, invalid_length };
enum class FrameKind { joint, version };

struct DecodedFrame {
    FrameKind kind = FrameKind::joint;
    std::size_t block_index = 0;
    std::array<float, 4> positions_degrees{};
    std::size_t count = 0;
    // Missing fault data is unknown, never an assertion that joints are healthy.
    std::optional<std::uint8_t> fault_bits;
    std::array<std::uint8_t, 8> version_bytes{};
    std::size_t version_size = 0;
};

struct DecodeResult {
    DecodeError error = DecodeError::none;
    std::optional<DecodedFrame> frame;
};

// Signed big-endian deci-degrees, represented in [-180, 180). This is a wire
// representation conversion, not a joint limit or velocity plausibility test.
float decodeAngle(std::uint8_t high, std::uint8_t low) noexcept;
// Smallest signed circular delta in [-180, 180), useful for continuous joints.
// Callers must choose a physical joint model before using this for unwrapping.
float shortestAngularDelta(float previous, float current) noexcept;
DecodeResult decodeFrame(const Frame &frame) noexcept;

struct ArmSnapshot {
    std::array<float, kJointCount> positions_degrees{};
    std::array<bool, kJointCount> faults{};
    std::array<bool, kJointCount> fault_valid{};
    std::array<TimePoint, kBlockCount> received_at{};
    TimePoint captured_at{}; // Latest constituent arrival, not hardware sample time.
    std::uint64_t sequence = 0;
    std::uint64_t generation = 0;
    std::uint8_t received_mask = 0;
    bool valid = false;

    // All constituent frames, including the oldest, must be within max_age.
    bool isFresh(TimePoint now, Clock::duration max_age) const noexcept;
};

// Single-owner state machine; callers synchronize access if used across threads.
// Four distinct blocks within max_skew form a host-coherent snapshot. Firmware
// has no sample sequence in these frames, so this cannot establish simultaneous
// hardware sampling or distinguish a late previous response arriving afterward.
class SnapshotAssembler {
public:
    explicit SnapshotAssembler(Clock::duration max_skew = std::chrono::milliseconds(100));

    // Start an explicit request cycle. The last committed snapshot stays visible.
    void beginCycle() noexcept;
    // Also reject frames whose receive timestamp predates the request boundary.
    void beginCycle(TimePoint not_before) noexcept;
    // Invalidate cached state, e.g. on disconnect/recovery. Sequence remains
    // monotonic for this assembler; generation identifies a transport lifetime.
    void reset(std::uint64_t generation = 0) noexcept;

    // Invalid/foreign frames and backwards timestamps have no state effect.
    // A duplicate block or an expired partial set starts a new candidate set.
    // Returns true only when a complete new snapshot was committed.
    bool ingest(const DecodedFrame &frame, TimePoint received_at) noexcept;
    ArmSnapshot latest() const noexcept;
    std::uint8_t pendingMask() const noexcept;

private:
    Clock::duration max_skew_;
    ArmSnapshot pending_{};
    ArmSnapshot latest_{};
    std::optional<TimePoint> first_at_;
    std::optional<TimePoint> last_at_;
    std::optional<TimePoint> not_before_;
    std::uint64_t sequence_ = 0;
    std::uint64_t generation_ = 0;
};

} // namespace LinkerArm::Protocol

#endif
