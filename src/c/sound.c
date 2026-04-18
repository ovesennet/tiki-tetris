/* TIKI TETRIS — sound.c
 * Sound effects and title music via AY-3-8910 (YM2149) PSG.
 *
 * TIKI-100 PSG clock assumed to match z88dk psgT() = 100000/freq.
 * All tone periods are pre-computed integers to avoid float math.
 */

#include <stdint.h>
#include <arch/tiki100.h>
#include "sound.h"\n\nextern uint8_t kbd_scan(void);

/* ── Note period table (100000 / frequency) ── */
/* Octave 3 */
#define NOTE_A3   454   /* 220.0 Hz */
#define NOTE_B3   405   /* 246.9 Hz */

/* Octave 4 */
#define NOTE_C4   382   /* 261.6 Hz */
#define NOTE_D4   340   /* 293.7 Hz */
#define NOTE_E4   303   /* 329.6 Hz */
#define NOTE_F4   287   /* 349.2 Hz */
#define NOTE_G4   255   /* 392.0 Hz */
#define NOTE_A4   227   /* 440.0 Hz */
#define NOTE_B4   202   /* 493.9 Hz */

/* Octave 5 */
#define NOTE_C5   191   /* 523.3 Hz */
#define NOTE_E5   152   /* 659.3 Hz */
#define NOTE_G5   128   /* 784.0 Hz */
#define NOTE_C6    96   /* 1046.5 Hz */

#define REST        0

/* ── Duration units (delay ticks) ── */
#define D8   1600   /* eighth note      */
#define D4   3200   /* quarter note     */
#define D4D  4800   /* dotted quarter   */
#define D2   6400   /* half note        */

/* ── RNG counter (extern from main.c) ── */
extern uint16_t g_rng;

/* Sound enabled flag (default on) */
uint8_t g_sound_on = 1;

/* ────────────────────────────────────── */

void sound_init(void)
{
    psg_init();
}

void sound_off(void)
{
    psg_volume(0, 0);
    psg_volume(1, 0);
    psg_volume(2, 0);
}

/* ── Sound effects ── */

void sfx_line_clear(void)
{
    if (!g_sound_on) return;
    psg_channels(chan0, chanNone);
    psg_tone(0, NOTE_E5);
    psg_volume(0, 14);
    delay(1500);
    psg_tone(0, NOTE_C6);
    delay(1000);
    sound_off();
}

void sfx_level_up(void)
{
    if (!g_sound_on) return;
    /* Rising arpeggio: C5 → E5 → G5 → C6 */
    static const uint16_t periods[] = { NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6 };
    uint8_t i;
    psg_channels(chan0, chanNone);
    for (i = 0; i < 4; i++) {
        psg_tone(0, periods[i]);
        psg_volume(0, 14);
        delay(1000);
    }
    sound_off();
}

void sfx_drop(void)
{
    if (!g_sound_on) return;
    /* Short low thud: quick descending tone */
    psg_channels(chan0, chanNone);
    psg_tone(0, NOTE_A3);
    psg_volume(0, 13);
    delay(600);
    psg_tone(0, 600);  /* very low */
    delay(400);
    sound_off();
}

void sfx_move(void)
{
    if (!g_sound_on) return;
    /* Tiny click/tick */
    psg_channels(chan0, chanNone);
    psg_tone(0, NOTE_C6);
    psg_volume(0, 8);
    delay(200);
    sound_off();
    sound_off();
}

/* ── Korobeiniki — Tetris Theme A ── */
/* Correct melody in A minor (Game Boy arrangement) */

struct note { uint16_t period; uint16_t dur; };

static const struct note g_melody[] = {
    /* ── Part A (bars 1–4) ── */
    /* Bar 1: E4.  B3  C4  D4  C4  B3 */
    { NOTE_E4, D4D }, { NOTE_B3, D8  }, { NOTE_C4, D4  },
    { NOTE_D4, D8  }, { NOTE_C4, D8  }, { NOTE_B3, D4  },
    /* Bar 2: A3  A3  C4  E4  D4  C4 */
    { NOTE_A3, D4  }, { NOTE_A3, D8  }, { NOTE_C4, D8  },
    { NOTE_E4, D4  }, { NOTE_D4, D8  }, { NOTE_C4, D8  },
    /* Bar 3: B3.  C4  D4  E4 */
    { NOTE_B3, D4D }, { NOTE_C4, D8  }, { NOTE_D4, D4  },
    { NOTE_E4, D4  },
    /* Bar 4: C4  A3  A3  (rest) */
    { NOTE_C4, D4  }, { NOTE_A3, D4  }, { NOTE_A3, D2  },
    { REST,    D4  },

    /* ── Part B (bars 5–8) ── */
    /* Bar 5: D4.  F4  A4  G4  F4 */
    { NOTE_D4, D4D }, { NOTE_F4, D8  }, { NOTE_A4, D4  },
    { NOTE_G4, D8  }, { NOTE_F4, D8  },
    /* Bar 6: E4.  C4  E4  D4  C4 */
    { NOTE_E4, D4D }, { NOTE_C4, D8  }, { NOTE_E4, D4  },
    { NOTE_D4, D8  }, { NOTE_C4, D8  },
    /* Bar 7: B3.  C4  D4  E4 */
    { NOTE_B3, D4D }, { NOTE_C4, D8  }, { NOTE_D4, D4  },
    { NOTE_E4, D4  },
    /* Bar 8: C4  A3  A3  (rest) */
    { NOTE_C4, D4  }, { NOTE_A3, D4  }, { NOTE_A3, D2  },
    { REST,    D4  },
};

#define MELODY_LEN (sizeof(g_melody) / sizeof(g_melody[0]))

uint8_t play_title_music(void)
{
    uint8_t i;
    uint16_t d;

    /* Channel A = melody, Channel B = same melody one octave lower */
    psg_channels(chan0 | chan1, chanNone);

    for (i = 0; i < MELODY_LEN; i++) {
        if (!g_sound_on || g_melody[i].period == REST) {
            psg_volume(0, 0);
            psg_volume(1, 0);
        } else {
            /* Channel A: melody */
            psg_tone(0, g_melody[i].period);
            psg_volume(0, 12);
            /* Channel B: one octave lower (period × 2) */
            psg_tone(1, g_melody[i].period * 2);
            psg_volume(1, 10);
        }
        /* Check for keypress in chunks — larger steps = steadier tempo */
        for (d = 0; d < g_melody[i].dur; d += 600) {
            g_rng++;
            delay(600);
            kbd_scan();          /* discard first scan (port may be stale) */
            if (kbd_scan()) {    /* real check */
                sound_off();
                return 1;
            }
        }
    }
    sound_off();
    return 0;
}
