# The early-expansion script does not load the CMake cache. The root project
# mirrors ESPKIT_PROFILE into the environment before project().
if(NOT DEFINED ESPKIT_PROFILE OR ESPKIT_PROFILE STREQUAL "")
    if(NOT "$ENV{ESPKIT_PROFILE}" STREQUAL "")
        set(ESPKIT_PROFILE "$ENV{ESPKIT_PROFILE}")
    else()
        set(ESPKIT_PROFILE "sensor")
    endif()
endif()

set(_espkit_profile_sources "")
set(_espkit_profile_requires "")
# ESPKIT_PROFILE is a CMake variable, so source selection works during IDF's
# early expansion. The Kconfig choice is checked only after sdkconfig exists.
if(ESPKIT_PROFILE STREQUAL "minimal")
    set(_espkit_profile_sources "../profiles/minimal/app_main.cpp")
    set(_espkit_profile_requires log)
    set(_espkit_profile_config CONFIG_ESPKIT_PROFILE_MINIMAL)
elseif(ESPKIT_PROFILE STREQUAL "actuator")
    set(_espkit_profile_sources "../profiles/actuator/app_main.cpp")
    set(_espkit_profile_requires application platform node_drivers wiring domain freertos log)
    set(_espkit_profile_config CONFIG_ESPKIT_PROFILE_ACTUATOR)
else()
    set(_espkit_profile_sources "app_main.cpp" "device.cpp")
    set(_espkit_profile_requires configuration application platform drivers node_drivers wiring networking log)
    set(_espkit_profile_config CONFIG_ESPKIT_PROFILE_SENSOR)
endif()
if(NOT CMAKE_BUILD_EARLY_EXPANSION AND NOT ${_espkit_profile_config})
    message(FATAL_ERROR "sdkconfig does not match ESPKIT_PROFILE=${ESPKIT_PROFILE}. Delete sdkconfig and the build directory, then reconfigure.")
endif()

idf_component_register(SRCS ${_espkit_profile_sources} INCLUDE_DIRS "."
    PRIV_REQUIRES ${_espkit_profile_requires})
set_target_properties(${COMPONENT_LIB} PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++20>)
