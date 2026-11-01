#include "application/workers.hpp"
#include "device_config.hpp"
#include "drivers/synthetic_sensor.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/periodic_task.hpp"
#include "wiring/clock.hpp"
#include "wiring/snapshot.hpp"

#include <cstdlib>

namespace {

constexpr float demo_sensor_value = 23.5F;
constexpr domain::Milliseconds sampling_period_ms = 500;
constexpr unsigned reporting_period_ms = 1000;

} // namespace

extern "C" void app_main() {
    static wiring::Clock clock;
    static drivers::SyntheticSensor sensor(demo_sensor_value);
    static wiring::Snapshot<domain::Reading> readings;
    static application::SensorWorker worker(sensor, clock, readings);
    static platform::PeriodicTask<application::SensorWorker> task(
        worker, {"sensor", device::defaults::sensor_stack_bytes, device::defaults::sensor_priority,
                 sampling_period_ms});
    if (!readings.ready()) {
        std::abort();
    }
    worker.step();
    if (!task.start()) {
        std::abort();
    }
    for (;;) {
        domain::Reading reading{};
        if (readings.latest(reading)) {
            ESP_LOGI("sensor_node", "value=%.1f sampled_at_ms=%lld valid=%d",
                     static_cast<double>(reading.value), static_cast<long long>(reading.sampled_at),
                     reading.valid);
        }
        vTaskDelay(pdMS_TO_TICKS(reporting_period_ms));
    }
}
