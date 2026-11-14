# Rastreabilidade dos requisitos

## Requisitos do projeto

| # | Requisito | Como foi atendido | Código |
|---|---|---|---|
| 1 | Interface gráfica no ST7789: labirinto, paredes, caminhos, personagem, coletáveis | Labirinto 20×18 células de 12 px, paredes com contorno, pastilhas e pastilhas de poder, Pac-Man animado, fantasmas, placar no topo | `task_display.c`, `st7789.c`, `maze.c` |
| 2 | Controle por inclinação nos dois eixos, proporcional, com aceleração, velocidade e inércia | `a = ganho × inclinação` (com zona morta), `v = (v + a) × atrito`, `p = p + v`; velocidade máxima limitada. Calibração do zero no início de cada nível | `task_pacman.c` (`pacman_step`), `task_io.c` (`task_imu`), `config.h` |
| 3 | Colisões: não atravessar paredes nem sair do cenário | Caixa de colisão testada eixo a eixo contra as células de parede; fora do mapa conta como parede | `task_pacman.c` (`box_hits_wall`), `maze.c` (`maze_is_wall`) |
| 4 | Coletáveis e pontuação no display | Pastilha 10, pastilha de poder 50, fantasma 200, bônus de tempo; placar, nível, tempo e vidas no topo | `task_game.c`, `task_display.c` (`draw_hud`) |
| 5 | Máquina de estados com botões físicos | MENU, READY, PLAYING, PAUSED, DYING, LEVEL_CLEAR, GAME_OVER, VICTORY; botões A/KEY e B | `task_game.c` (`handle_button`, `timed_states`) |
| 6 | Dificuldade progressiva, ≥ 2 níveis distintos | 3 níveis: labirinto A/2 fantasmas → labirinto B/3 fantasmas mais rápidos → labirinto A/4 fantasmas, mais rápidos e com menos tempo | `maze.c` (`levels`) |
| 7 | Vitória e derrota | Vitória: limpar os 3 níveis. Derrota: perder as 3 vidas ou o tempo do nível acabar | `task_game.c` |
| 8 | Placa perfurada com headers | Pinagem em `README.md` / `config.h` (montagem física) | — |

## Requisitos do RTOS

| # | Requisito | Como foi atendido |
|---|---|---|
| 1 | Pac-Man e cada inimigo como tarefas independentes | `task_pacman` + 4 instâncias de `task_ghost` (parâmetro = índice do fantasma, cada um com sua personalidade) |
| 2 | Tarefas para MPU6050 e display | `task_imu` (100 Hz) e `task_display` |
| 3 | Mutexes e semáforos | Mutexes `m_game`, `m_lcd`, `m_i2c`; semáforos binários `s_step[5]` e `s_frame` |
| 4 | Filas de mensagens | `q_tilt` (sensor), `q_input` (botões), `q_game_evt` (eventos do jogo), `q_render` (comandos de tela), `q_log` (serial) |
| 5 | Prioridades definidas e justificadas | Tabela no `README.md` e comentário em `main.c` |
| 6 | Sem condições de corrida | Todo acesso a `g_game` com `m_game`; o display trabalha numa cópia; SPI e I2C só com seus mutexes; dados entre tarefas passam por filas (cópia) |
