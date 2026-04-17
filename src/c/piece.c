/* TIKI TETRIS — piece.c
 * Tetromino data and collision detection.
 */

#include "piece.h"
#include "board.h"

/* Tetromino bitmasks: [type-1][rotation]
 * 4x4 grid, row-major, bit 15 = (0,0).
 *
 * Layout reference (bit positions):
 *   15 14 13 12
 *   11 10  9  8
 *    7  6  5  4
 *    3  2  1  0
 */
static const uint16_t tetro_masks[7][4] = {
    /* I */
    { 0x00F0, 0x2222, 0x00F0, 0x2222 },
    /* O */
    { 0x0660, 0x0660, 0x0660, 0x0660 },
    /* T */
    { 0x0E40, 0x4C40, 0x4E00, 0x4640 },
    /* S */
    { 0x06C0, 0x8C40, 0x06C0, 0x8C40 },
    /* Z */
    { 0x0C60, 0x4C80, 0x0C60, 0x4C80 },
    /* J */
    { 0x0E80, 0xC440, 0x2E00, 0x44C0 },
    /* L */
    { 0x0E20, 0x44C0, 0x8E00, 0xC440 }
};

uint16_t piece_get_mask(uint8_t type, uint8_t rot)
{
    if (type < 1 || type > 7) return 0;
    return tetro_masks[type - 1][rot & 3];
}

uint8_t piece_can_place(uint8_t type, uint8_t rot, int8_t px, int8_t py)
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
                /* Out of bounds? */
                if (bx < 0 || bx >= BOARD_W) return 0;
                if (by >= BOARD_ROWS) return 0;
                /* Above the board is OK (hidden rows can be negative for spawn) */
                if (by >= 0) {
                    if (board_get(bx, by) != PIECE_NONE) return 0;
                }
            }
            mask <<= 1;
        }
    }
    return 1;
}
