#include "esp_log.h"
#include "networking/telemetry.hpp"

extern "C" void app_main() {
    networking::ReconnectBackoff backoff;
    const auto delay = backoff.next_delay(0);
    ESP_LOGI("app", "networking linked delay=%lu", static_cast<unsigned long>(delay));
}
