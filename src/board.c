#include "stm32f4xx.h"
#include "board.h"

#define LED_PIN 13U     // PC13


void led_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    __DSB();

    GPIOC->MODER &= ~(3U << (LED_PIN * 2U));
    GPIOC->MODER |=  (1U << (LED_PIN * 2U));
    GPIOC->OTYPER &= ~(1U << LED_PIN);

    led_set(0);
}

void led_set(int on)
{
    if (on)
        GPIOC->BSRR = (1U << (LED_PIN + 16U));
    else
        GPIOC->BSRR = (1U << LED_PIN);
}

void led_toggle(void)
{
    GPIOC->ODR ^= (1U << LED_PIN);
}

void busy_delay_ms(uint32_t ms)
{
    // ~4 ciclos por iteracao a 84 MHz
    volatile uint32_t n = ms * (SystemCoreClock / 4000U);

    while (n--) {}
}
