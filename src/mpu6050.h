#pragma once

#include <stdint.h>

typedef struct
{
    int16_t ax, ay, az;     // aceleracao bruta (16384 = 1 g na escala +-2 g)
} mpu_raw_t;

// I2C1 em PB6 (SCL) / PB7 (SDA), 400 kHz.
// Retorna o WHO_AM_I lido (0x68 no MPU6050 original; clones respondem
// 0x70/0x72/0x98) ou -1 se o sensor nao respondeu.
int mpu_init(void);

// Le os tres eixos do acelerometro. Retorna 0 em caso de sucesso.
int mpu_read_accel(mpu_raw_t *out);
