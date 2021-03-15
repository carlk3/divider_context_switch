cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(spi_dma C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise the Pico SDK
pico_sdk_init()

add_executable(spi_dma
        spi_dma.c
        my_debug.c
)
target_compile_options(spi_dma PRIVATE -Wall -Wextra -Wshadow)

pico_set_program_name(spi_dma "spi_dma")
pico_set_program_version(spi_dma "0.1")

pico_enable_stdio_uart(spi_dma 1)
pico_enable_stdio_usb(spi_dma 0)        

add_library(FreeRTOS-Kernel STATIC 
        FreeRTOS-Kernel/event_groups.c
        FreeRTOS-Kernel/list.c
        FreeRTOS-Kernel/queue.c
        FreeRTOS-Kernel/stream_buffer.c
        FreeRTOS-Kernel/tasks.c
        FreeRTOS-Kernel/timers.c 
        FreeRTOS-Kernel/portable/GCC/ARM_CM0/port.c 
        FreeRTOS-Kernel/portable/MemMang/heap_4.c 
)
target_link_libraries(FreeRTOS-Kernel 
        hardware_timer
        pico_stdlib 
)
target_include_directories(FreeRTOS-Kernel PUBLIC 
        include/ 
        FreeRTOS-Kernel/include 
        FreeRTOS-Kernel/portable/GCC/ARM_CM0
)
target_include_directories(spi_dma PUBLIC 
        include/ 
        FreeRTOS-Kernel/include 
        FreeRTOS-Kernel/portable/GCC/ARM_CM0
)
target_link_libraries(spi_dma 
        FreeRTOS-Kernel
        pico_stdlib 
)

# create map/bin/hex file etc.
pico_add_extra_outputs(spi_dma)
