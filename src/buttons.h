#pragma once

#include <stdint.h>

typedef enum
{
    BTN_A = 0,      // PB12: confirmar / iniciar
    BTN_B,          // PB13: pausar / voltar
    BTN_KEY,        // PA0 (botao KEY da BlackPill): mesmo papel do A
    BTN_COUNT
} button_id_t;

void buttons_init(void);

// Le os pinos sem filtro: bit i = 1 se o botao i esta pressionado
uint32_t buttons_read_raw(void);
