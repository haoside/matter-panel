cmake_minimum_required(VERSION 3.5)

# Matter SDK 路径（通过环境变量或相对路径）
if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "ESP_MATTER_PATH environment variable is not set")
endif()

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})

# 包含 Matter 项目的通用 CMake
include(${ESP_MATTER_PATH}/examples/common/cmake_common/cmake_include.cmake)

# 项目名
set(PROJECT_NAME "matter_panel")
set(EXTRA_COMPONENT_DIRS
    ${ESP_MATTER_PATH}/examples/common
    ${ESP_MATTER_PATH}/components
)

include(${ESP_MATTER_PATH}/examples/common/cmake_common/target_cmake_include.cmake)

project(${PROJECT_NAME})
