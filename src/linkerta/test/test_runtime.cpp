#include "LinkerArm.h"
#include <deque>
#include <future>
#include <gtest/gtest.h>
using namespace std::chrono_literals;
namespace {
class Fake final : public Communication::ICanBus {
public:
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<CANFrame> frames;
    bool respond = true, fail = false;
    std::chrono::milliseconds write_delay{0};
    int queries = 0;
    void send(const std::vector<uint8_t> &data, uint32_t id, bool = false) override {
        std::unique_lock<std::mutex> lock(mutex);
        if (data.size() > 8)
            throw std::invalid_argument("large");
        if (fail)
            throw std::runtime_error("injected write failure");
        if (id == 4) {
            ++queries;
            cv.notify_all();
            if (respond) {
                for (int i = 0; i < 4; ++i) {
                    CANFrame f{};
                    f.can_id = 0x65 + i;
                    f.can_dlc = i % 2 ? 7 : 8;
                    f.data[1] = 100;
                    frames.push_back(f);
                }
                cv.notify_all();
            }
            const auto delay = write_delay;
            lock.unlock();
            std::this_thread::sleep_for(delay);
        }
    }
    CANFrame recv(uint32_t = 0xFF) override {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, 5ms, [&] { return !frames.empty(); });
        if (frames.empty())
            return {};
        auto f = frames.front();
        frames.pop_front();
        return f;
    }
    void inject(CANFrame f) {
        std::lock_guard<std::mutex> lock(mutex);
        frames.push_back(f);
        cv.notify_one();
    }
};
TEST(Runtime, FreshReadAndCacheHaveDistinctSemantics) {
    auto f = std::make_unique<Fake>();
    auto *raw = f.get();
    LinkerArm::LinkerArm arm(std::move(f));
    EXPECT_FALSE(arm.latestSnapshot().valid);
    auto s = arm.readJointState();
    EXPECT_TRUE(s.valid);
    EXPECT_FLOAT_EQ(s.positions_degrees[0], 10);
    EXPECT_EQ(raw->queries, 1);
    EXPECT_EQ(arm.latestSnapshot().sequence, s.sequence);
    EXPECT_EQ(raw->queries, 1);
    EXPECT_GT(arm.readJointState().sequence, s.sequence);
}
TEST(Runtime, MissingResponseAndWriteFailurePropagate) {
    auto f = std::make_unique<Fake>();
    auto *raw = f.get();
    f->respond = false;
    LinkerArm::LinkerArm arm(std::move(f));
    EXPECT_THROW(arm.readJointState(15ms), std::runtime_error);
    {
        std::lock_guard<std::mutex> lock(raw->mutex);
        raw->fail = true;
    }
    EXPECT_THROW(arm.readJointState(), std::runtime_error);
}
TEST(Runtime, ParameterValidationAndResetInvalidateCache) {
    auto f = std::make_unique<Fake>();
    LinkerArm::LinkerArm arm(std::move(f));
    EXPECT_TRUE(arm.readJointState().valid);
    EXPECT_THROW(arm.readJointState(0ms), std::invalid_argument);
    EXPECT_THROW(arm.resetZero(14), std::invalid_argument);
    arm.resetZero(0);
    EXPECT_FALSE(arm.latestSnapshot().valid);
}
TEST(Runtime, RequestLockWaitIsIncludedInDeadline) {
    auto f = std::make_unique<Fake>();
    f->respond = false;
    auto *raw = f.get();
    LinkerArm::LinkerArm arm(std::move(f));
    auto first = std::async(std::launch::async,
                            [&] { EXPECT_THROW(arm.readJointState(500ms), std::runtime_error); });
    {
        std::unique_lock<std::mutex> lock(raw->mutex);
        ASSERT_TRUE(raw->cv.wait_for(lock, 1s, [&] { return raw->queries == 1; }));
    }
    auto start = std::chrono::steady_clock::now();
    EXPECT_THROW(arm.readJointState(15ms), std::runtime_error);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 300ms);
    first.get();
}
TEST(Runtime, ConcurrentCacheVersionAndReads) {
    auto f = std::make_unique<Fake>();
    auto *raw = f.get();
    LinkerArm::LinkerArm arm(std::move(f));
    CANFrame version{};
    version.can_id = 0x64;
    version.can_dlc = 1;
    version.data[0] = 123;
    raw->inject(version);
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
    auto start = std::chrono::steady_clock::now();
    { LinkerArm::LinkerArm arm(std::make_unique<Fake>()); }
    EXPECT_LT(std::chrono::steady_clock::now() - start, 200ms);
}
} // namespace

TEST(Runtime, BlockingWriteCannotTurnExpiredReadIntoSuccess) {
    auto fake = std::make_unique<Fake>();
    fake->write_delay = 60ms;
    LinkerArm::LinkerArm arm(std::move(fake));
    EXPECT_THROW(arm.readJointState(15ms), std::runtime_error);
}

TEST(Runtime, RejectsUnrepresentableTimeoutBeforeSending) {
    auto fake = std::make_unique<Fake>();
    auto *raw = fake.get();
    LinkerArm::LinkerArm arm(std::move(fake));
    EXPECT_THROW(arm.readJointState(std::chrono::milliseconds::max()), std::invalid_argument);
    EXPECT_EQ(raw->queries, 0);
}
