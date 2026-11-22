#include "flight/controller.hpp"
#include "flight/plant.hpp"
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

TEST(Mixer, HighThrottlePreservesARollDifferential) {
    flight::Motors motors;
    flight::mix_quad_x(1.0F, 0.3F, 0, 0, motors);
    const float left = motors.command[2] + motors.command[3];
    const float right = motors.command[0] + motors.command[1];
    EXPECT_NEAR(left - right, 1.2F, 0.02F);
    for (float command : motors.command) {
        EXPECT_GE(command, 0);
        EXPECT_LE(command, 1);
    }
}

TEST(Arming, RefusesATiltedAttitudeAndAFrozenImu) {
    flight::Vehicle vehicle;
    flight::reset_vehicle(vehicle);
    flight::Gains gains;
    flight::Limits limits;
    flight::Pilot pilot;
    pilot.arm_switch = true;
    pilot.throttle = 0;
    pilot.battery_v = 12.0F;
    Body tilted;
    tilted.roll = 1.2F;
    flight::step_vehicle(vehicle, pilot, gains, limits, sense(tilted), 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::disarmed);

    Body level;
    motion::ImuSample first = sense(level);
    first.time_us = 1000;
    flight::step_vehicle(vehicle, pilot, gains, limits, first, 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::armed);

    motion::ImuSample frozen = first;
    frozen.time_us = 1000;
    pilot.throttle = 0.45F;
    flight::step_vehicle(vehicle, pilot, gains, limits, frozen, 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::failsafe);
    for (float command : vehicle.motors.command) {
        EXPECT_FLOAT_EQ(command, 0);
    }

    flight::reset_vehicle(vehicle);
    pilot.throttle = 0;
    motion::ImuSample late = sense(level);
    late.time_us = 2000;
    flight::step_vehicle(vehicle, pilot, gains, limits, late, 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::armed);
    late.time_us = 2000 + 100000;
    flight::step_vehicle(vehicle, pilot, gains, limits, late, 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::failsafe);

    flight::reset_vehicle(vehicle);
    motion::ImuSample invalid = sense(level);
    invalid.valid = false;
    invalid.time_us = 1000;
    flight::step_vehicle(vehicle, pilot, gains, limits, invalid, 0.001F);
    EXPECT_EQ(vehicle.arm, flight::ArmState::failsafe);
    EXPECT_FLOAT_EQ(vehicle.motors.command[0], 0);
}

TEST(Plant, LevelRestMeasuresGravityOnZ) {
    flight::PlantConfig config;
    flight::PlantState state;
    flight::reset_plant(state, config);
    flight::Motors motors;
    for (float& command : motors.command) {
        command = config.hover_command;
        // Spool the lag so this sample is a hover, not free fall.
    }
    for (float& rotor : state.rotor) {
        rotor = config.hover_command;
    }
    flight::Wind wind;
    flight::step_plant(state, motors, wind, config, 0.001F);
    motion::ImuSample sample = flight::sense_plant(state, config);
    EXPECT_NEAR(sample.accel_m_s2[0], 0, 0.2F);
    EXPECT_NEAR(sample.accel_m_s2[1], 0, 0.2F);
    EXPECT_NEAR(sample.accel_m_s2[2], config.gravity, 0.3F);
    EXPECT_TRUE(sample.valid);
}

TEST(Plant, ClosedLoopLevelsThroughLagNoiseAndAGust) {
    flight::Vehicle vehicle;
    flight::reset_vehicle(vehicle);
    flight::Gains gains;
    flight::Limits limits;
    flight::PlantConfig config;
    config.motor_tau_s = 0.02F;
    config.gyro_noise_rad_s = 0.01F;
    config.accel_noise_m_s2 = 0.05F;
    config.seed = 7;
    flight::PlantState state;
    flight::reset_plant(state, config);
    flight::Wind wind;
    flight::Pilot pilot;
    pilot.arm_switch = true;
    pilot.throttle = 0;
    pilot.battery_v = 12.0F;
    flight::step_vehicle(vehicle, pilot, gains, limits, flight::sense_plant(state, config), 0.001F);
    ASSERT_EQ(vehicle.arm, flight::ArmState::armed);
    state.roll = 0.3F;
    pilot.throttle = config.hover_command;
    for (int step = 0; step < 8000; ++step) {
        if (step == 2000) {
            wind.world_m_s[1] = 4.0F;
        }
        if (step == 2400) {
            wind.world_m_s[1] = 0;
        }
        motion::ImuSample sample = flight::sense_plant(state, config);
        flight::step_vehicle(vehicle, pilot, gains, limits, sample, 0.001F);
        flight::step_plant(state, vehicle.motors, wind, config, 0.001F);
    }
    EXPECT_EQ(vehicle.arm, flight::ArmState::armed);
    EXPECT_LT(std::fabs(state.roll), 0.12F);
    EXPECT_LT(std::fabs(state.pitch), 0.12F);
    EXPECT_LT(std::fabs(state.roll_rate), 0.5F);
}

} // namespace
