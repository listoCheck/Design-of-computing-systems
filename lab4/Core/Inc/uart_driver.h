#ifndef UART_DRIVER_H
#define UART_DRIVER_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifndef UART_RX_CAPACITY
#define UART_RX_CAPACITY 256U
#endif
#ifndef UART_TX_CAPACITY
#define UART_TX_CAPACITY 256U
#endif
#ifndef UART_START_INTERRUPTS
#define UART_START_INTERRUPTS 1
#endif
typedef enum { UART_OK, UART_EMPTY, UART_FULL, UART_BUSY, UART_ERROR } UartStatus;
typedef struct {
    uint32_t rx_overflow, hardware_errors, tx_errors;
    size_t rx_count, tx_count;
    bool interrupts, transmitting;
} UartState;
/* Single USART6 instance, 57600 8N1; APIs called from foreground only. */
UartStatus uart_init(bool interrupts);
UartStatus uart_read(uint8_t *byte);
/* Atomic acceptance: FULL accepts no bytes; caller retains and retries data. */
UartStatus uart_write(const uint8_t *data, size_t length);
UartStatus uart_set_interrupts(bool enabled);
UartState uart_state(void);
/* Call each loop, including while switching modes. */
void uart_service(void);
void uart_irq(void);
#endif
