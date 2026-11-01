#include "timed_output/timed_output.hpp"

#include <gtest/gtest.h>

namespace {

timed_output::SampleTime fresh(timed_output::Milliseconds now) {
    return {now, true};
}

TEST(TimedOutput, StartsOffAndAcceptsLocalRequest) {
    timed_output::TimedOutput output({1000, 200});
    EXPECT_FALSE(output.update(false, fresh(0), 0));
    EXPECT_TRUE(output.update(true, fresh(100), 100));
    EXPECT_FALSE(output.update(false, fresh(150), 150));
}

TEST(TimedOutput, RepeatedRequestsDoNotExtendDeadline) {
    timed_output::TimedOutput output({1000, 200});
    ASSERT_TRUE(output.update(true, fresh(10), 10));
    EXPECT_TRUE(output.update(true, fresh(1009), 1009));
    EXPECT_FALSE(output.update(true, fresh(1010), 1010));
    EXPECT_TRUE(output.fault());
    EXPECT_FALSE(output.update(true, fresh(1011), 1011));
}

TEST(TimedOutput, StaleReadingStopsAndLatchesFault) {
    timed_output::TimedOutput output({1000, 200});
    ASSERT_TRUE(output.update(true, fresh(0), 0));
    EXPECT_FALSE(output.update(true, fresh(0), 201));
    EXPECT_FALSE(output.update(true, fresh(202), 202));
    ASSERT_TRUE(output.reset_fault());
    EXPECT_TRUE(output.update(true, fresh(203), 203));
}

TEST(TimedOutput, InvalidAndFutureSamplesFailClosed) {
    timed_output::TimedOutput invalid({1000, 200});
    EXPECT_FALSE(invalid.update(true, {0, false}, 0));
    EXPECT_TRUE(invalid.fault());
    timed_output::TimedOutput future({1000, 200});
    EXPECT_FALSE(future.update(true, fresh(10), 0));
    EXPECT_TRUE(future.fault());
}

TEST(TimedOutput, ClockRegressionFailsClosed) {
    timed_output::TimedOutput output({1000, 200});
    ASSERT_TRUE(output.update(true, fresh(100), 100));
    EXPECT_FALSE(output.update(true, fresh(99), 99));
    EXPECT_TRUE(output.fault());
}

TEST(TimedOutput, FaultCannotBeResetWhileRunning) {
    timed_output::TimedOutput output({1000, 200});
    ASSERT_TRUE(output.update(true, fresh(0), 0));
    EXPECT_FALSE(output.reset_fault());
}

TEST(TimedOutput, InvalidPolicyFailsClosed) {
    timed_output::TimedOutput output({0, 200});
    EXPECT_FALSE(output.update(true, fresh(0), 0));
    EXPECT_TRUE(output.fault());
}

TEST(TimedOutput, SampleAgeBoundaryIsInclusive) {
    timed_output::TimedOutput output({1000, 200});
    EXPECT_TRUE(output.update(true, fresh(0), 200));
    EXPECT_FALSE(output.update(true, fresh(0), 201));
}

} // namespace
