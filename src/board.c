#include "stm32f4xx.h"
#include "board.h"

#define LED_PIN 13U     // PC13

// Valor gravado num registrador de backup do RTC: sobrevive ao reset
#define DFU_MAGIC 0xDF00B007U

// Endereco da memoria de sistema (bootloader da ST) no STM32F411
#define SYSMEM_ADDR 0x1FFF0000U

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

static void backup_access_enable(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    __DSB();
    PWR->CR |= PWR_CR_DBP;
}

void board_check_dfu_request(void)
{
    backup_access_enable();

    if (RTC->BKP0R != DFU_MAGIC)
        return;

    RTC->BKP0R = 0U;

    // Ainda estamos no HSI logo apos o reset: basta remapear e saltar
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    __DSB();
    SYSCFG->MEMRMP = 0x01U;     // memoria de sistema em 0x00000000

    uint32_t sp = *(volatile uint32_t *)SYSMEM_ADDR;
    uint32_t pc = *(volatile uint32_t *)(SYSMEM_ADDR + 4U);

    __set_MSP(sp);
    ((void (*)(void))pc)();

    while (1) {}
}

void board_reboot_to_dfu(void)
{
    backup_access_enable();
    RTC->BKP0R = DFU_MAGIC;
    NVIC_SystemReset();
}

void busy_delay_ms(uint32_t ms)
{
    // ~4 ciclos por iteracao a 84 MHz
    volatile uint32_t n = ms * (SystemCoreClock / 4000U);

    while (n--) {}
}
