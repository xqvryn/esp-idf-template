#include "channels/record.hpp"
#include "esp_log.h"
#include "flight/controller.hpp"
#include "motion/imu.hpp"
#include "motion/pins.hpp"
#include "parameters/registry.hpp"

#include <cmath>
#include <cstdint>

namespace {

constexpr char tag[] = "drone";

motion::ImuSample level_sample(std::int64_t time_us) {
    motion::ImuSample sample{};
    sample.accel_m_s2[2] = 9.80665F;
    sample.time_us = time_us;
    sample.valid = true;
    return sample;
}

float motor_sum(const flight::Vehicle& vehicle) {
    return vehicle.motors.command[0] + vehicle.motors.command[1] + vehicle.motors.command[2] +
           vehicle.motors.command[3];
}

} // namespace

extern "C" void app_main() {
    flight::Vehicle vehicle;
    flight::reset_vehicle(vehicle);
    flight::Gains gains;
    flight::Limits limits;
    parameters::Registry registry;
    if (!flight::define_flight_parameters(registry, gains, limits) ||
        !flight::load_flight_parameters(registry, gains, limits)) {
        ESP_LOGE(tag, "parameters rejected");
        return;
    }

    motion::MotionPins pins;
    motion::SimulatedImu imu;
    imu.set(level_sample(0));

    flight::Pilot pilot;
    pilot.battery_v = 12.0F;
    channels::RecordChannel<4> records;
    for (int step = 0; step < 5; ++step) {
        motion::ImuSample sample;
        if (!imu.read(sample)) {
            ESP_LOGE(tag, "simulated imu rejected");
            return;
        }
        flight::step_vehicle(vehicle, pilot, gains, limits, sample, 0.01F);
        channels::Record record;
        record.time_us = step * 10000;
        record.severity = channels::Severity::info;
        record.code = static_cast<std::uint16_t>(vehicle.arm);
        record.a = vehicle.motors.command[0];
        records.emit(record);
    }

    const float boot_sum = motor_sum(vehicle);
    ESP_LOGI(tag, "simulation only: arm=%d motor_sum=%.2f", static_cast<int>(vehicle.arm),
             static_cast<double>(boot_sum));
    ESP_LOGI(tag, "pins=%s", motion::hardware_pins_selected(pins) ? "hardware" : "simulated");

    // Scripted checks after the boot log. GPIOs stay -1 and the sequence ends
    // with zero motor commands. This is not a flight test.
    std::int64_t now_us = 100000;
    auto step = [&](const motion::ImuSample& sample) {
        imu.set(sample);
        motion::ImuSample reading;
        if (!imu.read(reading)) {
            ESP_LOGE(tag, "simulated imu rejected");
            return false;
        }
        flight::step_vehicle(vehicle, pilot, gains, limits, reading, 0.01F);
        return true;
    };

    pilot.arm_switch = true;
    pilot.throttle = 0.5F;
    now_us += 10000;
    if (!step(level_sample(now_us))) {
        return;
    }
    ESP_LOGI(tag, "arm denied throttle: arm=%d motor_sum=%.2f", static_cast<int>(vehicle.arm),
             static_cast<double>(motor_sum(vehicle)));

    pilot.throttle = 0;
    motion::ImuSample tilted = level_sample(now_us + 10000);
    now_us = tilted.time_us;
    tilted.accel_m_s2[1] = std::sin(1.2F) * 9.80665F;
    tilted.accel_m_s2[2] = std::cos(1.2F) * 9.80665F;
    if (!step(tilted)) {
        return;
    }
    ESP_LOGI(tag, "arm denied tilt: arm=%d motor_sum=%.2f", static_cast<int>(vehicle.arm),
             static_cast<double>(motor_sum(vehicle)));

    now_us += 10000;
    if (!step(level_sample(now_us))) {
        return;
    }
    pilot.rc_lost = true;
    now_us += 10000;
    if (!step(level_sample(now_us))) {
        return;
    }
    ESP_LOGI(tag, "failsafe rc_lost motor_sum=%.2f arm=%d", static_cast<double>(motor_sum(vehicle)),
             static_cast<int>(vehicle.arm));

    pilot.rc_lost = false;
    now_us += 10000;
    if (!step(level_sample(now_us))) {
        return;
    }
    pilot.arm_switch = false;
    now_us += 10000;
    if (!step(level_sample(now_us))) {
        return;
    }
    pilot.arm_switch = true;
    now_us += 10000;
    if (!step(level_sample(now_us))) {
        return;
    }
    motion::ImuSample frozen = level_sample(now_us);
    if (!step(frozen)) {
        return;
    }
    ESP_LOGI(tag, "failsafe imu motor_sum=%.2f arm=%d", static_cast<double>(motor_sum(vehicle)),
             static_cast<int>(vehicle.arm));
}
