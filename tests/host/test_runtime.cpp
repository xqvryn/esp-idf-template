#include "runtime/loop_budget.hpp"
#include "runtime/mailbox.hpp"
#include "runtime/scripted_clock.hpp"
#include "runtime/scripted_service.hpp"
#include "runtime/spsc_queue.hpp"
#include "runtime/task_spec.hpp"

#include <atomic>
#include <cstdint>
#include <gtest/gtest.h>
#include <limits>
#include <thread>

namespace {

TEST(TaskSpec, RejectsEmptyNameBadCoreAndPriority) {
    EXPECT_FALSE(runtime::task_spec_valid({}, 5));
    EXPECT_FALSE(runtime::task_spec_valid({"", 2048, 1, -1}, 5));
    EXPECT_FALSE(runtime::task_spec_valid({"task", 2048, 1, 2}, 5));
    EXPECT_FALSE(runtime::task_spec_valid({"task", 1024, 1, -1}, 5));
    EXPECT_FALSE(runtime::task_spec_valid({"task", 2048, 5, -1}, 5));
    EXPECT_FALSE(runtime::task_spec_valid({"task", 2048, 1, 0}, 0));
    EXPECT_TRUE(runtime::task_spec_valid({"task", 2048, 1, -1}, 5));
    EXPECT_TRUE(runtime::task_spec_valid({"task", 4096, 0, 1}, 5));
}

TEST(ScriptedService, StartStopAndFailedStart) {
    runtime::ScriptedService service;
    EXPECT_EQ(service.health().state, runtime::ServiceState::stopped);
    EXPECT_TRUE(service.start());
    EXPECT_EQ(service.health().state, runtime::ServiceState::running);
    service.stop();
    EXPECT_EQ(service.health().state, runtime::ServiceState::stopped);
    EXPECT_EQ(service.start_calls(), 1);
    EXPECT_EQ(service.stop_calls(), 1);
    service.fail_next_start(9);
    EXPECT_FALSE(service.start());
    EXPECT_EQ(service.health().state, runtime::ServiceState::failed);
    EXPECT_EQ(service.health().fault_code, 9U);
}

TEST(LoopBudget, OnTimeWakeDoesNotCountAMiss) {
    runtime::ScriptedClock clock;
    runtime::LoopBudget budget;
    ASSERT_TRUE(budget.arm(clock.now_us, 1000));
    clock.advance(1000);
    budget.observe(clock.now_us);
    EXPECT_EQ(budget.last_jitter_us(), 0);
    EXPECT_EQ(budget.max_jitter_us(), 0);
    EXPECT_EQ(budget.deadline_misses(), 0U);
    EXPECT_FALSE(budget.clock_fault());
}

TEST(LoopBudget, LateWakeCountsSkippedDeadlines) {
    runtime::LoopBudget budget;
    ASSERT_TRUE(budget.arm(0, 1000));
    budget.observe(1500);
    EXPECT_EQ(budget.last_jitter_us(), 500);
    EXPECT_EQ(budget.max_jitter_us(), 500);
    EXPECT_EQ(budget.deadline_misses(), 1U);
    budget.observe(3000);
    EXPECT_EQ(budget.deadline_misses(), 2U);
    EXPECT_EQ(budget.max_jitter_us(), 1000);
    budget.observe(4000);
    EXPECT_EQ(budget.deadline_misses(), 2U);
    EXPECT_EQ(budget.last_jitter_us(), 0);
}

TEST(LoopBudget, EarlyWakeIsNotAMissAndBackwardClockFaults) {
    runtime::LoopBudget budget;
    ASSERT_TRUE(budget.arm(0, 1000));
    budget.observe(500);
    EXPECT_EQ(budget.last_jitter_us(), -500);
    EXPECT_EQ(budget.deadline_misses(), 0U);
    EXPECT_EQ(budget.max_jitter_us(), 0);
    budget.observe(400);
    EXPECT_TRUE(budget.clock_fault());
    EXPECT_EQ(budget.deadline_misses(), 0U);
}

TEST(LoopBudget, RejectsAPeriodThatOverflowsTheDeadline) {
    runtime::LoopBudget budget;
    EXPECT_FALSE(budget.arm(std::numeric_limits<std::int64_t>::max() - 10, 100));
    EXPECT_FALSE(budget.armed());
    EXPECT_FALSE(budget.arm(-1, 1000));
    EXPECT_FALSE(budget.arm(0, 0));
}

TEST(SpscQueue, FailsWhenFullAndKeepsOrder) {
    runtime::SpscQueue<int, 4> queue;
    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_FALSE(queue.try_push(4));
    int value = 0;
    ASSERT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(queue.try_push(4));
    ASSERT_TRUE(queue.try_pop(value));
    EXPECT_EQ(value, 2);
}

TEST(SpscQueue, ProducerAndConsumerAgree) {
    runtime::SpscQueue<int, 8> queue;
    constexpr int count = 10000;
    std::atomic<bool> failed{false};
    std::thread producer([&] {
        for (int value = 0; value < count; ++value) {
            while (!queue.try_push(value)) {
            }
        }
    });
    std::thread consumer([&] {
        for (int expected = 0; expected < count; ++expected) {
            int value = -1;
            while (!queue.try_pop(value)) {
            }
            if (value != expected) {
                failed.store(true);
            }
        }
    });
    producer.join();
    consumer.join();
    EXPECT_FALSE(failed.load());
}

TEST(LatestMailbox, ReplacesThePreviousValue) {
    runtime::LatestMailbox<std::int32_t> box;
    std::int32_t value = 0;
    EXPECT_FALSE(box.load(value));
    box.publish(4);
    ASSERT_TRUE(box.load(value));
    EXPECT_EQ(value, 4);
    box.publish(9);
    ASSERT_TRUE(box.load(value));
    EXPECT_EQ(value, 9);
}

struct WideSample {
    std::uint16_t id;
    float value;
    std::int64_t time_us;
};

TEST(LatestMailbox, CopiesAMultiWordSample) {
    runtime::LatestMailbox<WideSample> box;
    box.publish(WideSample{7, 1.5F, 42});
    WideSample sample{};
    ASSERT_TRUE(box.load(sample));
    EXPECT_EQ(sample.id, 7);
    EXPECT_FLOAT_EQ(sample.value, 1.5F);
    EXPECT_EQ(sample.time_us, 42);
}

} // namespace
