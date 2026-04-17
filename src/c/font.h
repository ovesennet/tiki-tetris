/* TIKI TETRIS — font.h */

#ifndef FONT_H
#define FONT_H

#include "defs.h"

#define FONT_W  5
#define FONT_CH 7
#define FONT_SPACING  1

void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);
void draw_text_centred(uint16_t y, const char *str, uint8_t colour);
void erase_text(uint16_t x, uint16_t y, uint8_t len);

#endif /* FONT_H */
