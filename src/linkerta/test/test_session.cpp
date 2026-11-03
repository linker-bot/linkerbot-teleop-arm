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
Message frame(uint32_t id, uint8_t value = 0) {
    Message message;
    message.id = id;
    message.dlc = 1;
    message.data[0] = value;
    return message;
}
struct SessionRig {
    FakeTransport *fake;
    std::shared_ptr<Session> session;
    explicit SessionRig(size_t tx_capacity = 4) {
        session = Session::create(
            [this] {
                auto transport = std::make_unique<FakeTransport>();
                fake = transport.get();
                return transport;
            },
            tx_capacity);
    }
    ~SessionRig() {
        session->close();
    }
};
struct ReleaseGate {
    FakeTransport *fake;
    ~ReleaseGate() {
        fake->release_tx_gate();
    }
};

TEST(SessionIntegration, ExactRoutesKeepOtherDevicesAndExtendedFramesSeparate) {
    SessionRig rig;
    auto arm = rig.session->subscribe_routes(
        {{0x64, false}, {0x65, false}, {0x66, false}, {0x67, false}, {0x68, false}}, 8);
    auto other = rig.session->subscribe_routes({{0x300, false}}, 4);
    auto extended = rig.session->subscribe_routes({{0x65, true}}, 4);
    auto ext = frame(0x65, 8);
    ext.is_extended = true;
    rig.fake->inject(ext);
    rig.fake->inject(frame(0x300, 9));
    rig.fake->inject(frame(0x65, 10));
    const auto a = arm->receive_until(linkerbot::transport::Clock::now() + 1s);
    const auto o = other->receive_until(linkerbot::transport::Clock::now() + 1s);
    const auto e = extended->receive_until(linkerbot::transport::Clock::now() + 1s);
    ASSERT_TRUE(a);
    ASSERT_TRUE(o);
    ASSERT_TRUE(e);
    EXPECT_EQ(a->message.data[0], 10);
    EXPECT_EQ(o->message.data[0], 9);
    EXPECT_EQ(e->message.data[0], 8);
    EXPECT_THROW(rig.session->subscribe_routes({{0x65, false}}), linkerbot::can::ValidationError);
}

TEST(SessionIntegration, BoundedInboxReportsOverflowInsteadOfReplacingOldFrames) {
    SessionRig rig;
    auto arm = rig.session->subscribe_routes({{0x65, false}, {0x66, false}}, 1);
    auto barrier = rig.session->subscribe_routes({{0x300, false}}, 1);
    rig.fake->inject(frame(0x65, 1));
    rig.fake->inject(frame(0x66, 2));
    rig.fake->inject(frame(0x65, 3));
    rig.fake->inject(frame(0x300));
    ASSERT_TRUE(barrier->receive_until(linkerbot::transport::Clock::now() + 1s));
    EXPECT_EQ(arm->health().overflow, 2);
    EXPECT_EQ(rig.session->health().overflows, 2);
    const auto first = arm->try_receive();
    ASSERT_TRUE(first);
    EXPECT_EQ(first->message.data[0], 1);
    EXPECT_FALSE(arm->try_receive());
}

TEST(SessionIntegration, FullQueueAndCancellationDoNotReplayCommands) {
    SessionRig rig(1);
    rig.fake->arm_tx_gate();
    ReleaseGate release{rig.fake};
    auto active = rig.session->submit({{frame(0x300, 1)}});
    ASSERT_TRUE(rig.fake->wait_until_tx_entered(1s));
    linkerbot::transport::CancellationSource cancellation;
    linkerbot::transport::SendOptions options;
    options.endpoint = "arm";
    options.cancellation = cancellation.token();
    auto queued = rig.session->submit({{frame(0xc1)}}, options);
    EXPECT_THROW(rig.session->submit({{frame(0x04)}}), linkerbot::can::QueueFullError);
    cancellation.cancel();
    rig.fake->release_tx_gate();
    active.wait();
    EXPECT_THROW((void)queued.wait_until(linkerbot::transport::Clock::now() + 1s),
                 linkerbot::transport::CancelledError);
    const auto sent = rig.fake->sent();
    ASSERT_EQ(sent.size(), 1);
    EXPECT_EQ(sent[0].id, 0x300);
}

TEST(SessionIntegration, ClosingInboxWakesReceiverWithoutClosingSession) {
    SessionRig rig;
    auto inbox = rig.session->subscribe_routes({{0x65, false}});
    auto waiting = std::async(std::launch::async, [&] {
        EXPECT_THROW(inbox->receive_until(linkerbot::transport::Clock::now() + 5s),
                     linkerbot::can::ClosedError);
    });
    inbox->close();
    EXPECT_EQ(waiting.wait_for(1s), std::future_status::ready);
    waiting.get();
    EXPECT_TRUE(rig.session->health().open);
    EXPECT_NO_THROW(rig.session->subscribe_routes({{0x65, false}}));
}

struct DiagnosticGate {
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<unsigned> inspections{0};
    bool entered = false;
    bool released = false;
    void release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        cv.notify_all();
    }
};

class GatedTransmitError : public linkerbot::can::TransportIOError {
public:
    explicit GatedTransmitError(std::shared_ptr<DiagnosticGate> gate)
        : TransportIOError("gated transmit failure"), gate_(std::move(gate)) {}
    [[nodiscard]] const char *what() const noexcept override {
        // The dispatcher inspects first; the Session worker inspects second.
        if (gate_->inspections.fetch_add(1) == 1) {
            std::unique_lock<std::mutex> lock(gate_->mutex);
            gate_->entered = true;
            gate_->cv.notify_all();
            gate_->cv.wait(lock, [&] { return gate_->released; });
        }
        return TransportIOError::what();
    }

private:
    std::shared_ptr<DiagnosticGate> gate_;
};

TEST(SessionIntegration, FailureReceiptWaitsForExceptionDiagnostics) {
    auto gate = std::make_shared<DiagnosticGate>();
    auto fake = std::make_unique<FakeTransport>();
    fake->on_request(
        0x300, [gate](const Message &) -> std::vector<Message> { throw GatedTransmitError(gate); });
    auto session = Session::own(std::move(fake));
    struct ReleaseDiagnostic {
        std::shared_ptr<DiagnosticGate> gate;
        ~ReleaseDiagnostic() {
            gate->release();
        }
    } release{gate};
    auto receipt = session->submit({{frame(0x300)}});
    {
        std::unique_lock<std::mutex> lock(gate->mutex);
        ASSERT_TRUE(gate->cv.wait_for(lock, 1s, [&] { return gate->entered; }));
    }
    // The caller must not rethrow/destroy the exception while the worker still
    // reads it for diagnostics. This also checks installed dependency packages.
    EXPECT_FALSE(receipt.completed());
    gate->release();
    EXPECT_THROW((void)receipt.wait_until(linkerbot::transport::Clock::now() + 1s),
                 linkerbot::can::TransportIOError);
    EXPECT_EQ(session->health().last_error, "gated transmit failure");
    session->close();
}
} // namespace
