/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include "pico/stdlib.h"
//
#include "FreeRTOS.h"
#include "task.h"
//
#include "my_debug.h"

// Passes:
//#define N_TASKS 1

// Fails
#define N_TASKS 2

// Passes
//#define N_TASKS 2

//#define N_TASKS 4

#define TEST_SIZE 1024

#define TRACE_PRINTF(fmt, args...)
//#define TRACE_PRINTF task_printf

static uint8_t txbufs[N_TASKS][TEST_SIZE];
static uint8_t rxbufs[N_TASKS][TEST_SIZE];

static void testTask(void *arg) {
    unsigned task_no = (unsigned)arg;
    task_printf("%s(task_no=%u)\n", __FUNCTION__, task_no);    

    for (size_t c = 0;; ++c) {
        unsigned seed = task_no + c;
        unsigned rand_st = seed;
        for (uint i = 0; i < TEST_SIZE; ++i)
            txbufs[task_no][i] = rand_r(&rand_st);

        memcpy(rxbufs[task_no], txbufs[task_no], TEST_SIZE);

        task_printf("Done. Checking...");
        // Has the transmit buffer changed?!
        rand_st = seed;
        for (uint i = 0; i < TEST_SIZE; ++i) {
            uint8_t x = rand_r(&rand_st);
            if (txbufs[task_no][i] != x) {
                FAIL("txbuf", txbufs[task_no], TEST_SIZE, seed,
                     "Mismatch at %d/%d: expected %02x, got %02x\n", i,
                     TEST_SIZE, x, txbufs[task_no][i]);
            }
        }
        // Check the receive buffer:
        rand_st = seed;
        for (uint i = 0; i < TEST_SIZE; ++i) {
            uint8_t x = rand_r(&rand_st);
            if (rxbufs[task_no][i] != x) {
                FAIL("rxbuf", rxbufs[task_no], TEST_SIZE, seed,
                     "Mismatch at %d/%d: expected %02x, got %02x\n", i,
                     TEST_SIZE, x, rxbufs[task_no][i]);
            }
        }
        // Compare the tx and rx buffers:
        rand_st = seed;
        for (uint i = 0; i < TEST_SIZE; ++i) {
            if (rxbufs[task_no][i] != txbufs[task_no][i]) {
                hexdump_8("txbuf", txbufs[task_no], TEST_SIZE);
                FAIL("rxbuf", rxbufs[task_no], TEST_SIZE, seed,
                     "Mismatch at %d/%d: expected %02x, got %02x\n", i,
                     TEST_SIZE, txbufs[task_no][i], rxbufs[task_no][i]);
            }
        }
        task_printf("All good\n");
        TaskStatus_t xTaskDetails;
        vTaskGetInfo(NULL, &xTaskDetails, pdTRUE, 0);
        task_printf("Stack High Water Mark: %hu\n",
                    xTaskDetails.usStackHighWaterMark);
    }  // for
    vTaskDelete(NULL);
}

int main() {
    // Enable UART so we can print status output
    stdio_init_all();

    printf("\033[2J\033[H");  // Clear Screen
    printf("example\n");

    gpio_init(7);  // Task 0
    gpio_set_dir(7, GPIO_OUT);
    gpio_init(8);  // Task 1
    gpio_set_dir(8, GPIO_OUT);
    gpio_init(9);  // Trigger
    gpio_set_dir(9, GPIO_OUT);

    for (size_t i = 0; i < N_TASKS; ++i) {
        char buf[16];
        snprintf(buf, sizeof buf, "T%zu", i);
        BaseType_t rc = xTaskCreate(testTask, buf, 1536, (void *)i, 2, NULL);
        configASSERT(pdPASS == rc);
    }

    vTaskStartScheduler();
    configASSERT(!"Can't happen!");
    return 0;
}
