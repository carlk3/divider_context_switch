# divider_context_switch
Demonstrating a problem with context switching on Raspberry Pi Pico.

Install with steps like these:
* `git clone --recurse-submodules git@github.com:carlk3/divider_context_switch.git`
* `cd divider_context_switch`
* `mkdir build`
* `cd build`
* `cmake ..`
* `make`

You can change the behavior by toggling the commented target_sources lines in CMakeLists.txt:
```
#To see the problem:        
        ${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Kernel/portable/GCC/ARM_CM0/port.c 
#To see the fix:        
        #${CMAKE_CURRENT_LIST_DIR}/port.c 
```
and rebuilding.
