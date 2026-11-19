# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated CMake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == Raspberry Pi Pico VS Code Extension setup ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)

set(picoVscode "${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake")

if(EXISTS "${picoVscode}")
    include("${picoVscode}")
endif()
# ==============================================

set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Must be before project().
include(pico_sdk_import.cmake)

project(projekti C CXX ASM)

# Select the project's Mbed TLS configuration before SDK initialization.
set(PICO_MBEDTLS_CONFIG_FILE
    "${CMAKE_CURRENT_LIST_DIR}/mbedtls_config.h"
)
# Initialise the Raspberry Pi Pico SDK.
pico_sdk_init()

# Main program.
add_executable(projekti
    main.c
    aws_client.c
    ntp_clock.c
    lcd.c
    dht22.c
    adc_sensors.c
    storage.c
    history_send.c
    network.c
    http_server.c
)

pico_set_program_name(projekti "projekti")
pico_set_program_version(projekti "0.1")

# USB serial output enabled, UART output disabled.
pico_enable_stdio_uart(projekti 0)
pico_enable_stdio_usb(projekti 1)

# Finds the project's lwipopts.h.
target_include_directories(projekti PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Libraries required by the latest main.c.
# pico_lwip_mbedtls ja pico_mbedtls tarvitaan HTTPS/TLS-lähetykseen.
# Ilman niitä altcp_tls.h-funktiot eivät linkity.
target_link_libraries(projekti
    pico_stdlib
    pico_flash

    hardware_adc
    hardware_flash
    hardware_i2c
    hardware_sync

    pico_cyw43_arch_lwip_poll
    pico_mbedtls
    pico_lwip_mbedtls
    pico_lwip_sntp
    pico_rand

    m
)

# Generate additional output files, including UF2.
pico_add_extra_outputs(projekti)