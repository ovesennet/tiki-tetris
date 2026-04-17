/* TIKI TETRIS — sound.h
 * Sound effects and music via AY-3-8910 PSG
 */

#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

/* Shared delay loop (defined in main.c) */
extern void delay(uint16_t n);

extern uint8_t g_sound_on;  /* 1=on (default), 0=off */

void sound_init(void);
void sound_off(void);
void sfx_line_clear(void);
void sfx_level_up(void);
void sfx_drop(void);
void sfx_move(void);

/* Play Korobeiniki on title screen. Returns 1 if key pressed. */
uint8_t play_title_music(void);

#endif
