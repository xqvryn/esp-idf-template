#include "flight/controller.hpp"
#include "parameters/text.hpp"

#include <cmath>
#include <gtest/gtest.h>

namespace {

struct Body {
    float roll{0};
    float pitch{0};
    float yaw{0};
    float roll_rate{0};
    float pitch_rate{0};
    float yaw_rate{0};
};

motion::ImuSample sense(const Body& body) {
    motion::ImuSample sample;
    sample.accel_m_s2[0] = -std::sin(body.pitch) * 9.80665F;
    sample.accel_m_s2[1] = std::sin(body.roll) * std::cos(body.pitch) * 9.80665F;
    sample.accel_m_s2[2] = std::cos(body.roll) * std::cos(body.pitch) * 9.80665F;
    sample.gyro_rad_s[0] = body.roll_rate;
    sample.gyro_rad_s[1] = body.pitch_rate;
    sample.gyro_rad_s[2] = body.yaw_rate;
    sample.valid = true;
    return sample;
}

void plant(Body& body, const flight::Motors& motors, float dt) {
    constexpr float arm = 0.2F;
    constexpr float inertia = 0.05F;
    const float left = motors.command[2] + motors.command[3];
    const float right = motors.command[0] + motors.command[1];
    const float front = motors.command[0] + motors.command[3];
    const float rear = motors.command[1] + motors.command[2];
    const float clockwise = motors.command[1] + motors.command[3];
    const float counter = motors.command[0] + motors.command[2];
    body.roll_rate += ((left - right) * arm / inertia) * dt;
    body.pitch_rate += ((front - rear) * arm / inertia) * dt;
    body.yaw_rate += ((clockwise - counter) * arm / inertia) * dt;
    body.roll += body.roll_rate * dt;
    body.pitch += body.pitch_rate * dt;
    body.yaw += body.yaw_rate * dt;
}

flight::Pilot armed_pilot() {
    flight::Pilot pilot;
    pilot.arm_switch = true;
    pilot.throttle = 0.45F;
    pilot.battery_v = 12.0F;
    return pilot;
}

void arm(flight::Vehicle& vehicle, const flight::Gains& gains, const flight::Limits& limits) {
    flight::Pilot pilot;
    pilot.arm_switch = true;
    pilot.throttle = 0;
    pilot.battery_v = 12.0F;
    Body body;
    flight::step_vehicle(vehicle, pilot, gains, limits, sense(body), 0.001F);
}

TEST(Mixer, PositiveRollRaisesTheLeftMotors) {
    flight::Motors motors;
    flight::mix_quad_x(0.5F, 0.1F, 0, 0, motors);
    EXPECT_GT(motors.command[2], motors.command[0]);
    EXPECT_GT(motors.command[3], motors.command[1]);
    flight::mix_quad_x(0, 0, 0, 0, motors);
    EXPECT_FLOAT_EQ(motors.command[0], 0);
}

TEST(Arming, StaysDisarmedUntilThrottleIsLowAndClearsMotorsOnFailsafe) {
    flight::Vehicle vehicle;
    flight::reset_vehicle(vehicle);
    flight::Gains gains;
    flight::Limits limits;
    flight::Pilot high = armed_pilot();
    Body body;
    flight::step_vehicle(vehicle, high, gains, limits, sense(body), 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::disarmed);
    EXPECT_FLOAT_EQ(vehicle.motors.command[0], 0);
    arm(vehicle, gains, limits);
    EXPECT_EQ(vehicle.arm, flight::ArmState::armed);
    high.rc_lost = true;
    flight::step_vehicle(vehicle, high, gains, limits, sense(body), 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::failsafe);
    for (float command : vehicle.motors.command) {
        EXPECT_FLOAT_EQ(command, 0);
    }
    high.rc_lost = false;
    high.kill_switch = true;
    flight::step_vehicle(vehicle, high, gains, limits, sense(body), 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::disarmed);
    high.kill_switch = false;
    high.battery_v = 9.0F;
    flight::step_vehicle(vehicle, high, gains, limits, sense(body), 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::failsafe);
}

TEST(Hover, ReturnsATiltedQuadTowardLevel) {
    flight::Vehicle vehicle;
    flight::reset_vehicle(vehicle);
    flight::Gains gains;
    flight::Limits limits;
    arm(vehicle, gains, limits);
    Body body;
    body.roll = 0.35F;
    flight::Pilot pilot = armed_pilot();
    for (int step = 0; step < 4000; ++step) {
        flight::step_vehicle(vehicle, pilot, gains, limits, sense(body), 0.001F);
        plant(body, vehicle.motors, 0.001F);
    }
    EXPECT_EQ(vehicle.arm, flight::ArmState::armed);
    EXPECT_LT(std::fabs(body.roll), 0.05F);
    EXPECT_LT(std::fabs(body.roll_rate), 0.3F);
    EXPECT_LT(std::fabs(body.pitch), 0.05F);
}

TEST(StepResponse, TracksASmallStickAngle) {
    flight::Vehicle vehicle;
    flight::reset_vehicle(vehicle);
    flight::Gains gains;
    flight::Limits limits;
    arm(vehicle, gains, limits);
    Body body;
    flight::Pilot pilot = armed_pilot();
    pilot.roll = 0.5F;
    for (int step = 0; step < 4000; ++step) {
        flight::step_vehicle(vehicle, pilot, gains, limits, sense(body), 0.001F);
        plant(body, vehicle.motors, 0.001F);
    }
    EXPECT_NEAR(body.roll, 0.2F, 0.05F);
    EXPECT_LT(std::fabs(body.roll_rate), 0.3F);
}

TEST(FlightParameters, TextAssignmentChangesAGain) {
    parameters::Registry registry;
    flight::Gains gains;
    flight::Limits limits;
    ASSERT_TRUE(flight::define_flight_parameters(registry, gains, limits));
    EXPECT_EQ(parameters::write_text(registry, "rate_kp", "0.1"),
              parameters::ParameterWrite::applied);
    EXPECT_EQ(parameters::write_text(registry, "rate_kp", "3"),
              parameters::ParameterWrite::rejected);
    ASSERT_TRUE(flight::load_flight_parameters(registry, gains, limits));
    EXPECT_FLOAT_EQ(gains.rate_kp, 0.1F);
}

} // namespace
