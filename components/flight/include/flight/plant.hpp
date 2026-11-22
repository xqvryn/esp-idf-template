#pragma once

#include "flight/controller.hpp"

#include <cstdint>

namespace flight {

// A rigid-body plus first-order motor lag for host and on-device simulation.
// It is not a model of a real airframe and it is not flight-tested.
struct PlantConfig {
    float mass_kg{0.8F};
    float arm_m{0.12F};
    float inertia_xx{0.008F};
    float inertia_yy{0.008F};
    float inertia_zz{0.014F};
    float motor_tau_s{0.025F};
    float hover_command{0.45F};
    float gravity{9.80665F};
    float linear_drag{0.2F};
    float gyro_noise_rad_s{0};
    float accel_noise_m_s2{0};
    std::uint32_t seed{1};
};

struct Wind {
    float world_m_s[3]{0, 0, 0};
};

struct PlantState {
    float pos_m[3]{0, 0, 0};
    float vel_m_s[3]{0, 0, 0};
    float roll{0};
    float pitch{0};
    float yaw{0};
    float roll_rate{0};
    float pitch_rate{0};
    float yaw_rate{0};
    float rotor[4]{0, 0, 0, 0};
    float accel_world_m_s2[3]{0, 0, 0};
    std::uint64_t rng{1};
    std::int64_t time_us{0};
};

void reset_plant(PlantState& state, const PlantConfig& config) noexcept;
void step_plant(PlantState& state, const Motors& motors, const Wind& wind,
                const PlantConfig& config, float dt_s) noexcept;
motion::ImuSample sense_plant(PlantState& state, const PlantConfig& config) noexcept;

} // namespace flight
