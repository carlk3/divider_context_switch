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

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
//
#include "hardware/timer.h"
#include "pico/stdio.h"
#include "pico/stdlib.h"
//
#include "FreeRTOS.h"
#include "my_debug.h"
#include "semphr.h"
#include "task.h"
//
#include "my_debug.h"

static SemaphoreHandle_t xSemaphore;
static BaseType_t printf_locked;
static void lock_printf() {
    static StaticSemaphore_t xMutexBuffer;
    static bool initialized;
    if (!__atomic_test_and_set(&initialized, __ATOMIC_SEQ_CST)) {
        xSemaphore = xSemaphoreCreateMutexStatic(&xMutexBuffer);
    }
    configASSERT(xSemaphore);
    printf_locked = xSemaphoreTake(xSemaphore, pdMS_TO_TICKS(1000));
}
static void unlock_printf() {
    if (pdTRUE == printf_locked) xSemaphoreGive(xSemaphore);
}

void task_printf(const char *pcFormat, ...) {
    char pcBuffer[256] = {0};
    va_list xArgs;
    va_start(xArgs, pcFormat);
    vsnprintf(pcBuffer, sizeof pcBuffer, pcFormat, xArgs);
    va_end(xArgs);
    lock_printf();
    printf("%s: %s", pcTaskGetName(NULL), pcBuffer);
    fflush(stdout);
    unlock_printf();
}
void my_assert_func(const char *file, int line, const char *func,
                    const char *pred) {
    task_printf(
        "%s: assertion \"%s\" failed: file \"%s\", line %d, function: %s\n",
        pcTaskGetName(NULL), pred, file, line, func);
    fflush(stdout);
    vTaskSuspendAll();
    __asm volatile("cpsid i" : : : "memory"); /* Disable global interrupts. */
    while (1) {
        __asm("bkpt #0");
    };  // Stop in GUI as if at a breakpoint (if debugging, otherwise loop
        // forever)
}
void hexdump_8(const char *s, const uint8_t *pbytes, size_t nbytes) {
    lock_printf();
    printf("\n%s: %s(%s, 0x%p, %zu)\n", pcTaskGetName(NULL), __FUNCTION__, s,
           pbytes, nbytes);
    fflush(stdout);
    size_t col = 0;
    for (size_t byte_ix = 0; byte_ix < nbytes; ++byte_ix) {
        printf("%02hhx ", pbytes[byte_ix]);
        if (++col > 31) {
            printf("\n");
            col = 0;
        }
        fflush(stdout);
    }
    unlock_printf();
}
// nwords is size in bytes
bool compare_buffers_8(const char *s0, const uint8_t *pbytes0, const char *s1,
                       const uint8_t *pbytes1, const size_t nbytes) {
    /* Verify the data. */
    if (0 != memcmp(pbytes0, pbytes1, nbytes)) {
        hexdump_8(s0, pbytes0, nbytes);
        hexdump_8(s1, pbytes1, nbytes);
        return false;
    }
    return true;
}

/**********  FreeRTOS support stuff ***********/

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide
an implementation of vApplicationGetIdleTaskMemory() to provide the memory that
is used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize) {
    /* If the buffers to be provided to the Idle task are declared inside this
    function then they must be declared static – otherwise they will be
    allocated on the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE]
        __attribute__((aligned));

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*———————————————————–*/

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so
the application must provide an implementation of
vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize) {
    /* If the buffers to be provided to the Timer task are declared inside this
    function then they must be declared static – otherwise they will be
    allocated on the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH]
        __attribute__((aligned));

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task’s state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task’s stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    /* The stack space has been exceeded for a task, considering allocating
     * more. */
    printf("\nOut of stack space!\n");
    printf(pcTaskGetName(NULL));
    printf("\n");
    printf("\nOut of stack space! Task: %p %s\n", xTask, pcTaskName);
    __asm volatile("cpsid i" : : : "memory"); /* Disable global interrupts. */
    vTaskSuspendAll();
    while (1) {
        __asm("bkpt #0");
    };  // Stop in GUI as if at a breakpoint (if debugging, otherwise loop
        // forever)
}
void vApplicationMallocFailedHook(void) {
    printf("\nMalloc failed!\n");
    printf("\nMalloc failed! Task: %s\n", pcTaskGetName(NULL));
    __asm volatile("cpsid i" : : : "memory"); /* Disable global interrupts. */
    vTaskSuspendAll();
    while (1) {
        __asm("bkpt #0");
    };  // Stop in GUI as if at a breakpoint (if debugging, otherwise loop
        // forever)
}

void fail_func(const char *file, const int line, const char *function,
               const char *buf_name, uint8_t buf[], size_t buf_sz,
               unsigned seed, const char *fmt, ...) {
    gpio_put(9, 1);  // Trigger
    char pcBuffer[256] = {0};
    int n = snprintf(pcBuffer, sizeof pcBuffer, "%s:%d: %s\n: ", file, line,
                     function);
    va_list xArgs;
    va_start(xArgs, fmt);
    vsnprintf(pcBuffer + n, sizeof pcBuffer - n, fmt, xArgs);
    va_end(xArgs);
    lock_printf();
    fflush(stdout);
    printf("%s: %s", pcTaskGetName(NULL), pcBuffer);
    hexdump_8(buf_name, buf, buf_sz);
    printf("Expected:\n");
    unsigned rand_st = seed;
    size_t col = 0;
    for (size_t byte_ix = 0; byte_ix < buf_sz; ++byte_ix) {
        uint8_t x = rand_r(&rand_st);
        printf("%02hhx ", x);
        if (++col > 31) {
            printf("\n");
            col = 0;
        }
        fflush(stdout);
    }
    unlock_printf();
    vTaskSuspendAll();
    __asm volatile("cpsid i" : : : "memory");
    while (1) {
        __asm("bkpt #0");
    };
}

/* [] END OF FILE */
