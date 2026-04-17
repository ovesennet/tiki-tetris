/* TIKI TETRIS — video.h
 * Video subsystem — mode set (via ROM), direct VRAM access for drawing.
 */

#ifndef VIDEO_H
#define VIDEO_H

#include "defs.h"

/* ── Lifecycle ─────────────────────────────────────────── */
void vid_init(void);
void vid_shutdown(void);

/* ── Drawing primitives ────────────────────────────────── */
void vid_clear(void);
void vid_hline(uint16_t x1, uint16_t x2, uint16_t y, uint8_t colour);
void vid_vline(uint16_t x, uint16_t y1, uint16_t y2, uint8_t colour);
void vid_fill_rect(uint16_t x, uint16_t y, uint8_t w, uint8_t h, uint8_t colour);
void vid_plot(uint16_t x, uint16_t y, uint8_t colour);
void vid_draw_text(uint16_t x, uint16_t y, const char *str, uint8_t colour);
void vid_draw_text_rotcw(uint16_t x, uint16_t y, const char *str, uint8_t colour);
/* Fast 10x10 tile blit: copies pre-rendered tile_data to VRAM.
 * x must be even. tile_data is 50 bytes (5 bytes/row x 10 rows). */
void vid_blit_tile(uint16_t x, uint16_t y, uint8_t *tile);

#endif /* VIDEO_H */
