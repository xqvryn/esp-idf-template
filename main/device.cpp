#include "device.hpp"

#include "application/command_path.hpp"
#include "application/telemetry_codec.hpp"
#include "application/workers.hpp"
#include "boards/esp32s3/board_config.hpp"
#include "configuration/nvs_config_store.hpp"
#include "device_config.hpp"
#include "drivers/sht3x.hpp"
#include "drivers/simulated_output.hpp"
#include "drivers/synthetic_sensor.hpp"
#include "esp_log.h"
#include "networking/mqtt_telemetry.hpp"
#include "networking/network_service.hpp"
#include "networking/sntp_time.hpp"
#include "networking/wifi_station.hpp"
#include "platform/clock.hpp"
#include "platform/i2c.hpp"
#include "platform/periodic_task.hpp"
#include "wiring/fifo.hpp"
#include "wiring/log_telemetry.hpp"
#include "wiring/snapshot.hpp"

#include <array>
#include <cstdlib>
#include <span>

namespace device {

namespace app = application;

namespace {

#if CONFIG_ESPKIT_REMOTE_COMMANDS
constexpr bool remote_commands_enabled = true;
#else
constexpr bool remote_commands_enabled = false;
#endif

class DeviceClock final : public application::Clock, public networking::SteadyTime {
    public:
    std::int64_t now() const noexcept override {
        return clock_.now();
    }

    private:
    platform::EspClock clock_;
};

class BoardBus final : public drivers::I2cBus {
    public:
    bool begin(int sda, int scl, unsigned address, unsigned clock_hz, int timeout_ms) noexcept {
        return device_.begin(sda, scl, address, clock_hz, timeout_ms);
    }

    bool write(std::span<const std::uint8_t> bytes) noexcept override {
        return device_.write(bytes);
    }

    bool read(std::span<std::uint8_t> bytes) noexcept override {
        return device_.read(bytes);
    }

    private:
    platform::EspI2cDevice device_;
};

class BoardDelay final : public drivers::Delay {
    public:
    void sleep_for(std::int64_t duration_ms) noexcept override {
        platform::FreeRtosDelay::sleep_for(duration_ms);
    }
};

class Sht3xSource final : public app::SensorSource {
    public:
    explicit Sht3xSource(drivers::Sht3xSensor& sensor) noexcept : sensor_(sensor) {
    }

    domain::Reading read(domain::Milliseconds now) noexcept override {
        const auto sample = sensor_.measure();
        return {sample.temperature_celsius, now, sample.valid, domain::MeasurementUnit::celsius,
                domain::DataOrigin::physical};
    }

    private:
    drivers::Sht3xSensor& sensor_;
};

class CombinedReporter final : public app::TelemetrySink {
    public:
    CombinedReporter(app::TelemetrySink& log, networking::NetworkService& network,
                     app::BoundedFifo<domain::CommandResult>& results)
        : log_(log), network_(network), results_(results) {
    }

    void report(const domain::Telemetry& status) noexcept override {
        log_.report(status);
        std::array<char, app::telemetry_payload_capacity> payload{};
        const auto length = app::encode_telemetry(status, payload);
        network_.forward(std::span<const char>(payload.data(), length));
        domain::CommandResult result{};
        while (results_.try_pop(result)) {
            ESP_LOGI("commands", "id=%llu sequence=%u source=%d type=%d status=%d",
                     static_cast<unsigned long long>(result.id),
                     static_cast<unsigned>(result.sequence), static_cast<int>(result.source),
                     static_cast<int>(result.type), static_cast<int>(result.status));
        }
    }

    private:
    app::TelemetrySink& log_;
    networking::NetworkService& network_;
    app::BoundedFifo<domain::CommandResult>& results_;
};

class DeviceRuntime {
    public:
    DeviceRuntime()
        : sensor_worker_(sensor_, clock_, readings_),
          control_worker_(policy_, clock_, readings_, requests_, output_, telemetry_),
          reporter_worker_(telemetry_, reporter_),
          sensor_task_(sensor_worker_,
                       {"sensor", device::defaults::sensor_stack_bytes,
                        device::defaults::sensor_priority, defaults::sensor_period_ms}),
          control_task_(control_worker_,
                        {"control", device::defaults::control_stack_bytes,
                         device::defaults::control_priority, defaults::control_period_ms}),
          reporter_task_(reporter_worker_,
                         {"reporter", device::defaults::reporter_stack_bytes,
                          device::defaults::reporter_priority, defaults::reporter_period_ms}) {
    }

    void start() {
        if (started_ || !readings_.ready() || !telemetry_.ready() || !commands_.ready() ||
            !command_results_.ready() || !stops_.ready() ||
            ingress_.remote_commands_enabled() != remote_commands_enabled) {
            std::abort();
        }
        started_ = true;
#if CONFIG_ESPKIT_NETWORK_ENABLED
        configuration::NetworkConfig config{};
        configuration::NvsConfigStore store;
#if CONFIG_ESPKIT_ALLOW_PLAINTEXT_MQTT
        constexpr bool allow_plaintext = true;
#else
        constexpr bool allow_plaintext = false;
#endif
        if (store.load(config, allow_plaintext)) {
            network_.configure(config);
        } else {
            ESP_LOGW("network", "Network configuration unavailable; local control continues");
        }
#endif
#if CONFIG_ESPKIT_SENSOR_SHT3X
        if (!bus_.begin(board::sda_gpio, board::scl_gpio, board::sensor_address,
                        board::i2c_clock_hz, board::i2c_timeout_ms)) {
            ESP_LOGW("board", "I2C unavailable: check explicitly configured pins and wiring");
        }
#endif
        // Seed snapshots and apply safe OFF before any concurrent execution.
        sensor_worker_.step();
        control_worker_.step();
        if (!sensor_task_.start() || !control_task_.start() || !reporter_task_.start()) {
            std::abort();
        }
#if CONFIG_ESPKIT_NETWORK_ENABLED
        if (!network_task_.start()) {
            std::abort();
        }
#endif
    }

    private:
    DeviceClock clock_;
    wiring::Snapshot<domain::Reading> readings_;
    wiring::Snapshot<domain::Telemetry> telemetry_;
    wiring::Fifo<app::QueuedCommand> commands_{defaults::command_queue_capacity};
    wiring::Fifo<domain::CommandResult> command_results_{defaults::command_result_capacity};
    wiring::Snapshot<app::StopNotice> stops_;
    app::CommandIngress ingress_{commands_, command_results_, stops_, defaults::command_limits,
                                 remote_commands_enabled};
#if CONFIG_ESPKIT_SENSOR_SHT3X
    BoardBus bus_;
    BoardDelay delay_;
    drivers::Sht3xSensor sht_{bus_, delay_};
    Sht3xSource sensor_{sht_};
#else
    drivers::SyntheticSensor sensor_{defaults::synthetic_reading};
#endif
    drivers::SimulatedOutput output_;
    app::CommandRequests requests_{commands_, command_results_, stops_,
                                   defaults::output_policy.max_run_ms};
    wiring::LogTelemetry log_{"simulated"};
    networking::WifiStation wifi_{clock_};
    networking::MqttTelemetry mqtt_;
    networking::SntpTime network_time_;
    networking::NetworkService network_{wifi_, mqtt_, network_time_, clock_};
    CombinedReporter reporter_{log_, network_, command_results_};
    timed_output::TimedOutput policy_{defaults::output_policy};
    app::SensorWorker sensor_worker_;
    app::ControlWorker control_worker_;
    app::ReporterWorker reporter_worker_;
    platform::PeriodicTask<app::SensorWorker> sensor_task_;
    platform::PeriodicTask<app::ControlWorker> control_task_;
    platform::PeriodicTask<app::ReporterWorker> reporter_task_;
#if CONFIG_ESPKIT_NETWORK_ENABLED
    platform::PeriodicTask<networking::NetworkService> network_task_{
        network_,
        {"network", device::defaults::network_stack_bytes, device::defaults::network_priority,
         defaults::network_period_ms}};
#endif
    bool started_{false};
};

} // namespace

void start_device() {
    // Construct after ESP-IDF startup, never from global hardware constructors.
    static DeviceRuntime device;
    device.start();
}

} // namespace device
