/* TIKI TETRIS — video.c
 * Video initialisation using z88dk tiki100 library.
 * Drawing done by assembly routines in screen.asm (high memory).
 */

#include <arch/tiki100.h>
#include "video.h"

/* Assembly routines in screen.asm */
extern void vid_clear_asm(void);
extern void vid_plot_gfx(void);
extern void vid_hline_gfx(void);
extern void vid_vline_gfx(void);
extern void vid_fill_rect_gfx(void);
extern void vid_draw_text_gfx(void);
extern void vid_draw_text_rotcw_gfx(void);
extern void vid_blit_tile_gfx(void);

/* Global parameter block (bss_graphics section) */
extern uint16_t txt_x;
extern uint16_t txt_y;
extern uint16_t txt_str;
extern uint8_t  txt_colour;
extern uint16_t gfx_x1;
extern uint16_t gfx_y1;
extern uint16_t gfx_x2;
extern uint16_t gfx_y2;
extern uint8_t  gfx_colour;
extern uint8_t  gfx_width;
extern uint8_t  gfx_height;

/* Tile blit params */
extern uint16_t tile_px;
extern uint16_t tile_py;
extern uint8_t  tile_data[50];

/* Palette */
static const char palette[16] = {
    /* 0  Black   */  0x00,
    /* 1  Blue    */  0x03,
    /* 2  Green   */  0x1C,
    /* 3  Cyan    */  0x1F,
    /* 4  Red     */  0xE0,
    /* 5  Magenta */  0xE3,
    /* 6  Orange  */  0xF0,
    /* 7  LtGrey  */  0xB5,
    /* 8  DkGrey  */  0x49,
    /* 9  BrBlue  */  0x4B,
    /*10  BrGreen */  0x5C,
    /*11  BrCyan  */  0x5F,
    /*12  BrRed   */  0xEC,
    /*13  Pink    */  0xF3,
    /*14  Yellow  */  0xFC,
    /*15  White   */  0xFF,
};

void vid_init(void)
{
    gr_defmod(3);
    gr_setpalette(16, palette);

    __asm
    ld   a, $C9
    ld   ($F057), a
    __endasm;

    vid_clear();
}

void vid_shutdown(void)
{
    __asm
    ld   a, $C3
    ld   ($F057), a
    __endasm;

    gr_defmod(1);
}

void vid_clear(void)
{
    vid_clear_asm();
}

void vid_plot(uint16_t x, uint16_t y, uint8_t colour)
{
    if (x >= 256 || y >= 256) return;
    gfx_x1 = x;
    gfx_y1 = y;
    gfx_colour = colour;
    vid_plot_gfx();
}

void vid_hline(uint16_t x1, uint16_t x2, uint16_t y, uint8_t colour)
{
    if (y >= 256) return;
    if (x1 >= 256 && x2 >= 256) return;
    if (x1 > x2) { uint16_t t = x1; x1 = x2; x2 = t; }
    if (x2 >= 256) x2 = 255;
    gfx_x1 = x1;
    gfx_x2 = x2;
    gfx_y1 = y;
    gfx_colour = colour;
    vid_hline_gfx();
}

void vid_vline(uint16_t x, uint16_t y1, uint16_t y2, uint8_t colour)
{
    if (x >= 256) return;
    if (y1 > y2) { uint16_t t = y1; y1 = y2; y2 = t; }
    if (y1 >= 256) return;
    if (y2 >= 256) y2 = 255;
    gfx_x1 = x;
    gfx_y1 = y1;
    gfx_y2 = y2;
    gfx_colour = colour;
    vid_vline_gfx();
}

void vid_fill_rect(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint8_t colour)
{
    if (w == 0 || h == 0) return;
    gfx_x1 = x;
    gfx_y1 = y;
    gfx_width = w;
    gfx_height = h;
    gfx_colour = colour;
    vid_fill_rect_gfx();
}

void vid_draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour)
{
    txt_x = x;
    txt_y = y;
    txt_str = (uint16_t)str;
    txt_colour = colour;
    vid_draw_text_gfx();
}

void vid_draw_text_rotcw(uint16_t x, uint16_t y, const char *str, uint8_t colour)
{
    txt_x = x;
    txt_y = y;
    txt_str = (uint16_t)str;
    txt_colour = colour;
    vid_draw_text_rotcw_gfx();
}

void vid_blit_tile(uint16_t x, uint16_t y, uint8_t *tile)
{
    uint8_t i;
    tile_px = x;
    tile_py = y;
    for (i = 0; i < 50; i++)
        tile_data[i] = tile[i];
    vid_blit_tile_gfx();
}

void vid_set_palette_entry(uint8_t index, uint8_t rgb)
{
    char buf[16];
    uint8_t i;
    for (i = 0; i < 16; i++)
        buf[i] = palette[i];
    buf[index] = (char)rgb;
    gr_setpalette(16, buf);
}
