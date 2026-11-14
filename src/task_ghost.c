#include <stdlib.h>

#include "game.h"
#include "config.h"

// Uma instancia desta tarefa por fantasma (parametro = indice 0..3).
// Os fantasmas andam celula a celula; em cada celula escolhem a direcao
// (sem dar meia-volta) que mais aproxima do alvo da sua personalidade.

static const int8_t DX[4] = { 1, 0, -1, 0 };
static const int8_t DY[4] = { 0, 1, 0, -1 };

static uint32_t rng = 0x9E3779B9U;

static uint32_t rand_u32(void)
{
    // xorshift32 (so chamado com m_game obtido)
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static dir_t opposite(dir_t d)
{
    return (dir_t)((d + 2) % 4);
}

static void ghost_target(const game_t *g, const ghost_t *gh, int *tx, int *ty)
{
    int px = (int)(g->pac.x / TILE);
    int py = (int)(g->pac.y / TILE);
    dir_t f = g->pac.face == DIR_NONE ? DIR_LEFT : g->pac.face;

    if (gh->mode == GHOST_EYES)
    {
        *tx = gh->home_tx;
        *ty = gh->home_ty;
        return;
    }

    if (g->tiles[gh->ty][gh->tx] == T_HOUSE)
    {
        // Ainda na casa: vai para a celula logo acima da saida
        *tx = g->ghost[0].home_tx;
        *ty = g->ghost[0].home_ty - 1;
        return;
    }

    switch (gh->personality)
    {
    case 0:     // vermelho: direto no Pac-Man
        *tx = px;
        *ty = py;
        break;

    case 1:     // rosa: 4 celulas a frente do Pac-Man
        *tx = px + 4 * DX[f];
        *ty = py + 4 * DY[f];
        break;

    case 2:     // ciano: espelha o vermelho em torno de 2 celulas a frente
    {
        const ghost_t *red = &g->ghost[0];
        int ax = px + 2 * DX[f];
        int ay = py + 2 * DY[f];
        *tx = 2 * ax - red->tx;
        *ty = 2 * ay - red->ty;
        break;
    }

    default:    // laranja: persegue de longe, foge para o canto de perto
    {
        int dx = px - gh->tx, dy = py - gh->ty;

        if (dx * dx + dy * dy > 36)
        {
            *tx = px;
            *ty = py;
        }
        else
        {
            *tx = 0;
            *ty = MAZE_H - 1;
        }
        break;
    }
    }
}

static dir_t choose_dir(const game_t *g, ghost_t *gh)
{
    dir_t options[4];
    int n = 0;

    for (int d = 0; d < 4; d++)
    {
        if ((dir_t)d == opposite(gh->dir))
            continue;

        int nx = gh->tx + DX[d], ny = gh->ty + DY[d];

        if (maze_is_wall(g, nx, ny))
            continue;

        // Depois de sair, so volta para dentro da casa como "olhos"
        if (gh->mode != GHOST_EYES && g->tiles[ny][nx] == T_HOUSE &&
            g->tiles[gh->ty][gh->tx] != T_HOUSE)
            continue;

        options[n++] = (dir_t)d;
    }

    if (n == 0)
        return opposite(gh->dir);   // beco sem saida

    if (gh->mode == GHOST_FRIGHT && g->tiles[gh->ty][gh->tx] != T_HOUSE)
        return options[rand_u32() % (uint32_t)n];

    int tx, ty;
    ghost_target(g, gh, &tx, &ty);

    dir_t best = options[0];
    int best_d = 0x7FFFFFFF;

    for (int i = 0; i < n; i++)
    {
        int dx = gh->tx + DX[options[i]] - tx;
        int dy = gh->ty + DY[options[i]] - ty;
        int d = dx * dx + dy * dy;

        if (d < best_d)
        {
            best_d = d;
            best = options[i];
        }
    }

    return best;
}

static void ghost_step(game_t *g, ghost_t *gh)
{
    if (gh->release_frames > 0U)
    {
        gh->release_frames--;
        return;
    }

    float speed = g->ghost_speed;

    if (gh->mode == GHOST_FRIGHT)
        speed *= 0.55f;
    else if (gh->mode == GHOST_EYES)
        speed = 2.5f;

    if (gh->reverse)
    {
        // Meia-volta: a celula de destino vira a de origem
        gh->reverse = 0;

        if (gh->progress > 0.0f)
        {
            gh->tx = (int8_t)(gh->tx + DX[gh->dir]);
            gh->ty = (int8_t)(gh->ty + DY[gh->dir]);
            gh->progress = TILE - gh->progress;
        }

        gh->dir = opposite(gh->dir);
    }

    float remaining = speed;

    while (remaining > 0.0f)
    {
        if (gh->progress == 0.0f)
        {
            // No centro de uma celula
            if (gh->mode == GHOST_EYES && gh->tx == gh->home_tx && gh->ty == gh->home_ty)
                gh->mode = GHOST_NORMAL;

            gh->dir = choose_dir(g, gh);

            if (maze_is_wall(g, gh->tx + DX[gh->dir], gh->ty + DY[gh->dir]))
                break;
        }

        float step = TILE - gh->progress;

        if (remaining < step)
        {
            gh->progress += remaining;
            remaining = 0.0f;
        }
        else
        {
            gh->tx = (int8_t)(gh->tx + DX[gh->dir]);
            gh->ty = (int8_t)(gh->ty + DY[gh->dir]);
            gh->progress = 0.0f;
            remaining -= step;
        }
    }

    gh->x = gh->tx * TILE + TILE / 2.0f + DX[gh->dir] * gh->progress;
    gh->y = gh->ty * TILE + TILE / 2.0f + DY[gh->dir] * gh->progress;
}

void task_ghost(void *arg)
{
    int id = (int)(intptr_t)arg;

    for (;;)
    {
        xSemaphoreTake(s_step[1 + id], portMAX_DELAY);

        xSemaphoreTake(m_game, portMAX_DELAY);

        ghost_t *gh = &g_game.ghost[id];

        if (g_game.state == ST_PLAYING && gh->active)
            ghost_step(&g_game, gh);

        xSemaphoreGive(m_game);
    }
}
