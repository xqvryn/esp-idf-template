#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "ota/ota.hpp"

bool ota_support_init() noexcept;
ota::ImageState ota_running_state() noexcept;
const char* ota_running_version() noexcept;
const char* ota_partition_label() noexcept;
const char* ota_image_state_name(ota::ImageState state) noexcept;
void ota_confirm_or_rollback(bool wifi_connected) noexcept;
esp_err_t ota_handle_update(httpd_req_t* req);
bool ota_is_busy() noexcept;
