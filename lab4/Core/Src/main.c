#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "clock.h"
#include "bus.h"
#include "clock_app.h"
#include "oled.h"
#include "uart_driver.h"
#include "frame_stats.h"

/* CubeIDE Expressions: inspect app, display_stats, lab4_fault. */
ClockApp app;
Oled display;
volatile FrameStats display_stats;
volatile uint32_t lab4_fault;
static QueueHandle_t snapshot_queue;
static volatile bool oled_ok;
static void fatal(uint32_t code) { lab4_fault = code; __disable_irq(); for (;;) {} }

/* Owns app, RTC state, protocol parsing and UART foreground. */
static void application_task(void *unused)
{
    (void)unused;
    uint32_t now = HAL_GetTick(), last_sample = now, last_overflow = 0;
    app_init(&app, now);
    app.oled_ok = oled_ok;
    (void)xQueueOverwrite(snapshot_queue, &app.rtc.snapshot);
    for (;;) {
        now = HAL_GetTick();
        uart_service();
        uint8_t byte;
        for (unsigned i = 0; i < 64; ++i) {
            if (!queue_peek(&app.output, &byte) || uart_write(&byte, 1) != UART_OK) break;
            (void)queue_pop(&app.output, &byte);
        }
        UartState state = uart_state();
        if (state.rx_overflow != last_overflow) { app.collecting = false; last_overflow = state.rx_overflow; }
        for (unsigned i = 0; i < 64 && app_can_receive(&app); ++i) {
            if (uart_read(&byte) != UART_OK) break;
            app_receive(&app, byte, HAL_GetTick());
        }
        now = HAL_GetTick();
        bool due = (uint32_t)(now - last_sample) >= 250U;
        if (due) last_sample = now;
        app.oled_ok = oled_ok;
        app_service(&app, now, due);
        (void)xQueueOverwrite(snapshot_queue, &app.rtc.snapshot);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/* 40 ms nominal period (25 Hz). Between pages let the app task run. */
static void display_task(void *unused)
{
    (void)unused;
    FrameStats stats;
    frame_stats_init(&stats, HAL_GetTick());
    ClockSnapshot snapshot = {0};
    TickType_t next = xTaskGetTickCount();
    for (;;) {
        uint32_t start = HAL_GetTick();
        if (!display.ready) {
            oled_ok = oled_init(&display);
            if (!oled_ok) {
                frame_stats_fail(&stats); display_stats = stats;
                vTaskDelay(pdMS_TO_TICKS(1000)); next = xTaskGetTickCount(); continue;
            }
        }
        (void)xQueuePeek(snapshot_queue, &snapshot, 0);
        oled_render(&display, &snapshot, stats.counter);
        bool ok = true;
        while (display.page < 8U) {
            if (!oled_service(&display)) { ok = false; break; }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        oled_ok = ok;
        uint32_t finished = HAL_GetTick();
        if (ok) frame_stats_complete(&stats, finished, finished - start);
        else frame_stats_fail(&stats);
        display_stats = stats;
        /* Never spin at high priority after an overrun. */
        TickType_t current = xTaskGetTickCount();
        if ((TickType_t)(current - next) >= pdMS_TO_TICKS(40)) {
            next = current;
            vTaskDelay(pdMS_TO_TICKS(1));
        } else vTaskDelayUntil(&next, pdMS_TO_TICKS(40));
    }
}

int main(void)
{
    SystemCoreClockUpdate();
    if (HAL_Init() != HAL_OK) fatal(1);
    if (!board_clock_init()) fatal(2);
    if (!bus_init() || !bus_lock_init()) fatal(3);
    if (uart_init(true) != UART_OK) fatal(4);
    snapshot_queue = xQueueCreate(1, sizeof(ClockSnapshot));
    if (!snapshot_queue) fatal(5);
    ClockSnapshot initial = {0}; initial.status = RTC_STARTING;
    (void)xQueueOverwrite(snapshot_queue, &initial);
    if (xTaskCreate(display_task, "display", 768, NULL, 3, NULL) != pdPASS) fatal(6);
    if (xTaskCreate(application_task, "clock-uart", 1024, NULL, 2, NULL) != pdPASS) fatal(7);
    vTaskStartScheduler();
    fatal(8);
}

extern void xPortSysTickHandler(void);
void SysTick_Handler(void)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) xPortSysTickHandler();
    else HAL_IncTick();
}
void vApplicationTickHook(void) { HAL_IncTick(); }
void vApplicationMallocFailedHook(void) { fatal(9); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{ (void)task; (void)name; fatal(10); }
void USART6_IRQHandler(void) { uart_irq(); }
void HardFault_Handler(void) { fatal(0xF1); }
void MemManage_Handler(void) { fatal(0xF2); }
void BusFault_Handler(void) { fatal(0xF3); }
void UsageFault_Handler(void) { fatal(0xF4); }
