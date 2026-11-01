#include "application/workers.hpp"
#include "drivers/simulated_output.hpp"
#include "drivers/synthetic_sensor.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "timed_output/timed_output.hpp"
#include "wiring/clock.hpp"
#include "wiring/snapshot.hpp"

#include <cstdlib>

namespace {

constexpr float demo_sensor_value = 42.0F;
constexpr domain::Milliseconds max_run_ms = 1000;
constexpr domain::Milliseconds max_sample_age_ms = 500;
constexpr unsigned step_period_ms = 100;

class LocalEnableRequest final : public application::RequestSource {
    public:
    // Intentionally keeps requesting ON to demonstrate the latched runtime limit.
    bool enable_requested(domain::Milliseconds) noexcept override {
        return true;
    }
};

} // namespace

extern "C" void app_main() {
    static wiring::Clock clock;
    static drivers::SyntheticSensor sensor(demo_sensor_value);
    static drivers::SimulatedOutput output;
    static LocalEnableRequest requests;
    static wiring::Snapshot<domain::Reading> readings;
    static wiring::Snapshot<domain::Telemetry> telemetry;
    static timed_output::TimedOutput policy({max_run_ms, max_sample_age_ms});
    static application::SensorWorker sampler(sensor, clock, readings);
    static application::ControlWorker controller(policy, clock, readings, requests, output,
                                                 telemetry);
    if (!readings.ready() || !telemetry.ready()) {
        std::abort();
    }
    ESP_LOGI("actuator_node",
             "Simulation only: no GPIO; expected ON then latched OFF after 1 second");
    // This minimal example steps both workers on the main task for easy inspection.
    // The root simulation demonstrates independently scheduled workers.
    for (;;) {
        sampler.step();
        controller.step();
        domain::Telemetry status{};
        if (telemetry.latest(status)) {
            ESP_LOGI("actuator_node", "output=%d fault=%d", output.enabled(), status.fault);
        }
        vTaskDelay(pdMS_TO_TICKS(step_period_ms));
    }
}
