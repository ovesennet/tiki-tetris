/* TIKI TETRIS — piece.h
 * Tetromino definitions, collision detection, rotation.
 */

#ifndef PIECE_H
#define PIECE_H

#include "defs.h"

/* Each tetromino rotation stored as a 16-bit mask over a 4x4 grid.
 * Bit 15 = top-left (row0,col0), bit 0 = bottom-right (row3,col3).
 * Row-major: bits 15-12 = row 0, bits 11-8 = row 1, etc. */

/* Active piece state */
typedef struct {
    uint8_t type;       /* 1..7 (PIECE_I..PIECE_L) */
    uint8_t rot;        /* 0..3 */
    int8_t  x;          /* board column of 4x4 top-left */
    int8_t  y;          /* board row of 4x4 top-left */
} Piece;

/* Get the 16-bit bitmask for a piece type and rotation */
uint16_t piece_get_mask(uint8_t type, uint8_t rot);

/* Test if piece can be placed at (x,y) on the board.
 * Returns 1 if valid, 0 if collision. */
uint8_t piece_can_place(uint8_t type, uint8_t rot, int8_t x, int8_t y);

/* Get the colour index for a piece type */
uint8_t piece_colour(uint8_t type);

#endif /* PIECE_H */
