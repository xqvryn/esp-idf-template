#include "parameters/registry.hpp"
#include "parameters/store.hpp"
#include "parameters/text.hpp"

#include <gtest/gtest.h>

namespace {

parameters::Registry sample_registry() {
    parameters::Registry registry;
    EXPECT_TRUE(registry.define_bool("armed", false));
    EXPECT_TRUE(registry.define_int("rate_p", 10, 0, 100));
    EXPECT_TRUE(registry.define_real("alpha", 0.98F, 0.0F, 1.0F));
    return registry;
}

TEST(Registry, RejectsDuplicatesAndOutOfRangeValues) {
    auto registry = sample_registry();
    EXPECT_FALSE(registry.define_int("rate_p", 1, 0, 2));
    EXPECT_FALSE(registry.define_int("bad", 5, 0, 1));
    EXPECT_FALSE(registry.define_real("nan", 0.0F / 0.0F, 0.0F, 1.0F));
    std::int32_t rate = 0;
    ASSERT_TRUE(registry.get_int("rate_p", rate));
    EXPECT_EQ(rate, 10);
    EXPECT_FALSE(registry.set_int("rate_p", 101));
    ASSERT_TRUE(registry.get_int("rate_p", rate));
    EXPECT_EQ(rate, 10);
    EXPECT_TRUE(registry.set_int("rate_p", 20));
    EXPECT_FALSE(registry.set_bool("rate_p", true));
}

TEST(ParameterText, CommandAssignmentHonorsRange) {
    auto registry = sample_registry();
    EXPECT_EQ(parameters::write_text(registry, "missing", "1"),
              parameters::ParameterWrite::unknown);
    EXPECT_EQ(parameters::write_text(registry, "armed", "true"),
              parameters::ParameterWrite::applied);
    bool armed = false;
    ASSERT_TRUE(registry.get_bool("armed", armed));
    EXPECT_TRUE(armed);
    EXPECT_EQ(parameters::write_text(registry, "armed", "yes"),
              parameters::ParameterWrite::rejected);
    EXPECT_EQ(parameters::write_text(registry, "rate_p", "100"),
              parameters::ParameterWrite::applied);
    EXPECT_EQ(parameters::write_text(registry, "rate_p", "101"),
              parameters::ParameterWrite::rejected);
    EXPECT_EQ(parameters::write_text(registry, "rate_p", "10x"),
              parameters::ParameterWrite::rejected);
    EXPECT_EQ(parameters::write_text(registry, "alpha", "0.5"),
              parameters::ParameterWrite::applied);
    EXPECT_EQ(parameters::write_text(registry, "alpha", "1.5"),
              parameters::ParameterWrite::rejected);
    float alpha = 0;
    ASSERT_TRUE(registry.get_real("alpha", alpha));
    EXPECT_FLOAT_EQ(alpha, 0.5F);
}

TEST(ParameterStore, RoundTripKeepsValuesAndRejectsACorruptBlob) {
    auto registry = sample_registry();
    ASSERT_TRUE(registry.set_bool("armed", true));
    ASSERT_TRUE(registry.set_int("rate_p", 40));
    parameters::MemoryParameterStore store;
    ASSERT_TRUE(parameters::save_parameters(registry, store));
    parameters::Registry restored;
    ASSERT_TRUE(restored.define_bool("armed", false));
    ASSERT_TRUE(restored.define_int("rate_p", 10, 0, 100));
    ASSERT_TRUE(restored.define_real("alpha", 0.98F, 0.0F, 1.0F));
    ASSERT_TRUE(parameters::load_parameters(restored, store));
    bool armed = false;
    std::int32_t rate = 0;
    ASSERT_TRUE(restored.get_bool("armed", armed));
    ASSERT_TRUE(restored.get_int("rate_p", rate));
    EXPECT_TRUE(armed);
    EXPECT_EQ(rate, 40);

    std::uint8_t blob[parameters::max_blob_bytes];
    const auto size = registry.encode(blob);
    ASSERT_GT(size, 4U);
    blob[0] = 9;
    parameters::MemoryParameterStore corrupt;
    ASSERT_TRUE(corrupt.store(std::span<const std::uint8_t>(blob, size)));
    EXPECT_FALSE(parameters::load_parameters(restored, corrupt));
    ASSERT_TRUE(restored.get_int("rate_p", rate));
    EXPECT_EQ(rate, 40);
}

TEST(ParameterStore, EmptyStoreLeavesDefaults) {
    parameters::MemoryParameterStore store;
    auto registry = sample_registry();
    ASSERT_TRUE(parameters::load_parameters(registry, store));
    bool armed = true;
    ASSERT_TRUE(registry.get_bool("armed", armed));
    EXPECT_FALSE(armed);
}

} // namespace
