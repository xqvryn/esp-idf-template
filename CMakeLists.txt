cmake_minimum_required(VERSION 3.16)

# Application profile is independent of IDF_TARGET and of any board defaults.
set(ESPKIT_PROFILE "sensor" CACHE STRING "Application profile: minimal, sensor, or actuator")
set_property(CACHE ESPKIT_PROFILE PROPERTY STRINGS minimal sensor actuator)
if(NOT ESPKIT_PROFILE MATCHES "^(minimal|sensor|actuator)$")
    message(FATAL_ERROR "ESPKIT_PROFILE must be minimal, sensor, or actuator (got '${ESPKIT_PROFILE}')")
endif()
set(ESPKIT_PROFILE_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/profiles/${ESPKIT_PROFILE}/sdkconfig.defaults")
if(NOT EXISTS "${ESPKIT_PROFILE_DEFAULTS}")
    message(FATAL_ERROR "Missing ${ESPKIT_PROFILE_DEFAULTS}")
endif()
# Component requirement expansion runs in a child CMake process. It does not
# see this cache variable unless it is also in the environment.
set(ENV{ESPKIT_PROFILE} "${ESPKIT_PROFILE}")

# Components under components/ are discovered automatically. Exclude the ones this
# profile does not call so they are not compiled. Node components are not in that
# directory; they are added only for the sensor and actuator profiles.
if(ESPKIT_PROFILE STREQUAL "minimal")
    set(EXCLUDE_COMPONENTS platform configuration networking drivers)
elseif(ESPKIT_PROFILE STREQUAL "actuator")
    list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/applications/node/components")
    set(EXCLUDE_COMPONENTS configuration networking drivers)
else()
    list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/applications/node/components")
endif()

# Keep caller/CI defaults, then append the profile. IDF also loads
# <file>.${IDF_TARGET} beside each entry, so chip defaults stay orthogonal.
if(NOT SDKCONFIG_DEFAULTS AND NOT "$ENV{SDKCONFIG_DEFAULTS}" STREQUAL "")
    set(SDKCONFIG_DEFAULTS "$ENV{SDKCONFIG_DEFAULTS}")
endif()
if(NOT SDKCONFIG_DEFAULTS)
    set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
endif()
list(APPEND SDKCONFIG_DEFAULTS "${ESPKIT_PROFILE_DEFAULTS}")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/ProjectVersion.cmake")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(device)
