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

#define FAIL(buf_name, buf, buf_sz, seed, fmt, ...)                               \
    fail_func(__FILE__, __LINE__, __FUNCTION__, buf_name, buf, buf_sz, seed, fmt, \
              __VA_ARGS__);
void fail_func(const char *file, const int line, const char *function,
               const char *buf_name, uint8_t buf[], size_t buf_sz,
               unsigned seed, const char *fmt, ...);

void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes);
bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, const size_t nbytes);

/* [] END OF FILE */
