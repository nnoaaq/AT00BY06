cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(prac C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_compile_options(-Wall)

add_executable(prac
    dht22_pico.c
    ds18b20_pico.c
    measurements.c
    json_pico.c
    pico_wifi_scan.c
    main.c
    sensor_tasks.c
    tls.c
    aws.c
    ntp.c
)
add_subdirectory(drivers/ds18b20/onewire_library)

target_include_directories(prac PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/drivers/ds18b20
)

target_link_libraries(prac
    pico_stdlib
    hardware_gpio
    hardware_pio
    hardware_vreg
    hardware_clocks
    pico_cyw43_arch_lwip_poll
    onewire_library
    pico_mbedtls
    pico_lwip_mbedtls
)

pico_enable_stdio_usb(prac 1)
pico_enable_stdio_uart(prac 0)

pico_add_extra_outputs(prac)

get_filename_component(TOOLCHAIN_DIR ${CMAKE_C_COMPILER} DIRECTORY)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${TOOLCHAIN_DIR}/arm-none-eabi-size" --format=berkeley $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Muistinkulutus (Flash ja RAM):"
)
