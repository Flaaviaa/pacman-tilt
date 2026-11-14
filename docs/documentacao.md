# Documentação — Pac-Tilt

## 1. CONOPS (conceito de operação)

**O que é:** um jogo portátil no estilo labirinto/perseguição. O jogador segura a placa
com as duas mãos, olha o display e inclina a placa para guiar o personagem; dois botões
controlam o fluxo (iniciar, pausar, voltar).

**Usuário:** qualquer pessoa, sem treinamento. Uma partida dura poucos minutos.

**Cenário de uso:**
1. Liga a placa pelo USB-C → tela de menu (mostra se a IMU foi encontrada).
2. Aperta A → aparece o labirinto e "PRONTO!"; durante 2 s a placa calibra a posição neutra.
3. Joga inclinando a placa; o placar mostra pontos, nível, tempo e vidas.
4. Pode pausar com B. Ao limpar o labirinto passa de nível; ao perder as vidas ou o tempo,
   fim de jogo. Após o 3º nível, vitória. A volta ao menu.

**Restrições:** alimentação pelo USB; tudo em 3,3 V; montagem em placa perfurada.

## 2. Domínio do problema

- **Entrada analógica contínua (inclinação):** o acelerômetro mede a gravidade projetada
  nos eixos X/Y; para inclinações pequenas, `a ≈ g·sen θ`. É ruidoso (tremor da mão) e a
  posição "neutra" depende de como o jogador segura → filtro passa-baixas + calibração.
- **Movimento com inércia:** o personagem não deve "teleportar" para a direção da
  inclinação: acelera, atinge uma velocidade terminal proporcional à inclinação e desliza
  ao nivelar a placa.
- **Labirinto em grade vs. movimento contínuo:** os corredores têm 1 célula (12 px).
  Movimento livre em pixels torna difícil acertar as curvas → assistência que centraliza
  o personagem no corredor no eixo transversal.
- **Concorrência:** vários agentes (Pac-Man, fantasmas, sensor, display) atuam sobre o
  mesmo estado; precisam de sincronização para não ler posições "pela metade".
- **Recursos limitados:** 128 KB de RAM não comportam um framebuffer 240×240×16 bits
  junto com o RTOS → desenho incremental por regiões.

## 3. Especificação

### Funcionais
- RF1 Exibir labirinto, pastilhas, Pac-Man, fantasmas e placar no ST7789.
- RF2 Mover o Pac-Man pela inclinação, nos dois eixos, com aceleração proporcional e inércia.
- RF3 Impedir passagem por paredes e saída do mapa.
- RF4 Pastilhas (10), pastilhas de poder (50 + fantasmas vulneráveis por 7 s), fantasma (200), bônus de tempo.
- RF5 Estados: menu, pronto, jogando, pausa, morte, nível concluído, fim de jogo, vitória.
- RF6 Pelo menos 2 níveis com labirintos/dificuldade diferentes (implementados 3).
- RF7 3 vidas e limite de tempo por nível.
- RF8 Botão A/KEY confirma; B pausa/volta.

### Não funcionais
- RNF1 Lógica a 50 Hz (quadro de 20 ms), IMU a 100 Hz, botões a 100 Hz.
- RNF2 Tempo de resposta à inclinação < 100 ms (filtro + 1 quadro).
- RNF3 O jogo não trava sem o MPU6050: timeouts no I2C, nova tentativa a cada 1 s, aviso no menu.
- RNF4 Logs pela USB para depuração (estado, IMU, uso de pilha).

### Máquina de estados

```
          A/KEY                 2 s                  pastilhas = 0
 MENU ─────────> READY ───────────────> PLAYING ─────────────────> LEVEL_CLEAR
  ^               ^  ^                   │  │ │                       │ 3 s
  │               │  │ A                 │  │ └─ B ──> PAUSED ─ B ─> MENU
  │               │  └───────────────────┼──┼────────────┘            │
  │               │ vidas > 0            │  │ fantasma               ├─ próximo nível ─> READY
  │               └──────── DYING <──────┘  │                        └─ último ──────> VICTORY ─ A ─> MENU
  │                          │ vidas = 0    │ tempo = 0
  └──── A ──── GAME_OVER <───┴──────────────┘
```

## 4. Design

### Tarefas, prioridades e sincronização
Ver tabelas em [README.md](../README.md#arquitetura-freertos).

**Fluxo de um quadro (20 ms):**
1. `jogo` (p3) acorda por `vTaskDelayUntil`, pega `m_game`, consome `q_input` e
   `q_game_evt`, aplica regras e colisões Pac-Man×fantasmas, solta o mutex.
2. Dá `s_step[0..4]` → `pacman` e `fantasma` (p2) rodam, cada um pegando `m_game`
   para atualizar só o próprio personagem. O Pac-Man lê a inclinação com `xQueuePeek` em
   `q_tilt` e, ao comer uma pastilha, envia um evento em `q_game_evt`.
3. Dá `s_frame` → `display` (p1) copia `g_game` (com `m_game`), solta o mutex e desenha
   com `m_lcd`.

`imu` (p5) e `botoes` (p4) são periódicas e independentes do quadro; comunicam-se apenas
por filas, então nunca bloqueiam no estado do jogo (exceto para marcar `imu_ok`).

### Física do Pac-Man
```
a  = ganho · zona_morta(inclinação)          ganho = 0,30 px/quadro² por g
v  = clamp((v + a) · 0,92, ±1,8 px/quadro)   (atrito → inércia e velocidade terminal ∝ inclinação)
p' = p + v ; se a caixa 9×9 tocar parede → encosta na parede e zera v naquele eixo
```
A colisão é resolvida separadamente em X e Y, o que permite deslizar ao longo das paredes.

### IA dos fantasmas
Movimento em grade; em cada célula escolhe, sem meia-volta, a direção que minimiza a
distância ao alvo: vermelho → Pac-Man; rosa → 4 células à frente; ciano → reflexo do
vermelho em torno de 2 células à frente; laranja → persegue de longe e foge de perto.
Assustados escolhem ao acaso; "olhos" voltam para casa.

### Renderização
Sem framebuffer. Cada sprite é redesenhado na união do retângulo antigo com o novo:
a região é composta num buffer pequeno (fundo do labirinto + todos os sprites que caem
nela) e enviada numa única janela SPI — sem cintilação. O placar só é redesenhado
quando algum valor muda.

## 5. Estudo da plataforma

| Recurso | STM32F411CE (BlackPill) | Uso no projeto |
|---|---|---|
| CPU | Cortex-M4F, 100 MHz máx. | 84 MHz (HSE 25 MHz + PLL, para ter 48 MHz na USB) |
| FPU | precisão simples | física e filtros em `float` (ABI softfp, como na aula) |
| RAM / Flash | 128 KB / 512 KB | ~57 KB RAM (48 KB de heap do FreeRTOS) / ~34 KB Flash |
| SPI1 | até 42 MHz | ST7789 a 21 MHz, modo 3, quadros de 16 bits nos pixels |
| I2C1 | até 400 kHz | MPU6050 em fast mode |
| USB OTG FS | device | CDC (TinyUSB) para logs; DFU da ST para gravar |
| NVIC | 4 bits de prioridade | FreeRTOS: kernel 15, limite de syscalls 5 |

**ST7789:** controlador 240×320; o módulo de 240×240 usa a janela 0..239 sem offset com
MADCTL = 0. Precisa de `INVON` (painel IPS). Modo SPI 3 porque muitos módulos não têm CS.

**MPU6050:** acelerômetro ±2 g (16384 LSB/g), filtro digital interno de ~44 Hz,
endereço 0x68. Lido em rajada de 6 bytes a partir de `ACCEL_XOUT_H` (0x3B).

**FreeRTOS:** V11.1.0, port `GCC/ARM_CM4F`, `heap_4`, tick de 1 ms, preempção com
time slicing, verificação de estouro de pilha (método 2).
