/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Example of writing via DMA to the SPI interface and similarly reading it back
// via a loopback.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
//
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
//
#include "my_debug.h"

// Passes:
#define N_TASKS 1
#define BAUD_RATE 20833333

// Fails
//#define N_TASKS 2
//#define BAUD_RATE 5000 * 1000

// Passes
//#define N_TASKS 2
//#define BAUD_RATE 2000 * 1000

#define TEST_SIZE 1024

#define PIN_MISO 4
#define PIN_CS 5
#define PIN_SCK 2
#define PIN_MOSI 3
#define SPI_INST spi0

#define LED_PIN 25
#define SPI_FILL_CHAR (0xFF)

#define TRACE_PRINTF(fmt, args...)
//#define TRACE_PRINTF task_printf

// "Class" representing SPIs
typedef struct {
    spi_inst_t *hw_inst;
    const uint miso_gpio;  // SPI MISO pin number for GPIO
    const uint mosi_gpio;
    const uint sck_gpio;
    const uint baud_rate;
    uint tx_dma;
    uint rx_dma;
    dma_channel_config tx_dma_cfg;
    dma_channel_config rx_dma_cfg;
    irq_handler_t dma_isr;
    bool initialized;         // Assigned dynamically
    TaskHandle_t owner;       // Assigned dynamically
    SemaphoreHandle_t mutex;  // Assigned dynamically
} spi_t;

void spi0_dma_isr();

static spi_t spi = {
    .hw_inst = SPI_INST,  // SPI component
    .miso_gpio = PIN_MISO,
    .mosi_gpio = PIN_MOSI,
    .sck_gpio = PIN_SCK,
    .dma_isr = spi0_dma_isr,
    .owner = 0,  // Owning task, assigned dynamically
    .mutex = 0   // Guard semaphore, assigned dynamically
};

void spi_irq_handler(spi_t *this) {
    // Clear the interrupt request.
    dma_hw->ints0 = 1u << this->rx_dma;
    configASSERT(this->owner);
    configASSERT(!dma_channel_is_busy(this->rx_dma));

    /* The xHigherPriorityTaskWoken parameter must be initialized to pdFALSE as
     it will get set to pdTRUE inside the interrupt safe API function if a
     context switch is required. */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Send a notification directly to the task to which interrupt processing is
     being deferred. */
    vTaskNotifyGiveFromISR(this->owner,  // The handle of the task to which the
                                         // notification is being sent.
                           &xHigherPriorityTaskWoken);

    /* Pass the xHigherPriorityTaskWoken value into portYIELD_FROM_ISR().
    If xHigherPriorityTaskWoken was set to pdTRUE inside
     vTaskNotifyGiveFromISR() then calling portYIELD_FROM_ISR() will
     request a context switch. If xHigherPriorityTaskWoken is still
     pdFALSE then calling portYIELD_FROM_ISR() will have no effect. */
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void spi0_dma_isr() { spi_irq_handler(&spi); }

bool my_spi_init(spi_t *this) {
    // The SPI may be shared, protect it
    this->mutex = xSemaphoreCreateRecursiveMutex();

    /* Configure component */
    spi_init(this->hw_inst, 100 * 1000);
    spi_set_format(this->hw_inst, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    gpio_set_function(this->miso_gpio, GPIO_FUNC_SPI);
    gpio_set_function(this->mosi_gpio, GPIO_FUNC_SPI);
    gpio_set_function(this->sck_gpio, GPIO_FUNC_SPI);
    gpio_set_function(PIN_CS, GPIO_FUNC_SPI);

    // Force loopback for testing
    hw_set_bits(&spi_get_hw(SPI_INST)->cr1, SPI_SSPCR1_LBM_BITS);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Grab some unused dma channels
    this->tx_dma = dma_claim_unused_channel(true);
    this->rx_dma = dma_claim_unused_channel(true);

    this->tx_dma_cfg = dma_channel_get_default_config(this->tx_dma);
    this->rx_dma_cfg = dma_channel_get_default_config(this->rx_dma);
    channel_config_set_transfer_data_size(&this->tx_dma_cfg, DMA_SIZE_8);
    channel_config_set_transfer_data_size(&this->rx_dma_cfg, DMA_SIZE_8);

    // We set the outbound DMA to transfer from a memory buffer to the SPI
    // transmit FIFO paced by the SPI TX FIFO DREQ The default is for the
    // read address to increment every element (in this case 1 byte -
    // DMA_SIZE_8) and for the write address to remain unchanged.
    channel_config_set_dreq(&this->tx_dma_cfg, spi_get_index(this->hw_inst)
                                                   ? DREQ_SPI1_TX
                                                   : DREQ_SPI0_TX);
    channel_config_set_write_increment(&this->tx_dma_cfg, false);

    // We set the inbound DMA to transfer from the SPI receive FIFO to a
    // memory buffer paced by the SPI RX FIFO DREQ We coinfigure the read
    // address to remain unchanged for each element, but the write address
    // to increment (so data is written throughout the buffer)
    channel_config_set_dreq(&this->rx_dma_cfg, spi_get_index(this->hw_inst)
                                                   ? DREQ_SPI1_RX
                                                   : DREQ_SPI0_RX);
    channel_config_set_read_increment(&this->rx_dma_cfg, false);

    /* Theory: we only need an interrupt on rx complete,
    since if rx is complete, tx must also be complete. */

    // Configure the processor to run dma_handler() when DMA IRQ 0 is
    // asserted
    irq_set_exclusive_handler(DMA_IRQ_0, this->dma_isr);

    /* Any interrupt that uses interrupt-safe FreeRTOS API functions must also
     * execute at the priority defined by configKERNEL_INTERRUPT_PRIORITY. */
    irq_set_priority(DMA_IRQ_0, 0xFF);  // Lowest urgency.

    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    dma_channel_set_irq0_enabled(this->rx_dma, true);
    irq_set_enabled(DMA_IRQ_0, true);

    return true;
}
static void spi_lock(spi_t *this) {
    TRACE_PRINTF("%s\n", __FUNCTION__);
    configASSERT(this->mutex);
    xSemaphoreTakeRecursive(this->mutex, portMAX_DELAY);
    this->owner = xTaskGetCurrentTaskHandle();
    gpio_put(LED_PIN, 1);
}
static void spi_unlock(spi_t *this) {
    TRACE_PRINTF("%s\n", __FUNCTION__);
    gpio_put(LED_PIN, 0);
    this->owner = 0;
    xSemaphoreGiveRecursive(this->mutex);
}
// SPI Transfer: Read & Write (simultaneously) on SPI bus
//   If the data that will be received is not important, pass NULL as rx.
//   If the data that will be transmitted is not important,
//     pass NULL as tx and then the SPI_FILL_CHAR is sent out as each data
//     element.
bool spi_transfer(spi_t *this, const uint8_t *tx, uint8_t *rx, size_t length) {
    TRACE_PRINTF("%s\n", __FUNCTION__);
    configASSERT(xTaskGetCurrentTaskHandle() == this->owner);
    configASSERT(tx || rx);

    // tx write increment is already false
    if (tx) {
        channel_config_set_read_increment(&this->tx_dma_cfg, true);
    } else {
        const static uint8_t dummy = SPI_FILL_CHAR;
        tx = &dummy;
        channel_config_set_read_increment(&this->tx_dma_cfg, false);
    }
    // rx read increment is already false
    if (rx) {
        channel_config_set_write_increment(&this->rx_dma_cfg, true);
    } else {
        static uint8_t dummy = 0xA5;
        rx = &dummy;
        channel_config_set_write_increment(&this->rx_dma_cfg, false);
    }
    /* Ensure this task does not already have a notification pending by calling
     ulTaskNotifyTake() with the xClearCountOnExit parameter set to pdTRUE, and
     a block time of 0 (don't block). */
    BaseType_t rc = ulTaskNotifyTake(pdTRUE, 0);
    configASSERT(!rc);

    dma_channel_configure(this->tx_dma, &this->tx_dma_cfg,
                          &spi_get_hw(this->hw_inst)->dr,  // write address
                          tx,                              // read address
                          length,  // element count (each element is of
                                   // size transfer_data_size)
                          false);  // start
    dma_channel_configure(this->rx_dma, &this->rx_dma_cfg,
                          rx,                              // write address
                          &spi_get_hw(this->hw_inst)->dr,  // read address
                          length,  // element count (each element is of
                                   // size transfer_data_size)
                          false);  // start

    // start them exactly simultaneously to avoid races (in extreme cases
    // the FIFO could overflow)
    dma_start_channel_mask((1u << this->tx_dma) | (1u << this->rx_dma));

    /* Timeout 1 sec */
    uint32_t timeOut = 1000;
    /* Wait until master completes transfer or time out has occured. */
    rc = ulTaskNotifyTake(
        pdFALSE, pdMS_TO_TICKS(timeOut));  // Wait for notification from ISR
    if (!rc) {
        // This indicates that xTaskNotifyWait() returned without the
        // calling task receiving a task notification. The calling task will
        // have been held in the Blocked state to wait for its notification
        // state to become pending, but the specified block time expired
        // before that happened.
        task_printf("Task %s timed out in %s\n",
                    pcTaskGetName(xTaskGetCurrentTaskHandle()), __FUNCTION__);
        return false;
    }
    dma_channel_wait_for_finish_blocking(this->tx_dma);
    dma_channel_wait_for_finish_blocking(this->rx_dma);
    configASSERT(!dma_channel_is_busy(this->tx_dma));
    configASSERT(!dma_channel_is_busy(this->rx_dma));

    return true;
}
static void testTask(void *arg) {
    TRACE_PRINTF("%s\n", __FUNCTION__);
    unsigned task_no = (unsigned)arg;

    uint8_t txbuf[TEST_SIZE];
    uint8_t rxbuf[TEST_SIZE];

    for (size_t x = 0; ; ++x) {
        unsigned seed = task_no + x;
        unsigned rand_st = seed;
        for (uint i = 0; i < TEST_SIZE; ++i) {
            txbuf[i] = rand_r(&rand_st);
        }
        spi_lock(&spi);
        spi_transfer(&spi, txbuf, rxbuf, TEST_SIZE);
        spi_unlock(&spi);

        task_printf("Done. Checking...");
        if (0 != memcmp(txbuf, rxbuf, TEST_SIZE))
            for (uint i = 0; i < TEST_SIZE; ++i) {
                if (rxbuf[i] != txbuf[i]) {
                    FAIL("Mismatch at %d/%d: expected %02x, got %02x\n", i,
                         TEST_SIZE, txbuf[i], rxbuf[i]);
                }
            }
        rand_st = seed;
        for (uint i = 0; i < TEST_SIZE; ++i) {
            uint8_t x = rand_r(&rand_st);
            if (txbuf[i] != x) {
                FAIL("Mismatch at %d/%d: expected %02x, got %02x\n", i, TEST_SIZE,
                     x, txbuf[i]);
            }
        }
        rand_st = seed;
        for (uint i = 0; i < TEST_SIZE; ++i) {
            uint8_t x = rand_r(&rand_st);
            if (rxbuf[i] != x) {
                FAIL("Mismatch at %d/%d: expected %02x, got %02x\n", i, TEST_SIZE,
                     x, rxbuf[i]);
            }
        }
        task_printf("All good\n");
    }  // for
    vTaskDelete(NULL);
}

void main() {
    // Enable UART so we can print status output
    stdio_init_all();

    printf("\033[2J\033[H");  // Clear Screen
    printf("SPI DMA example\n");

    my_spi_init(&spi);

    uint actual = spi_set_baudrate(spi.hw_inst, BAUD_RATE);
    printf("Actual frequency: %lu\n", (long unsigned)actual);

   for (size_t i = 0; i < N_TASKS; ++i) {
        char buf[16];
        snprintf(buf, sizeof buf, "T%zu", i);
        BaseType_t rc = xTaskCreate(testTask, buf, 1536, (void *)i, 2, NULL);
        configASSERT(pdPASS == rc);
    }

    vTaskStartScheduler();
    configASSERT(!"Can't happen!");
}
