#include "stm32f4xx.h"
#include "buttons.h"

// Botoes ligados ao GND com pull-up interno: solto = 1, pressionado = 0.
// O debounce e feito pela tarefa de botoes (task_buttons.c).

static void input_pullup(GPIO_TypeDef *port, uint32_t pin)
{
    port->MODER &= ~(3U << (pin * 2U));
    port->PUPDR &= ~(3U << (pin * 2U));
    port->PUPDR |=  (1U << (pin * 2U));
}

void buttons_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    __DSB();

    input_pullup(GPIOB, 12U);
    input_pullup(GPIOB, 13U);
    input_pullup(GPIOA, 0U);
}

uint32_t buttons_read_raw(void)
{
    uint32_t r = 0U;

    if ((GPIOB->IDR & (1U << 12U)) == 0U) r |= 1U << BTN_A;
    if ((GPIOB->IDR & (1U << 13U)) == 0U) r |= 1U << BTN_B;
    if ((GPIOA->IDR & (1U << 0U))  == 0U) r |= 1U << BTN_KEY;

    return r;
}
