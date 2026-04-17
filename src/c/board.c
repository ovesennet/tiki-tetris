/* TIKI TETRIS — board.c
 * Board array and line clearing logic.
 */

#include "board.h"
#include "piece.h"
#include <string.h>

static uint8_t g_board[BOARD_ROWS][BOARD_W];

void board_init(void)
{
    memset(g_board, PIECE_NONE, sizeof(g_board));
}

uint8_t board_get(int8_t x, int8_t y)
{
    if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_ROWS)
        return PIECE_NONE;
    return g_board[y][x];
}

void board_set(uint8_t x, uint8_t y, uint8_t val)
{
    if (x < BOARD_W && y < BOARD_ROWS)
        g_board[y][x] = val;
}

void board_lock_piece(uint8_t type, uint8_t rot, int8_t px, int8_t py)
{
    uint16_t mask;
    uint8_t r, c;
    int8_t bx, by;

    mask = piece_get_mask(type, rot);

    for (r = 0; r < 4; r++) {
        for (c = 0; c < 4; c++) {
            if (mask & 0x8000) {
                bx = px + (int8_t)c;
                by = py + (int8_t)r;
                if (bx >= 0 && bx < BOARD_W && by >= 0 && by < BOARD_ROWS) {
                    g_board[by][bx] = type;
                }
            }
            mask <<= 1;
        }
    }
}

uint8_t board_clear_lines(void)
{
    uint8_t cleared = 0;
    int8_t y, x;
    uint8_t full;

    /* Scan from bottom to top */
    for (y = BOARD_ROWS - 1; y >= 0; y--) {
        full = 1;
        for (x = 0; x < BOARD_W; x++) {
            if (g_board[y][x] == PIECE_NONE) {
                full = 0;
                break;
            }
        }
        if (full) {
            cleared++;
            /* Shift everything above down by one row */
            if (y > 0) {
                memmove(&g_board[1], &g_board[0], (uint16_t)y * BOARD_W);
            }
            memset(&g_board[0], PIECE_NONE, BOARD_W);
            y++; /* Re-check this row since we shifted down */
        }
    }
    return cleared;
}
