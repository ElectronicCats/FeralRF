cmake_minimum_required(VERSION 3.20)

include(pico_sdk_import.cmake)

project(feralrf_rp2040 C CXX ASM)

# Initialize Pico SDK
pico_sdk_init()

# C++ standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Project sources
set(SOURCES
    src/main.c
    src/usb_bridge.c
    src/uart.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Executable
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

# Pull in dependencies
target_link_libraries(${CMAKE_PROJECT_NAME}
    pico_stdlib
    pico_unique_id
    hardware_uart
    hardware_gpio
)

# Enable USB stdio
pico_enable_stdio_usb(${CMAKE_PROJECT_NAME} 1)
pico_enable_stdio_uart(${CMAKE_PROJECT_NAME} 0)

# Create output files (uf2 for drag-drop flash)
pico_add_extra_outputs(${CMAKE_PROJECT_NAME})
