/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
 */

#pragma once
#include <stdio.h>
#include <string.h>

void task_printf(const char *pcFormat, ...)
    __attribute__((format(__printf__, 1, 2)));

// See FreeRTOSConfig.h
void my_assert_func(const char *file, int line, const char *func,
                    const char *pred);

#define FAIL(fmt, ...)                                                 \
    {                                                                  \
        task_printf(fmt, __VA_ARGS__);                                 \
        task_printf("\tat %s:%d: %s\n", __FILE__, __LINE__, __func__); \
        fflush(stdout);                                                \
        vTaskSuspendAll();                                             \
        __asm volatile("cpsid i" : : : "memory");                      \
        while (1) {                                                    \
            __asm("bkpt #0");                                          \
        };                                                             \
    }

void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes);
bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, const size_t nbytes);

/* [] END OF FILE */
