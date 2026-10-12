#include "stm32f4xx_hal.h"
#include "board.h"
#include "scheduler.h"
static TIM_HandleTypeDef timer;
static volatile bool due;
bool scheduler_init(void)
{
    __HAL_RCC_TIM6_CLK_ENABLE();
    timer.Instance = TIM6;
    timer.Init.Prescaler = BOARD_TIM_APB1_HZ / 1000U - 1U;
    timer.Init.CounterMode = TIM_COUNTERMODE_UP;
    timer.Init.Period = 249;
    timer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    due = false;
    if (HAL_TIM_Base_Init(&timer) != HAL_OK) return false;
    __HAL_TIM_CLEAR_FLAG(&timer, TIM_FLAG_UPDATE);
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 6, 0); HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn); return true;
}
bool scheduler_start(void) { return HAL_TIM_Base_Start_IT(&timer) == HAL_OK; }
bool scheduler_take(void)
{
    uint32_t key = __get_PRIMASK(); __disable_irq(); bool result = due; due = false;
    __set_PRIMASK(key); return result;
}
void scheduler_irq(void) { HAL_TIM_IRQHandler(&timer); }
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *h) { if (h == &timer) due = true; }
