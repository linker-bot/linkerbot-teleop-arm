#include "LinkerArm.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <future>
#include <gtest/gtest.h>
#include <linkerbot/can/fake_transport.hpp>
#include <linkerbot/transport/session.hpp>
#include <mutex>

using namespace std::chrono_literals;
namespace {
using linkerbot::can::FakeTransport;
using linkerbot::can::Message;
using linkerbot::transport::Session;

std::vector<Message> jointReplies() {
    std::vector<Message> result;
    for (unsigned i = 0; i < 4; ++i) {
        Message frame;
        frame.id = 0x65 + i;
        frame.dlc = i % 2 ? 7 : 8;
        frame.data[1] = 100;
        result.push_back(frame);
    }
    return result;
}

struct ReleaseGate {
    FakeTransport *fake;
    ~ReleaseGate() {
        fake->release_tx_gate();
    }
};

struct ArmRig {
    std::atomic<bool> respond{true};
    std::atomic<bool> version_respond{true};
    std::atomic<bool> fail{false};
    std::atomic<unsigned> queries{0};
    std::mutex observed_mutex;
    std::condition_variable observed;
    FakeTransport *fake;
    std::shared_ptr<Session> session;

    explicit ArmRig(bool recoverable = false) {
        auto factory = [this] {
            auto transport = std::make_unique<FakeTransport>();
            fake = transport.get();
            transport->on_request(0x64, [this](const Message &) {
                if (!version_respond.load())
                    return std::vector<Message>{};
                Message version;
                version.id = 0x64;
                version.dlc = 1;
                version.data[0] = 123;
                return std::vector<Message>{version};
            });
            transport->on_request(4, [this](const Message &) {
                {
                    std::lock_guard<std::mutex> lock(observed_mutex);
                    queries.fetch_add(1);
                }
                observed.notify_all();
                if (fail.load())
                    throw linkerbot::can::TransportIOError("injected write failure");
                return respond.load() ? jointReplies() : std::vector<Message>{};
            });
            return transport;
        };
        session = recoverable ? Session::create(factory) : Session::own(factory());
    }
    ~ArmRig() {
        session->close();
    }
    LinkerArm::SessionOptions options() const {
        LinkerArm::SessionOptions result;
        result.endpoint = "test-arm";
        result.transmit_gap = 0us;
        return result;
    }
    bool waitForQueries(unsigned count) {
        std::unique_lock<std::mutex> lock(observed_mutex);
        return observed.wait_for(lock, 1s, [&] { return queries.load() >= count; });
    }
};

TEST(Runtime, FreshReadAndCacheHaveDistinctSemantics) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    EXPECT_FALSE(arm.latestSnapshot().valid);
    const auto first = arm.readJointState();
    EXPECT_TRUE(first.valid);
    EXPECT_FLOAT_EQ(first.positions_degrees[0], 10);
    EXPECT_EQ(rig.queries.load(), 1);
    EXPECT_EQ(arm.latestSnapshot().sequence, first.sequence);
    EXPECT_EQ(rig.queries.load(), 1);
    EXPECT_GT(arm.readJointState().sequence, first.sequence);
}

TEST(Runtime, MissingResponseAndWriteFailurePropagate) {
    ArmRig rig;
    rig.respond = false;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    EXPECT_THROW(arm.readJointState(20ms), std::exception);
    rig.fail = true;
    EXPECT_THROW(arm.readJointState(), std::exception);
}

TEST(Runtime, ParameterValidationAndResetInvalidateCache) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    EXPECT_TRUE(arm.readJointState().valid);
    EXPECT_THROW(arm.readJointState(0ms), std::invalid_argument);
    EXPECT_THROW(arm.resetZero(14), std::invalid_argument);
    arm.resetZero(0);
    EXPECT_FALSE(arm.latestSnapshot().valid);
    const auto sent = rig.fake->sent();
    ASSERT_FALSE(sent.empty());
    EXPECT_EQ(sent.back().id, 0xc0);
    EXPECT_EQ(sent.back().dlc, 1);
    EXPECT_EQ(sent.back().data[0], 0);
}

TEST(Runtime, RequestLockWaitIsIncludedInDeadline) {
    ArmRig rig;
    rig.respond = false;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    auto first = std::async(std::launch::async,
                            [&] { EXPECT_THROW(arm.readJointState(500ms), std::exception); });
    ASSERT_TRUE(rig.waitForQueries(1));
    const auto start = std::chrono::steady_clock::now();
    EXPECT_THROW(arm.readJointState(20ms), std::exception);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 300ms);
    EXPECT_EQ(rig.queries.load(), 1);
    first.get();
}

TEST(Runtime, ConcurrentCacheVersionAndReads) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    auto reader = std::async(std::launch::async, [&] {
        for (int i = 0; i < 50; ++i) {
            (void)arm.latestSnapshot();
            (void)arm.getVersion();
            (void)arm.getJointErrorCode();
        }
    });
    for (int i = 0; i < 10; ++i)
        EXPECT_TRUE(arm.readJointState().valid);
    reader.get();
}

TEST(Runtime, DestructorCompletesWithIdleBoundedTransport) {
    ArmRig rig;
    const auto start = std::chrono::steady_clock::now();
    { LinkerArm::LinkerArm arm(rig.session, rig.options()); }
    EXPECT_LT(std::chrono::steady_clock::now() - start, 1s);
    EXPECT_TRUE(rig.session->health().open);
}

TEST(Runtime, BlockingWriteCannotTurnExpiredReadIntoSuccess) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    rig.fake->arm_tx_gate();
    ReleaseGate release{rig.fake};
    auto request = std::async(std::launch::async,
                              [&] { EXPECT_THROW(arm.readJointState(30ms), std::exception); });
    ASSERT_TRUE(rig.fake->wait_until_tx_entered(1s));
    EXPECT_EQ(request.wait_for(300ms), std::future_status::ready);
    rig.fake->release_tx_gate();
    request.get();
}

TEST(Runtime, RejectsUnrepresentableTimeoutBeforeSending) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    EXPECT_THROW(arm.readJointState(std::chrono::milliseconds::max()), std::invalid_argument);
    EXPECT_EQ(rig.queries.load(), 0);
}

TEST(Runtime, QueuedRequestProceedsAfterPrecedingTimeout) {
    ArmRig rig;
    rig.respond = false;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    auto first = std::async(std::launch::async,
                            [&] { EXPECT_THROW(arm.readJointState(100ms), std::exception); });
    ASSERT_TRUE(rig.waitForQueries(1));
    rig.respond = true;
    EXPECT_TRUE(arm.readJointState(1s).valid);
    first.get();
}

TEST(Runtime, FailedWriteReleasesRequestSlot) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    rig.fail = true;
    EXPECT_THROW(arm.readJointState(), std::exception);
    rig.fail = false;
    EXPECT_TRUE(arm.readJointState().valid);
}

TEST(Runtime, DestructionPreservesOtherRoutesAndEndpointsOnSharedSession) {
    ArmRig rig;
    auto other = rig.session->subscribe_routes({{0x300, false}});
    {
        LinkerArm::LinkerArm arm(rig.session, rig.options());
        EXPECT_TRUE(arm.readJointState().valid);
    }
    Message message;
    message.id = 0x300;
    message.dlc = 1;
    message.data[0] = 42;
    rig.fake->inject(message);
    const auto received = other->receive_until(linkerbot::transport::Clock::now() + 1s);
    ASSERT_TRUE(received);
    EXPECT_EQ(received->message.data[0], 42);
    linkerbot::transport::SendOptions options;
    options.endpoint = "other-device";
    options.deadline = linkerbot::transport::Clock::now() + 1s;
    rig.session->send({{message}}, options);
    EXPECT_EQ(rig.fake->close_calls(), 0);
    EXPECT_TRUE(rig.session->health().open);
}

TEST(Runtime, ClosingSharedSessionWakesActiveRead) {
    ArmRig rig;
    rig.respond = false;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    auto request = std::async(std::launch::async,
                              [&] { EXPECT_THROW(arm.readJointState(5s), std::exception); });
    ASSERT_TRUE(rig.waitForQueries(1));
    rig.session->close();
    EXPECT_EQ(request.wait_for(1s), std::future_status::ready);
    request.get();
    EXPECT_FALSE(arm.latestSnapshot().valid);
}

TEST(Runtime, ConflictingDeviceRoutesFailWithoutClosingSharedSession) {
    ArmRig rig;
    LinkerArm::LinkerArm first(rig.session, rig.options());
    auto second_options = rig.options();
    second_options.endpoint = "second-arm";
    EXPECT_THROW(LinkerArm::LinkerArm second(rig.session, second_options), std::exception);
    EXPECT_TRUE(rig.session->health().open);
    EXPECT_TRUE(first.readJointState().valid);
}

TEST(Runtime, QueuedTimedOutRequestIsCancelledBeforeReachingBackend) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    rig.fake->arm_tx_gate();
    ReleaseGate release{rig.fake};
    Message blocker;
    blocker.id = 0x300;
    blocker.dlc = 1;
    linkerbot::transport::SendOptions options;
    options.endpoint = "other-device";
    auto active = rig.session->submit({{blocker}}, options);
    ASSERT_TRUE(rig.fake->wait_until_tx_entered(1s));
    EXPECT_THROW(arm.readJointState(20ms), std::exception);
    rig.fake->release_tx_gate();
    active.wait();
    // A subsequent successful query is a FIFO barrier after the cancelled item.
    EXPECT_TRUE(arm.readJointState(1s).valid);
    EXPECT_EQ(rig.queries.load(), 1);
}

} // namespace

namespace {
TEST(Runtime, RecoveryRevalidatesVersionAndDoesNotReplayCalibration) {
    std::mutex recorded_mutex;
    std::vector<uint32_t> recorded;
    auto session = Session::create([&] {
        auto fake = std::make_unique<FakeTransport>();
        fake->on_request(0x64, [&](const Message &) {
            {
                std::lock_guard<std::mutex> lock(recorded_mutex);
                recorded.push_back(0x64);
            }
            Message version;
            version.id = 0x64;
            version.dlc = 1;
            version.data[0] = 123;
            return std::vector<Message>{version};
        });
        fake->on_request(0x04, [&](const Message &) {
            std::lock_guard<std::mutex> lock(recorded_mutex);
            recorded.push_back(0x04);
            return jointReplies();
        });
        fake->on_request(0xc1, [&](const Message &) {
            std::lock_guard<std::mutex> lock(recorded_mutex);
            recorded.push_back(0xc1);
            return std::vector<Message>{};
        });
        return fake;
    });
    LinkerArm::SessionOptions options;
    options.own_session = true;
    options.transmit_gap = 0us;
    LinkerArm::LinkerArm arm(session, options);
    const auto before = arm.readJointState();
    arm.resetZero();
    const auto generation = arm.recover(1s);
    EXPECT_GT(generation, before.generation);
    EXPECT_FALSE(arm.latestSnapshot().valid);
    const auto after = arm.readJointState();
    EXPECT_EQ(after.generation, generation);
    std::lock_guard<std::mutex> lock(recorded_mutex);
    EXPECT_EQ(std::count(recorded.begin(), recorded.end(), 0xc1), 1);
    EXPECT_GE(std::count(recorded.begin(), recorded.end(), 0x64), 2);
}

TEST(Runtime, BorrowedSessionCannotBeRecoveredByIndividualArm) {
    ArmRig rig;
    LinkerArm::LinkerArm arm(rig.session, rig.options());
    const auto generation = rig.session->health().generation;
    EXPECT_THROW(arm.recover(1s), std::exception);
    EXPECT_EQ(rig.session->health().generation, generation);
    EXPECT_TRUE(arm.readJointState().valid);
}
} // namespace

namespace {
TEST(Runtime, ExternalRecoveryInvalidatesOldSnapshotAndAllowsReconstruction) {
    ArmRig rig(true);
    {
        LinkerArm::LinkerArm arm(rig.session, rig.options());
        const auto before = arm.readJointState();
        const auto generation = rig.session->recover(rig.session->health().generation);
        EXPECT_GT(generation, before.generation);
        EXPECT_FALSE(arm.latestSnapshot().valid);
        EXPECT_THROW(arm.readJointState(), std::exception);
    }
    LinkerArm::LinkerArm replacement(rig.session, rig.options());
    const auto fresh = replacement.readJointState();
    EXPECT_TRUE(fresh.valid);
    EXPECT_EQ(fresh.generation, rig.session->health().generation);
}

TEST(Runtime, RecoveryWithoutIdentityReplyLeavesReadsDisabled) {
    ArmRig rig(true);
    auto options = rig.options();
    options.own_session = true;
    LinkerArm::LinkerArm arm(rig.session, options);
    EXPECT_TRUE(arm.readJointState().valid);
    rig.version_respond = false;
    EXPECT_THROW(arm.recover(50ms), std::exception);
    EXPECT_FALSE(arm.latestSnapshot().valid);
    const auto before = rig.queries.load();
    EXPECT_THROW(arm.readJointState(), std::exception);
    EXPECT_EQ(rig.queries.load(), before);
}

TEST(Runtime, ConflictingSameEndpointDoesNotCancelExistingQueuedRequest) {
    ArmRig rig;
    LinkerArm::LinkerArm first(rig.session, rig.options());
    rig.fake->arm_tx_gate();
    ReleaseGate release{rig.fake};
    Message blocker;
    blocker.id = 0x300;
    auto blocking_receipt = rig.session->submit({{blocker}});
    ASSERT_TRUE(rig.fake->wait_until_tx_entered(1s));
    auto waiting = std::async(std::launch::async, [&] { return first.readJointState(2s); });
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (rig.session->health().pending_tx == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);
    ASSERT_GT(rig.session->health().pending_tx, 0u);
    EXPECT_THROW(LinkerArm::LinkerArm second(rig.session, rig.options()), std::exception);
    rig.fake->release_tx_gate();
    blocking_receipt.wait();
    EXPECT_TRUE(waiting.get().valid);
}

TEST(Runtime, InboxOverflowStressInvalidatesCommittedSnapshot) {
    ArmRig rig;
    rig.respond = false;
    auto options = rig.options();
    options.receive_capacity = 1;
    LinkerArm::LinkerArm arm(rig.session, options);
    auto fence = rig.session->subscribe_routes({{0x300, false}}, 1);
    const auto drain = [&] {
        Message barrier;
        barrier.id = 0x300;
        rig.fake->inject(barrier);
        if (!fence->receive_until(std::chrono::steady_clock::now() + 1s))
            return false;
        const auto deadline = std::chrono::steady_clock::now() + 1s;
        while (arm.routeHealth().pending != 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(1ms);
        return arm.routeHealth().pending == 0;
    };
    ASSERT_TRUE(drain());
    auto initial = std::async(std::launch::async, [&] { return arm.readJointState(2s); });
    ASSERT_TRUE(rig.waitForQueries(1));
    for (const auto &frame : jointReplies()) {
        rig.fake->inject(frame);
        ASSERT_TRUE(drain());
    }
    ASSERT_TRUE(initial.get().valid);
    const auto previous = arm.latestSnapshot();
    ASSERT_TRUE(previous.valid);
    const auto overflow_before = arm.routeHealth().overflow;
    // Bounded pressure drives the actual Session queue; the fence proves the
    // complete burst has reached route dispatch before inspecting its counters.
    const auto burst_frame = jointReplies()[0];
    for (int attempt = 0; attempt < 8 && arm.routeHealth().overflow == overflow_before; ++attempt) {
        for (int i = 0; i < 4096; ++i)
            rig.fake->inject(burst_frame);
        ASSERT_TRUE(drain());
    }
    ASSERT_GT(arm.routeHealth().overflow, overflow_before);
    EXPECT_FALSE(arm.latestSnapshot().valid);
}
} // namespace

namespace {
TEST(Runtime, InvalidSessionOptionsFailBeforeEnqueueOrRouteOwnership) {
    ArmRig rig;
    auto options = rig.options();
    EXPECT_THROW(LinkerArm::LinkerArm(std::shared_ptr<Session>{}, options), std::invalid_argument);
    options.receive_capacity = 0;
    EXPECT_THROW(LinkerArm::LinkerArm(rig.session, options), std::invalid_argument);
    options = rig.options();
    options.command_timeout = 0ms;
    EXPECT_THROW(LinkerArm::LinkerArm(rig.session, options), std::invalid_argument);
    options = rig.options();
    options.transmit_gap = -1us;
    EXPECT_THROW(LinkerArm::LinkerArm(rig.session, options), std::invalid_argument);
    options = rig.options();
    options.transmit_gap = std::chrono::microseconds::max();
    EXPECT_THROW(LinkerArm::LinkerArm(rig.session, options), std::invalid_argument);
    EXPECT_TRUE(rig.fake->sent().empty());
    LinkerArm::LinkerArm valid(rig.session, rig.options());
    EXPECT_TRUE(valid.readJointState().valid);
}
} // namespace
