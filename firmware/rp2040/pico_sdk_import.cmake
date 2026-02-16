# pico_sdk_import.cmake
# This file is based on the official Raspberry Pi Pico SDK template

# PICO_SDK_PATH can be set in environment or as cmake argument
if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    message("Using PICO_SDK_PATH from environment ('${PICO_SDK_PATH}')")
endif ()

if (NOT PICO_SDK_PATH)
    # Try to find SDK in common locations
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/pico-sdk")
        set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../sdk/pico-sdk")
        message("Using PICO_SDK_PATH from relative path ('${PICO_SDK_PATH}')")
    elseif (EXISTS "/usr/share/pico-sdk")
        set(PICO_SDK_PATH "/usr/share/pico-sdk")
        message("Using PICO_SDK_PATH from /usr/share ('${PICO_SDK_PATH}')")
    else ()
        message(FATAL_ERROR "PICO_SDK_PATH not defined. Please set it to the path of the Pico SDK.")
    endif ()
endif ()

get_filename_component(PICO_SDK_PATH "${PICO_SDK_PATH}" ABSOLUTE BASE_DIR "${CMAKE_BINARY_DIR}")
if (NOT EXISTS ${PICO_SDK_PATH})
    message(FATAL_ERROR "Directory '${PICO_SDK_PATH}' not found")
endif ()

set(PICO_SDK_INIT_CMAKE_FILE ${PICO_SDK_PATH}/pico_sdk_init.cmake)
if (NOT EXISTS ${PICO_SDK_INIT_CMAKE_FILE})
    message(FATAL_ERROR "Could not find '${PICO_SDK_INIT_CMAKE_FILE}'")
endif ()

include(${PICO_SDK_INIT_CMAKE_FILE})
