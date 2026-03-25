cmake_minimum_required(VERSION 3.20)

# SDK path (set before project)
set(TI_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24"
    CACHE PATH "TI SimpleLink SDK path")

if(NOT EXISTS ${TI_SDK_PATH})
    message(FATAL_ERROR "TI SDK not found at ${TI_SDK_PATH}. Please download and extract the SDK.")
endif()

# Device variant: CC1352P (352KB flash) or CC1352P7 (704KB flash)
set(DEVICE_VARIANT "CC1352P" CACHE STRING "Target device: CC1352P or CC1352P7")
set_property(CACHE DEVICE_VARIANT PROPERTY STRINGS "CC1352P" "CC1352P7")

if(DEVICE_VARIANT STREQUAL "CC1352P7")
    set(DEVICE_SUBFAMILY "cc13x2x7_cc26x2x7")
    set(DEVICE_FAMILY_DEFINE "DeviceFamily_CC13X2X7")
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/cc1352p7.ld")
elseif(DEVICE_VARIANT STREQUAL "CC1352P")
    set(DEVICE_SUBFAMILY "cc13x2_cc26x2")
    set(DEVICE_FAMILY_DEFINE "DeviceFamily_CC13X2")
    set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/cc1352p.ld")
else()
    message(FATAL_ERROR "Unknown DEVICE_VARIANT: ${DEVICE_VARIANT}. Use CC1352P or CC1352P7.")
endif()

message(STATUS "Building for ${DEVICE_VARIANT} (${DEVICE_SUBFAMILY})")

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
set(CMAKE_EXE_LINKER_FLAGS "-T ${LINKER_SCRIPT} -nostartfiles -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map" CACHE INTERNAL "")

# Include paths from SDK (device-variant aware)
set(SDK_INCLUDE_DIRS
    ${TI_SDK_PATH}/source
    ${TI_SDK_PATH}/kernel/nortos
    ${TI_SDK_PATH}/kernel/nortos/dpl
    ${TI_SDK_PATH}/kernel/nortos/posix
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/inc
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/driverlib
    ${TI_SDK_PATH}/source/ti/drivers
    ${TI_SDK_PATH}/source/ti/drivers/rf
)

# DriverLib library (pre-compiled for GCC)
set(DRIVERLIB_LIB
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/driverlib/bin/gcc/driverlib.lib
)

set(RF_MULTI_MODE_LIB
    ${TI_SDK_PATH}/source/ti/drivers/rf/lib/gcc/m4f/rf_multiMode_cc13x2.a
)

# Source files
set(SOURCES
    ccfg.c
    src/command_processor.c
    src/control_task.c
    src/data_task.c
    src/host_if.c
    src/host_if_task.c
    src/ll_manager.c
    src/main.c
    src/output_if.c
    src/packet_queue.c
    src/phy_manager.c
    src/protocol.c
    src/radio_if.c
    src/smartrf_ble5_0.c
    src/smartrf_ieee_15_4_0.c
    src/ti_rf_config_min.c
    src/startup_cc13x2_cc26x2_gcc.c
    src/task_event.c

    # TI NoRTOS DPL sources required by RF multiMode
    ${TI_SDK_PATH}/kernel/nortos/dpl/ClockPTimer_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/DebugP_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/HwiPCC26XX_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/QueueP_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/SemaphoreP_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/SwiP_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/SystemP_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/TimerPCC26XX_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/TimestampPCC26XX_nortos.c
    ${TI_SDK_PATH}/kernel/nortos/dpl/PowerCC26X2_nortos.c

    # TI driver sources required by RF multiMode
    ${TI_SDK_PATH}/source/ti/drivers/ITM.c
    ${TI_SDK_PATH}/source/ti/drivers/Temperature.c
    ${TI_SDK_PATH}/source/ti/drivers/utils/List.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2_helpers.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2_calibrateRCOSC.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2_calibrateRCOSC_helpers.c
    ${TI_SDK_PATH}/source/ti/drivers/temperature/TemperatureCC26X2.c
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDK_INCLUDE_DIRS}
)

# Executable
add_executable(${CMAKE_PROJECT_NAME}.elf ${SOURCES})

target_compile_definitions(${CMAKE_PROJECT_NAME}.elf PRIVATE
    ${DEVICE_FAMILY_DEFINE}
)

target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE
    ${DRIVERLIB_LIB}
    ${RF_MULTI_MODE_LIB}
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
    COMMENT "Generating hex and binary files for ${DEVICE_VARIANT}"
)
