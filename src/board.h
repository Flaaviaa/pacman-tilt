#pragma once

#include <stdint.h>

// HSE 25 MHz -> 84 MHz CPU / 48 MHz USB (clock.c)
void clock_init(void);

// LED da placa (PC13, ativo em nivel baixo)
void led_init(void);
void led_set(int on);
void led_toggle(void);

// Se a placa foi reiniciada por board_reboot_to_dfu(), salta para o
// bootloader DFU da ST. Deve ser a primeira coisa do main().
void board_check_dfu_request(void);

// Reinicia a placa direto no bootloader DFU (gravar sem apertar BOOT0).
// Disparado quando o PC abre a serial a 1200 baud.
void board_reboot_to_dfu(void);

// Espera ocupada (antes do escalonador ou dentro de drivers)
void busy_delay_ms(uint32_t ms);
