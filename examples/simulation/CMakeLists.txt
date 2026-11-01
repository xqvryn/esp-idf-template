cmake_minimum_required(VERSION 3.16)
# Reusable components come from main/idf_component.yml path dependencies.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../applications/node/components")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ProjectVersion.cmake")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(simulation)
