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
static uint8_t  g_state;
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
    uint8_t i;
    for (i = 0; i < 50; i++)
        g_black_tile[i] = 0;
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

/* Draw all statistics */
static void draw_stats(void)
{
    uint8_t i;
    uint16_t y;
    char buf[6];
    static const uint8_t piece_colours[] = {
        0, COL_BRCYAN, COL_YELLOW, COL_PINK,
        COL_BRGREEN, COL_BRRED, COL_BRBLUE, COL_ORANGE
    };

    for (i = 1; i <= NUM_PIECES; i++) {
        y = 40 + (uint16_t)(i - 1) * 20;
        draw_mini_piece(STATS_PX_X, y, i, piece_colours[i]);
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

/* After erasing + drawing piece at new position, repair any board cells
 * that were under the OLD position but are NOT under the NEW position. */
static void repair_board(Piece *oldp, Piece *newp)
{
    uint16_t omask, nmask;
    uint8_t r, c;
    int8_t bx, by;

    omask = piece_get_mask(oldp->type, oldp->rot);
    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (omask & 0x8000) {
                bx = oldp->x + (int8_t)c;
                by = oldp->y + (int8_t)r;
                /* Check if this cell is NOT covered by the new piece */
                if (bx >= 0 && bx < BOARD_W && by >= HIDDEN_ROWS && by < BOARD_ROWS) {
                    /* Is there a locked block here? */
                    if (board_get(bx, by) != PIECE_NONE) {
                        /* Is the new piece NOT covering this cell? */
                        int8_t dx = bx - newp->x;
                        int8_t dy = by - newp->y;
                        uint8_t covered = 0;
                        if (dx >= 0 && dx < 4 && dy >= 0 && dy < 4) {
                            nmask = piece_get_mask(newp->type, newp->rot);
                            nmask >>= (uint8_t)((3 - dy) * 4 + (3 - dx));
                            covered = nmask & 1;
                        }
                        if (!covered) {
                            draw_cell((uint8_t)bx, (uint8_t)by, COL_WHITE);
                        }
                    }
                }
            }
            omask <<= 1;
        }
    }
}

/* Draw a gem tile at arbitrary pixel position (for preview) */
static void draw_gem_at(uint16_t px, uint16_t py)
{
    vid_blit_tile(px, py, g_gem_tile);
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
                draw_gem_at(px, py);
            }
            mask <<= 1;
        }
    }
}

/* Format and display score info */
static void draw_info(void)
{
    char buf[12];
    uint16_t x, y;

    x = INFO_PX_X;
    y = INFO_PX_Y;

    draw_text(x, y, "SCORE", COL_WHITE);
    y += 10;
    u32_to_str(g_score, buf);
    erase_text(x, y, 8);
    draw_text(x, y, buf, COL_YELLOW);

    y += 16;
    draw_text(x, y, "LINES", COL_WHITE);
    y += 10;
    u16_to_str(g_lines, buf);
    erase_text(x, y, 5);
    draw_text(x, y, buf, COL_YELLOW);

    y += 16;
    draw_text(x, y, "LEVEL", COL_WHITE);
    y += 10;
    u16_to_str((uint16_t)g_level, buf);
    erase_text(x, y, 3);
    draw_text(x, y, buf, COL_YELLOW);

    y += 16;
    draw_text(x, y, "TOP", COL_WHITE);
    y += 10;
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

static void do_move_left(void)
{
    if (piece_can_place(g_cur.type, g_cur.rot, g_cur.x - 1, g_cur.y)) {
        erase_piece(&g_cur);
        g_cur.x--;
        draw_piece(&g_cur);
    }
}

static void do_move_right(void)
{
    if (piece_can_place(g_cur.type, g_cur.rot, g_cur.x + 1, g_cur.y)) {
        erase_piece(&g_cur);
        g_cur.x++;
        draw_piece(&g_cur);
    }
}

static void do_rotate(void)
{
    uint8_t new_rot;
    new_rot = (g_cur.rot + 1) & 3;
    if (piece_can_place(g_cur.type, new_rot, g_cur.x, g_cur.y)) {
        erase_piece(&g_cur);
        g_cur.rot = new_rot;
        draw_piece(&g_cur);
    }
}

static uint8_t do_move_down(void)
{
    if (piece_can_place(g_cur.type, g_cur.rot, g_cur.x, g_cur.y + 1)) {
        erase_piece(&g_cur);
        g_cur.y++;
        draw_piece(&g_cur);
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
    lock_and_advance();
}

/* ── Title screen ── */
static void show_title(void)
{
    char buf[12];
    vid_clear();
    draw_text_centred(60, "TIKI TETRIS V0.8", COL_YELLOW);
    draw_text_centred(80, "BY ARCTIC RETRO", COL_WHITE);
    if (g_top_score > 0) {
        draw_text_centred(100, "TOP", COL_WHITE);
        u32_to_str(g_top_score, buf);
        draw_text_centred(112, buf, COL_YELLOW);
    }
    draw_text_centred(130, "A-LEFT  D-RIGHT", COL_BRCYAN);
    draw_text_centred(144, "W-ROTATE  S-DROP", COL_BRCYAN);
    draw_text_centred(158, "SPACE-HARD DROP", COL_BRCYAN);
    draw_text_centred(172, "P-PAUSE  Q-QUIT", COL_BRCYAN);
    draw_text_centred(192, "PRESS ANY KEY", COL_WHITE);

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
    draw_stats();

    /* Side branding */
    vid_draw_text_rotcw(3, 16, "TIKI TETRIS BY ARCTIC RETRO", COL_ORANGE);

    draw_text(NEXT_PX_X, NEXT_PX_Y - 12, "NEXT", COL_WHITE);
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
