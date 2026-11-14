#include "stm32f4xx.h"
#include "mpu6050.h"
#include "board.h"

#define MPU_ADDR    0x68U   // AD0 em GND

#define REG_CONFIG       0x1AU
#define REG_ACCEL_CONFIG 0x1CU
#define REG_ACCEL_XOUT_H 0x3BU
#define REG_PWR_MGMT_1   0x6BU
#define REG_WHO_AM_I     0x75U

#define PIN_SCL 6U      // PB6 I2C1_SCL (AF4)
#define PIN_SDA 7U      // PB7 I2C1_SDA (AF4)

// Limite de espera em cada etapa: se o sensor nao estiver ligado o jogo
// nao pode travar (a tarefa so marca erro e tenta de novo depois).
#define I2C_TIMEOUT 20000U

static int wait_flag(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t t = I2C_TIMEOUT;

    while ((*reg & mask) == 0U)
    {
        if (I2C1->SR1 & I2C_SR1_AF)     // NACK
            return -1;
        if (--t == 0U)
            return -1;
    }

    return 0;
}

static int i2c_fail(void)
{
    I2C1->SR1 &= ~I2C_SR1_AF;
    I2C1->CR1 |= I2C_CR1_STOP;
    return -1;
}

// Libera o barramento se o escravo ficou segurando SDA (reset no meio
// de uma leitura): ate 9 pulsos de clock manuais em SCL.
static void i2c_bus_recover(void)
{
    GPIOB->MODER &= ~((3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U)));
    GPIOB->MODER |=  (1U << (PIN_SCL * 2U));
    GPIOB->OTYPER |= (1U << PIN_SCL) | (1U << PIN_SDA);
    GPIOB->PUPDR &= ~((3U << (PIN_SCL * 2U)) | (3U << (PIN_SDA * 2U)));
    GPIOB->PUPDR |=  (1U << (PIN_SCL * 2U)) | (1U << (PIN_SDA * 2U));

    for (int i = 0; i < 9 && (GPIOB->IDR & (1U << PIN_SDA)) == 0U; i++)
    {
        GPIOB->BSRR = (1U << (PIN_SCL + 16U));
        busy_delay_ms(1);
        GPIOB->BSRR = (1U << PIN_SCL);
        busy_delay_ms(1);
    }

    // Volta para funcao alternativa I2C
    GPIOB->MODER &= ~(3U << (PIN_SCL * 2U));
    GPIOB->MODER |= (2U << (PIN_SCL * 2U)) | (2U << (PIN_SDA * 2U));
    GPIOB->AFR[0] &= ~((0xFU << (PIN_SCL * 4U)) | (0xFU << (PIN_SDA * 4U)));
    GPIOB->AFR[0] |=  (4U << (PIN_SCL * 4U)) | (4U << (PIN_SDA * 4U));
}

static void i2c_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    __DSB();

    i2c_bus_recover();

    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    // APB1 = 42 MHz. Fast mode 400 kHz, duty 2: CCR = 42e6 / (3 * 400e3) = 35
    I2C1->CR2 = 42U;
    I2C1->CCR = I2C_CCR_FS | 35U;
    I2C1->TRISE = (42U * 300U / 1000U) + 1U;
    I2C1->CR1 = I2C_CR1_PE;
}

static int i2c_write_reg(uint8_t reg, uint8_t value)
{
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_flag(&I2C1->SR1, I2C_SR1_SB)) return i2c_fail();

    I2C1->DR = MPU_ADDR << 1;
    if (wait_flag(&I2C1->SR1, I2C_SR1_ADDR)) return i2c_fail();
    (void)I2C1->SR2;

    I2C1->DR = reg;
    if (wait_flag(&I2C1->SR1, I2C_SR1_TXE)) return i2c_fail();

    I2C1->DR = value;
    if (wait_flag(&I2C1->SR1, I2C_SR1_BTF)) return i2c_fail();

    I2C1->CR1 |= I2C_CR1_STOP;
    return 0;
}

static int i2c_read_regs(uint8_t reg, uint8_t *buf, uint32_t len)
{
    // Escreve o endereco do registrador
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_flag(&I2C1->SR1, I2C_SR1_SB)) return i2c_fail();

    I2C1->DR = MPU_ADDR << 1;
    if (wait_flag(&I2C1->SR1, I2C_SR1_ADDR)) return i2c_fail();
    (void)I2C1->SR2;

    I2C1->DR = reg;
    if (wait_flag(&I2C1->SR1, I2C_SR1_BTF)) return i2c_fail();

    // Repeated start + leitura
    I2C1->CR1 |= I2C_CR1_START | I2C_CR1_ACK;
    if (wait_flag(&I2C1->SR1, I2C_SR1_SB)) return i2c_fail();

    I2C1->DR = (MPU_ADDR << 1) | 1U;
    if (wait_flag(&I2C1->SR1, I2C_SR1_ADDR)) return i2c_fail();

    if (len == 1U)
        I2C1->CR1 &= ~I2C_CR1_ACK;

    (void)I2C1->SR2;

    for (uint32_t i = 0; i < len; i++)
    {
        if (i == len - 1U)
        {
            I2C1->CR1 &= ~I2C_CR1_ACK;      // NACK no ultimo byte
            I2C1->CR1 |= I2C_CR1_STOP;
        }

        if (wait_flag(&I2C1->SR1, I2C_SR1_RXNE)) return i2c_fail();
        buf[i] = (uint8_t)I2C1->DR;
    }

    return 0;
}

int mpu_init(void)
{
    uint8_t who = 0;

    i2c_init();

    if (i2c_write_reg(REG_PWR_MGMT_1, 0x01U))     // acorda, clock do giroscopio X
        return -1;

    busy_delay_ms(50);

    i2c_write_reg(REG_CONFIG, 0x03U);           // filtro passa-baixas ~44 Hz
    i2c_write_reg(REG_ACCEL_CONFIG, 0x00U);     // +-2 g

    if (i2c_read_regs(REG_WHO_AM_I, &who, 1U))
        return -1;

    return who;
}

int mpu_read_accel(mpu_raw_t *out)
{
    uint8_t b[6];

    if (i2c_read_regs(REG_ACCEL_XOUT_H, b, 6U))
        return -1;

    out->ax = (int16_t)((b[0] << 8) | b[1]);
    out->ay = (int16_t)((b[2] << 8) | b[3]);
    out->az = (int16_t)((b[4] << 8) | b[5]);
    return 0;
}
