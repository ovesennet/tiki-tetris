/* TIKI TETRIS — board.h
 * Board operations and line clearing.
 */

#ifndef BOARD_H
#define BOARD_H

#include "defs.h"

/* Initialise board to empty */
void board_init(void);

/* Get cell value at (x, y). Returns PIECE_NONE if empty or out of bounds. */
uint8_t board_get(int8_t x, int8_t y);

/* Lock a piece into the board */
void board_lock_piece(uint8_t type, uint8_t rot, int8_t px, int8_t py);

/* Check and clear full lines. Returns number of lines cleared (0-4). */
uint8_t board_clear_lines(void);

#endif /* BOARD_H */
