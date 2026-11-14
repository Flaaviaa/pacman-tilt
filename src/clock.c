#include "stm32f4xx.h"
#include "board.h"

// Clock do exemplo "BlackPill_F411CE - USB CDC":
// HSE 25 MHz -> PLL -> 84 MHz para a CPU e 48 MHz para a USB.
void clock_init(void)
{
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    RCC->CFGR &= ~RCC_CFGR_SW;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
    }

    RCC->CR &= ~RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) != 0U)
    {
    }

    RCC->CR |= RCC_CR_HSEON;

    while ((RCC->CR & RCC_CR_HSERDY) == 0U)
    {
    }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;

#ifdef PWR_CR_VOS
    PWR->CR |= PWR_CR_VOS;
#endif

    FLASH->ACR =
        FLASH_ACR_ICEN |
        FLASH_ACR_DCEN |
        FLASH_ACR_PRFTEN |
        FLASH_ACR_LATENCY_2WS;

    // 84 MHz para CPU e 48 MHz para USB
    RCC->PLLCFGR =
        (25U  << 0U) |
        (336U << 6U) |
        (1U   << 16U) |
        RCC_PLLCFGR_PLLSRC_HSE |
        (7U   << 24U);

    RCC->CFGR &= ~(
        RCC_CFGR_HPRE |
        RCC_CFGR_PPRE1 |
        RCC_CFGR_PPRE2
    );

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

    RCC->CR |= RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    SystemCoreClockUpdate();
}
