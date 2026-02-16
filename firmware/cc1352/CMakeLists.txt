cmake_minimum_required(VERSION 3.20)

project(feralrf_cc1352 C ASM)

# SDK path
set(TI_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24"
    CACHE PATH "TI SimpleLink SDK path")

if(NOT EXISTS ${TI_SDK_PATH})
    message(FATAL_ERROR "TI SDK not found at ${TI_SDK_PATH}. Please download and extract the SDK.")
endif()

# Toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft -Wall -Wextra -Werror -std=c11 -ffunction-sections -fdata-sections -g -O0" CACHE INTERNAL "")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m4 -mthumb -g -x assembler-with-cpp" CACHE INTERNAL "")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/linker/cc1352p.ld -nostartfiles -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map" CACHE INTERNAL "")

# Include paths from SDK
set(SDK_INCLUDE_DIRS
    ${TI_SDK_PATH}/source
    ${TI_SDK_PATH}/source/ti/devices/cc13x2_cc26x2
    ${TI_SDK_PATH}/source/ti/devices/cc13x2_cc26x2/inc
    ${TI_SDK_PATH}/source/ti/drivers
    ${TI_SDK_PATH}/source/ti/drivers/rf
    ${TI_SDK_PATH}/kernel/tirtos7/packages/ti/sysbios
    ${TI_SDK_PATH}/kernel/tirtos7/packages/ti/sysbios/BIOS
    ${TI_SDK_PATH}/third_party/CMSIS/Include
)

# Source files
set(SOURCES
    src/main.c
    src/startup_cc13x2_cc26x2_gcc.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDK_INCLUDE_DIRS}
)

# Executable
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES})

# Post-build to generate hex and bin
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf
    COMMENT "Generating hex and binary files"
)

# Print size
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf
)
