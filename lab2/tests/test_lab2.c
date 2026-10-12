#include "stm32f4xx_hal.h"
#include "console.h"
#include "uart_driver.h"
#include "button.h"
#include <string.h>
volatile uint32_t test_done, test_failure, test_checks;
#define CHECK(x) do { ++test_checks; if (!(x)) { test_failure = __LINE__; test_done = 1; for (;;) {} } } while (0)
static void drain(Console *c, char *text)
{
    uint8_t byte; size_t n = 0;
    while (queue_pop(&c->output, &byte)) text[n++] = (char)byte;
    text[n] = 0;
}
static char output[CONSOLE_OUTPUT_SIZE + 1];
static Console console;
static void command(const char *text)
{
    for (; *text; ++text) { CHECK(console_can_receive(&console)); console_receive(&console, (uint8_t)*text); }
    drain(&console, output);
}
static void app_tests(void)
{
    console_init(&console, 0, true); drain(&console, output);
    command("?\r"); CHECK(strstr(output, "red, mode 1, timeout 12, I") != 0);
    CHECK(output[0] == '?' && output[1] == '\r');
    command("\n"); CHECK(!strcmp(output, "\n")); /* CRLF: one command, exact echo */
    command("set mode 2\n"); CHECK(!console.traffic.button_enabled); CHECK(strstr(output, "OK") != 0);
    traffic_update(&console.traffic, true, 4000); CHECK(!console.traffic.request);
    command("set mode 1\r"); CHECK(console.traffic.button_enabled);
    command("set timeout 20\r"); CHECK(console.traffic.timeout_s == 20);
    command("set timeout 0\r"); CHECK(strstr(output, "ERROR") != 0); CHECK(console.traffic.timeout_s == 20);
    command("set timeout -1\r"); CHECK(strstr(output, "ERROR") != 0);
    command("set timeout 2147483\r"); CHECK(console.traffic.timeout_s == 2147483);
    command("set timeout 2147484\r"); CHECK(strstr(output, "ERROR") != 0);
    command("set timeout 999999999999999999\r"); CHECK(strstr(output, "ERROR") != 0);
    command("set timeout 12x\r"); CHECK(strstr(output, "ERROR") != 0);
    command("set mode 3\r"); CHECK(strstr(output, "unsupported") != 0);
    command("?x\r"); CHECK(strstr(output, "unsupported") != 0);
    command("set interrupts off\r"); CHECK(console.mode_pending); CHECK(!console.requested_interrupts);
    CHECK(!console_can_receive(&console));
    console.mode_pending = false; console.interrupts = false;
    command("?\r"); CHECK(strstr(output, ", P") != 0);
    command("set interrupts on\r"); CHECK(console.mode_pending && console.requested_interrupts);
    console.mode_pending = false; console.interrupts = true;
    command("set interrupts on\r"); CHECK(!console.mode_pending);
    command("set timeout 12\r");
    for (unsigned i = 0; i < 100; ++i) {
        CHECK(console_can_receive(&console)); console_receive(&console, 'a'); drain(&console, output);
        CHECK(!strcmp(output, "a"));
    }
    command("\r"); CHECK(strstr(output, "too long") != 0);
    command("?\r"); CHECK(strstr(output, "timeout 12") != 0);
    console_receive(&console, '?'); console_receive(&console, 0); console_receive(&console, '\r');
    drain(&console, output); CHECK(console.traffic.timeout_s == 12);
    CHECK(console.length == 0); /* embedded NUL cannot execute valid prefix */
    command("set mode 2\b1\r"); CHECK(console.traffic.button_enabled);
    console.traffic.state = TRAFFIC_BLINK;
    command("?\r"); CHECK(strstr(output, "blinking green") != 0);
    while (console_can_receive(&console)) console_receive(&console, 'a');
    CHECK(console.output.count <= CONSOLE_OUTPUT_SIZE);
    CHECK(console.output.capacity - console.output.count < CONSOLE_RESERVE);
}
static void traffic_tests(void)
{
    Traffic t;
    traffic_init(&t, 0);
    CHECK(traffic_leds(&t, 0) == (1U << 15));
    traffic_update(&t, false, 11999); CHECK(t.state == TRAFFIC_RED);
    traffic_update(&t, false, 12000); CHECK(t.state == TRAFFIC_GREEN);
    traffic_update(&t, true, 13000); CHECK(!t.request);
    traffic_update(&t, false, 15000); CHECK(t.state == TRAFFIC_BLINK);
    CHECK(traffic_leds(&t, 15000) == (1U << 13)); CHECK(!traffic_leds(&t, 15500));
    traffic_update(&t, true, 16000); CHECK(t.request);
    traffic_update(&t, false, 18000); CHECK(t.state == TRAFFIC_YELLOW);
    traffic_update(&t, false, 19000); CHECK(t.state == TRAFFIC_RED && t.request);
    traffic_update(&t, false, 22000); CHECK(t.state == TRAFFIC_GREEN && !t.request);
    traffic_init(&t, 0); traffic_update(&t, true, 2999); CHECK(t.state == TRAFFIC_RED);
    traffic_update(&t, true, 3000); CHECK(t.state == TRAFFIC_GREEN);
    traffic_init(&t, 0); traffic_update(&t, true, 100); traffic_mode(&t, false);
    CHECK(!t.request); traffic_update(&t, true, 3000); CHECK(t.state == TRAFFIC_RED);
    CHECK(traffic_timeout(&t, 1)); traffic_update(&t, false, 3000); CHECK(t.state == TRAFFIC_GREEN);
    CHECK(!traffic_timeout(&t, 0)); CHECK(!traffic_timeout(&t, 2147484U));
    traffic_init(&t, 0xFFFFF000U);
    traffic_update(&t, false, 0xFFFFF000U + 12000U); CHECK(t.state == TRAFFIC_GREEN);
    Button b; button_init(&b, false, 0, 30);
    CHECK(!button_update(&b, true, 10)); CHECK(!button_update(&b, true, 39));
    CHECK(button_update(&b, true, 40)); CHECK(!button_update(&b, true, 1000));
}
static void queue_tests(void)
{
    uint8_t storage[3], byte; ByteQueue q;
    queue_init(&q, storage, 3); CHECK(!queue_pop(&q, &byte));
    for (unsigned turn = 0; turn < 20; ++turn) {
        CHECK(queue_push(&q, 1)); CHECK(queue_push(&q, 2)); CHECK(queue_push(&q, 3));
        CHECK(!queue_push(&q, 4)); CHECK(queue_peek(&q, &byte) && byte == 1);
        CHECK(queue_pop(&q, &byte) && byte == 1); CHECK(queue_pop(&q, &byte) && byte == 2);
        CHECK(queue_push(&q, 4)); CHECK(queue_pop(&q, &byte) && byte == 3);
        CHECK(queue_pop(&q, &byte) && byte == 4); CHECK(!queue_pop(&q, &byte));
    }
}
static void inject(uint8_t byte)
{ USART6->DR = byte; USART6->SR = USART_SR_TXE | USART_SR_TC | USART_SR_RXNE; }
static void pump(unsigned n) { while (n--) { uart_irq(); uart_service(); } }
static void driver_tests(void)
{
    uint8_t byte;
    USART6->SR = USART_SR_TXE | USART_SR_TC;
    CHECK(uart_init(false) == UART_OK);
    CHECK((USART6->CR1 & (USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_PEIE)) == 0);
    CHECK(!(USART6->CR3 & USART_CR3_EIE));
    CHECK(!(NVIC->ISER[(unsigned)USART6_IRQn / 32U] & (1U << ((unsigned)USART6_IRQn % 32U))));
    CHECK(uart_read(&byte) == UART_EMPTY); CHECK(uart_read(0) == UART_ERROR);
    CHECK(USART6->BRR == 278U);
    CHECK(((GPIOC->AFR[0] >> 24) & 255U) == 0x88U);
    inject('p'); CHECK(uart_read(&byte) == UART_OK && byte == 'p');
    CHECK(uart_read(&byte) == UART_EMPTY);
    inject('e'); USART6->SR |= USART_SR_ORE;
    CHECK(uart_read(&byte) == UART_ERROR); CHECK(uart_state().hardware_errors == 1);
    CHECK(uart_read(&byte) == UART_EMPTY);
    inject('q'); uart_service(); CHECK(uart_read(&byte) == UART_OK && byte == 'q');
    CHECK(uart_write((const uint8_t *)"AB", 2) == UART_OK);
    CHECK(uart_state().tx_count == 2); CHECK(uart_set_interrupts(true) == UART_BUSY);
    uart_service(); uart_service(); CHECK(uart_state().tx_count == 0);
    CHECK(uart_set_interrupts(true) == UART_OK); CHECK(uart_state().interrupts);
    CHECK(NVIC->ISER[(unsigned)USART6_IRQn / 32U] & (1U << ((unsigned)USART6_IRQn % 32U)));
    inject('i'); uart_irq(); CHECK(uart_read(&byte) == UART_OK && byte == 'i');
    for (unsigned i = 0; i < UART_RX_CAPACITY + 2U; ++i) { inject((uint8_t)i); uart_irq(); }
    CHECK(uart_state().rx_count == UART_RX_CAPACITY); CHECK(uart_state().rx_overflow == 2);
    for (unsigned i = 0; i < UART_RX_CAPACITY; ++i) CHECK(uart_read(&byte) == UART_OK && byte == (uint8_t)i);
    CHECK(uart_read(&byte) == UART_EMPTY);
    CHECK(uart_write((const uint8_t *)"XYZ", 3) == UART_OK);
    CHECK(uart_state().transmitting); CHECK(uart_set_interrupts(false) == UART_BUSY);
    pump(12); CHECK(!uart_state().transmitting && uart_state().tx_count == 0);
    inject('s'); uart_irq(); /* queued RX survives switch */
    CHECK(uart_set_interrupts(false) == UART_OK);
    CHECK(uart_read(&byte) == UART_OK && byte == 's');
    CHECK(!(USART6->CR1 & (USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_PEIE)));
    CHECK(!(USART6->CR3 & USART_CR3_EIE));
    CHECK(!(NVIC->ISER[(unsigned)USART6_IRQn / 32U] & (1U << ((unsigned)USART6_IRQn % 32U))));
    uint8_t fill[UART_TX_CAPACITY]; memset(fill, 'f', sizeof fill);
    CHECK(uart_write(fill, sizeof fill) == UART_OK);
    CHECK(uart_write((const uint8_t *)"!", 1) == UART_FULL);
    CHECK(uart_state().tx_count == UART_TX_CAPACITY);
    for (unsigned i = 0; i < UART_TX_CAPACITY; ++i) uart_service();
    CHECK(uart_state().tx_count == 0);
    CHECK(uart_write(0, 1) == UART_ERROR); CHECK(uart_write(0, 0) == UART_OK);
    CHECK(uart_set_interrupts(true) == UART_OK);
    inject('e'); USART6->SR |= USART_SR_ORE; uart_irq();
    CHECK(uart_state().hardware_errors >= 2);
    inject('r'); uart_irq();
    /* HAL may deliver the ORE byte before reporting error; drain all. */
    bool found = false;
    while (uart_read(&byte) == UART_OK) if (byte == 'r') found = true;
    CHECK(found);
}
void test_entry(void)
{
    queue_tests(); traffic_tests(); app_tests(); driver_tests();
    test_done = 1; for (;;) {}
}
