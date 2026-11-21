#include "channels/record.hpp"
#include "channels/sample.hpp"

#include <gtest/gtest.h>

namespace {

TEST(RecordChannel, DropsWhenFullAndKeepsEarlierRecords) {
    channels::RecordChannel<4> channel;
    for (std::uint16_t code = 1; code <= 3; ++code) {
        EXPECT_TRUE(channel.emit(channels::Record{code, channels::Severity::info, code, 0, 0}));
    }
    EXPECT_FALSE(channel.emit(channels::Record{4, channels::Severity::warn, 4, 0, 0}));
    EXPECT_EQ(channel.dropped(), 1U);
    channels::Record record{};
    ASSERT_TRUE(channel.take(record));
    EXPECT_EQ(record.code, 1);
    EXPECT_TRUE(channel.emit(channels::Record{5, channels::Severity::error, 5, 1.5F, -2.0F}));
    ASSERT_TRUE(channel.take(record));
    EXPECT_EQ(record.code, 2);
}

TEST(SampleMailbox, KeepsOnlyTheLatestTelemetry) {
    channels::SampleMailbox box;
    channels::Sample sample{};
    EXPECT_FALSE(box.load(sample));
    box.publish(channels::Sample{3, 9.0F, 100});
    box.publish(channels::Sample{3, 4.0F, 200});
    ASSERT_TRUE(box.load(sample));
    EXPECT_EQ(sample.id, 3);
    EXPECT_FLOAT_EQ(sample.value, 4.0F);
    EXPECT_EQ(sample.time_us, 200);
}

} // namespace
