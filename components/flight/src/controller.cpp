#include "flight/controller.hpp"

#include <cmath>

namespace flight {

namespace {

float clampf(float value, float limit) noexcept {
    if (value > limit) {
        return limit;
    }
    if (value < -limit) {
        return -limit;
    }
    return value;
}

float pid(AxisLoop& axis, float error, float kp, float ki, float kd, float limit,
          float dt) noexcept {
    float derivative = 0;
    if (axis.has_last) {
        derivative = (error - axis.last_error) / dt;
    }
    axis.has_last = true;
    axis.last_error = error;
    const float unclamped = (kp * error) + (ki * axis.integrator) + (kd * derivative);
    const bool high = unclamped > limit;
    const bool low = unclamped < -limit;
    if ((!high && !low) || (high && error < 0) || (low && error > 0)) {
        axis.integrator = clampf(axis.integrator + (error * dt), limit);
    }
    return clampf(unclamped, limit);
}

void reset_loops(Vehicle& vehicle) noexcept {
    vehicle.roll_angle = {};
    vehicle.pitch_angle = {};
    vehicle.yaw_angle = {};
    vehicle.roll_rate = {};
    vehicle.pitch_rate = {};
    vehicle.yaw_rate = {};
}

float finite_or_zero(float value) noexcept {
    return std::isfinite(value) ? value : 0;
}

} // namespace

void mix_quad_x(float throttle, float roll, float pitch, float yaw, Motors& motors) noexcept {
    const float raw[4] = {
        throttle - roll + pitch - yaw,
        throttle - roll - pitch + yaw,
        throttle + roll - pitch - yaw,
        throttle + roll + pitch + yaw,
    };
    for (int index = 0; index < 4; ++index) {
        float command = raw[index];
        if (command < 0) {
            command = 0;
        }
        if (command > 1) {
            command = 1;
        }
        motors.command[index] = command;
    }
}

bool define_flight_parameters(parameters::Registry& registry, const Gains& gains,
                              const Limits& limits) noexcept {
    return registry.define_real("angle_kp", gains.angle_kp, 0.0F, 20.0F) &&
           registry.define_real("angle_ki", gains.angle_ki, 0.0F, 10.0F) &&
           registry.define_real("angle_kd", gains.angle_kd, 0.0F, 2.0F) &&
           registry.define_real("rate_kp", gains.rate_kp, 0.0F, 2.0F) &&
           registry.define_real("rate_ki", gains.rate_ki, 0.0F, 5.0F) &&
           registry.define_real("rate_kd", gains.rate_kd, 0.0F, 1.0F) &&
           registry.define_real("battery_min_v", limits.battery_min_v, 0.0F, 30.0F);
}

bool load_flight_parameters(const parameters::Registry& registry, Gains& gains,
                            Limits& limits) noexcept {
    return registry.get_real("angle_kp", gains.angle_kp) &&
           registry.get_real("angle_ki", gains.angle_ki) &&
           registry.get_real("angle_kd", gains.angle_kd) &&
           registry.get_real("rate_kp", gains.rate_kp) &&
           registry.get_real("rate_ki", gains.rate_ki) &&
           registry.get_real("rate_kd", gains.rate_kd) &&
           registry.get_real("battery_min_v", limits.battery_min_v);
}

void reset_vehicle(Vehicle& vehicle) noexcept {
    vehicle = {};
    vehicle.arm_switch_released = true;
    reset_loops(vehicle);
}

void step_vehicle(Vehicle& vehicle, const Pilot& pilot, const Gains& gains, const Limits& limits,
                  const motion::ImuSample& imu, float dt_s) noexcept {
    for (float& command : vehicle.motors.command) {
        command = 0;
    }
    if (!(dt_s > 0) || !imu.valid) {
        vehicle.arm = ArmState::failsafe;
        reset_loops(vehicle);
        return;
    }

    const float roll_accel =
        std::atan2(finite_or_zero(imu.accel_m_s2[1]), finite_or_zero(imu.accel_m_s2[2]));
    const float pitch_accel =
        std::atan2(-finite_or_zero(imu.accel_m_s2[0]), finite_or_zero(imu.accel_m_s2[2]));
    const float alpha =
        gains.filter_accel < 0 ? 0 : (gains.filter_accel > 1 ? 1 : gains.filter_accel);
    const float gyro_roll = finite_or_zero(imu.gyro_rad_s[0]);
    const float gyro_pitch = finite_or_zero(imu.gyro_rad_s[1]);
    const float gyro_yaw = finite_or_zero(imu.gyro_rad_s[2]);
    vehicle.attitude.roll =
        ((1.0F - alpha) * (vehicle.attitude.roll + (gyro_roll * dt_s))) + (alpha * roll_accel);
    vehicle.attitude.pitch =
        ((1.0F - alpha) * (vehicle.attitude.pitch + (gyro_pitch * dt_s))) + (alpha * pitch_accel);
    vehicle.attitude.yaw += gyro_yaw * dt_s;
    vehicle.attitude.roll_rate = gyro_roll;
    vehicle.attitude.pitch_rate = gyro_pitch;
    vehicle.attitude.yaw_rate = gyro_yaw;

    if (!pilot.arm_switch) {
        vehicle.arm_switch_released = true;
    }
    const bool battery_low = pilot.battery_v < limits.battery_min_v;
    if (pilot.kill_switch) {
        vehicle.arm = ArmState::disarmed;
        vehicle.arm_switch_released = false;
        reset_loops(vehicle);
        return;
    }
    if (pilot.rc_lost || battery_low) {
        vehicle.arm = ArmState::failsafe;
        reset_loops(vehicle);
        return;
    }
    if (vehicle.arm == ArmState::failsafe) {
        vehicle.arm = ArmState::disarmed;
    }
    if (vehicle.arm == ArmState::disarmed && pilot.arm_switch && vehicle.arm_switch_released &&
        pilot.throttle <= limits.throttle_arm_max) {
        vehicle.arm = ArmState::armed;
        vehicle.arm_switch_released = false;
        reset_loops(vehicle);
    }
    if (vehicle.arm == ArmState::armed && !pilot.arm_switch) {
        vehicle.arm = ArmState::disarmed;
        reset_loops(vehicle);
    }
    if (vehicle.arm != ArmState::armed) {
        return;
    }

    const float roll_target = clampf(pilot.roll, 1.0F) * gains.max_stick_angle;
    const float pitch_target = clampf(pilot.pitch, 1.0F) * gains.max_stick_angle;
    const float yaw_target = clampf(pilot.yaw, 1.0F) * gains.max_rate;
    const float roll_rate_sp =
        pid(vehicle.roll_angle, roll_target - vehicle.attitude.roll, gains.angle_kp, gains.angle_ki,
            gains.angle_kd, gains.max_rate, dt_s);
    const float pitch_rate_sp =
        pid(vehicle.pitch_angle, pitch_target - vehicle.attitude.pitch, gains.angle_kp,
            gains.angle_ki, gains.angle_kd, gains.max_rate, dt_s);
    const float yaw_rate_sp =
        pid(vehicle.yaw_angle, yaw_target - gyro_yaw, gains.angle_kp, 0, 0, gains.max_rate, dt_s);
    const float roll = pid(vehicle.roll_rate, roll_rate_sp - gyro_roll, gains.rate_kp,
                           gains.rate_ki, gains.rate_kd, gains.max_command, dt_s);
    const float pitch = pid(vehicle.pitch_rate, pitch_rate_sp - gyro_pitch, gains.rate_kp,
                            gains.rate_ki, gains.rate_kd, gains.max_command, dt_s);
    const float yaw = pid(vehicle.yaw_rate, yaw_rate_sp - gyro_yaw, gains.rate_kp, gains.rate_ki,
                          gains.rate_kd, gains.max_command, dt_s);
    float throttle = pilot.throttle;
    if (throttle < 0) {
        throttle = 0;
    }
    if (throttle > 1) {
        throttle = 1;
    }
    mix_quad_x(throttle, roll, pitch, yaw, vehicle.motors);
}

} // namespace flight
