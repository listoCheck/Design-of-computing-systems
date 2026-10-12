#include "stm32f4xx.h"
#include "uart_driver.h"
#include "uart_backend.h"
#include "queue.h"
_Static_assert(UART_RX_CAPACITY > 0 && UART_TX_CAPACITY > 0, "nonempty UART queues required");
static uint8_t rx_storage[UART_RX_CAPACITY], tx_storage[UART_TX_CAPACITY], tx_byte;
static ByteQueue rx, tx;
static volatile bool interrupt_mode, tx_active;
static volatile uint32_t overflows, errors, tx_errors;
static uint32_t lock(void) { uint32_t key = __get_PRIMASK(); __disable_irq(); return key; }
static void unlock(uint32_t key) { __set_PRIMASK(key); }
static void start_tx(void)
{
    if (tx_active || !queue_peek(&tx, &tx_byte)) return;
    tx_active = true; /* stable staging byte until completion */
    UartStatus s = backend_transmit(&tx_byte);
    if (s == UART_OK) {
        uint8_t discarded;
        (void)queue_pop(&tx, &discarded);
        if (!interrupt_mode) tx_active = false;
    } else {
        tx_active = false;
        if (s == UART_ERROR) ++tx_errors;
    }
}
UartStatus uart_init(bool interrupts)
{
    queue_init(&rx, rx_storage, sizeof rx_storage);
    queue_init(&tx, tx_storage, sizeof tx_storage);
    tx_active = false;
    overflows = errors = tx_errors = 0;
    interrupt_mode = interrupts;
    if (backend_init() != UART_OK) return UART_ERROR;
    return backend_mode(interrupts);
}
void uart_received(uint8_t byte) { if (!queue_push(&rx, byte)) ++overflows; }
void uart_transmitted(void) { tx_active = false; start_tx(); }
void uart_hardware_error(void) { ++errors; }
UartStatus uart_read(uint8_t *byte)
{
    if (!byte) return UART_ERROR;
    uint32_t key = lock();
    bool ok = queue_pop(&rx, byte);
    if (!ok && !interrupt_mode) {
        UartStatus result = backend_receive(byte);
        unlock(key);
        return result;
    }
    unlock(key);
    return ok ? UART_OK : UART_EMPTY;
}
UartStatus uart_write(const uint8_t *data, size_t length)
{
    if (!data && length) return UART_ERROR;
    uint32_t key = lock();
    if (length > tx.capacity - tx.count) { unlock(key); return UART_FULL; }
    for (size_t i = 0; i < length; ++i) (void)queue_push(&tx, data[i]);
    /* Polling transmit runs in service with interrupts enabled (HAL timebase). */
    if (interrupt_mode) start_tx();
    unlock(key);
    return UART_OK;
}
void uart_service(void)
{
    backend_service();
    if (!interrupt_mode) {
        uint8_t byte;
        if (backend_receive(&byte) == UART_OK) uart_received(byte);
        start_tx();
    } else {
        uint32_t key = lock(); start_tx(); unlock(key);
    }
}
UartStatus uart_set_interrupts(bool enabled)
{
    uint32_t key = lock();
    if (enabled == interrupt_mode) { unlock(key); return UART_OK; }
    if (tx.count || tx_active || !backend_idle()) { unlock(key); return UART_BUSY; }
    UartStatus s = backend_mode(enabled);
    if (s == UART_OK) interrupt_mode = enabled;
    unlock(key);
    return s;
}
UartState uart_state(void)
{
    uint32_t key = lock();
    UartState s = {overflows, errors, tx_errors, rx.count, tx.count, interrupt_mode, tx_active};
    unlock(key); return s;
}
void uart_irq(void) { backend_irq(); }
