#include "stm32f4xx_hal.h"
#include "uart_backend.h"
#if UART_BACKEND_REG
/* No HAL/LL UART types, configuration or transfer functions in this backend. */
typedef struct { USART_TypeDef *regs; bool interrupts, active; } RegisterUart;
static RegisterUart device;
UartStatus backend_init(void)
{
    device = (RegisterUart){.regs = USART6};
    NVIC_DisableIRQ(USART6_IRQn);
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
    (void)RCC->APB2ENR;
    /* PC6/PC7: alternate function 8, push-pull, pull-up, high speed. */
    GPIOC->MODER = (GPIOC->MODER & ~((3U << 12) | (3U << 14))) | (2U << 12) | (2U << 14);
    GPIOC->OTYPER &= ~((1U << 6) | (1U << 7));
    GPIOC->OSPEEDR = (GPIOC->OSPEEDR & ~((3U << 12) | (3U << 14))) | (2U << 12) | (2U << 14);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~((3U << 12) | (3U << 14))) | (1U << 12) | (1U << 14);
    GPIOC->AFR[0] = (GPIOC->AFR[0] & ~((15U << 24) | (15U << 28))) | (8U << 24) | (8U << 28);
    RCC->APB2RSTR |= RCC_APB2RSTR_USART6RST;
    RCC->APB2RSTR &= ~RCC_APB2RSTR_USART6RST;
    device.regs->CR1 = 0;
    device.regs->CR2 = 0;
    device.regs->CR3 = 0;
    /* APB2 fixed at 16 MHz by main, OVER8=0: BRR=round(PCLK/baud). */
    device.regs->BRR = (16000000U + 57600U / 2U) / 57600U;
    device.regs->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
    NVIC_SetPriority(USART6_IRQn, 5);
    return UART_OK;
}
UartStatus backend_mode(bool enabled)
{
    NVIC_DisableIRQ(USART6_IRQn);
    device.regs->CR1 &= ~(USART_CR1_RXNEIE | USART_CR1_TXEIE | USART_CR1_TCIE |
                          USART_CR1_IDLEIE | USART_CR1_PEIE);
    device.regs->CR3 &= ~USART_CR3_EIE;
    NVIC_ClearPendingIRQ(USART6_IRQn);
    device.interrupts = enabled;
    if (enabled) {
        device.regs->CR1 |= USART_CR1_RXNEIE | USART_CR1_PEIE;
        device.regs->CR3 |= USART_CR3_EIE;
        NVIC_EnableIRQ(USART6_IRQn);
    }
    return UART_OK;
}
UartStatus backend_receive(uint8_t *byte)
{
    uint32_t sr = device.regs->SR;
    if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) {
        (void)device.regs->DR; /* SR -> DR clears receive error flags. */
        uart_hardware_error();
        return UART_ERROR;
    }
    if (!(sr & USART_SR_RXNE)) return UART_EMPTY;
    *byte = (uint8_t)device.regs->DR;
    return UART_OK;
}
UartStatus backend_transmit(uint8_t *byte)
{
    if (!(device.regs->SR & USART_SR_TXE)) return UART_BUSY;
    device.regs->DR = *byte;
    if (device.interrupts) {
        device.active = true;
        device.regs->CR1 |= USART_CR1_TCIE;
    }
    return UART_OK;
}
bool backend_idle(void) { return (device.regs->SR & USART_SR_TC) != 0; }
void backend_service(void) {}
void backend_irq(void)
{
    uint8_t byte;
    if (backend_receive(&byte) == UART_OK) uart_received(byte);
    if (device.active && (device.regs->SR & USART_SR_TC)) {
        device.regs->CR1 &= ~USART_CR1_TCIE;
        device.active = false;
        uart_transmitted();
    }
}
#endif
