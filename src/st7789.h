#pragma once

#include <stdint.h>

// Driver do display ST7789 240x240 via SPI1 (registradores CMSIS).
// Somente a tarefa do display chama estas funcoes, sempre com o
// mutex do LCD (m_lcd) obtido.

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | ((b) >> 3)))

void lcd_init(void);
void lcd_fill_rect(int x, int y, int w, int h, uint16_t color);

// Envia um bloco w x h de pixels (linha a linha)
void lcd_blit(int x, int y, int w, int h, const uint16_t *pixels);

void lcd_backlight(int on);
