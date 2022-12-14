cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_i2s_mic_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(pico_i2s_mic
  main.c
  mic_i2s.c
)

pico_generate_pio_header(pico_i2s_mic ${CMAKE_CURRENT_LIST_DIR}/mic_i2s.pio)

target_link_libraries(pico_i2s_mic
  pico_stdlib
  hardware_dma
  hardware_pio
  hardware_irq
)

pico_enable_stdio_usb(pico_i2s_mic 1)
pico_add_extra_outputs(pico_i2s_mic)
