#include "stm32f4xx_hal.h"
#include "board.h"
#include "bus.h"
static I2C_HandleTypeDef handle;
void HAL_I2C_MspInit(I2C_HandleTypeDef *h)
{
    if (h->Instance != I2C1) return;
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    GPIO_InitTypeDef pins = {0};
    pins.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    pins.Mode = GPIO_MODE_AF_OD;
    pins.Pull = GPIO_PULLUP;
    pins.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    pins.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &pins);
    HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
}
bool bus_init(void)
{
    handle.Instance = I2C1;
    handle.Init.ClockSpeed = BOARD_I2C_HZ;
    handle.Init.DutyCycle = I2C_DUTYCYCLE_2;
    handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    return HAL_I2C_Init(&handle) == HAL_OK;
}
bool bus_read(uint8_t address, uint8_t reg, uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Read(&handle, (uint16_t)(address << 1), reg, I2C_MEMADD_SIZE_8BIT,
                          data, length, 10) == HAL_OK;
}
bool bus_write(uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length)
{
    return HAL_I2C_Mem_Write(&handle, (uint16_t)(address << 1), reg, I2C_MEMADD_SIZE_8BIT,
                           (uint8_t *)data, length, 10) == HAL_OK;
}
