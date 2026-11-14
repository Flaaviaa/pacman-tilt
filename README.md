# Pac-Tilt — ELF74 Sistemas Embarcados (Projeto Final)

Jogo inspirado no Pac-Man em que o personagem é controlado **inclinando a placa**.
BlackPill **STM32F411CE** + **FreeRTOS** + display **ST7789 240×240** + acelerômetro **MPU6050**,
escrito em C sobre CMSIS (registradores), no PlatformIO, como nas aulas.

| Item | Onde |
|---|---|
| Requisitos → implementação | [docs/requisitos.md](docs/requisitos.md) |
| Documentação (CONOPS, domínio, especificação, design, plataforma) | [docs/documentacao.md](docs/documentacao.md) |
| Montagem / pinagem | abaixo e [include/config.h](include/config.h) |

## Montagem

Todos os módulos em **3V3**. Botões entre o pino e o **GND** (pull-up interno).

| Módulo | Pino do módulo | BlackPill |
|---|---|---|
| ST7789 240×240 | SCL / SCK | **PA5** (SPI1_SCK) |
| | SDA / MOSI | **PA7** (SPI1_MOSI) |
| | RES | **PB1** |
| | DC | **PB0** |
| | CS (se existir) | **PA4** |
| | BLK | **PB10** |
| MPU6050 | SCL | **PB6** (I2C1) |
| | SDA | **PB7** (I2C1) |
| | AD0 | GND (endereço 0x68) |
| Botão A (confirmar) | — | **PB12** |
| Botão B (pausa / voltar) | — | **PB13** |
| Botão KEY da placa | — | PA0 (mesma função do A) |

A maioria dos módulos MPU6050 (GY-521) já tem resistores de pull-up no I2C.

## Compilar e gravar

Requisitos: VS Code + extensão PlatformIO (ou `pio` na linha de comando). O FreeRTOS
já está em `lib/FreeRTOS` (V11.1.0) e a TinyUSB é baixada automaticamente.

```bash
pio run
```

Para gravar, coloque a BlackPill em modo DFU — segure **BOOT0**, aperte e solte
**NRST**, solte **BOOT0** — e então:

```bash
pio run -t upload
```

O firmware começa a rodar logo após a gravação (não precisa apertar NRST).

Logs (estado do jogo, IMU, botões) pela serial USB; no monitor digite `h` (ajuda),
`t` (liga/desliga leituras da IMU) ou `s` (uso de pilha das tarefas e heap):

```bash
pio device monitor
```

## Como jogar

1. Menu: aperte **A** (ou **KEY**). Segure a placa **na posição em que vai jogar** durante
   o "PRONTO!" — essa inclinação vira o zero (calibração).
2. Incline para mover. A velocidade é proporcional à inclinação e o Pac-Man tem inércia.
3. Coma todas as pastilhas antes do tempo acabar. A pastilha grande deixa os fantasmas
   azuis por 7 s (dá para comê-los: +200).
4. **B** pausa; na pausa, **A** continua e **B** volta ao menu.

3 vidas. Três níveis: labirinto A com 2 fantasmas → labirinto B com 3 fantasmas mais
rápidos → labirinto A com 4 fantasmas ainda mais rápidos e menos tempo.
Pontos: pastilha 10, pastilha grande 50, fantasma 200, bônus de 5 por segundo restante.

Se o Pac-Man andar para o lado errado, ajuste `IMU_SWAP_XY`, `IMU_INVERT_X` e
`IMU_INVERT_Y` em [include/config.h](include/config.h) conforme a posição do módulo.

## Arquitetura (FreeRTOS)

```
 MPU6050 ──I2C──> [imu p5] ──q_tilt──> [pacman p2] ──q_game_evt──┐
 botões ────────> [botoes p4] ──q_input────────────────────────────┤
                                                                   v
                     s_step[0..4] (a cada 20 ms)  <──────────── [jogo p3]
                     [fantasma 0..3 p2]                            │ s_frame + q_render
                                                                   v
                                                [display p1] ──SPI──> ST7789
 todas ──q_log──> [usb p4] ──USB CDC──> PC
```

| Tarefa | Prio | Período | Por quê |
|---|---|---|---|
| `imu` | 5 | 10 ms | Amostragem periódica do sensor; curta. Jitter aqui vira ruído no controle. |
| `botoes` | 4 | 10 ms | Debounce depende de amostragem regular; curta. |
| `usb` | 4 | 1 ms | A pilha USB tem prazos do host; cada execução é curta. |
| `jogo` | 3 | 20 ms | Relógio do jogo e regras (FSM, colisões, tempo). Precisa rodar antes dos personagens. |
| `pacman` | 2 | por quadro | Física; liberada pelo jogo via semáforo. |
| `fantasma` ×4 | 2 | por quadro | IA; mesma prioridade do Pac-Man (time slicing entre eles). |
| `display` | 1 | por quadro | A mais longa (SPI). Fica com o tempo que sobra: se atrasar, só cai o FPS, a lógica não é afetada. |

Sincronização e comunicação:

| Objeto | Tipo | Uso |
|---|---|---|
| `m_game` | mutex | Protege `g_game` (labirinto, posições, pontos). Todas as tarefas de personagem, o jogo e o display (cópia/snapshot) |
| `m_lcd` | mutex | Acesso exclusivo ao SPI1/ST7789 |
| `m_i2c` | mutex | Acesso exclusivo ao I2C1/MPU6050 |
| `s_step[5]` | semáforos binários | O jogo libera um passo por quadro para cada personagem |
| `s_frame` | semáforo binário | O jogo libera um quadro para o display |
| `q_tilt` | fila (1, sobrescrita) | IMU → Pac-Man: sempre a leitura mais recente |
| `q_input` | fila | Botões → jogo |
| `q_game_evt` | fila | Pac-Man → jogo: pastilhas comidas |
| `q_render` | fila | Jogo → display: trocar de tela |
| `q_log` | fila | Qualquer tarefa → USB |
| notificação | task notification | Jogo → IMU: recalibrar |

O display nunca desenha direto do estado compartilhado: copia `g_game` com `m_game`
e solta o mutex antes de usar o SPI, então os personagens não esperam pelo display.

## Estrutura

```
include/   config.h (pinos e ajustes), FreeRTOSConfig.h, tusb_config.h
src/       main.c          criação das tarefas e objetos do RTOS
           task_game.c     máquina de estados e regras
           task_pacman.c   física do Pac-Man e colisões com paredes
           task_ghost.c    IA dos fantasmas
           task_display.c  renderização (labirinto, sprites, placar, telas)
           task_io.c       IMU, botões, USB/log
           maze.c          labirintos e níveis
           st7789.c, mpu6050.c, buttons.c, board.c, clock.c, font5x7.c
lib/FreeRTOS  kernel V11.1.0 (port GCC/ARM_CM4F, heap_4)
tools/     check_mazes.py (valida labirintos)
```
