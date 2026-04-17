/* TIKI TETRIS — defs.h
 * Shared definitions, types, and constants.
 */

#ifndef DEFS_H
#define DEFS_H

#include <stdint.h>

/* ── Board geometry ────────────────────────────────────── */
#define BOARD_W        10
#define BOARD_ROWS     22      /* 20 visible + 2 hidden rows at top */
#define VISIBLE_ROWS   20
#define HIDDEN_ROWS    2

/* ── Display layout (256×256 screen, mode 3) ───────────── */
#define CELL_SIZE      10      /* pixels per cell */
#define BOARD_PX_X     78      /* playfield left edge pixel */
#define BOARD_PX_Y     16      /* playfield top edge pixel (row 2 = first visible) */

/* Next piece preview area */
#define NEXT_PX_X      200
#define NEXT_PX_Y      40

/* Score/info area */
#define INFO_PX_X      200
#define INFO_PX_Y      100

/* Statistics panel */
#define STATS_PX_X     30
#define STATS_PX_Y     16
#define STATS_CELL     4      /* mini cell size for piece preview */

/* ── Piece types (1-based for board cell values) ───────── */
#define PIECE_NONE     0
#define PIECE_I        1
#define PIECE_O        2
#define PIECE_T        3
#define PIECE_S        4
#define PIECE_Z        5
#define PIECE_J        6
#define PIECE_L        7
#define NUM_PIECES     7

/* ── Palette indices ───────────────────────────────────── */
#define COL_BLACK      0
#define COL_BLUE       1
#define COL_GREEN      2
#define COL_CYAN       3
#define COL_RED        4
#define COL_MAGENTA    5
#define COL_ORANGE     6
#define COL_LTGREY     7
#define COL_DKGREY     8
#define COL_BRBLUE     9
#define COL_BRGREEN    10
#define COL_BRCYAN     11
#define COL_BRRED      12
#define COL_PINK       13
#define COL_YELLOW     14
#define COL_WHITE      15

/* ── Game states ───────────────────────────────────────── */
#define STATE_TITLE    0
#define STATE_PLAY     1
#define STATE_GAMEOVER 2

/* ── Scoring ───────────────────────────────────────────── */
#define LINES_PER_LEVEL 10

/* ── Input keys ────────────────────────────────────────── */
#define KEY_LEFT       'a'
#define KEY_RIGHT      'd'
#define KEY_ROTATE     'w'
#define KEY_DOWN       's'
#define KEY_DROP       ' '
#define KEY_QUIT       'q'
#define KEY_PAUSE      'p'

#endif /* DEFS_H */
