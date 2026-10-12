#include "stm32f4xx_hal.h"
#include "clock.h"
#include "bus.h"
#include "clock_app.h"
#include "oled.h"
#include "scheduler.h"
#include "uart_driver.h"
ClockApp app;
Oled display;
volatile uint32_t lab3_fault;
static void fatal(uint32_t code) { lab3_fault = code; for (;;) {} }
int main(void)
{
    SystemCoreClockUpdate();
    if (HAL_Init() != HAL_OK) fatal(1);
    if (!board_clock_init()) fatal(2);
    if (uart_init(true) != UART_OK) fatal(3);
    if (!bus_init()) fatal(4);
    if (!scheduler_init()) fatal(5);
    uint32_t boot = HAL_GetTick(), oled_attempt = boot, last_overflow = 0;
    bool started = false;
    app_init(&app, boot); /* Preserve battery-backed RTC; never overwrite it on boot. */
    for (;;) {
        uint32_t now = HAL_GetTick(); uart_service();
        uint8_t byte;
        if (queue_peek(&app.output, &byte) && uart_write(&byte, 1) == UART_OK)
            (void)queue_pop(&app.output, &byte);
        UartState uart = uart_state();
        if (uart.rx_overflow != last_overflow) { app.collecting = false; last_overflow = uart.rx_overflow; }
        if (app_can_receive(&app) && uart_read(&byte) == UART_OK) app_receive(&app, byte, now);
        bool due = started && scheduler_take();
        app_service(&app, now, due);
        if (!started && (uint32_t)(now - boot) >= 100U) {
            app.oled_ok = oled_init(&display); oled_attempt = now;
            if (!scheduler_start()) fatal(6);
            started = true; oled_render(&display, &app.rtc.snapshot); app_publish(&app, 0);
        } else if (started && !display.ready && (uint32_t)(now - oled_attempt) >= 1000U) {
            oled_attempt = now; app.oled_ok = oled_init(&display); oled_render(&display, &app.rtc.snapshot);
        }
        if (due) oled_render(&display, &app.rtc.snapshot);
        if (started) app.oled_ok = oled_service(&display);
    }
}
void USART6_IRQHandler(void) { uart_irq(); }
void TIM6_DAC_IRQHandler(void) { scheduler_irq(); }
void SysTick_Handler(void) { HAL_IncTick(); }
void HardFault_Handler(void) { fatal(0xF1); }
void MemManage_Handler(void) { fatal(0xF2); }
void BusFault_Handler(void) { fatal(0xF3); }
void UsageFault_Handler(void) { fatal(0xF4); }
