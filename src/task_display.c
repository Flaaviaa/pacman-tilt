#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game.h"
#include "config.h"
#include "st7789.h"

// Tarefa do display (prioridade mais baixa): a cada quadro tira uma copia
// do estado do jogo (com m_game) e redesenha so as regioes que mudaram.
// Nao ha framebuffer completo (240x240x2 = 115 KB nao cabe junto com o
// RTOS); cada regiao e composta num buffer pequeno (fundo + sprites) e
// enviada de uma vez, o que evita cintilacao.

extern const uint8_t font5x7[][5];

#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_YELLOW  RGB565(255, 230,   0)
#define C_RED     RGB565(255,  40,  40)
#define C_CYAN    RGB565(  0, 220, 255)
#define C_GREEN   RGB565( 40, 220,  60)
#define C_GREY    RGB565(140, 140, 140)
#define C_WALL    RGB565( 33,  33, 222)
#define C_WALL_IN RGB565(  0,   0,  60)
#define C_PELLET  RGB565(255, 184, 151)
#define C_FRIGHT  RGB565( 40,  40, 255)
#define C_DOOR    RGB565(255, 184, 255)

#define TRANSPARENT 0x0001      // cor "reservada" para pixel vazio de sprite

#define NUM_SPRITES (1 + MAX_GHOSTS)
#define SPRITE      12

static game_t snap;                         // copia local do estado
static uint16_t pix[LCD_W * TILE];          // buffer de composicao (5,6 KB)

typedef struct { int x, y, valid; } rect_t;
static rect_t prev[NUM_SPRITES];            // onde cada sprite foi desenhado

static struct
{
    uint32_t score, secs;
    uint8_t level, lives, state, valid;
} hud;

static void take_snapshot(void)
{
    xSemaphoreTake(m_game, portMAX_DELAY);
    memcpy(&snap, &g_game, sizeof(snap));
    xSemaphoreGive(m_game);
}

static void lcd_lock(void)   { xSemaphoreTake(m_lcd, portMAX_DELAY); }
static void lcd_unlock(void) { xSemaphoreGive(m_lcd); }

// ---------------------------------------------------------------------------
// Fundo (labirinto)
// ---------------------------------------------------------------------------
static int is_wall(int tx, int ty)
{
    return maze_is_wall(&snap, tx, ty);
}

static uint16_t tile_pixel(int px, int py)
{
    int tx = px / TILE, ty = py / TILE;
    int lx = px % TILE, ly = py % TILE;

    switch (snap.tiles[ty][tx])
    {
    case T_WALL:
        // Contorno azul nos lados que encostam num corredor
        if ((lx < 2 && !is_wall(tx - 1, ty)) || (lx > TILE - 3 && !is_wall(tx + 1, ty)) ||
            (ly < 2 && !is_wall(tx, ty - 1)) || (ly > TILE - 3 && !is_wall(tx, ty + 1)))
            return C_WALL;
        return C_WALL_IN;

    case T_PELLET:
        return (lx >= 5 && lx <= 6 && ly >= 5 && ly <= 6) ? C_PELLET : C_BLACK;

    case T_POWER:
    {
        int dx = 2 * lx - 11, dy = 2 * ly - 11;     // distancia ao centro x2
        return (dx * dx + dy * dy <= 64) ? C_PELLET : C_BLACK;
    }

    case T_HOUSE:
        // "Porta" rosa na parede de cima da casa
        return (ly == 0 && is_wall(tx, ty - 1) == 0 && snap.tiles[ty - 1][tx] != T_HOUSE)
               ? C_DOOR : C_BLACK;

    default:
        return C_BLACK;
    }
}

// ---------------------------------------------------------------------------
// Sprites (12 x 12, coordenadas relativas ao canto superior esquerdo)
// ---------------------------------------------------------------------------
static uint16_t pacman_pixel(int sx, int sy)
{
    float dx = sx - 5.5f, dy = sy - 5.5f;
    float r2 = dx * dx + dy * dy;

    if (r2 > 5.6f * 5.6f)
        return TRANSPARENT;

    // Boca abrindo e fechando: cos do meio angulo da abertura
    static const float mouth_cos[4] = { 1.01f, 0.92f, 0.70f, 0.92f };
    float c = mouth_cos[(snap.frame / 3U) % 4U];
    float fx = 0.0f, fy = 0.0f;

    switch (snap.pac.face)
    {
    case DIR_RIGHT: fx = 1.0f; break;
    case DIR_LEFT:  fx = -1.0f; break;
    case DIR_DOWN:  fy = 1.0f; break;
    case DIR_UP:    fy = -1.0f; break;
    default: break;
    }

    float dot = dx * fx + dy * fy;

    if (dot > 0.0f && dot * dot > c * c * r2)
        return TRANSPARENT;

    return C_YELLOW;
}

static uint16_t ghost_pixel(const ghost_t *gh, int sx, int sy)
{
    float dx = sx - 5.5f, dy = sy - 5.5f;
    int frightened = (gh->mode == GHOST_FRIGHT);

    // Olhos (unica parte visivel no modo "olhos")
    static const int8_t look_x[5] = { 1, 0, -1, 0, 0 };
    static const int8_t look_y[5] = { 0, 1, 0, -1, 0 };

    if (!frightened)
    {
        for (int e = 0; e < 2; e++)
        {
            float ex = (e ? 2.5f : -2.5f) + look_x[gh->dir] * 0.8f;
            float ey = -1.0f + look_y[gh->dir] * 0.8f;
            float ddx = dx - ex, ddy = dy - ey;

            if (ddx * ddx + ddy * ddy <= 2.3f)
            {
                float px = ex + look_x[gh->dir] * 0.9f, py = ey + look_y[gh->dir] * 0.9f;
                float pdx = dx - px, pdy = dy - py;
                return (pdx * pdx + pdy * pdy <= 0.7f) ? RGB565(30, 30, 200) : C_WHITE;
            }
        }
    }

    if (gh->mode == GHOST_EYES)
        return TRANSPARENT;

    // Corpo: meio circulo em cima, retangulo embaixo, "saia" ondulada
    if (dy < 0.0f)
    {
        if (dx * dx + dy * dy > 5.6f * 5.6f)
            return TRANSPARENT;
    }
    else if (sy >= 10 && ((sx + (int)(snap.frame / 6U)) % 4) == 0)
    {
        return TRANSPARENT;
    }

    if (frightened)
    {
        // Pisca em branco quando o efeito esta acabando
        int blink = snap.fright_frames < 100U && (snap.frame / 8U) % 2U;

        if (sy == 3 && (sx == 4 || sx == 7))
            return C_PELLET;                            // olhinhos
        if (sy == 8 && sx >= 2 && sx <= 9 && (sx % 2) == (sy % 2))
            return C_PELLET;                            // boca ondulada
        return blink ? C_WHITE : C_FRIGHT;
    }

    return gh->color;
}

static int sprite_visible(int i)
{
    if (snap.state != ST_READY && snap.state != ST_PLAYING && snap.state != ST_DYING)
        return 0;

    if (i == 0)
        return snap.state != ST_DYING || ((snap.frame / 8U) % 2U) == 0U;

    return snap.ghost[i - 1].active && snap.state != ST_DYING;
}

static void sprite_rect(int i, int *x, int *y)
{
    float cx = i == 0 ? snap.pac.x : snap.ghost[i - 1].x;
    float cy = i == 0 ? snap.pac.y : snap.ghost[i - 1].y;

    *x = (int)lroundf(cx) - SPRITE / 2;
    *y = (int)lroundf(cy) - SPRITE / 2;
}

// ---------------------------------------------------------------------------
// Composicao de uma regiao do labirinto (coordenadas do labirinto)
// ---------------------------------------------------------------------------
static void compose_and_blit(int rx, int ry, int w, int h)
{
    // Recorta nos limites do labirinto
    if (rx < 0) { w += rx; rx = 0; }
    if (ry < 0) { h += ry; ry = 0; }
    if (rx + w > MAZE_W * TILE) w = MAZE_W * TILE - rx;
    if (ry + h > MAZE_H * TILE) h = MAZE_H * TILE - ry;
    if (w <= 0 || h <= 0 || w * h > (int)(sizeof(pix) / sizeof(pix[0])))
        return;

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            pix[y * w + x] = tile_pixel(rx + x, ry + y);

    // Fantasmas primeiro, Pac-Man por cima
    for (int i = NUM_SPRITES - 1; i >= 0; i--)
    {
        if (!sprite_visible(i))
            continue;

        int sx0, sy0;
        sprite_rect(i, &sx0, &sy0);

        for (int y = 0; y < SPRITE; y++)
        {
            int py = sy0 + y - ry;
            if (py < 0 || py >= h)
                continue;

            for (int x = 0; x < SPRITE; x++)
            {
                int px = sx0 + x - rx;
                if (px < 0 || px >= w)
                    continue;

                uint16_t c = i == 0 ? pacman_pixel(x, y) : ghost_pixel(&snap.ghost[i - 1], x, y);

                if (c != TRANSPARENT)
                    pix[py * w + px] = c;
            }
        }
    }

    lcd_blit(rx, ry + HUD_H, w, h, pix);
}

static void draw_maze_full(void)
{
    for (int ty = 0; ty < MAZE_H; ty++)
        compose_and_blit(0, ty * TILE, MAZE_W * TILE, TILE);

    for (int i = 0; i < NUM_SPRITES; i++)
    {
        sprite_rect(i, &prev[i].x, &prev[i].y);
        prev[i].valid = sprite_visible(i);
    }
}

// Redesenha cada sprite na uniao da posicao antiga com a nova
static void draw_sprites(void)
{
    for (int i = 0; i < NUM_SPRITES; i++)
    {
        int x, y, vis = sprite_visible(i);
        sprite_rect(i, &x, &y);

        if (!vis && !prev[i].valid)
            continue;

        int x0 = x, y0 = y, x1 = x + SPRITE, y1 = y + SPRITE;

        if (prev[i].valid)
        {
            if (prev[i].x < x0) x0 = prev[i].x;
            if (prev[i].y < y0) y0 = prev[i].y;
            if (prev[i].x + SPRITE > x1) x1 = prev[i].x + SPRITE;
            if (prev[i].y + SPRITE > y1) y1 = prev[i].y + SPRITE;
        }

        // Saltos grandes (reinicio de posicoes): duas regioes separadas
        if ((x1 - x0) * (y1 - y0) > 32 * 32)
        {
            compose_and_blit(prev[i].x, prev[i].y, SPRITE, SPRITE);
            compose_and_blit(x, y, SPRITE, SPRITE);
        }
        else
        {
            compose_and_blit(x0, y0, x1 - x0, y1 - y0);
        }

        prev[i].x = x;
        prev[i].y = y;
        prev[i].valid = vis;
    }
}

// ---------------------------------------------------------------------------
// Texto
// ---------------------------------------------------------------------------
static int text_width(const char *s, int scale)
{
    return (int)strlen(s) * 6 * scale - scale;
}

static void draw_text(int x, int y, const char *s, int scale, uint16_t fg, uint16_t bg)
{
    int cw = 6 * scale, ch = 8 * scale;

    for (; *s; s++, x += cw)
    {
        char c = *s;

        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        if (c < ' ' || c > 'Z')
            c = '?';

        const uint8_t *glyph = font5x7[c - ' '];

        for (int py = 0; py < ch; py++)
            for (int px = 0; px < cw; px++)
            {
                int col = px / scale, row = py / scale;
                int on = col < 5 && row < 7 && (glyph[col] & (1U << row));
                pix[py * cw + px] = on ? fg : bg;
            }

        if (x + cw <= LCD_W)
            lcd_blit(x, y, cw, ch, pix);
    }
}

static void draw_text_center(int y, const char *s, int scale, uint16_t fg, uint16_t bg)
{
    draw_text((LCD_W - text_width(s, scale)) / 2, y, s, scale, fg, bg);
}

// ---------------------------------------------------------------------------
// Placar
// ---------------------------------------------------------------------------
static void draw_life_icon(int x, int y, uint16_t color)
{
    for (int py = 0; py < 12; py++)
        for (int px = 0; px < 12; px++)
        {
            float dx = px - 5.5f, dy = py - 5.5f;
            int mouth = dx > 0.0f && fabsf(dy) < dx * 0.7f;
            pix[py * 12 + px] = (dx * dx + dy * dy <= 25.0f && !mouth) ? color : C_BLACK;
        }

    lcd_blit(x, y, 12, 12, pix);
}

static void draw_hud(int force)
{
    char buf[24];
    uint32_t secs = (snap.time_left_ms + 999U) / 1000U;

    if (force || !hud.valid || hud.state != snap.state || hud.level != snap.level)
    {
        lcd_fill_rect(0, 0, LCD_W, HUD_H, C_BLACK);
        hud.valid = 0;
    }

    if (snap.state == ST_READY)
    {
        if (!hud.valid)
        {
            snprintf(buf, sizeof(buf), "NIVEL %u  PRONTO!", snap.level + 1U);
            draw_text_center(4, buf, 2, C_YELLOW, C_BLACK);
        }
    }
    else
    {
        if (!hud.valid || hud.score != snap.score)
        {
            snprintf(buf, sizeof(buf), "%05lu", (unsigned long)snap.score);
            draw_text(4, 4, buf, 2, C_WHITE, C_BLACK);
        }

        if (!hud.valid)
        {
            snprintf(buf, sizeof(buf), "N%u", snap.level + 1U);
            draw_text(76, 4, buf, 2, C_CYAN, C_BLACK);
        }

        if (!hud.valid || hud.secs != secs)
        {
            snprintf(buf, sizeof(buf), "T%03lu", (unsigned long)secs);
            draw_text(112, 4, buf, 2, secs <= 15U ? C_RED : C_GREEN, C_BLACK);
        }

        if (!hud.valid || hud.lives != snap.lives)
        {
            for (int i = 0; i < (int)START_LIVES; i++)
                draw_life_icon(176 + i * 20, 6, i < snap.lives ? C_YELLOW : C_BLACK);
        }
    }

    hud.score = snap.score;
    hud.secs = secs;
    hud.level = snap.level;
    hud.lives = snap.lives;
    hud.state = (uint8_t)snap.state;
    hud.valid = 1;
}

// ---------------------------------------------------------------------------
// Telas
// ---------------------------------------------------------------------------
static void draw_menu(void)
{
    lcd_fill_rect(0, 0, LCD_W, LCD_H, C_BLACK);
    draw_text_center(30, "PAC-TILT", 4, C_YELLOW, C_BLACK);
    draw_text_center(70, "ELF74 - UTFPR", 1, C_GREY, C_BLACK);
    draw_text_center(100, "INCLINE A PLACA", 2, C_WHITE, C_BLACK);
    draw_text_center(120, "PARA MOVER", 2, C_WHITE, C_BLACK);
    draw_text_center(160, "A/KEY: JOGAR", 2, C_CYAN, C_BLACK);
    draw_text_center(182, "B: PAUSA", 2, C_CYAN, C_BLACK);

    if (snap.imu_ok)
        draw_text_center(220, "IMU OK", 1, C_GREEN, C_BLACK);
    else
        draw_text_center(220, "IMU NAO ENCONTRADA (PB6/PB7)", 1, C_RED, C_BLACK);
}

static void draw_pause(void)
{
    int x = 30, y = 80, w = 180, h = 90;

    lcd_fill_rect(x, y, w, h, C_WALL);
    lcd_fill_rect(x + 3, y + 3, w - 6, h - 6, C_BLACK);
    draw_text_center(y + 14, "PAUSA", 3, C_YELLOW, C_BLACK);
    draw_text_center(y + 46, "A: CONTINUAR", 2, C_WHITE, C_BLACK);
    draw_text_center(y + 66, "B: MENU", 2, C_WHITE, C_BLACK);
}

static void draw_end_screen(const char *title, uint16_t color, const char *subtitle)
{
    char buf[24];

    lcd_fill_rect(0, 0, LCD_W, LCD_H, C_BLACK);
    draw_text_center(60, title, 3, color, C_BLACK);

    if (subtitle)
        draw_text_center(100, subtitle, 2, C_WHITE, C_BLACK);

    snprintf(buf, sizeof(buf), "PONTOS %lu", (unsigned long)snap.score);
    draw_text_center(140, buf, 2, C_YELLOW, C_BLACK);
}

static void handle_render(const render_msg_t *r)
{
    char buf[24];

    if (r->type == R_OVERLAY_PAUSE)
    {
        draw_pause();
        return;
    }

    switch (r->state)
    {
    case ST_MENU:
        draw_menu();
        break;

    case ST_READY:
        hud.valid = 0;
        draw_hud(1);
        draw_maze_full();
        break;

    case ST_PLAYING:
    case ST_DYING:
        draw_hud(1);
        break;

    case ST_LEVEL_CLEAR:
        snprintf(buf, sizeof(buf), "NIVEL %u", snap.level + 1U);
        draw_end_screen(buf, C_GREEN, "CONCLUIDO!");
        if (snap.level + 1U < NUM_LEVELS)
            draw_text_center(190, "PREPARE-SE...", 2, C_GREY, C_BLACK);
        break;

    case ST_GAME_OVER:
        draw_end_screen("FIM DE JOGO", C_RED,
                        snap.time_left_ms == 0U ? "TEMPO ESGOTADO" : "SEM VIDAS");
        draw_text_center(190, "A: MENU", 2, C_CYAN, C_BLACK);
        break;

    case ST_VICTORY:
        draw_end_screen("VITORIA!", C_YELLOW, "TODOS OS NIVEIS");
        draw_text_center(190, "A: MENU", 2, C_CYAN, C_BLACK);
        break;

    default:
        break;
    }
}

void task_display(void *arg)
{
    (void)arg;
    render_msg_t r;

    lcd_lock();
    lcd_init();
    lcd_unlock();

    for (;;)
    {
        // Espera o proximo quadro (ou no maximo 100 ms para tratar telas)
        xSemaphoreTake(s_frame, pdMS_TO_TICKS(100));

        take_snapshot();
        lcd_lock();

        while (xQueueReceive(q_render, &r, 0) == pdPASS)
        {
            take_snapshot();
            handle_render(&r);
        }

        if (snap.state == ST_READY || snap.state == ST_PLAYING || snap.state == ST_DYING)
        {
            draw_sprites();
            draw_hud(0);
        }

        lcd_unlock();
    }
}
