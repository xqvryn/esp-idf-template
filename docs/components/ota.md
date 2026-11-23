# ota

`components/ota` is a portable helper with no ESP-IDF includes. It has two jobs:

- `decide_boot_confirm` — given the running image state, whether Wi-Fi associated, and a completed self-check, return `none`, `hold`, `confirm`, or `refuse`. A pending or new image is confirmed only when Wi-Fi is up and the self-check passed. A completed check that failed, or a missing association, is refused. Broker reachability is not an input.
- `inspect_app_image_prefix` — accept the first 80 bytes of an ESP-IDF application image (0xE9, app-descriptor magic `0xABCD5432`, and a NUL-terminated ASCII version) and reject anything else.

The HTTP server, `esp_ota_*` / `esp_https_ota` calls, LED, and Wi-Fi station live in `examples/ota-update`. Host tests cover the helper in `tests/host/test_ota.cpp`.
