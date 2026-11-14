#include <math.h>

#include "game.h"
#include "config.h"

// Tarefa do Pac-Man: a cada passo liberado pelo jogo (s_step[0]) le a
// inclinacao mais recente (q_tilt) e integra aceleracao -> velocidade ->
// posicao, com atrito (inercia) e colisao contra as paredes.

#define HALF 4.5f       // meia largura da caixa de colisao (corredor tem 12 px)

static int box_hits_wall(const game_t *g, float x, float y)
{
    float x0 = x - HALF, x1 = x + HALF - 0.01f;
    float y0 = y - HALF, y1 = y + HALF - 0.01f;

    return maze_is_wall(g, (int)floorf(x0 / TILE), (int)floorf(y0 / TILE)) ||
           maze_is_wall(g, (int)floorf(x1 / TILE), (int)floorf(y0 / TILE)) ||
           maze_is_wall(g, (int)floorf(x0 / TILE), (int)floorf(y1 / TILE)) ||
           maze_is_wall(g, (int)floorf(x1 / TILE), (int)floorf(y1 / TILE));
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static float deadzone(float v)
{
    if (fabsf(v) < TILT_DEADZONE)
        return 0.0f;

    return v > 0.0f ? v - TILT_DEADZONE : v + TILT_DEADZONE;
}

// Puxa a coordenada transversal para o centro do corredor; facilita entrar
// nas curvas, ja que com inclinacao e dificil acertar o pixel exato.
static void lane_assist(game_t *g, int horizontal)
{
    pacman_t *p = &g->pac;

    if (horizontal)
    {
        float cy = floorf(p->y / TILE) * TILE + TILE / 2.0f;
        float ny = p->y + clampf((cy - p->y) * 0.25f, -0.8f, 0.8f);

        if (!box_hits_wall(g, p->x, ny))
            p->y = ny;
    }
    else
    {
        float cx = floorf(p->x / TILE) * TILE + TILE / 2.0f;
        float nx = p->x + clampf((cx - p->x) * 0.25f, -0.8f, 0.8f);

        if (!box_hits_wall(g, nx, p->y))
            p->x = nx;
    }
}

static void pacman_step(game_t *g, const tilt_msg_t *tilt)
{
    pacman_t *p = &g->pac;
    float ax = deadzone(tilt->x) * PAC_ACCEL_GAIN;
    float ay = deadzone(tilt->y) * PAC_ACCEL_GAIN;

    // Aceleracao proporcional a inclinacao + atrito: velocidade terminal
    // proporcional a inclinacao e o personagem "desliza" ao soltar
    p->vx = clampf((p->vx + ax) * PAC_FRICTION, -PAC_MAX_SPEED, PAC_MAX_SPEED);
    p->vy = clampf((p->vy + ay) * PAC_FRICTION, -PAC_MAX_SPEED, PAC_MAX_SPEED);

    // Eixo X
    float nx = p->x + p->vx;

    if (box_hits_wall(g, nx, p->y))
    {
        if (p->vx > 0.0f)
            nx = floorf((nx + HALF) / TILE) * TILE - HALF - 0.001f;
        else
            nx = (floorf((nx - HALF) / TILE) + 1.0f) * TILE + HALF + 0.001f;

        p->vx = 0.0f;
    }

    p->x = nx;

    // Eixo Y
    float ny = p->y + p->vy;

    if (box_hits_wall(g, p->x, ny))
    {
        if (p->vy > 0.0f)
            ny = floorf((ny + HALF) / TILE) * TILE - HALF - 0.001f;
        else
            ny = (floorf((ny - HALF) / TILE) + 1.0f) * TILE + HALF + 0.001f;

        p->vy = 0.0f;
    }

    p->y = ny;

    // Alinhamento ao corredor no eixo transversal ao movimento pretendido
    // (inclinacao pesa mais que a velocidade atual, para virar nas curvas)
    float intent_x = fabsf(ax) * 10.0f + fabsf(p->vx);
    float intent_y = fabsf(ay) * 10.0f + fabsf(p->vy);

    if (intent_x > 0.05f || intent_y > 0.05f)
        lane_assist(g, intent_x >= intent_y);

    // Direcao da boca
    if (fabsf(p->vx) > 0.15f || fabsf(p->vy) > 0.15f)
    {
        if (fabsf(p->vx) >= fabsf(p->vy))
            p->face = p->vx > 0.0f ? DIR_RIGHT : DIR_LEFT;
        else
            p->face = p->vy > 0.0f ? DIR_DOWN : DIR_UP;
    }

    // Comer pastilha quando o centro passa perto do centro da celula
    int tx = (int)(p->x / TILE);
    int ty = (int)(p->y / TILE);
    float dx = p->x - (tx * TILE + TILE / 2.0f);
    float dy = p->y - (ty * TILE + TILE / 2.0f);
    uint8_t t = g->tiles[ty][tx];

    if ((t == T_PELLET || t == T_POWER) && fabsf(dx) < 5.0f && fabsf(dy) < 5.0f)
    {
        game_event_t ev = { t == T_POWER ? EV_POWER : EV_PELLET, (int8_t)tx, (int8_t)ty };

        g->tiles[ty][tx] = T_EMPTY;
        xQueueSend(q_game_evt, &ev, 0);
    }
}

void task_pacman(void *arg)
{
    (void)arg;
    tilt_msg_t tilt;

    for (;;)
    {
        xSemaphoreTake(s_step[0], portMAX_DELAY);

        // A fila tem tamanho 1 e e sobrescrita pela IMU: Peek pega sempre
        // a leitura mais recente sem consumi-la
        if (xQueuePeek(q_tilt, &tilt, 0) != pdPASS)
            tilt.x = tilt.y = 0.0f;

        xSemaphoreTake(m_game, portMAX_DELAY);

        if (g_game.state == ST_PLAYING)
            pacman_step(&g_game, &tilt);

        xSemaphoreGive(m_game);
    }
}
