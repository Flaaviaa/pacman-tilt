#include <string.h>

#include "game.h"
#include "st7789.h"

// Legenda: '#' parede, '.' pastilha, 'o' pastilha de poder,
//          'P' inicio do Pac-Man, 'G' casa dos fantasmas, ' ' vazio.
// O script tools/check_mazes.py confere tamanho, bordas e conectividade.

static const char *const maze_a[MAZE_H] =
{
    "####################",
    "#........##........#",
    "#o##.###.##.###.##o#",
    "#..................#",
    "#.##.#.######.#.##.#",
    "#....#...##...#....#",
    "####.###.##.###.####",
    "####.#........#.####",
    "####.#.##GG##.#.####",
    "#......#GGGG#......#",
    "####.#.######.#.####",
    "####.#........#.####",
    "####.#.######.#.####",
    "#........##........#",
    "#.##.###.##.###.##.#",
    "#o.#.....P......#.o#",
    "#..................#",
    "####################",
};

static const char *const maze_b[MAZE_H] =
{
    "####################",
    "#o.......##.......o#",
    "#.###.##.##.##.###.#",
    "#.#...#......#...#.#",
    "#.#.#.#.####.#.#.#.#",
    "#...#..........#...#",
    "###.#.###GG###.#.###",
    "#.....#GGGGGG#.....#",
    "#.###.########.###.#",
    "#.#..............#.#",
    "#.#.####.##.####.#.#",
    "#......#.##.#......#",
    "###.##.#....#.##.###",
    "#...#....P.....#...#",
    "#.#.#.###..###.#.#.#",
    "#.#......##......#.#",
    "#o..##........##..o#",
    "####################",
};

typedef struct
{
    const char *const *rows;
    uint8_t  ghosts;
    float    ghost_speed;   // pixels por quadro
    uint32_t time_s;        // limite de tempo do nivel
} level_t;

// Dificuldade progressiva: mais fantasmas, mais rapidos, menos tempo
static const level_t levels[NUM_LEVELS] =
{
    { maze_a, 2, 0.90f, 150U },
    { maze_b, 3, 1.10f, 130U },
    { maze_a, 4, 1.30f, 110U },
};

static const uint16_t ghost_colors[MAX_GHOSTS] =
{
    RGB565(255,   0,   0),  // vermelho: persegue
    RGB565(255, 184, 255),  // rosa: embosca a frente do Pac-Man
    RGB565(  0, 255, 255),  // ciano: flanqueia junto com o vermelho
    RGB565(255, 184,  82),  // laranja: foge quando chega perto
};

int maze_is_wall(const game_t *g, int tx, int ty)
{
    if (tx < 0 || ty < 0 || tx >= MAZE_W || ty >= MAZE_H)
        return 1;

    return g->tiles[ty][tx] == T_WALL;
}

void maze_reset_actors(game_t *g, uint8_t level)
{
    const level_t *lv = &levels[level];
    int ghost = 0;

    memset(&g->pac, 0, sizeof(g->pac));
    g->pac.face = DIR_LEFT;
    g->fright_frames = 0;

    for (int ty = 0; ty < MAZE_H; ty++)
    {
        for (int tx = 0; tx < MAZE_W; tx++)
        {
            char c = lv->rows[ty][tx];

            if (c == 'P')
            {
                g->pac.x = (float)(tx * TILE + TILE / 2);
                g->pac.y = (float)(ty * TILE + TILE / 2);
            }
            else if (c == 'G' && ghost < MAX_GHOSTS)
            {
                // Fantasmas comecam nas primeiras celulas 'G' (a de cima e a saida)
                ghost_t *gh = &g->ghost[ghost];

                memset(gh, 0, sizeof(*gh));
                gh->active = (uint8_t)(ghost < lv->ghosts);
                gh->personality = (uint8_t)ghost;
                gh->color = ghost_colors[ghost];
                gh->tx = gh->home_tx = (int8_t)tx;
                gh->ty = gh->home_ty = (int8_t)ty;
                gh->dir = DIR_UP;
                gh->x = (float)(tx * TILE + TILE / 2);
                gh->y = (float)(ty * TILE + TILE / 2);
                gh->mode = GHOST_NORMAL;
                gh->release_frames = (uint16_t)(ghost * 100U);   // 2 s entre cada um
                ghost++;
            }
        }
    }
}

void maze_load_level(game_t *g, uint8_t level)
{
    const level_t *lv = &levels[level];

    g->level = level;
    g->ghost_count = lv->ghosts;
    g->ghost_speed = lv->ghost_speed;
    g->time_left_ms = lv->time_s * 1000U;
    g->pellets_left = 0;

    for (int ty = 0; ty < MAZE_H; ty++)
    {
        for (int tx = 0; tx < MAZE_W; tx++)
        {
            switch (lv->rows[ty][tx])
            {
            case '#': g->tiles[ty][tx] = T_WALL; break;
            case '.': g->tiles[ty][tx] = T_PELLET; g->pellets_left++; break;
            case 'o': g->tiles[ty][tx] = T_POWER;  g->pellets_left++; break;
            case 'G': g->tiles[ty][tx] = T_HOUSE; break;
            default:  g->tiles[ty][tx] = T_EMPTY; break;
            }
        }
    }

    maze_reset_actors(g, level);
}
