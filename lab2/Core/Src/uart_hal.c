#include "stm32f4xx_hal.h"
#include "uart_backend.h"
#if !UART_BACKEND_REG
static UART_HandleTypeDef handle;
static uint8_t rx_byte;
static bool interrupts;
static UartStatus arm_rx(void)
{
    if (handle.RxState != HAL_UART_STATE_READY) return UART_OK;
    return HAL_UART_Receive_IT(&handle, &rx_byte, 1) == HAL_OK ? UART_OK : UART_ERROR;
}
void HAL_UART_MspInit(UART_HandleTypeDef *h)
{
    if (h->Instance != USART6) return;
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();
    GPIO_InitTypeDef pins = {0};
    pins.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    pins.Mode = GPIO_MODE_AF_PP;
    pins.Pull = GPIO_PULLUP;
    pins.Speed = GPIO_SPEED_FREQ_HIGH;
    pins.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &pins);
    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
}
UartStatus backend_init(void)
{
    HAL_NVIC_DisableIRQ(USART6_IRQn);
    handle.Instance = USART6;
    handle.Init.BaudRate = 57600;
    handle.Init.WordLength = UART_WORDLENGTH_8B;
    handle.Init.StopBits = UART_STOPBITS_1;
    handle.Init.Parity = UART_PARITY_NONE;
    handle.Init.Mode = UART_MODE_TX_RX;
    handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    handle.Init.OverSampling = UART_OVERSAMPLING_16;
    return HAL_UART_Init(&handle) == HAL_OK ? UART_OK : UART_ERROR;
}
UartStatus backend_mode(bool enabled)
{
    HAL_NVIC_DisableIRQ(USART6_IRQn);
    if (HAL_UART_AbortReceive(&handle) != HAL_OK) return UART_ERROR;
    /* TX is already complete; remove every USART interrupt source. */
    __HAL_UART_DISABLE_IT(&handle, UART_IT_TXE);
    __HAL_UART_DISABLE_IT(&handle, UART_IT_TC);
    __HAL_UART_DISABLE_IT(&handle, UART_IT_IDLE);
    HAL_NVIC_ClearPendingIRQ(USART6_IRQn);
    interrupts = enabled;
    if (enabled) {
        if (arm_rx() != UART_OK) return UART_ERROR;
        HAL_NVIC_EnableIRQ(USART6_IRQn);
    }
    /* AbortReceive preserves DR and the software RX queue. */
    return UART_OK;
}
UartStatus backend_receive(uint8_t *byte)
{
    uint32_t sr = handle.Instance->SR;
    if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        __HAL_UART_CLEAR_OREFLAG(&handle);
        uart_hardware_error();
        return UART_ERROR;
    }
    if (!__HAL_UART_GET_FLAG(&handle, UART_FLAG_RXNE)) return UART_EMPTY;
    /* Size 1 and Timeout 0: never waits for a future character. */
    HAL_StatusTypeDef s = HAL_UART_Receive(&handle, byte, 1, 0);
    return s == HAL_OK ? UART_OK : s == HAL_TIMEOUT ? UART_EMPTY : UART_ERROR;
}
UartStatus backend_transmit(uint8_t *byte)
{
    if (interrupts)
        return HAL_UART_Transmit_IT(&handle, byte, 1) == HAL_OK ? UART_OK : UART_BUSY;
    if (!__HAL_UART_GET_FLAG(&handle, UART_FLAG_TXE)) return UART_BUSY;
    /* Polling TX is bounded and blocking, as allowed by the assignment. */
    return HAL_UART_Transmit(&handle, byte, 1, 2) == HAL_OK ? UART_OK : UART_ERROR;
}
bool backend_idle(void) { return __HAL_UART_GET_FLAG(&handle, UART_FLAG_TC) != RESET; }
void backend_service(void)
{
    if (interrupts) {
        uint32_t key = __get_PRIMASK(); __disable_irq();
        if (arm_rx() != UART_OK) uart_hardware_error();
        __set_PRIMASK(key);
    }
}
void backend_irq(void) { HAL_UART_IRQHandler(&handle); }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h != &handle) return;
    uart_received(rx_byte);
    if (arm_rx() != UART_OK) uart_hardware_error();
}
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *h)
{ if (h == &handle) uart_transmitted(); }
void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h != &handle) return;
    uart_hardware_error();
    /* HAL completes/aborts RX before callback; rearm after ORE as well. */
    if (arm_rx() != UART_OK) uart_hardware_error();
}
#endif
