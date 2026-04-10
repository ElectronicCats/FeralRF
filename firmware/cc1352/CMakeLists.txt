cmake_minimum_required(VERSION 3.20)

# SDK path (set before project)
set(TI_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01"
    CACHE PATH "TI SimpleLink SDK path")

if(NOT EXISTS ${TI_SDK_PATH})
    message(FATAL_ERROR "TI SDK not found at ${TI_SDK_PATH}. Please download and extract the SDK.")
endif()

# Device variant
set(DEVICE_VARIANT "CC1352P7" CACHE STRING "Target device: CC1352P or CC1352P7")
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
    message(FATAL_ERROR "Unknown DEVICE_VARIANT: ${DEVICE_VARIANT}")
endif()

# RTOS selection
option(USE_TIRTOS "Build with TI-RTOS7 (required for BLE5-Stack)" ON)

message(STATUS "Building for ${DEVICE_VARIANT} (${DEVICE_SUBFAMILY}), RTOS=${USE_TIRTOS}")

# Toolchain
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR ARM)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(feralrf_cc1352 C ASM)

set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_SIZE arm-none-eabi-size)

# Compiler flags
set(CMAKE_C_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -Wall -Wextra -std=c11 -ffunction-sections -fdata-sections -g -O0" CACHE INTERNAL "")
set(CMAKE_ASM_FLAGS "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -g -x assembler-with-cpp" CACHE INTERNAL "")
set(CMAKE_EXE_LINKER_FLAGS "-T ${LINKER_SCRIPT} -nostartfiles -Wl,--gc-sections -Wl,-Map=${CMAKE_PROJECT_NAME}.map" CACHE INTERNAL "")

# ─── GCC 13+ workaround for TI SDK POSIX headers ───
# TI SDK's ti/posix/gcc/time.h does #include <../include/time.h> expecting
# the compiler's newlib time.h at posix/include/time.h. TI's bundled GCC has
# this layout, but system arm-none-eabi-gcc 13.x does not. Create a symlink
# from posix/include → newlib's include dir so the relative path resolves.
get_filename_component(_POSIX_DIR "${TI_SDK_PATH}/source/ti/posix" REALPATH)
set(POSIX_INCLUDE_LINK ${_POSIX_DIR}/include)
if(NOT EXISTS ${POSIX_INCLUDE_LINK})
    # Find newlib include directory
    foreach(_candidate
        /usr/lib/arm-none-eabi/include
        /usr/arm-none-eabi/include
    )
        if(EXISTS ${_candidate}/time.h)
            file(CREATE_LINK ${_candidate} ${POSIX_INCLUDE_LINK} SYMBOLIC)
            message(STATUS "Created POSIX include symlink: ${POSIX_INCLUDE_LINK} → ${_candidate}")
            break()
        endif()
    endforeach()
    if(NOT EXISTS ${POSIX_INCLUDE_LINK})
        message(WARNING "Could not find newlib includes for POSIX symlink. "
                        "Build may fail with: fatal error: ../include/time.h: No such file")
    endif()
endif()

# TI-RTOS kernel paths
set(TIRTOS7_PATH ${TI_SDK_PATH}/kernel/tirtos7)
set(SYSBIOS_PATH ${TIRTOS7_PATH}/packages/ti/sysbios)

# ─── Include paths ───

set(SDK_INCLUDE_DIRS
    ${TI_SDK_PATH}/source
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/inc
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/driverlib
    ${TI_SDK_PATH}/source/ti/drivers
    ${TI_SDK_PATH}/source/ti/drivers/rf
)

if(USE_TIRTOS)
    list(APPEND SDK_INCLUDE_DIRS
        ${TIRTOS7_PATH}/packages
        ${TI_SDK_PATH}/kernel/tirtos7/packages/ti/dpl
        ${TI_SDK_PATH}/source/ti/posix/tirtos
        # BLE5-Stack includes — added back in Fase 0.6 (GATT)
    )
else()
    list(APPEND SDK_INCLUDE_DIRS
        ${TI_SDK_PATH}/kernel/nortos
        ${TI_SDK_PATH}/kernel/nortos/dpl
        ${TI_SDK_PATH}/kernel/nortos/posix
    )
endif()

# ─── Libraries ───

# DriverLib — SDK 8.30 provides source, compile from source
file(GLOB DRIVERLIB_SOURCES
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/driverlib/*.c
)

# Full SDK install path (has precompiled libs)
set(TI_SDK_FULL "$ENV{HOME}/ti/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/simplelink_cc13xx_cc26xx_sdk_8_30_01_01"
    CACHE PATH "Full TI SDK install with precompiled libs")

# Precompiled libraries from full SDK
set(RF_DRIVER_LIB ${TI_SDK_FULL}/source/ti/drivers/rf/lib/gcc/m4f/rf_multiMode_cc13x2.a)
set(DRIVERS_LIB ${TI_SDK_FULL}/source/ti/drivers/lib/gcc/m4f/drivers_cc13x2x7.a)
set(DRIVERLIB_PREBUILT ${TI_SDK_FULL}/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/bin/gcc/driverlib.lib)
set(SYSBIOS_LIB ${TI_SDK_FULL}/kernel/tirtos7/packages/ti/sysbios/lib/gcc/m4f/sysbios.a)

# ─── Application sources ───

set(APP_SOURCES
    ccfg.c
    src/ble_conn.c
    src/ble_conn_mgr.c
    src/csa2.c
    src/command_processor.c
    src/control_task.c
    src/data_task.c
    src/host_if.c
    src/host_if_task.c
    src/ll_manager.c
    src/output_if.c
    src/packet_queue.c
    src/phy_manager.c
    src/protocol.c
    src/radio_if.c
    src/smartrf_ble5_0.c
    src/smartrf_ieee_15_4_0.c
    src/smartrf_prop_0.c
    src/att_client.c
    src/task_event.c
    src/tx_queue.c
)

# ─── TI Driver sources (common) ───

set(RF_PATCH_SOURCES
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/rf_patches/rf_patch_cpe_bt5.c
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/rf_patches/rf_patch_mce_bt5.c
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/rf_patches/rf_patch_cpe_multi_protocol.c
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/rf_patches/rf_patch_cpe_prop.c
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/rf_patches/rf_patch_mce_genook.c
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/rf_patches/rf_patch_rfe_genook.c
)

# Driver sources — now using precompiled libs instead
set(DRIVER_SOURCES "")

if(USE_TIRTOS)
    # ─── Using precompiled libs from full SDK install ───
    # No more source compilation of driverlib, sysbios, DPL, POSIX, drivers.
    # Only compile: RF patches + SysConfig config + app sources + stubs.

    set(RTOS_STUBS src/rtos_stubs.c)
    set(MAIN_SOURCE src/main_rtos.c)

    set(SYSCFG_SOURCES
        syscfg/ti_sysbios_config.c
        syscfg/ti_drivers_config.c
        syscfg/ti_radio_config_433.c
    )

    set(ALL_SOURCES
        ${APP_SOURCES}
        ${RF_PATCH_SOURCES}
        ${RTOS_STUBS}
        ${SYSCFG_SOURCES}
        ${MAIN_SOURCE}
    )

else()
    # ─── NoRTOS DPL sources ───
    set(DPL_SOURCES
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
    )

    set(MAIN_SOURCE src/main.c)

    set(ALL_SOURCES
        ${APP_SOURCES}
        ${DRIVERLIB_SOURCES}
        ${DRIVER_SOURCES}
        ${DPL_SOURCES}
        ${MAIN_SOURCE}
        src/startup_cc13x2_cc26x2_gcc.c
    )
endif()

# ─── Include directories ───

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/syscfg
    ${SDK_INCLUDE_DIRS}
)

# ─── Executable ───

add_executable(${CMAKE_PROJECT_NAME}.elf ${ALL_SOURCES})

target_compile_definitions(${CMAKE_PROJECT_NAME}.elf PRIVATE
    ${DEVICE_FAMILY_DEFINE}
)

if(USE_TIRTOS)
    target_compile_definitions(${CMAKE_PROJECT_NAME}.elf PRIVATE
        xdc_runtime_Log_DISABLE_ALL=1
    )
    # Force-include RTOS config
    target_compile_options(${CMAKE_PROJECT_NAME}.elf PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/syscfg/ti_sysbios_config.h>
        -I${TI_SDK_PATH}/source/ti/posix/gcc
    )
endif()

if(USE_TIRTOS)
    target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE
        ${RF_DRIVER_LIB}
        ${DRIVERS_LIB}
        -Wl,--whole-archive
        ${SYSBIOS_LIB}
        -Wl,--no-whole-archive
        ${DRIVERLIB_PREBUILT}
    )
else()
    target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE
        ${DRIVERLIB_LIB}
        ${RF_DRIVER_LIB}
    )
endif()

set_target_properties(${CMAKE_PROJECT_NAME}.elf PROPERTIES
    OUTPUT_NAME ${CMAKE_PROJECT_NAME}
    SUFFIX .elf
)

# Post-build
add_custom_command(TARGET ${CMAKE_PROJECT_NAME}.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary ${CMAKE_PROJECT_NAME}.elf ${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} ${CMAKE_PROJECT_NAME}.elf
    COMMENT "Generating hex and binary files for ${DEVICE_VARIANT}"
)
