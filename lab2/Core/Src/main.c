#include "stm32f4xx_hal.h"
#include "board.h"
#include "led.h"
#include "button.h"
#include "console.h"
#include "uart_driver.h"
Console console;
Button button;
volatile uint32_t lab2_fault;
static LedBank leds;
static void fatal(uint32_t code) { lab2_fault = code; for (;;) {} }
static void clock_init(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) fatal(1);
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) fatal(2);
    osc.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) fatal(3);
}
int main(void)
{
    SystemCoreClockUpdate();
    if (HAL_Init() != HAL_OK) fatal(4);
    clock_init();
    if (!led_init(&leds, BOARD_LED_PORT, BOARD_LED_MASK, 0)) fatal(5);
    const IoConfig input = {IO_INPUT, IO_NO_PULL, 0, false, 0};
    if (!io_clock_enable(BOARD_BUTTON_PORT) ||
        io_configure(BOARD_BUTTON_PORT, (uint16_t)(1U << BOARD_BUTTON_PIN), &input, 0) != IO_OK) fatal(6);
    uint32_t now = HAL_GetTick();
    button_init(&button, !io_read_pin(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN), now, BOARD_DEBOUNCE_MS);
    if (uart_init(UART_START_INTERRUPTS != 0) != UART_OK) fatal(7);
    console_init(&console, now, UART_START_INTERRUPTS != 0);
    for (;;) {
        uart_service();
        now = HAL_GetTick();
        bool press = button_update(&button, !io_read_pin(BOARD_BUTTON_PORT, BOARD_BUTTON_PIN), now);
        traffic_update(&console.traffic, press, now);
        led_write(&leds, traffic_leds(&console.traffic, now));
        /* Output retained until UART accepts it. No foreground busy wait. */
        uint8_t byte;
        if (queue_peek(&console.output, &byte) && uart_write(&byte, 1) == UART_OK)
            (void)queue_pop(&console.output, &byte);
        if (console.mode_pending && !console.output.count) {
            UartStatus status = uart_set_interrupts(console.requested_interrupts);
            if (status == UART_OK) {
                console.interrupts = console.requested_interrupts;
                console.mode_pending = false;
            } else if (status != UART_BUSY) fatal(8);
        }
        if (console_can_receive(&console) && uart_read(&byte) == UART_OK)
            console_receive(&console, byte);
    }
}
void USART6_IRQHandler(void) { uart_irq(); }
void SysTick_Handler(void) { HAL_IncTick(); }
void HardFault_Handler(void) { fatal(0xF1); }
void MemManage_Handler(void) { fatal(0xF2); }
void BusFault_Handler(void) { fatal(0xF3); }
void UsageFault_Handler(void) { fatal(0xF4); }
