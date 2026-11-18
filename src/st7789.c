#include "stm32f4xx.h"
#include "st7789.h"
#include "board.h"
#include "config.h"

// Pinos (ver config.h)
#define PIN_SCK  5U     // PA5  SPI1_SCK  (AF5)
#define PIN_MOSI 7U     // PA7  SPI1_MOSI (AF5)
#define PIN_CS   4U     // PA4
#define PIN_DC   LCD_PIN_DC     // PB0 (protoboard) ou PB1 (placa)
#define PIN_RST  LCD_PIN_RST    // PB1 (protoboard) ou PB0 (placa)
#define PIN_BLK  10U    // PB10

#define CS_LOW()   (GPIOA->BSRR = (1U << (PIN_CS + 16U)))
#define CS_HIGH()  (GPIOA->BSRR = (1U << PIN_CS))
#define DC_CMD()   (GPIOB->BSRR = (1U << (PIN_DC + 16U)))
#define DC_DATA()  (GPIOB->BSRR = (1U << PIN_DC))

static void gpio_out(GPIO_TypeDef *port, uint32_t pin)
{
    port->MODER &= ~(3U << (pin * 2U));
    port->MODER |=  (1U << (pin * 2U));
    port->OSPEEDR |= (3U << (pin * 2U));
}

static void gpio_af(GPIO_TypeDef *port, uint32_t pin, uint32_t af)
{
    port->MODER &= ~(3U << (pin * 2U));
    port->MODER |=  (2U << (pin * 2U));
    port->OSPEEDR |= (3U << (pin * 2U));
    port->AFR[pin / 8U] &= ~(0xFU << ((pin % 8U) * 4U));
    port->AFR[pin / 8U] |=  (af << ((pin % 8U) * 4U));
}

static void spi_wait_idle(void)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0U) {}
    while ((SPI1->SR & SPI_SR_BSY) != 0U) {}
}

// Troca entre quadros de 8 bits (comandos) e 16 bits (pixels)
static void spi_set_16bit(int on)
{
    spi_wait_idle();
    SPI1->CR1 &= ~SPI_CR1_SPE;

    if (on)
        SPI1->CR1 |= SPI_CR1_DFF;
    else
        SPI1->CR1 &= ~SPI_CR1_DFF;

    SPI1->CR1 |= SPI_CR1_SPE;
}

static void spi_send(uint16_t v)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0U) {}
    *(volatile uint16_t *)&SPI1->DR = v;
}

static void lcd_cmd(uint8_t cmd)
{
    spi_wait_idle();
    DC_CMD();
    spi_send(cmd);
    spi_wait_idle();
    DC_DATA();
}

static void lcd_data(uint8_t d)
{
    spi_send(d);
}

static void lcd_window(int x, int y, int w, int h)
{
    y += LCD_Y_OFFSET;

    uint16_t x1 = (uint16_t)(x + w - 1);
    uint16_t y1 = (uint16_t)(y + h - 1);

    lcd_cmd(0x2A);  // CASET
    lcd_data((uint8_t)(x >> 8)); lcd_data((uint8_t)x);
    lcd_data((uint8_t)(x1 >> 8)); lcd_data((uint8_t)x1);

    lcd_cmd(0x2B);  // RASET
    lcd_data((uint8_t)(y >> 8)); lcd_data((uint8_t)y);
    lcd_data((uint8_t)(y1 >> 8)); lcd_data((uint8_t)y1);

    lcd_cmd(0x2C);  // RAMWR
}

void lcd_backlight(int on)
{
    if (on)
        GPIOB->BSRR = (1U << PIN_BLK);
    else
        GPIOB->BSRR = (1U << (PIN_BLK + 16U));
}

void lcd_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    __DSB();

    gpio_af(GPIOA, PIN_SCK, 5U);
    gpio_af(GPIOA, PIN_MOSI, 5U);
    gpio_out(GPIOA, PIN_CS);
    gpio_out(GPIOB, PIN_DC);
    gpio_out(GPIOB, PIN_RST);
    gpio_out(GPIOB, PIN_BLK);

    CS_HIGH();
    lcd_backlight(1);

    // SPI mestre, modo 3 (CPOL=1, CPHA=1): os modulos 240x240 sem CS exigem
    // esse modo; os com CS tambem funcionam com ele. SSM/SSI: NSS por software.
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI |
                SPI_CR1_CPOL | SPI_CR1_CPHA |
                (LCD_SPI_BR << SPI_CR1_BR_Pos);
    SPI1->CR2 = 0U;
    SPI1->CR1 |= SPI_CR1_SPE;

    // Reset por hardware
    GPIOB->BSRR = (1U << PIN_RST);
    busy_delay_ms(10);
    GPIOB->BSRR = (1U << (PIN_RST + 16U));
    busy_delay_ms(20);
    GPIOB->BSRR = (1U << PIN_RST);
    busy_delay_ms(150);

    CS_LOW();       // unico dispositivo no barramento: CS fica sempre ativo

    lcd_cmd(0x01);  // SWRESET
    busy_delay_ms(150);
    lcd_cmd(0x11);  // SLPOUT
    busy_delay_ms(120);
    lcd_cmd(0x3A);  // COLMOD: 16 bits por pixel
    lcd_data(0x55);
    lcd_cmd(0x36);  // MADCTL
    lcd_data(LCD_MADCTL);
    lcd_cmd(0x21);  // INVON (os paineis IPS 240x240 precisam de inversao)
    lcd_cmd(0x13);  // NORON
    lcd_cmd(0x29);  // DISPON
    busy_delay_ms(20);

    lcd_fill_rect(0, 0, LCD_W, LCD_H, 0x0000);
}

void lcd_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0)
        return;

    lcd_window(x, y, w, h);
    spi_set_16bit(1);

    for (int32_t n = (int32_t)w * h; n > 0; n--)
        spi_send(color);

    spi_set_16bit(0);
}

void lcd_blit(int x, int y, int w, int h, const uint16_t *pixels)
{
    if (w <= 0 || h <= 0)
        return;

    lcd_window(x, y, w, h);
    spi_set_16bit(1);

    for (int32_t n = (int32_t)w * h; n > 0; n--)
        spi_send(*pixels++);

    spi_set_16bit(0);
}
