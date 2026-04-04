cmake_minimum_required(VERSION 3.20)

# SDK path (set before project)
set(TI_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/simplelink_cc13xx_cc26xx_sdk_7_10_01_24"
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
        # BLE5-Stack includes
        ${TI_SDK_PATH}/source/ti/ble5stack/inc
        ${TI_SDK_PATH}/source/ti/ble5stack/common/cc26xx
        ${TI_SDK_PATH}/source/ti/ble5stack/icall/inc
        ${TI_SDK_PATH}/source/ti/ble5stack/icall/src/inc
        ${TI_SDK_PATH}/source/ti/ble5stack/osal/src/inc
        ${TI_SDK_PATH}/source/ti/ble5stack/hal/src/inc
        ${TI_SDK_PATH}/source/ti/ble5stack/hal/src/target/_common
        ${TI_SDK_PATH}/source/ti/ble5stack/common/cc26xx/npi/stack
        ${TI_SDK_PATH}/source/ti/common/nv
        ${TI_SDK_PATH}/source/ti/ble5stack/rom
    )
else()
    list(APPEND SDK_INCLUDE_DIRS
        ${TI_SDK_PATH}/kernel/nortos
        ${TI_SDK_PATH}/kernel/nortos/dpl
        ${TI_SDK_PATH}/kernel/nortos/posix
    )
endif()

# ─── Libraries ───

set(DRIVERLIB_LIB
    ${TI_SDK_PATH}/source/ti/devices/${DEVICE_SUBFAMILY}/driverlib/bin/gcc/driverlib.lib
)
set(RF_MULTI_MODE_LIB
    ${TI_SDK_PATH}/source/ti/drivers/rf/lib/gcc/m4f/rf_multiMode_cc13x2.a
)

# BLE5-Stack libraries (Phase M2)
set(BLE5_LIBS_DIR ${TI_SDK_PATH}/source/ti/ble5stack/libraries/cc1352p)
set(BLE5_ROM_LIB ${TI_SDK_PATH}/source/ti/ble5stack/rom/enc_lib/cc26xx_ecc_rom_api.a)

# ─── Application sources (common to both RTOS modes) ───

set(APP_SOURCES
    ccfg.c
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
    src/ti_rf_config_min.c
    src/task_event.c
)

# ─── TI Driver sources (common) ───

set(DRIVER_SOURCES
    ${TI_SDK_PATH}/source/ti/drivers/ITM.c
    ${TI_SDK_PATH}/source/ti/drivers/Temperature.c
    ${TI_SDK_PATH}/source/ti/drivers/utils/List.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2_helpers.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2_calibrateRCOSC.c
    ${TI_SDK_PATH}/source/ti/drivers/power/PowerCC26X2_calibrateRCOSC_helpers.c
    ${TI_SDK_PATH}/source/ti/drivers/temperature/TemperatureCC26X2.c
)

if(USE_TIRTOS)
    # ─── TI-RTOS7 Kernel sources ───
    set(RTOS_KERNEL_SOURCES
        # Core
        ${SYSBIOS_PATH}/BIOS.c
        # Knl (scheduler)
        ${SYSBIOS_PATH}/knl/Clock.c
        ${SYSBIOS_PATH}/knl/Event.c
        ${SYSBIOS_PATH}/knl/Idle.c
        ${SYSBIOS_PATH}/knl/Mailbox.c
        ${SYSBIOS_PATH}/knl/Queue.c
        ${SYSBIOS_PATH}/knl/Semaphore.c
        ${SYSBIOS_PATH}/knl/Swi.c
        ${SYSBIOS_PATH}/knl/Task.c
        # Family ARM M3/M4F (CC1352 = Cortex-M4F)
        ${SYSBIOS_PATH}/family/arm/m3/Hwi.c
        ${SYSBIOS_PATH}/family/arm/m3/TaskSupport.c
        # Family CC26xx (timer, clock, boot)
        ${SYSBIOS_PATH}/family/arm/cc26xx/Boot.c
        ${SYSBIOS_PATH}/family/arm/cc26xx/ClockSupport.c
        ${SYSBIOS_PATH}/family/arm/cc26xx/Seconds.c
        ${SYSBIOS_PATH}/family/arm/cc26xx/Timer.c
        ${SYSBIOS_PATH}/family/arm/cc26xx/TimestampProvider.c
        # Runtime
        ${SYSBIOS_PATH}/runtime/Assert.c
        ${SYSBIOS_PATH}/runtime/Error.c
        ${SYSBIOS_PATH}/runtime/Memory.c
        ${SYSBIOS_PATH}/runtime/Startup.c
        ${SYSBIOS_PATH}/runtime/SysCallback.c
        # SysMin.c excluded — SysCallback.c provides SystemSupport functions
        ${SYSBIOS_PATH}/runtime/System.c
        ${SYSBIOS_PATH}/runtime/Timestamp.c
        # Gates
        ${SYSBIOS_PATH}/gates/GateHwi.c
        ${SYSBIOS_PATH}/gates/GateMutex.c
        ${SYSBIOS_PATH}/gates/GateMutexPri.c
        ${SYSBIOS_PATH}/gates/GateSwi.c
        ${SYSBIOS_PATH}/gates/GateTask.c
        # Heaps
        ${SYSBIOS_PATH}/heaps/HeapMem.c
        ${SYSBIOS_PATH}/heaps/HeapCallback.c
        # HAL
        ${SYSBIOS_PATH}/hal/time.c
        # RTS
        ${SYSBIOS_PATH}/rts/MemAlloc.c
        # Utils
        ${SYSBIOS_PATH}/utils/Load.c
    )

    # Assembly files (GCC specific)
    set(RTOS_ASM_SOURCES
        ${SYSBIOS_PATH}/family/arm/m3/Hwi_asm_gcc.s
        ${SYSBIOS_PATH}/family/arm/m3/Hwi_asm_switch_gcc.s
        ${SYSBIOS_PATH}/family/arm/m3/TaskSupport_asm_gcc.s
        ${SYSBIOS_PATH}/family/arm/vxm/boot/boot_gcc.s
        ${SYSBIOS_PATH}/family/arm/vxm/boot/startup_gcc.c
    )

    # DPL for TI-RTOS
    set(DPL_SOURCES
        ${TIRTOS7_PATH}/packages/ti/dpl/ClockP_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/DebugP_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/HwiP_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/MutexP_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/SemaphoreP_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/SwiP_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/SystemP_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/PowerCC26X2_tirtos.c
        ${TIRTOS7_PATH}/packages/ti/dpl/TimestampPCC26XX_tirtos.c
        # Additional DPL not in base list
        ${TIRTOS7_PATH}/packages/ti/dpl/TaskP_tirtos.c
    )

    # ICall sources (BLE stack messaging)
    set(ICALL_SOURCES
        ${TI_SDK_PATH}/source/ti/ble5stack/icall/src/icall.c
        ${TI_SDK_PATH}/source/ti/ble5stack/icall/src/icall_cc2650.c
        ${TI_SDK_PATH}/source/ti/ble5stack/icall/app/icall_hci_tl.c
    )

    # POSIX layer
    set(POSIX_SOURCES
        ${TI_SDK_PATH}/source/ti/posix/tirtos/clock.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/pthread.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/pthread_barrier.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/pthread_cond.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/pthread_key.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/pthread_mutex.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/pthread_rwlock.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/pthread_util.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/sched.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/semaphore.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/sleep.c
        ${TI_SDK_PATH}/source/ti/posix/tirtos/timer.c
    )

    # Additional kernel sources from unity build
    list(APPEND RTOS_KERNEL_SOURCES
        ${SYSBIOS_PATH}/heaps/HeapCallback.c
    )

    # SysConfig generated files
    set(SYSCFG_SOURCES
        syscfg/ti_sysbios_config.c
        # syscfg/ti_ble_config.c — SDK header bug with spaces in path, using stubs
    )

    set(RTOS_STUBS src/rtos_stubs.c)
    set(MAIN_SOURCE src/main_rtos.c)

    set(ALL_SOURCES
        ${APP_SOURCES}
        ${DRIVER_SOURCES}
        ${RTOS_KERNEL_SOURCES}
        ${RTOS_ASM_SOURCES}
        ${DPL_SOURCES}
        # POSIX_SOURCES omitted for Phase M1 — added in M2 with BLE stack
        ${ICALL_SOURCES}
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
        ICALL_JT
        ICALL_LITE
        CC13XX
        CC13X2P
        SYSCFG
    )
    # Force-include RTOS config + BLE include paths
    target_compile_options(${CMAKE_PROJECT_NAME}.elf PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/syscfg/ti_sysbios_config.h>
        -I${TI_SDK_PATH}/source/ti/ble5stack/hal/src/inc
        -I${TI_SDK_PATH}/source/ti/ble5stack/hal/src/target/_common
        -I${TI_SDK_PATH}/source/ti/ble5stack/icall/src/inc
        -I${TI_SDK_PATH}/source/ti/ble5stack/icall/inc
        -I${TI_SDK_PATH}/source/ti/common/nv
        -I${TI_SDK_PATH}/source/ti/ble5stack/controller/cc26xx/inc
        -I${TI_SDK_PATH}/source/ti/ble5stack/heapmgr
    )
endif()

if(USE_TIRTOS)
    target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE
        ${DRIVERLIB_LIB}
        ${RF_MULTI_MODE_LIB}
        -Wl,--whole-archive
        ${BLE5_LIBS_DIR}/OneLib.a
        ${BLE5_LIBS_DIR}/StackWrapper.a
        -Wl,--no-whole-archive
        ${BLE5_ROM_LIB}
    )
else()
    target_link_libraries(${CMAKE_PROJECT_NAME}.elf PRIVATE
        ${DRIVERLIB_LIB}
        ${RF_MULTI_MODE_LIB}
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
