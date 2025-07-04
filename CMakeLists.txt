# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(LuzesInteligentes C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(LuzesInteligentes LuzesInteligentes.c lib/matriz.c)

pico_set_program_name(LuzesInteligentes "LuzesInteligentes")
pico_set_program_version(LuzesInteligentes "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(LuzesInteligentes 1)
pico_enable_stdio_usb(LuzesInteligentes 1)

# Add the standard library to the build
target_link_libraries(LuzesInteligentes
        pico_stdlib
        hardware_adc
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mqtt
        pico_mbedtls
        pico_lwip_mbedtls
        hardware_pio
        )

# Add the standard include files to the build
target_include_directories(LuzesInteligentes PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

#Gerar arquivo cabeçalho pio
pico_generate_pio_header(${CMAKE_PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/lib/pio_matrix.pio)


pico_add_extra_outputs(LuzesInteligentes)

