#include <stdint.h>

#include "stm32f4xx.h"
#include "board.h"
#include "config.h"
#include "st7789.h"

// Teste isolado do display ST7789 (sem FreeRTOS, sem USB).
// Gravar com: pio run -e display_test -t upload
//
// O que deve acontecer:
//   - LED azul da placa pisca a cada troca de tela (o programa esta rodando)
//   - display alterna: vermelho, verde, azul, branco, barras coloridas + texto
// Se o LED pisca e a tela fica apagada/preta: problema de fiacao do display.

extern const uint8_t font5x7[][5];

static uint16_t buf[6 * 4 * 8 * 4];

static void draw_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg)
{
    int cw = 6 * scale, ch = 8 * scale;

    for (; *s; s++, x += cw)
    {
        char c = *s;

        if (c < ' ' || c > 'Z')
            c = '?';

        const uint8_t *g = font5x7[c - ' '];

        for (int py = 0; py < ch; py++)
            for (int px = 0; px < cw; px++)
            {
                int col = px / scale, row = py / scale;
                buf[py * cw + px] = (col < 5 && row < 7 && (g[col] & (1U << row))) ? fg : bg;
            }

        lcd_blit(x, y, cw, ch, buf);
    }
}

static void step(void)
{
    led_toggle();
    busy_delay_ms(1000);
}

int main(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    __DSB();
    SYSCFG->MEMRMP = 0U;
    SCB->VTOR = FLASH_BASE;

    clock_init();
    led_init();
    led_set(1);

    lcd_init();

    static const uint16_t bars[8] =
    {
        0xFFFF, RGB565(255, 255, 0), RGB565(0, 255, 255), RGB565(0, 255, 0),
        RGB565(255, 0, 255), RGB565(255, 0, 0), RGB565(0, 0, 255), 0x0000
    };

    for (;;)
    {
        lcd_fill_rect(0, 0, LCD_W, LCD_H, RGB565(255, 0, 0));   step();
        lcd_fill_rect(0, 0, LCD_W, LCD_H, RGB565(0, 255, 0));   step();
        lcd_fill_rect(0, 0, LCD_W, LCD_H, RGB565(0, 0, 255));   step();
        lcd_fill_rect(0, 0, LCD_W, LCD_H, 0xFFFF);              step();

        for (int i = 0; i < 8; i++)
            lcd_fill_rect(i * 30, 0, 30, LCD_H, bars[i]);

        draw_text(24, 90, "TESTE", 4, 0x0000, 0xFFFF);
        draw_text(36, 140, "DISPLAY OK", 2, 0xFFFF, 0x0000);
        step();
        step();
    }
}
