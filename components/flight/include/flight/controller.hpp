#pragma once

#include "motion/imu.hpp"
#include "parameters/registry.hpp"

#include <cstdint>

namespace flight {

enum class ArmState : std::uint8_t {
    disarmed,
    armed,
    failsafe
};

struct Gains {
    float angle_kp{5.0F};
    float angle_ki{0.4F};
    float angle_kd{0.02F};
    float rate_kp{0.45F};
    float rate_ki{0.6F};
    float rate_kd{0.01F};
    float max_rate{4.0F};
    float max_command{0.6F};
    float max_stick_angle{0.4F};
    float filter_accel{0.02F};
};

struct Limits {
    float throttle_arm_max{0.05F};
    float battery_min_v{10.0F};
};

struct Pilot {
    bool arm_switch{false};
    bool kill_switch{false};
    bool rc_lost{false};
    float throttle{0};
    float battery_v{12.0F};
    float roll{0};
    float pitch{0};
    float yaw{0};
};

struct Motors {
    float command[4]{0, 0, 0, 0};
};

struct Attitude {
    float roll{0};
    float pitch{0};
    float yaw{0};
    float roll_rate{0};
    float pitch_rate{0};
    float yaw_rate{0};
};

struct AxisLoop {
    float integrator{0};
    float last_error{0};
    bool has_last{false};
};

struct Vehicle {
    ArmState arm{ArmState::disarmed};
    bool arm_switch_released{true};
    Attitude attitude{};
    Motors motors{};
    AxisLoop roll_angle{};
    AxisLoop pitch_angle{};
    AxisLoop yaw_angle{};
    AxisLoop roll_rate{};
    AxisLoop pitch_rate{};
    AxisLoop yaw_rate{};
};

// Quad X, motors front-right, rear-right, rear-left, front-left.
// Positive roll raises the left motors. Positive pitch raises the front motors.
void mix_quad_x(float throttle, float roll, float pitch, float yaw, Motors& motors) noexcept;

bool define_flight_parameters(parameters::Registry& registry, const Gains& gains,
                              const Limits& limits) noexcept;
bool load_flight_parameters(const parameters::Registry& registry, Gains& gains,
                            Limits& limits) noexcept;

void reset_vehicle(Vehicle& vehicle) noexcept;
// dt_s must be positive. Disarmed, failsafe, and kill outputs are exactly zero.
void step_vehicle(Vehicle& vehicle, const Pilot& pilot, const Gains& gains, const Limits& limits,
                  const motion::ImuSample& imu, float dt_s) noexcept;

} // namespace flight
