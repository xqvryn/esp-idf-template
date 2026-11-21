#include "channels/record.hpp"
#include "esp_log.h"
#include "flight/controller.hpp"
#include "motion/imu.hpp"
#include "motion/pins.hpp"
#include "parameters/registry.hpp"

namespace {

constexpr char tag[] = "drone";

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
    motion::ImuSample level{};
    level.accel_m_s2[2] = 9.80665F;
    level.valid = true;
    imu.set(level);

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

    const float motor_sum = vehicle.motors.command[0] + vehicle.motors.command[1] +
                            vehicle.motors.command[2] + vehicle.motors.command[3];
    ESP_LOGI(tag, "simulation only: arm=%d motor_sum=%.2f", static_cast<int>(vehicle.arm),
             static_cast<double>(motor_sum));
    ESP_LOGI(tag, "pins=%s", motion::hardware_pins_selected(pins) ? "hardware" : "simulated");
}
