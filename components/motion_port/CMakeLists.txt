idf_component_register(SRCS "src/board.cpp" INCLUDE_DIRS "include" REQUIRES motion
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_driver_mcpwm
                                     esp_driver_rmt esp_driver_spi esp_driver_uart)
set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES
                                                                   CXX_EXTENSIONS NO)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++20>)
