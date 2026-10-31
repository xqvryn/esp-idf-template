#include "wiring/log_telemetry.hpp"

#include "esp_log.h"

namespace wiring {

void LogTelemetry::report(const domain::Telemetry& value) noexcept {
    ESP_LOGI("runtime",
             "sensor_origin=%d output_mode=%s uptime_ms=%lld requested=%d applied=%d "
             "fault=%d error=%d",
             static_cast<int>(value.reading.origin), output_mode_ ? output_mode_ : "unspecified",
             static_cast<long long>(value.uptime), value.output_requested,
             static_cast<int>(value.applied_output), value.fault,
             static_cast<int>(value.output_error));
}

} // namespace wiring
