cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(test C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise the Pico SDK
pico_sdk_init()

add_executable(test
        test.c
        my_debug.c
)
target_compile_options(test PRIVATE -Wall -Wextra -Wshadow)

pico_set_program_name(test "test")
pico_set_program_version(test "0.1")

pico_enable_stdio_uart(test 1)
pico_enable_stdio_usb(test 0)        

add_library(FreeRTOS-Kernel INTERFACE)
target_sources(FreeRTOS-Kernel INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/event_groups.c
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/list.c
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/queue.c
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/stream_buffer.c
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/tasks.c
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/timers.c 
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/MemMang/heap_4.c 
#To see the problem:        
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/GCC/ARM_CM0/port.c 
#To see the fix:        
        #${CMAKE_CURRENT_LIST_DIR}/port.c 
        )
target_link_libraries(FreeRTOS-Kernel INTERFACE 
        hardware_timer
        pico_stdlib 
)
target_include_directories(FreeRTOS-Kernel INTERFACE  
        include/ 
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/include 
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/GCC/ARM_CM0
)
target_include_directories(test PUBLIC 
        include/ 
)
target_link_libraries(test 
        FreeRTOS-Kernel
        pico_stdlib 
)

# create map/bin/hex file etc.
pico_add_extra_outputs(test)
