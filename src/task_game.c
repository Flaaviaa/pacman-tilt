#include "game.h"
#include "config.h"

// Tarefa do jogo: maquina de estados principal e "relogio" do jogo.
// A cada FRAME_MS:
//   1) trata botoes (q_input) e eventos dos personagens (q_game_evt)
//   2) verifica colisoes Pac-Man x fantasmas, tempo e fim de nivel
//   3) libera um passo para cada personagem (s_step) e um quadro (s_frame)

#define SECONDS(s)          ((uint32_t)((s) * 1000U / FRAME_MS))
#define READY_FRAMES        SECONDS(2)
#define DYING_FRAMES        SECONDS(2)
#define CLEAR_FRAMES        SECONDS(3)
#define FRIGHT_FRAMES       SECONDS(7)

#define SCORE_PELLET        10U
#define SCORE_POWER         50U
#define SCORE_GHOST         200U
#define SCORE_TIME_BONUS    5U      // por segundo restante ao concluir o nivel

static const char *const state_names[] =
{
    "MENU", "READY", "PLAYING", "PAUSED", "DYING", "LEVEL_CLEAR", "GAME_OVER", "VICTORY"
};

// Chamar com m_game obtido
static void set_state(game_state_t s, uint32_t timer)
{
    render_msg_t r = { R_SCREEN, s };

    g_game.state = s;
    g_game.state_timer = timer;

    if (s == ST_PAUSED)
        r.type = R_OVERLAY_PAUSE;

    xQueueSend(q_render, &r, 0);
    log_printf("estado -> %s (nivel %u, vidas %u, pontos %lu)\r\n", state_names[s],
               g_game.level + 1U, g_game.lives, (unsigned long)g_game.score);
}

static void start_level(uint8_t level)
{
    maze_load_level(&g_game, level);
    xTaskNotifyGive(h_imu);         // recalibra o "nivel zero" da IMU
    set_state(ST_READY, READY_FRAMES);
}

static void handle_button(button_id_t b)
{
    int confirm = (b == BTN_A || b == BTN_KEY);

    // Atalho A + B: vai direto para o proximo nivel (ou vitoria no ultimo)
    if (b == BTN_SKIP_LEVEL)
    {
        if (g_game.state == ST_READY || g_game.state == ST_PLAYING ||
            g_game.state == ST_PAUSED || g_game.state == ST_DYING)
        {
            if (g_game.level + 1U < NUM_LEVELS)
                start_level((uint8_t)(g_game.level + 1U));
            else
                set_state(ST_VICTORY, 0);
        }
        return;
    }

    switch (g_game.state)
    {
    case ST_MENU:
        if (confirm)
        {
            g_game.score = 0;
            g_game.lives = START_LIVES;
            start_level(0);
        }
        break;

    case ST_READY:
    case ST_PLAYING:
        if (b == BTN_B)
            set_state(ST_PAUSED, 0);
        break;

    case ST_PAUSED:
        if (confirm)
        {
            xTaskNotifyGive(h_imu);
            set_state(ST_READY, SECONDS(1));
        }
        else if (b == BTN_B)
        {
            set_state(ST_MENU, 0);
        }
        break;

    case ST_GAME_OVER:
    case ST_VICTORY:
        if (confirm)
            set_state(ST_MENU, 0);
        break;

    default:
        break;
    }
}

static void handle_event(const game_event_t *ev)
{
    if (g_game.pellets_left > 0U)
        g_game.pellets_left--;

    if (ev->type == EV_PELLET)
    {
        g_game.score += SCORE_PELLET;
    }
    else
    {
        g_game.score += SCORE_POWER;
        g_game.fright_frames = FRIGHT_FRAMES;

        for (int i = 0; i < MAX_GHOSTS; i++)
        {
            ghost_t *gh = &g_game.ghost[i];

            if (gh->active && gh->mode != GHOST_EYES)
            {
                if (gh->mode == GHOST_NORMAL)
                    gh->reverse = 1;
                gh->mode = GHOST_FRIGHT;
            }
        }
    }
}

static void check_ghost_collisions(void)
{
    for (int i = 0; i < MAX_GHOSTS; i++)
    {
        ghost_t *gh = &g_game.ghost[i];

        if (!gh->active || gh->mode == GHOST_EYES)
            continue;

        float dx = gh->x - g_game.pac.x;
        float dy = gh->y - g_game.pac.y;

        if (dx * dx + dy * dy > 8.0f * 8.0f)
            continue;

        if (gh->mode == GHOST_FRIGHT)
        {
            gh->mode = GHOST_EYES;
            g_game.score += SCORE_GHOST;
            log_printf("fantasma %d comido\r\n", i);
        }
        else
        {
            if (g_game.lives > 0U)
                g_game.lives--;
            set_state(ST_DYING, DYING_FRAMES);
            return;
        }
    }
}

static void playing_frame(void)
{
    g_game.frame++;

    if (g_game.fright_frames > 0U && --g_game.fright_frames == 0U)
    {
        for (int i = 0; i < MAX_GHOSTS; i++)
            if (g_game.ghost[i].mode == GHOST_FRIGHT)
                g_game.ghost[i].mode = GHOST_NORMAL;
    }

    check_ghost_collisions();

    if (g_game.state != ST_PLAYING)
        return;

    if (g_game.pellets_left == 0U)
    {
        g_game.score += (g_game.time_left_ms / 1000U) * SCORE_TIME_BONUS;
        set_state(ST_LEVEL_CLEAR, CLEAR_FRAMES);
        return;
    }

    if (g_game.time_left_ms <= FRAME_MS)
    {
        g_game.time_left_ms = 0;
        g_game.lives = 0;
        set_state(ST_GAME_OVER, 0);     // derrota por tempo
        return;
    }

    g_game.time_left_ms -= FRAME_MS;
}

static void timed_states(void)
{
    if (g_game.state_timer == 0U)
        return;

    if (--g_game.state_timer > 0U)
        return;

    switch (g_game.state)
    {
    case ST_READY:
        set_state(ST_PLAYING, 0);
        break;

    case ST_DYING:
        if (g_game.lives == 0U)
        {
            set_state(ST_GAME_OVER, 0);
        }
        else
        {
            maze_reset_actors(&g_game, g_game.level);
            xTaskNotifyGive(h_imu);
            set_state(ST_READY, READY_FRAMES);
        }
        break;

    case ST_LEVEL_CLEAR:
        if (g_game.level + 1U < NUM_LEVELS)
            start_level((uint8_t)(g_game.level + 1U));
        else
            set_state(ST_VICTORY, 0);
        break;

    default:
        break;
    }
}

void task_game(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    input_msg_t in;
    game_event_t ev;

    xSemaphoreTake(m_game, portMAX_DELAY);
    g_game.lives = START_LIVES;
    set_state(ST_MENU, 0);
    xSemaphoreGive(m_game);

    for (;;)
    {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(FRAME_MS));

        xSemaphoreTake(m_game, portMAX_DELAY);

        while (xQueueReceive(q_input, &in, 0) == pdPASS)
            handle_button(in.id);

        while (xQueueReceive(q_game_evt, &ev, 0) == pdPASS)
            handle_event(&ev);

        if (g_game.state == ST_PLAYING)
            playing_frame();
        else if (g_game.state == ST_DYING)
            g_game.frame++;     // anima o Pac-Man piscando

        timed_states();

        xSemaphoreGive(m_game);

        // Libera os personagens e o display para este quadro
        for (int i = 0; i < NUM_CHARACTERS; i++)
            xSemaphoreGive(s_step[i]);

        xSemaphoreGive(s_frame);
    }
}
