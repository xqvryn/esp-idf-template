idf_component_register(SRCS "src/pinned_task.cpp" INCLUDE_DIRS "include" REQUIRES runtime
                       PRIV_REQUIRES freertos esp_timer)
set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES
                                                                   CXX_EXTENSIONS NO)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++20>)
