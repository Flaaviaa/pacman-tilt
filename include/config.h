#pragma once

// ---------------------------------------------------------------------------
// Ligacoes na BlackPill STM32F411CE (ver README / docs/hardware.md)
// ---------------------------------------------------------------------------
//   ST7789 240x240 (SPI1)       MPU6050 (I2C1)        Botoes (ligam ao GND)
//   SCL/SCK -> PA5              SCL -> PB6            A   -> PB12 (confirmar)
//   SDA/MOSI-> PA7              SDA -> PB7            B   -> PB13 (pausa/voltar)
//   CS      -> PA4 (se houver)  VCC -> 3V3            KEY -> PA0 (botao da placa)
//   DC      -> PB0              GND -> GND
//   RES     -> PB1
//   BLK     -> PB10
//   VCC     -> 3V3, GND -> GND

#define LCD_W 240
#define LCD_H 240

// Rotacao da tela (MADCTL do ST7789): 0x00, 0x60, 0xC0 ou 0xA0
#define LCD_MADCTL 0x00U

// Divisor do SPI1 (APB2 = 84 MHz). 1 -> /4 = 21 MHz.
// Se a imagem sair com ruido (fios longos), troque para 2 (/8 = 10.5 MHz).
#define LCD_SPI_BR 1U

// ---------------------------------------------------------------------------
// Eixos do MPU6050: ajuste conforme a posicao do modulo na placa perfurada.
// tilt_x > 0 deve mover o Pac-Man para a direita, tilt_y > 0 para baixo.
// ---------------------------------------------------------------------------
#define IMU_SWAP_XY  0
#define IMU_INVERT_X 1
#define IMU_INVERT_Y 0

// ---------------------------------------------------------------------------
// Fisica do Pac-Man (unidades: pixels e quadros de 20 ms)
// ---------------------------------------------------------------------------
#define PAC_ACCEL_GAIN  0.30f   // aceleracao por g de inclinacao
#define PAC_FRICTION    0.92f   // fracao da velocidade mantida por quadro (inercia)
#define PAC_MAX_SPEED   1.80f   // pixels por quadro
#define TILT_DEADZONE   0.05f   // g

// Periodos das tarefas
#define FRAME_MS   20U          // 50 quadros por segundo
#define IMU_MS     10U          // 100 amostras por segundo
#define BUTTON_MS  10U

#define START_LIVES 3U
