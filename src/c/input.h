/* TIKI TETRIS — input.h
 * Direct keyboard matrix scanning — zero latency, true held state.
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* Bitmask bits returned by kbd_scan() / input_poll() */
#define KBIT_LEFT   0x01    /* 'a' held */
#define KBIT_RIGHT  0x02    /* 'd' held */
#define KBIT_ROT    0x04    /* 'w' held */
#define KBIT_DOWN   0x08    /* 's' held */
#define KBIT_DROP   0x10    /* space held */
#define KBIT_QUIT   0x20    /* 'q' held */
#define KBIT_PAUSE  0x40    /* 'p' held */
#define KBIT_SOUND  0x80    /* 'o' held */

void input_init(void);
void input_shutdown(void);

/* Call once per frame — scans keyboard matrix */
void input_poll(void);

/* Flush state */
void input_flush(void);

/* Returns 1 if key is currently held */
uint8_t input_held(uint8_t bit);

/* Returns 1 if key was just pressed this frame (edge) */
uint8_t input_pressed(uint8_t bit);

/* Returns 1 if any key is held */
uint8_t input_any(void);

/* Wait for any key press */
void input_wait_key(void);

#endif
