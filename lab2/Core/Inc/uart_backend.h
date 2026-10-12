#ifndef UART_BACKEND_H
#define UART_BACKEND_H
#include "uart_driver.h"
UartStatus backend_init(void);
UartStatus backend_mode(bool interrupts);
UartStatus backend_receive(uint8_t *byte);
UartStatus backend_transmit(uint8_t *byte);
bool backend_idle(void);
void backend_service(void);
void backend_irq(void);
/* Backend calls these from interrupt context, serialized with foreground. */
void uart_received(uint8_t byte);
void uart_transmitted(void);
void uart_hardware_error(void);
#endif
