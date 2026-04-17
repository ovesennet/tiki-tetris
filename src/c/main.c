/* TIKI TETRIS — main.c
 * Main game loop and state machine for Tiki-100 Tetris.
 *
 * Controls: A=left, D=right, W=rotate, S=soft drop, SPACE=hard drop, Q=quit
 */

#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "sound.h"
#include "video.h"
#include "font.h"
#include "board.h"
#include "piece.h"

/* ── Speed table: ticks per gravity drop, indexed by level ── */
static const uint8_t g_drop_delay[] = {
    48, 43, 38, 33, 28, 23, 18, 13, 8, 6,
    5,  5,  4,  4,  3,  3,  2,  2,  1, 1
};

/* ── Score table: points per lines cleared ── */
static const uint16_t g_score_table[] = {
    0, 40, 100, 300, 1200
};

/* ── Game state ── */
static uint32_t g_score;
static uint32_t g_top_score;
static uint16_t g_lines;
static uint8_t  g_level;
static uint8_t  g_next_piece;
static uint8_t  g_game_over;

/* Piece statistics */
static uint16_t g_stats[NUM_PIECES + 1]; /* index 1..7 */

/* Current piece */
static Piece    g_cur;

/* Timing */
static uint8_t  g_fall_timer;
static uint16_t g_tick;

/* ── Gem colour schemes: { bright, dark } — white highlight on all ── */
#define NUM_SCHEMES 6
static const uint8_t g_schemes[NUM_SCHEMES][2] = {
    { COL_BRCYAN,  COL_CYAN   },   /* cyan    */
    { COL_YELLOW,  COL_ORANGE },   /* yellow  */
    { COL_PINK,    COL_MAGENTA},   /* purple  */
    { COL_BRGREEN, COL_GREEN  },   /* green   */
    { COL_BRRED,   COL_RED    },   /* red     */
    { COL_BRBLUE,  COL_BLUE   },   /* blue    */
};
static uint8_t g_scheme;        /* current colour scheme index */
static uint8_t g_prev_scheme;   /* previous (to avoid repeats) */

/* Simple 16-bit PRNG (xorshift) */
uint16_t g_rng;

static uint16_t rng_next(void)
{
    g_rng ^= g_rng << 7;
    g_rng ^= g_rng >> 9;
    g_rng ^= g_rng << 8;
    return g_rng;
}

static uint8_t random_piece(void)
{
    return (uint8_t)((rng_next() % NUM_PIECES) + 1);
}

/* ── Delay loop ── */
void delay(uint16_t n)
{
    volatile uint16_t i;
    for (i = 0; i < n; i++) ;
}

/* ── Simple uint32 to string (avoids pulling in sprintf/printf) ── */
static void u32_to_str(uint32_t val, char *buf)
{
    char tmp[11];
    uint8_t i = 0, j;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    while (val > 0) {
        tmp[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    for (j = 0; j < i; j++)
        buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}

static void u16_to_str(uint16_t val, char *buf)
{
    u32_to_str((uint32_t)val, buf);
}

/* ── Get current drop delay ── */
static uint8_t get_drop_delay(void)
{
    uint8_t idx;
    idx = g_level;
    if (idx >= sizeof(g_drop_delay))
        idx = sizeof(g_drop_delay) - 1;
    return g_drop_delay[idx];
}


/* ── Drawing ── */

/* Forward declarations */
static void build_gem_tile(void);
static void build_black_tile(void);

/* Pick a new random colour scheme (different from previous) */
static void pick_scheme(void)
{
    uint8_t s;
    do {
        s = (uint8_t)(rng_next() % NUM_SCHEMES);
    } while (s == g_prev_scheme);
    g_prev_scheme = g_scheme;
    g_scheme = s;
    build_gem_tile();
}

/* Pre-rendered tile buffers (50 bytes each: 5 bytes/row × 10 rows).
 * Built once per colour scheme change. */
static uint8_t g_gem_tile[50];
static uint8_t g_black_tile[50];

/* Pack two 4-bit colour nibbles into one VRAM byte (even=low, odd=high) */
#define PACKPIX(a, b) (((b) << 4) | (a))

/* Build the gem tile data for the current colour scheme */
static void build_gem_tile(void)
{
    uint8_t bright, dark, r, *p;
    uint8_t bb, bd, bw, dd, wb, ww, wd, dw, db;

    bright = g_schemes[g_scheme][0];
    dark   = g_schemes[g_scheme][1];

    /* Pre-compute common pixel pairs */
    ww = PACKPIX(COL_WHITE, COL_WHITE);
    wb = PACKPIX(COL_WHITE, bright);
    wd = PACKPIX(COL_WHITE, dark);
    bw = PACKPIX(bright, COL_WHITE);
    bb = PACKPIX(bright, bright);
    bd = PACKPIX(bright, dark);
    db = PACKPIX(dark, bright);
    dd = PACKPIX(dark, dark);
    dw = PACKPIX(dark, COL_WHITE);

    p = g_gem_tile;

    /* Row 0: WWWWWWWWWW (all white) */
    *p++ = ww; *p++ = ww; *p++ = ww; *p++ = ww; *p++ = ww;
    /* Row 1: WWWWWWWWDD */
    *p++ = ww; *p++ = ww; *p++ = ww; *p++ = ww; *p++ = dd;
    /* Row 2: WWbbWWbbDD */
    *p++ = ww; *p++ = bb; *p++ = ww; *p++ = bb; *p++ = dd;
    /* Row 3: WWbWbbbbDD */
    *p++ = ww; *p++ = PACKPIX(bright, COL_WHITE); *p++ = bb; *p++ = bb; *p++ = dd;
    /* Row 4: WbbbbbbbDD */
    *p++ = wb; *p++ = bb; *p++ = bb; *p++ = bb; *p++ = dd;
    /* Row 5: WbbbbbbbDD */
    *p++ = wb; *p++ = bb; *p++ = bb; *p++ = bb; *p++ = dd;
    /* Row 6: WbbbbbbbDD */
    *p++ = wb; *p++ = bb; *p++ = bb; *p++ = bb; *p++ = dd;
    /* Row 7: WbbbbbbWDD */
    *p++ = wb; *p++ = bb; *p++ = bb; *p++ = PACKPIX(bright, COL_WHITE); *p++ = dd;
    /* Row 8: DDDDDDDDDD */
    *p++ = dd; *p++ = dd; *p++ = dd; *p++ = dd; *p++ = dd;
    /* Row 9: DDDDDDDDDD */
    *p++ = dd; *p++ = dd; *p++ = dd; *p++ = dd; *p++ = dd;
}

/* Build the black (empty) tile */
static void build_black_tile(void)
{
    memset(g_black_tile, 0, 50);
}

/* Draw a gem-style cell at board position (bx, by).
 * colour != COL_BLACK means draw a jewel tile; COL_BLACK = erase. */
static void draw_cell(uint8_t bx, uint8_t by, uint8_t colour)
{
    uint16_t px, py;
    if (by < HIDDEN_ROWS) return;
    px = BOARD_PX_X + (uint16_t)bx * CELL_SIZE;
    py = BOARD_PX_Y + (uint16_t)(by - HIDDEN_ROWS) * CELL_SIZE;
    vid_blit_tile(px, py, (colour == COL_BLACK) ? g_black_tile : g_gem_tile);
}

/* Draw the statistics header */
static void draw_stats_header(void)
{
    draw_text(STATS_PX_X, 28, "STATS", COL_WHITE);
}

/* Draw mini piece shape at (px, py) using 4px cells */
static void draw_mini_piece(uint16_t px, uint16_t py, uint8_t type, uint8_t colour)
{
    uint16_t mask;
    uint8_t r, c;
    mask = piece_get_mask(type, 0);
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (mask & 0x8000) {
                vid_fill_rect(px + (uint16_t)c * STATS_CELL,
                              py + (uint16_t)r * STATS_CELL,
                              STATS_CELL, STATS_CELL, colour);
            }
            mask <<= 1;
        }
    }
}

/* Draw all statistics mini pieces (called once) */
static void draw_stats_pieces(void)
{
    uint8_t i;
    uint16_t y;
    static const uint8_t piece_colours[] = {
        0, COL_BRCYAN, COL_YELLOW, COL_PINK,
        COL_BRGREEN, COL_BRRED, COL_BRBLUE, COL_ORANGE
    };
    for (i = 1; i <= NUM_PIECES; i++) {
        y = 40 + (uint16_t)(i - 1) * 20;
        draw_mini_piece(STATS_PX_X, y, i, piece_colours[i]);
    }
}

/* Update statistics counters only */
static void draw_stats(void)
{
    uint8_t i;
    uint16_t y;
    char buf[6];

    for (i = 1; i <= NUM_PIECES; i++) {
        y = 40 + (uint16_t)(i - 1) * 20;
        u16_to_str(g_stats[i], buf);
        erase_text(STATS_PX_X + 20, y + 5, 3);
        draw_text(STATS_PX_X + 20, y + 5, buf, COL_WHITE);
    }
}

/* Draw the playfield border */
static void draw_border(void)
{
    uint16_t x1, y1, x2, y2;
    x1 = BOARD_PX_X - 2;
    y1 = BOARD_PX_Y - 2;
    x2 = BOARD_PX_X + BOARD_W * CELL_SIZE + 1;
    y2 = BOARD_PX_Y + VISIBLE_ROWS * CELL_SIZE + 1;

    /* White border (2px thick) */
    vid_vline(x1, y1, y2, COL_WHITE);
    vid_vline(x1 + 1, y1, y2, COL_WHITE);
    vid_vline(x2, y1, y2, COL_WHITE);
    vid_vline(x2 - 1, y1, y2, COL_WHITE);
    vid_hline(x1, x2, y1, COL_WHITE);
    vid_hline(x1, x2, y1 + 1, COL_WHITE);
    vid_hline(x1, x2, y2, COL_WHITE);
    vid_hline(x1, x2, y2 - 1, COL_WHITE);

    /* 3D shadow: light gray on right + bottom (outside white) */
    vid_vline(x2 + 1, y1 + 2, y2 + 2, COL_LTGREY);
    vid_vline(x2 + 2, y1 + 2, y2 + 2, COL_LTGREY);
    vid_hline(x1 + 2, x2 + 2, y2 + 1, COL_LTGREY);
    vid_hline(x1 + 2, x2 + 2, y2 + 2, COL_LTGREY);
}

/* Draw the entire board */
static void draw_board(void)
{
    uint8_t x, y;
    for (y = HIDDEN_ROWS; y < BOARD_ROWS; y++) {
        for (x = 0; x < BOARD_W; x++) {
            uint8_t cell = board_get(x, y);
            draw_cell(x, y, (cell != PIECE_NONE) ? COL_WHITE : COL_BLACK);
        }
    }
}

/* Erase the active piece (fill cells with black) */
static void erase_piece(Piece *p)
{
    uint16_t mask;
    uint8_t r, c;
    int8_t bx, by;

    mask = piece_get_mask(p->type, p->rot);
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (mask & 0x8000) {
                bx = p->x + (int8_t)c;
                by = p->y + (int8_t)r;
                if (bx >= 0 && bx < BOARD_W && by >= HIDDEN_ROWS && by < BOARD_ROWS) {
                    draw_cell((uint8_t)bx, (uint8_t)by, COL_BLACK);
                }
            }
            mask <<= 1;
        }
    }
}

/* Draw the active piece as gem tiles */
static void draw_piece(Piece *p)
{
    uint16_t mask;
    uint8_t r, c;
    int8_t bx, by;

    mask = piece_get_mask(p->type, p->rot);
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (mask & 0x8000) {
                bx = p->x + (int8_t)c;
                by = p->y + (int8_t)r;
                if (bx >= 0 && bx < BOARD_W && by >= HIDDEN_ROWS && by < BOARD_ROWS) {
                    draw_cell((uint8_t)bx, (uint8_t)by, COL_WHITE);
                }
            }
            mask <<= 1;
        }
    }
}

/* Draw the next piece preview */
static void draw_next_piece(void)
{
    uint16_t mask;
    uint8_t r, c;
    uint16_t px, py;

    vid_fill_rect(NEXT_PX_X, NEXT_PX_Y, CELL_SIZE * 4, CELL_SIZE * 4, COL_BLACK);

    mask = piece_get_mask(g_next_piece, 0);
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (mask & 0x8000) {
                px = NEXT_PX_X + (uint16_t)c * CELL_SIZE;
                py = NEXT_PX_Y + (uint16_t)r * CELL_SIZE;
                vid_blit_tile(px, py, g_gem_tile);
            }
            mask <<= 1;
        }
    }
}

/* Draw the info panel labels (called once) */
static void draw_info_labels(void)
{
    uint16_t x, y;
    x = INFO_PX_X;
    y = INFO_PX_Y;
    draw_text(x, y, "SCORE", COL_WHITE);
    draw_text(x, y + 26, "LINES", COL_WHITE);
    draw_text(x, y + 52, "LEVEL", COL_WHITE);
    draw_text(x, y + 78, "TOP", COL_WHITE);
}

/* Update the info panel values only */
static void draw_info(void)
{
    char buf[12];
    uint16_t x, y;

    x = INFO_PX_X;
    y = INFO_PX_Y + 10;

    u32_to_str(g_score, buf);
    erase_text(x, y, 8);
    draw_text(x, y, buf, COL_YELLOW);

    y += 26;
    u16_to_str(g_lines, buf);
    erase_text(x, y, 5);
    draw_text(x, y, buf, COL_YELLOW);

    y += 26;
    u16_to_str((uint16_t)g_level, buf);
    erase_text(x, y, 3);
    draw_text(x, y, buf, COL_YELLOW);

    y += 26;
    u32_to_str(g_top_score, buf);
    erase_text(x, y, 8);
    draw_text(x, y, buf, COL_BRCYAN);
}

/* ── Piece spawn ── */
static uint8_t spawn_piece(void)
{
    g_cur.type = g_next_piece;
    g_cur.rot = 0;
    g_cur.x = 3;
    g_cur.y = 0;

    g_stats[g_cur.type]++;

    g_next_piece = random_piece();

    if (!piece_can_place(g_cur.type, g_cur.rot, g_cur.x, g_cur.y)) {
        return 0;  /* game over */
    }
    return 1;
}

/* ── Game logic ── */

/* Delta-draw: move piece from old to new position without flicker.
 * Only redraws cells that differ between old and new positions. */
static void delta_move(uint8_t new_rot, int8_t new_x, int8_t new_y)
{
    uint16_t omask, nmask;
    uint8_t r, c;
    int8_t bx, by;
    uint8_t old_cells[4][4];  /* 1 = old piece occupies this board cell */
    uint8_t new_cells[4][4];  /* 1 = new piece occupies this board cell */
    int8_t min_x, min_y, max_x, max_y;

    /* Bounding box covering both old and new 4x4 grids */
    min_x = g_cur.x < new_x ? g_cur.x : new_x;
    min_y = g_cur.y < new_y ? g_cur.y : new_y;
    max_x = g_cur.x > new_x ? g_cur.x : new_x;
    max_y = g_cur.y > new_y ? g_cur.y : new_y;

    /* Build old occupancy in bounding box coords */
    omask = piece_get_mask(g_cur.type, g_cur.rot);
    nmask = piece_get_mask(g_cur.type, new_rot);

    /* Iterate over bounding box (max 5x5 for 1-cell moves) */
    for (by = min_y; by < max_y + 4; by++) {
        for (bx = min_x; bx < max_x + 4; bx++) {
            uint8_t in_old = 0, in_new = 0;
            int8_t ox, oy, nx, ny;

            /* Check if (bx,by) is in old piece */
            ox = bx - g_cur.x;
            oy = by - g_cur.y;
            if (ox >= 0 && ox < 4 && oy >= 0 && oy < 4) {
                if (omask & ((uint16_t)0x8000 >> (oy * 4 + ox)))
                    in_old = 1;
            }

            /* Check if (bx,by) is in new piece */
            nx = bx - new_x;
            ny = by - new_y;
            if (nx >= 0 && nx < 4 && ny >= 0 && ny < 4) {
                if (nmask & ((uint16_t)0x8000 >> (ny * 4 + nx)))
                    in_new = 1;
            }

            if (in_old == in_new) continue;  /* no change */

            if (bx < 0 || bx >= BOARD_W || by < HIDDEN_ROWS || by >= BOARD_ROWS)
                continue;

            if (in_old && !in_new) {
                /* Cell vacated: draw black or board content */
                if (board_get(bx, by) != PIECE_NONE)
                    draw_cell((uint8_t)bx, (uint8_t)by, COL_WHITE);
                else
                    draw_cell((uint8_t)bx, (uint8_t)by, COL_BLACK);
            } else {
                /* Cell newly occupied: draw gem */
                draw_cell((uint8_t)bx, (uint8_t)by, COL_WHITE);
            }
        }
    }

    g_cur.rot = new_rot;
    g_cur.x = new_x;
    g_cur.y = new_y;
}

static void do_move_left(void)
{
    if (piece_can_place(g_cur.type, g_cur.rot, g_cur.x - 1, g_cur.y)) {
        delta_move(g_cur.rot, g_cur.x - 1, g_cur.y);
        sfx_move();
    }
}

static void do_move_right(void)
{
    if (piece_can_place(g_cur.type, g_cur.rot, g_cur.x + 1, g_cur.y)) {
        delta_move(g_cur.rot, g_cur.x + 1, g_cur.y);
        sfx_move();
    }
}

static void do_rotate(void)
{
    uint8_t new_rot;
    new_rot = (g_cur.rot + 1) & 3;
    if (piece_can_place(g_cur.type, new_rot, g_cur.x, g_cur.y)) {
        delta_move(new_rot, g_cur.x, g_cur.y);
        sfx_move();
    }
}

static uint8_t do_move_down(void)
{
    if (piece_can_place(g_cur.type, g_cur.rot, g_cur.x, g_cur.y + 1)) {
        delta_move(g_cur.rot, g_cur.x, g_cur.y + 1);
        return 1;
    }
    return 0;
}

/* Lock current piece, clear lines, update score, spawn next */
static void lock_and_advance(void)
{
    uint8_t cleared;

    board_lock_piece(g_cur.type, g_cur.rot, g_cur.x, g_cur.y);

    cleared = board_clear_lines();

    if (cleared > 0) {
        sfx_line_clear();

        g_score += (uint32_t)g_score_table[cleared] * (uint32_t)(g_level + 1);
        if (g_score > g_top_score) g_top_score = g_score;
        g_lines += cleared;

        /* Level up check */
        {
            uint8_t new_level = (uint8_t)(g_lines / LINES_PER_LEVEL);
            if (new_level != g_level) {
                g_level = new_level;
                pick_scheme();   /* new colour for new level */
                sfx_level_up();
            }
        }
    }

    if (cleared > 0) {
        draw_board();
    }
    draw_info();
    draw_next_piece();
    draw_stats();

    if (!spawn_piece()) {
        g_game_over = 1;
    } else {
        draw_piece(&g_cur);
        g_fall_timer = get_drop_delay();
    }
}

/* Hard drop: move piece down as far as possible instantly */
static void do_hard_drop(void)
{
    erase_piece(&g_cur);
    while (piece_can_place(g_cur.type, g_cur.rot, g_cur.x, g_cur.y + 1)) {
        g_cur.y++;
        g_score++;
    }
    draw_piece(&g_cur);
    sfx_drop();
    lock_and_advance();
}

/* ── St. Basil's Cathedral for title screen ── */
static void draw_cathedral(void)
{
    uint8_t i;

    /* ── Central spire + cross ── */
    vid_fill_rect(126, 8, 4, 4, COL_YELLOW);    /* cross top */
    vid_fill_rect(122, 12, 12, 3, COL_YELLOW);   /* cross arm */
    vid_fill_rect(126, 12, 4, 10, COL_YELLOW);   /* cross post */
    vid_fill_rect(124, 22, 8, 10, COL_YELLOW);   /* spire tip */
    /* Central onion dome (yellow/green) */
    vid_fill_rect(118, 32, 20, 4, COL_YELLOW);
    vid_fill_rect(114, 36, 28, 4, COL_GREEN);
    vid_fill_rect(110, 40, 36, 4, COL_YELLOW);
    vid_fill_rect(108, 44, 40, 5, COL_GREEN);
    vid_fill_rect(106, 49, 44, 4, COL_YELLOW);
    vid_fill_rect(108, 53, 40, 3, COL_GREEN);
    vid_fill_rect(112, 56, 32, 3, COL_YELLOW);
    vid_fill_rect(116, 59, 24, 2, COL_GREEN);
    /* Central tower */
    vid_fill_rect(112, 61, 32, 30, COL_RED);
    vid_fill_rect(116, 65, 4, 8, COL_YELLOW);   /* window */
    vid_fill_rect(124, 65, 4, 8, COL_YELLOW);
    vid_fill_rect(136, 65, 4, 8, COL_YELLOW);

    /* ── Left dome 1 (large, red/green stripes) ── */
    vid_fill_rect(78, 42, 6, 8, COL_YELLOW);    /* spire */
    vid_fill_rect(72, 50, 18, 4, COL_RED);
    vid_fill_rect(68, 54, 26, 3, COL_GREEN);
    vid_fill_rect(66, 57, 30, 4, COL_RED);
    vid_fill_rect(64, 61, 34, 4, COL_GREEN);
    vid_fill_rect(66, 65, 30, 3, COL_RED);
    vid_fill_rect(70, 68, 22, 2, COL_GREEN);
    /* Left tower 1 */
    vid_fill_rect(70, 70, 22, 21, COL_RED);
    vid_fill_rect(74, 74, 4, 6, COL_YELLOW);
    vid_fill_rect(84, 74, 4, 6, COL_YELLOW);

    /* ── Right dome 1 (large, green/red stripes) ── */
    vid_fill_rect(172, 42, 6, 8, COL_YELLOW);
    vid_fill_rect(166, 50, 18, 4, COL_GREEN);
    vid_fill_rect(162, 54, 26, 3, COL_RED);
    vid_fill_rect(160, 57, 30, 4, COL_GREEN);
    vid_fill_rect(158, 61, 34, 4, COL_RED);
    vid_fill_rect(160, 65, 30, 3, COL_GREEN);
    vid_fill_rect(164, 68, 22, 2, COL_RED);
    /* Right tower 1 */
    vid_fill_rect(164, 70, 22, 21, COL_RED);
    vid_fill_rect(168, 74, 4, 6, COL_YELLOW);
    vid_fill_rect(178, 74, 4, 6, COL_YELLOW);

    /* ── Left dome 2 (small, green) ── */
    vid_fill_rect(50, 56, 4, 6, COL_YELLOW);
    vid_fill_rect(44, 62, 16, 3, COL_GREEN);
    vid_fill_rect(42, 65, 20, 4, COL_RED);
    vid_fill_rect(44, 69, 16, 3, COL_GREEN);
    vid_fill_rect(48, 72, 8, 2, COL_RED);
    /* Left tower 2 */
    vid_fill_rect(44, 74, 16, 17, COL_ORANGE);
    vid_fill_rect(48, 77, 4, 5, COL_YELLOW);

    /* ── Right dome 2 (small, red) ── */
    vid_fill_rect(202, 56, 4, 6, COL_YELLOW);
    vid_fill_rect(196, 62, 16, 3, COL_RED);
    vid_fill_rect(194, 65, 20, 4, COL_GREEN);
    vid_fill_rect(196, 69, 16, 3, COL_RED);
    vid_fill_rect(200, 72, 8, 2, COL_GREEN);
    /* Right tower 2 */
    vid_fill_rect(196, 74, 16, 17, COL_ORANGE);
    vid_fill_rect(204, 77, 4, 5, COL_YELLOW);

    /* ── Building base ── */
    vid_fill_rect(38, 91, 180, 14, COL_RED);
    /* Base windows */
    for (i = 0; i < 8; i++) {
        vid_fill_rect(46 + i * 22, 94, 6, 8, COL_YELLOW);
    }
    /* Foundation */
    vid_fill_rect(34, 105, 188, 6, COL_ORANGE);
    vid_fill_rect(30, 111, 196, 4, COL_YELLOW);

    /* ── Ground line ── */
    vid_hline(20, 236, 115, COL_GREEN);
    vid_hline(20, 236, 116, COL_GREEN);
}

/* Classic TETRIS block logo with red→orange→yellow→green gradient.
 * Each letter is 18px wide × 20px tall (5 rows of 4px each).
 * Encoded as 5 rows, each row a bitmask of 18 columns. */

/* Letter bitmasks: each uint32_t has bits 17..0 = columns L→R */
#define LB(b17,b16,b15,b14,b13,b12,b11,b10,b9,b8,b7,b6,b5,b4,b3,b2,b1,b0) \
    ((uint32_t)( \
    ((uint32_t)(b17)<<17)|((uint32_t)(b16)<<16)|((uint32_t)(b15)<<15)|((uint32_t)(b14)<<14)| \
    ((uint32_t)(b13)<<13)|((uint32_t)(b12)<<12)|((uint32_t)(b11)<<11)|((uint32_t)(b10)<<10)| \
    ((uint32_t)(b9)<<9)|((uint32_t)(b8)<<8)|((uint32_t)(b7)<<7)|((uint32_t)(b6)<<6)| \
    ((uint32_t)(b5)<<5)|((uint32_t)(b4)<<4)|((uint32_t)(b3)<<3)|((uint32_t)(b2)<<2)| \
    ((uint32_t)(b1)<<1)|((uint32_t)(b0)) ))

static const uint32_t letter_T[] = {
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1),
    LB(0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0),
    LB(0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0),
    LB(0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0),
    LB(0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0),
};

static const uint32_t letter_E[] = {
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1),
    LB(1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0),
    LB(1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1),
};

static const uint32_t letter_R[] = {
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0),
    LB(1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,0),
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0),
    LB(1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,0),
    LB(1,1,1,1,0,0,0,0,0,0,0,0,1,1,1,1,1,0),
};

static const uint32_t letter_I[] = {
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1),
    LB(0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0),
    LB(0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0),
    LB(0,0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0),
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1),
};

static const uint32_t letter_S[] = {
    LB(0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1),
    LB(1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0),
    LB(0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0),
    LB(0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1),
    LB(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0),
};

static void draw_tetris_logo(uint16_t start_y)
{
    static const uint32_t * const letters[] = {
        letter_T, letter_E, letter_T, letter_R, letter_I, letter_S
    };
    static const uint8_t cols[] = {
        COL_RED, COL_BRRED, COL_ORANGE, COL_YELLOW, COL_GREEN
    };
    uint8_t li, row, col;
    uint16_t lx, px, py;
    uint32_t bits;

    for (li = 0; li < 6; li++) {
        lx = 64 + (uint16_t)li * 22;
        for (row = 0; row < 5; row++) {
            bits = letters[li][row];
            py = start_y + (uint16_t)row * 4;
            for (col = 0; col < 18; col++) {
                if (bits & ((uint32_t)1 << (17 - col))) {
                    px = lx + (uint16_t)col;
                    vid_vline(px, py, py + 3, cols[row]);
                }
            }
        }
    }
}

/* ── Title screen ── */
static void show_title(void)
{
    vid_clear();

    draw_cathedral();
    draw_tetris_logo(122);

    draw_text_centred(148, "BY ARCTIC RETRO", COL_WHITE);
    draw_text_centred(170, "A-LEFT  D-RIGHT", COL_BRCYAN);
    draw_text_centred(182, "W-ROTATE  S-DROP", COL_BRCYAN);
    draw_text_centred(194, "SPACE-HARD DROP", COL_BRCYAN);
    draw_text_centred(206, "P-PAUSE  O-SOUND", COL_BRCYAN);
    draw_text_centred(246, "PRESS ANY KEY", COL_WHITE);

    /* Play music in a loop, seed RNG from wait time */
    while (!play_title_music()) {
        /* loop melody until keypress */
    }
    {
        char ch = getch();
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        if (ch == KEY_QUIT) {
            vid_shutdown();
            exit(0);
        }
    }
    if (g_rng == 0) g_rng = 12345;
}

/* ── Game over screen ── */
static void show_game_over(void)
{
    char buf[12];
    uint8_t len;
    uint16_t w, x;

    if (g_score > g_top_score)
        g_top_score = g_score;

    /* Clear background behind text (text height 7 + 2px padding each side) */
    /* "GAME OVER" = 9 chars */
    len = 9;
    w = (uint16_t)len * (FONT_W + FONT_SPACING) - FONT_SPACING + 4;
    x = (256 - w) / 2;
    vid_fill_rect(x, 108, (uint8_t)w, 11, COL_BLACK);

    /* Score */
    u32_to_str(g_score, buf);
    len = (uint8_t)strlen(buf);
    w = (uint16_t)len * (FONT_W + FONT_SPACING) - FONT_SPACING + 4;
    x = (256 - w) / 2;
    vid_fill_rect(x, 128, (uint8_t)w, 11, COL_BLACK);

    /* "PRESS ANY KEY" = 13 chars */
    len = 13;
    w = (uint16_t)len * (FONT_W + FONT_SPACING) - FONT_SPACING + 4;
    x = (256 - w) / 2;
    vid_fill_rect(x, 148, (uint8_t)w, 11, COL_BLACK);

    draw_text_centred(110, "GAME OVER", COL_RED);
    u32_to_str(g_score, buf);
    draw_text_centred(130, buf, COL_YELLOW);
    draw_text_centred(150, "PRESS ANY KEY", COL_WHITE);
    while (!kbhit()) ;
    getch();
}

/* ── Init a new game ── */
static void game_init(void)
{
    g_score = 0;
    g_lines = 0;
    g_level = 0;
    g_game_over = 0;
    g_tick = 0;
    g_prev_scheme = 255;
    pick_scheme();
    build_black_tile();

    board_init();

    g_next_piece = random_piece();

    vid_clear();
    memset(g_stats, 0, sizeof(g_stats));
    draw_board();
    draw_border();
    draw_stats_header();
    draw_stats_pieces();
    draw_stats();

    /* Side branding */
    vid_draw_text_rotcw(3, 16, "TIKI TETRIS BY ARCTIC RETRO", COL_ORANGE);

    /* TETRIS logo below the board */
    draw_tetris_logo(222);

    draw_text(NEXT_PX_X, NEXT_PX_Y - 12, "NEXT", COL_WHITE);
    draw_info_labels();
    draw_info();

    spawn_piece();
    draw_next_piece();
    draw_piece(&g_cur);
    g_fall_timer = get_drop_delay();
}

/* ── Main loop ── */
void main(void)
{
    char key;

    vid_init();
    sound_init();

    while (1) {
        show_title();
        game_init();

        /* Play loop */
        while (!g_game_over) {
            /* Input */
            if (kbhit()) {
                key = getch();
                if (key >= 'A' && key <= 'Z') key += 32; /* tolower */

                switch (key) {
                    case KEY_LEFT:   do_move_left();  break;
                    case KEY_RIGHT:  do_move_right(); break;
                    case KEY_ROTATE: do_rotate();     break;
                    case KEY_DOWN:
                        /* Soft drop: move down one, add 1 point, reset timer */
                        if (do_move_down()) {
                            g_score++;
                        }
                        g_fall_timer = get_drop_delay();
                        break;
                    case KEY_DROP:
                        do_hard_drop();
                        break;
                    case KEY_SOUND:
                        g_sound_on ^= 1;
                        if (!g_sound_on) sound_off();
                        break;
                    case KEY_QUIT:
                        g_game_over = 1;
                        vid_shutdown();
                        return;
                    case KEY_PAUSE:
                        draw_text_centred(120, "PAUSED", COL_WHITE);
                        /* Wait until P is pressed again */
                        for (;;) {
                            while (!kbhit()) ;
                            key = getch();
                            if (key >= 'A' && key <= 'Z') key += 32;
                            if (key == KEY_PAUSE) break;
                        }
                        /* Redraw over pause text */
                        vid_fill_rect(0, 120, 255, FONT_CH, COL_BLACK);
                        break;
                }
            }

            /* Gravity */
            g_fall_timer--;
            if (g_fall_timer == 0) {
                if (!do_move_down()) {
                    lock_and_advance();
                }
                g_fall_timer = get_drop_delay();
            }

            /* Frame pacing - simple busy wait */
            delay(200);
            g_tick++;
        }

        show_game_over();
    }
}
