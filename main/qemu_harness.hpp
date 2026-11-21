#pragma once

#include "application/ports/device.hpp"
#include "domain/types.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace application {

class Clock;
class CommandIngress;

} // namespace application

namespace drivers {

class SimulatedOutput;

} // namespace drivers

namespace timed_output {

class TimedOutput;

} // namespace timed_output

namespace qemu_harness {

// Synthetic reading that a QEMU test can change over the console.
class ScriptedSensor final : public application::SensorSource {
    public:
    explicit ScriptedSensor(float value) noexcept : value_(value), valid_(true) {
    }

    domain::Reading read(domain::Milliseconds now) noexcept override {
        portENTER_CRITICAL(&lock_);
        const float value = value_;
        const bool valid = valid_;
        portEXIT_CRITICAL(&lock_);
        return {value, now, valid, domain::MeasurementUnit::unspecified,
                domain::DataOrigin::synthetic};
    }

    void set_value(float value) noexcept {
        portENTER_CRITICAL(&lock_);
        value_ = value;
        valid_ = true;
        portEXIT_CRITICAL(&lock_);
    }

    void fail() noexcept {
        portENTER_CRITICAL(&lock_);
        valid_ = false;
        portEXIT_CRITICAL(&lock_);
    }

    private:
    portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
    float value_;
    bool valid_;
};

struct Context {
    application::CommandIngress& ingress;
    const application::Clock& clock;
    timed_output::TimedOutput& policy;
    drivers::SimulatedOutput& output;
    ScriptedSensor& sensor;
};

// Starts the console task. The context must outlive the task.
void start(const Context& context);

} // namespace qemu_harness
