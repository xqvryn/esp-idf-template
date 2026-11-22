#include "flight/plant.hpp"

#include <cmath>

namespace flight {

namespace {

float clampf(float value, float lo, float hi) noexcept {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

float thrust_gain(const PlantConfig& config) noexcept {
    const float hover = config.hover_command > 0.05F ? config.hover_command : 0.45F;
    const float mass = config.mass_kg > 0.05F ? config.mass_kg : 0.8F;
    return (mass * config.gravity) / (4.0F * hover);
}

std::uint32_t next_rng(PlantState& state) noexcept {
    state.rng ^= state.rng >> 12;
    state.rng ^= state.rng << 25;
    state.rng ^= state.rng >> 27;
    return static_cast<std::uint32_t>(state.rng * 0x2545F4914F6CDD1DULL);
}

float unit_noise(PlantState& state, float scale) noexcept {
    if (!(scale > 0)) {
        return 0;
    }
    const float unit = (static_cast<float>(next_rng(state) & 0xFFFFU) / 32767.5F) - 1.0F;
    return unit * scale;
}

} // namespace

void reset_plant(PlantState& state, const PlantConfig& config) noexcept {
    state = {};
    state.rng = config.seed == 0 ? 1 : config.seed;
}

void step_plant(PlantState& state, const Motors& motors, const Wind& wind,
                const PlantConfig& config, float dt_s) noexcept {
    if (!(dt_s > 0)) {
        return;
    }
    const float tau = config.motor_tau_s > 0.001F ? config.motor_tau_s : 0.001F;
    const float lag = dt_s / (tau + dt_s);
    for (int index = 0; index < 4; ++index) {
        const float command = clampf(motors.command[index], 0, 1);
        state.rotor[index] += (command - state.rotor[index]) * lag;
    }

    const float k = thrust_gain(config);
    const float t0 = state.rotor[0] * k;
    const float t1 = state.rotor[1] * k;
    const float t2 = state.rotor[2] * k;
    const float t3 = state.rotor[3] * k;
    const float thrust = t0 + t1 + t2 + t3;
    const float ixx = config.inertia_xx > 1e-6F ? config.inertia_xx : 1e-6F;
    const float iyy = config.inertia_yy > 1e-6F ? config.inertia_yy : 1e-6F;
    const float izz = config.inertia_zz > 1e-6F ? config.inertia_zz : 1e-6F;
    const float arm = config.arm_m > 0 ? config.arm_m : 0.12F;
    const float roll_torque = ((t2 + t3) - (t0 + t1)) * arm;
    const float pitch_torque = ((t0 + t3) - (t1 + t2)) * arm;
    const float yaw_torque = ((t1 + t3) - (t0 + t2)) * arm;

    state.roll_rate += (roll_torque / ixx) * dt_s;
    state.pitch_rate += (pitch_torque / iyy) * dt_s;
    state.yaw_rate += (yaw_torque / izz) * dt_s;
    state.roll += state.roll_rate * dt_s;
    state.pitch += state.pitch_rate * dt_s;
    state.yaw += state.yaw_rate * dt_s;
    state.roll = clampf(state.roll, -1.2F, 1.2F);
    state.pitch = clampf(state.pitch, -1.2F, 1.2F);

    const float cr = std::cos(state.roll);
    const float sr = std::sin(state.roll);
    const float cp = std::cos(state.pitch);
    const float sp = std::sin(state.pitch);
    const float cy = std::cos(state.yaw);
    const float sy = std::sin(state.yaw);
    // Body +z thrust expressed in a z-up world frame (R = Rz(yaw) Ry(pitch) Rx(roll)).
    const float body_z_x = (cy * sp * cr) + (sy * sr);
    const float body_z_y = (sy * sp * cr) - (cy * sr);
    const float body_z_z = cp * cr;

    const float mass = config.mass_kg > 0.05F ? config.mass_kg : 0.8F;
    const float drag = config.linear_drag < 0 ? 0 : config.linear_drag;
    float accel[3] = {body_z_x * thrust / mass, body_z_y * thrust / mass,
                      (body_z_z * thrust / mass) - config.gravity};
    accel[0] += drag * (wind.world_m_s[0] - state.vel_m_s[0]) / mass;
    accel[1] += drag * (wind.world_m_s[1] - state.vel_m_s[1]) / mass;
    accel[2] += drag * (wind.world_m_s[2] - state.vel_m_s[2]) / mass;
    for (int axis = 0; axis < 3; ++axis) {
        state.accel_world_m_s2[axis] = accel[axis];
        state.vel_m_s[axis] += accel[axis] * dt_s;
        state.pos_m[axis] += state.vel_m_s[axis] * dt_s;
    }
    state.time_us += static_cast<std::int64_t>(dt_s * 1'000'000.0F);
}

motion::ImuSample sense_plant(PlantState& state, const PlantConfig& config) noexcept {
    const float cr = std::cos(state.roll);
    const float sr = std::sin(state.roll);
    const float cp = std::cos(state.pitch);
    const float sp = std::sin(state.pitch);
    const float cy = std::cos(state.yaw);
    const float sy = std::sin(state.yaw);
    const float axw = state.accel_world_m_s2[0];
    const float ayw = state.accel_world_m_s2[1];
    const float azw = state.accel_world_m_s2[2] + config.gravity;
    motion::ImuSample sample;
    // Specific force in body: R^T * (a_world - g_world), g_world = (0, 0, -g).
    const float xz = (cy * axw) + (sy * ayw);
    const float yz = -sy * axw + cy * ayw;
    const float zz = azw;
    const float xy = cp * xz - sp * zz;
    const float yy = yz;
    const float zy = sp * xz + cp * zz;
    sample.accel_m_s2[0] = xy;
    sample.accel_m_s2[1] = cr * yy + sr * zy;
    sample.accel_m_s2[2] = -sr * yy + cr * zy;
    sample.gyro_rad_s[0] = state.roll_rate + unit_noise(state, config.gyro_noise_rad_s);
    sample.gyro_rad_s[1] = state.pitch_rate + unit_noise(state, config.gyro_noise_rad_s);
    sample.gyro_rad_s[2] = state.yaw_rate + unit_noise(state, config.gyro_noise_rad_s);
    sample.accel_m_s2[0] += unit_noise(state, config.accel_noise_m_s2);
    sample.accel_m_s2[1] += unit_noise(state, config.accel_noise_m_s2);
    sample.accel_m_s2[2] += unit_noise(state, config.accel_noise_m_s2);
    sample.time_us = state.time_us;
    sample.valid = true;
    return sample;
}

} // namespace flight
