#include "ArmProtocol.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <limits>
#include <stdexcept>

namespace {
using namespace LinkerArm::Protocol;
using namespace std::chrono_literals;

TimePoint at(int milliseconds) {
    return TimePoint{} + std::chrono::milliseconds(milliseconds);
}

Frame jointFrame(std::size_t block, std::int16_t tenths = 100, bool with_faults = true) {
    Frame frame;
    frame.can_id = static_cast<std::uint32_t>(0x65 + block);
    const bool four = block % 2 == 0;
    frame.dlc = four ? 8 : (with_faults ? 7 : 6);
    const auto encoded = static_cast<std::uint16_t>(tenths);
    for (std::size_t i = 0; i < (four ? 4U : 3U); ++i) {
        frame.data[2 * i] = static_cast<std::uint8_t>(encoded >> 8U);
        frame.data[2 * i + 1] = static_cast<std::uint8_t>(encoded & 0xffU);
    }
    if (!four && with_faults) {
        frame.data[6] = 0x55;
    }
    return frame;
}

DecodedFrame joint(std::size_t block, std::int16_t tenths = 100, bool with_faults = true) {
    return *decodeFrame(jointFrame(block, tenths, with_faults)).frame;
}

void complete(SnapshotAssembler &assembler, int start = 0, bool with_faults = true) {
    for (std::size_t block = 0; block < kBlockCount; ++block) {
        EXPECT_EQ(
            assembler.ingest(joint(block, 100, with_faults), at(start + static_cast<int>(block))),
            block == kBlockCount - 1);
    }
}

TEST(ArmProtocol, DecodesSignedBigEndianAndWrappedAngles) {
    EXPECT_NEAR(decodeAngle(0x01, 0x2c), 30.0f, 0.0001f);
    EXPECT_NEAR(decodeAngle(0xff, 0x9c), -10.0f, 0.0001f);
    EXPECT_NEAR(decodeAngle(0x07, 0x08), -180.0f, 0.0001f);
    EXPECT_NEAR(decodeAngle(0xf8, 0xf8), -180.0f, 0.0001f);
    EXPECT_NEAR(decodeAngle(0x7f, 0xff), 36.7f, 0.001f);
    EXPECT_NEAR(decodeAngle(0x80, 0x00), -36.8f, 0.001f);
    EXPECT_NEAR(shortestAngularDelta(179.0f, -179.0f), 2.0f, 0.0001f);
    EXPECT_NEAR(shortestAngularDelta(-179.0f, 179.0f), -2.0f, 0.0001f);
}

TEST(ArmProtocol, LengthContractForEveryJointId) {
    for (std::size_t block = 0; block < kBlockCount; ++block) {
        for (unsigned length = 0; length <= 9; ++length) {
            auto frame = jointFrame(block);
            frame.dlc = static_cast<std::uint8_t>(length);
            const auto result = decodeFrame(frame);
            const bool valid = block % 2 == 0 ? length == 8 : length == 6 || length == 7;
            EXPECT_EQ(result.frame.has_value(), valid) << block << ":" << length;
            EXPECT_EQ(result.error, valid ? DecodeError::none : DecodeError::invalid_length);
        }
    }
}

TEST(ArmProtocol, RejectsRemoteErrorExtendedAndFdFrames) {
    auto frame = jointFrame(0);
    frame.is_remote = true;
    EXPECT_EQ(decodeFrame(frame).error, DecodeError::invalid_flags);
    frame = jointFrame(0);
    frame.is_error = true;
    EXPECT_EQ(decodeFrame(frame).error, DecodeError::invalid_flags);
    frame = jointFrame(0);
    frame.is_extended = true;
    EXPECT_EQ(decodeFrame(frame).error, DecodeError::invalid_flags);
    frame = jointFrame(0);
    frame.is_fd = true;
    EXPECT_EQ(decodeFrame(frame).error, DecodeError::invalid_flags);
    frame = jointFrame(0);
    frame.can_id |= 0x40000000U;
    EXPECT_EQ(decodeFrame(frame).error, DecodeError::invalid_flags);
    frame = jointFrame(0);
    frame.can_id = 0x123;
    EXPECT_EQ(decodeFrame(frame).error, DecodeError::unknown_id);
}

TEST(ArmProtocol, KeepsVersionBytesWithoutInventingAnEncoding) {
    Frame frame;
    frame.can_id = 0x64;
    frame.data = {123, 45, 6, 7, 8, 9, 10, 11};
    for (unsigned length = 0; length <= 9; ++length) {
        frame.dlc = static_cast<std::uint8_t>(length);
        const auto decoded = decodeFrame(frame);
        if (length == 0 || length > 8) {
            EXPECT_EQ(decoded.error, DecodeError::invalid_length);
        } else {
            ASSERT_TRUE(decoded.frame);
            EXPECT_EQ(decoded.frame->kind, FrameKind::version);
            EXPECT_EQ(decoded.frame->version_size, length);
            EXPECT_EQ(decoded.frame->version_bytes, frame.data);
        }
    }
}

TEST(ArmProtocol, DistinguishesMissingFaultByteFromHealthyFaultByte) {
    auto frame = jointFrame(1, 100, false);
    EXPECT_FALSE(decodeFrame(frame).frame->fault_bits);
    frame.dlc = 7;
    frame.data[6] = 0;
    EXPECT_EQ(decodeFrame(frame).frame->fault_bits, 0);
    frame.data[6] = 0x55;
    EXPECT_EQ(decodeFrame(frame).frame->fault_bits, 0x55);
}

TEST(ArmSnapshot, RequiresAllFourBlocksAndCommitsAtomicallyInAnyOrder) {
    SnapshotAssembler assembler;
    EXPECT_FALSE(assembler.latest().valid);
    constexpr std::array<std::size_t, 4> order{3, 1, 0, 2};
    for (std::size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(assembler.ingest(joint(order[i], static_cast<std::int16_t>(100 + order[i] * 10)),
                                   at(static_cast<int>(i))),
                  i == 3);
        EXPECT_EQ(assembler.latest().valid, i == 3);
    }
    const auto snapshot = assembler.latest();
    EXPECT_EQ(snapshot.received_mask, kCompleteMask);
    EXPECT_EQ(snapshot.sequence, 1);
    EXPECT_EQ(snapshot.captured_at, at(3));
    EXPECT_FLOAT_EQ(snapshot.positions_degrees[0], 10);
    EXPECT_FLOAT_EQ(snapshot.positions_degrees[4], 11);
    EXPECT_FLOAT_EQ(snapshot.positions_degrees[7], 12);
    EXPECT_FLOAT_EQ(snapshot.positions_degrees[13], 13);
    for (std::size_t i = 0; i < kJointCount; ++i) {
        EXPECT_TRUE(snapshot.fault_valid[i]);
        EXPECT_EQ(snapshot.faults[i], (i % 7) % 2 == 0);
    }
    EXPECT_EQ(assembler.pendingMask(), 0);
}

TEST(ArmSnapshot, MissingFaultsRemainUnknownAndDoNotReusePriorFaults) {
    SnapshotAssembler assembler;
    complete(assembler);
    complete(assembler, 10, false);
    const auto snapshot = assembler.latest();
    EXPECT_EQ(snapshot.sequence, 2);
    for (std::size_t i = 0; i < kJointCount; ++i) {
        EXPECT_FALSE(snapshot.fault_valid[i]);
        EXPECT_FALSE(snapshot.faults[i]);
    }
}

TEST(ArmSnapshot, DuplicateDiscardsThePriorPartialCandidate) {
    SnapshotAssembler assembler;
    EXPECT_FALSE(assembler.ingest(joint(0, 100), at(0)));
    EXPECT_FALSE(assembler.ingest(joint(1, 100), at(1)));
    EXPECT_FALSE(assembler.ingest(joint(0, 200), at(2)));
    EXPECT_EQ(assembler.pendingMask(), 1);
    EXPECT_FALSE(assembler.ingest(joint(2, 200), at(3)));
    EXPECT_FALSE(assembler.ingest(joint(3, 200), at(4)));
    EXPECT_FALSE(assembler.latest().valid);
    EXPECT_TRUE(assembler.ingest(joint(1, 200), at(5)));
    for (const auto position : assembler.latest().positions_degrees) {
        EXPECT_FLOAT_EQ(position, 20.0f);
    }
}

TEST(ArmSnapshot, ExpiredPartialSetStartsOverWithoutTouchingCommittedSnapshot) {
    SnapshotAssembler assembler(10ms);
    complete(assembler);
    EXPECT_FALSE(assembler.ingest(joint(0, 200), at(10)));
    EXPECT_FALSE(assembler.ingest(joint(1, 200), at(21)));
    EXPECT_EQ(assembler.pendingMask(), 2);
    EXPECT_EQ(assembler.latest().sequence, 1);
    EXPECT_FLOAT_EQ(assembler.latest().positions_degrees[0], 10.0f);
    EXPECT_FALSE(assembler.ingest(joint(2, 200), at(22)));
    EXPECT_FALSE(assembler.ingest(joint(3, 200), at(23)));
    EXPECT_TRUE(assembler.ingest(joint(0, 200), at(24)));
    EXPECT_EQ(assembler.latest().sequence, 2);
}

TEST(ArmSnapshot, FreshnessIncludesTheOldestPartAndRejectsFutureTimestamps) {
    SnapshotAssembler assembler;
    complete(assembler);
    EXPECT_TRUE(assembler.latest().isFresh(at(10), 10ms));
    EXPECT_FALSE(assembler.latest().isFresh(at(11), 10ms));
    EXPECT_FALSE(assembler.latest().isFresh(at(2), 10ms));
    EXPECT_FALSE(assembler.latest().isFresh(at(10), -1ms));
    EXPECT_FALSE(ArmSnapshot{}.isFresh(at(10), 10ms));
}

TEST(ArmSnapshot, ResetAndNewRequestDiscardOldPartialState) {
    SnapshotAssembler assembler;
    complete(assembler);
    EXPECT_FALSE(assembler.ingest(joint(0), at(10)));
    assembler.beginCycle(at(20));
    EXPECT_EQ(assembler.pendingMask(), 0);
    EXPECT_TRUE(assembler.latest().valid);
    EXPECT_FALSE(assembler.ingest(joint(0), at(19)));
    EXPECT_EQ(assembler.pendingMask(), 0);
    complete(assembler, 20);
    assembler.reset(42);
    EXPECT_FALSE(assembler.latest().valid);
    EXPECT_EQ(assembler.latest().generation, 42);
    EXPECT_EQ(assembler.pendingMask(), 0);
    complete(assembler, 30);
    EXPECT_EQ(assembler.latest().generation, 42);
    EXPECT_EQ(assembler.latest().sequence, 3);
}

TEST(ArmSnapshot, InvalidDecodedFramesAndBackwardsTimeHaveNoEffect) {
    SnapshotAssembler assembler;
    EXPECT_FALSE(assembler.ingest(joint(0), at(10)));
    EXPECT_FALSE(assembler.ingest(joint(1), at(9)));
    auto invalid = joint(1);
    invalid.block_index = 4;
    EXPECT_FALSE(assembler.ingest(invalid, at(11)));
    invalid = joint(1);
    invalid.count = 4;
    EXPECT_FALSE(assembler.ingest(invalid, at(11)));
    invalid = joint(1);
    invalid.positions_degrees[0] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(assembler.ingest(invalid, at(11)));
    invalid = joint(1);
    invalid.kind = FrameKind::version;
    EXPECT_FALSE(assembler.ingest(invalid, at(11)));
    EXPECT_EQ(assembler.pendingMask(), 1);
    EXPECT_FALSE(assembler.latest().valid);
}

TEST(ArmSnapshot, MalformedDecodedRangesAndCountsDoNotAdvanceOrCorruptState) {
    SnapshotAssembler assembler;
    complete(assembler);
    const auto committed = assembler.latest();
    EXPECT_FALSE(assembler.ingest(joint(0), at(10)));
    for (const auto index : {kBlockCount, std::numeric_limits<std::size_t>::max()}) {
        auto malformed = joint(1);
        malformed.block_index = index;
        EXPECT_FALSE(assembler.ingest(malformed, at(100)));
    }
    for (const auto count :
         {std::size_t{0}, std::size_t{4}, std::numeric_limits<std::size_t>::max()}) {
        auto malformed = joint(1);
        malformed.count = count;
        EXPECT_FALSE(assembler.ingest(malformed, at(100)));
    }
    for (const auto value : {std::numeric_limits<float>::infinity(),
                             -std::numeric_limits<float>::infinity(), -180.1f, 180.0f}) {
        auto malformed = joint(1);
        malformed.positions_degrees[0] = value;
        EXPECT_FALSE(assembler.ingest(malformed, at(100)));
    }
    auto malformed = joint(2);
    malformed.fault_bits = 1;
    EXPECT_FALSE(assembler.ingest(malformed, at(100)));
    EXPECT_EQ(assembler.pendingMask(), 1);
    EXPECT_EQ(assembler.latest().sequence, committed.sequence);
    EXPECT_EQ(assembler.latest().received_at, committed.received_at);
    EXPECT_EQ(assembler.latest().positions_degrees, committed.positions_degrees);
    // Invalid input timestamp 100 must not move the receive clock forward.
    EXPECT_FALSE(assembler.ingest(joint(1), at(11)));
    EXPECT_FALSE(assembler.ingest(joint(2), at(12)));
    EXPECT_TRUE(assembler.ingest(joint(3), at(13)));
    EXPECT_EQ(assembler.latest().received_at[0], at(10));
    EXPECT_EQ(assembler.latest().received_at[3], at(13));
    auto incomplete = assembler.latest();
    incomplete.received_mask = 3;
    EXPECT_FALSE(incomplete.isFresh(at(13), 20ms));
}

TEST(ArmSnapshot, DoesNotFreezeAtTheAngularBoundaryOrRejectInitialMinus180) {
    SnapshotAssembler assembler;
    for (std::size_t block = 0; block < 4; ++block) {
        assembler.ingest(joint(block, 1800), at(static_cast<int>(block)));
    }
    EXPECT_FLOAT_EQ(assembler.latest().positions_degrees[0], -180.0f);
    for (std::size_t block = 0; block < 4; ++block) {
        assembler.ingest(joint(block, 1790), at(10 + static_cast<int>(block)));
    }
    for (std::size_t block = 0; block < 4; ++block) {
        assembler.ingest(joint(block, -1790), at(20 + static_cast<int>(block)));
    }
    EXPECT_FLOAT_EQ(assembler.latest().positions_degrees[0], -179.0f);
}

TEST(ArmSnapshot, ValidatesSkewAndAllowsExactlyTheConfiguredBoundary) {
    EXPECT_THROW(SnapshotAssembler{-1ms}, std::invalid_argument);
    SnapshotAssembler assembler(3ms);
    complete(assembler);
    EXPECT_TRUE(assembler.latest().valid);
    SnapshotAssembler simultaneous(0ms);
    for (std::size_t i = 0; i < kBlockCount; ++i) {
        EXPECT_EQ(simultaneous.ingest(joint(i), at(0)), i == kBlockCount - 1);
    }
}

} // namespace
