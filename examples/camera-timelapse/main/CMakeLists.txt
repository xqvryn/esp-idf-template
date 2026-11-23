idf_component_register(SRCS "app_main.cpp" "photo_store.cpp" "ota_http.cpp"
                       PRIV_REQUIRES log esp_timer esp_netif esp_wifi nvs_flash esp_http_server
                                     app_update heap esp_psram configuration networking ota
                                     image_motion esp32-camera led_strip)
set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES
                                                                   CXX_EXTENSIONS NO)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++20>)
