#pragma once

#include <stdint.h>

// HSE 25 MHz -> 84 MHz CPU / 48 MHz USB (clock.c)
void clock_init(void);

// LED da placa (PC13, ativo em nivel baixo)
void led_init(void);
void led_set(int on);
void led_toggle(void);

// Espera ocupada (antes do escalonador ou dentro de drivers)
void busy_delay_ms(uint32_t ms);
