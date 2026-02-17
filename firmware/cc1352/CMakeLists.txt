cmake_minimum_required(VERSION 3.20)

# SDK path (set before project)
set(TI_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24"
    CACHE PATH "TI SimpleLink SDK path")

if(NOT EXISTS ${TI_SDK_PATH})
    message(FATAL_ERROR "TI SDK not found at ${TI_SDK_PATH}. Please download and extract the SDK.")
endif()

# Toolchain configuration (must be before project())
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(feralrf_cc1352 C ASM)

# Toolchain tools
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# Compiler flags
set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Wall -Wextra -std=c11 -ffunction-sections -fdata-sections -g -O0" CACHE INTERNAL "")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -g -x assembler-with-cpp" CACHE INTERNAL "")
set(CMAKE_EXE_LINKER_FLAGS "-T ${CMAKE_CURRENT_SOURCE_DIR}/linker/cc1352p.ld -nostartfiles -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map" CACHE INTERNAL "")

# Include paths from SDK
set(SDK_INCLUDE_DIRS
    ${TI_SDK_PATH}/source
    ${TI_SDK_PATH}/source/ti/devices/cc13x2x7_cc26x2x7
    ${TI_SDK_PATH}/source/ti/devices/cc13x2x7_cc26x2x7/inc
    ${TI_SDK_PATH}/source/ti/devices/cc13x2x7_cc26x2x7/driverlib
    ${TI_SDK_PATH}/source/ti/drivers
    ${TI_SDK_PATH}/source/ti/drivers/rf
)

# DriverLib library (pre-compiled for GCC)
set(DRIVERLIB_LIB
    ${TI_SDK_PATH}/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/bin/gcc/driverlib.lib
)

# Source files
set(SOURCES
    ccfg.c
    src/command_processor.c
    src/control_task.c
    src/data_task.c
    src/host_if.c
    src/host_if_task.c
    src/main.c
    src/output_if.c
    src/packet_queue.c
    src/protocol.c
    src/radio_if.c
    src/startup_cc13x2_cc26x2_gcc.c
    src/task_event.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDK_INCLUDE_DIRS}
)

# Executable
add_executable(${CMAKE_PROJECT_NAME}.elf ${SOURCES})

target_compile_definitions(${CMAKE_PROJECT_NAME}.elf PRIVATE
    DeviceFamily_CC13X2X7
)

target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE
    ${DRIVERLIB_LIB}
)

# Set output suffix
set_target_properties(${CMAKE_PROJECT_NAME}.elf PROPERTIES
    OUTPUT_NAME ${CMAKE_PROJECT_NAME}
    SUFFIX .elf
)

# Post-build to generate hex and bin
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf
    COMMENT "Generating hex and binary files"
)
