/* TIKI TETRIS — font.c */

#include "font.h"
#include "video.h"
#include <string.h>

void draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour)
{
    vid_draw_text(x, y, str, colour);
}

void draw_text_centred(uint16_t y, const char *str, uint8_t colour)
{
    uint8_t len;
    uint16_t w, x;
    len = (uint8_t)strlen(str);
    w   = (uint16_t)len * (FONT_W + FONT_SPACING) - FONT_SPACING;
    x   = (256 - w) / 2;
    draw_text(x, y, str, colour);
}

void erase_text(uint16_t x, uint16_t y, uint8_t len)
{
    uint16_t w = (uint16_t)len * (FONT_W + FONT_SPACING);
    vid_fill_rect(x, y, (uint8_t)w, FONT_CH, COL_BLACK);
}
