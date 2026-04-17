# TIKI TETRIS — Technical Notes

## Overview

Tiki Tetris is a classic Tetris implementation for the **TIKI-100** computer,
a Norwegian Z80-based CP/M machine from the 1980s. Written in C and Z80 assembly
using the **z88dk** toolchain (sccz80 compiler). Outputs a CP/M .COM executable
and a raw 400K disk image for the Djupdal emulator.

## Target Hardware

- **CPU:** Z80 @ 4 MHz
- **RAM:** 64 KB (shared with VRAM when banked)
- **Display:** Mode 3 — 256×256 pixels, 16 colours
- **Sound:** YM2149 / AY-3-8910 PSG — 3 tone channels, accessed via z88dk
  `psg_tone()`, `psg_volume()`, `psg_channels()` (ports $16/$17)
- **VRAM:** 32 KB mapped at $0000–$7FFF when system register port $1C bit 3 is set
- **VRAM layout:** 128 bytes/scanline × 256 scanlines. 2 pixels per byte:
  low nibble (bits 3–0) = even pixel, high nibble (bits 7–4) = odd pixel
- **Byte address formula:** `y × 128 + x / 2`
- **Palette:** 8-bit RGB encoding — bits 7–5 = red, bits 4–2 = green, bits 1–0 = blue

## Toolchain

- **Compiler:** z88dk sccz80 (`zcc +cpm -subtype=tiki100_400k -compiler=sccz80`)
- **Target:** CP/M with TIKI-100 400K disk subtype (links `-ltiki100` library)
- **Startup:** `-startup=0` (minimal CRT)
- **Disk image:** `-Cz--container=raw` produces a raw .img renamed to .dsk
- **Build:** PowerShell script `build.ps1`, deploy via `deploy.ps1`

## Memory Architecture

Code is split between normal memory and **high memory** (above $8000):

- **C code and data** live in normal sections (loaded at $0100+ by CP/M)
- **Assembly routines and font data** live in `SECTION code_graphics` (above $8000)
  so they remain accessible when VRAM is banked into $0000–$7FFF
- **BSS variables** for graphics parameters live in `SECTION bss_graphics` (also high memory)

The z88dk library provides `swapgfxbk` / `swapgfxbk1` to bank VRAM in and out.

## C ↔ Assembly Interface

C functions write parameters to global variables in `bss_graphics`, then call
assembly entry points. For example, `vid_fill_rect()` writes `gfx_x1`, `gfx_y1`,
`gfx_width`, `gfx_height`, `gfx_colour`, then calls `vid_fill_rect_gfx`.

## Source Structure

### C files (`src/c/`)

| File | Purpose |
|------|---------|
| `main.c` | Game loop, state machine, rendering, title screen, cathedral, TETRIS logo |
| `video.c` | Video init/shutdown, drawing primitive wrappers |
| `font.c` | Text rendering helpers (centred text, erase) |
| `board.c` | 10×22 board array, line clearing with row shifting |
| `piece.c` | 7 tetrominoes as 16-bit 4×4 bitmasks, collision detection |
| `sound.c` | PSG sound effects and Korobeiniki title music |
| `hello.c` | Toolchain test (standalone CP/M hello world) |

### Headers (`src/c/`)

| File | Purpose |
|------|---------|
| `defs.h` | Board geometry, palette indices, key bindings, game constants |
| `video.h` | Drawing primitive declarations |
| `font.h` | Font metrics and text function declarations |
| `board.h` | Board operations API |
| `piece.h` | Tetromino data and collision API |
| `sound.h` | Sound effects and music API |

### Assembly (`src/asm/`)

| File | Purpose |
|------|---------|
| `screen.asm` | All VRAM routines, font data, tile blit |

## Graphics Routines (screen.asm)

All in `SECTION code_graphics` — accessible during VRAM banking.

| Routine | Description |
|---------|-------------|
| `vid_clear_asm` | Zero all 32K VRAM via `LDIR` |
| `vid_plot_gfx` | Single pixel RMW (even=low nibble, odd=high nibble) |
| `vid_hline_gfx` | 3-phase horizontal line: left partial, middle `LDIR` fill, right partial |
| `vid_vline_gfx` | Per-scanline pixel plot, +128 bytes per row |
| `vid_fill_rect_gfx` | Calls hline per row of rectangle |
| `vid_blit_tile_gfx` | Fast 10×10 tile blit — LDIR source to high mem, then copy to VRAM |
| `vid_draw_text_gfx` | Fast string rendering with single VRAM bank switch |
| `vid_draw_text_rotcw_gfx` | Rotated 90° CW text rendering (5×7 → 7×5 transpose) |

### Font

- 5×7 pixel glyphs, 41 characters (A–Z, 0–9, space, `!`, `.`, `:`, `-`)
- Stored as 287 bytes in assembly (`_font_gfx`)
- 128-entry ASCII lookup table (`_char_map`) maps to glyph indices
- Characters advance by 6px (5 + 1 spacing)

## Tile Blit System

The key performance optimization. Each 10×10 pixel game tile is pre-rendered
into a 50-byte buffer (5 bytes/row × 10 rows) in VRAM nibble format. The
`vid_blit_tile_gfx` routine copies this to VRAM with a **single bank switch**
— 10 rows × 5 byte copies per row, advancing +128 per scanline.

This replaces multiple `vid_fill_rect` calls (each requiring its own bank switch)
with one fast block copy. The C wrapper passes a pointer to the tile data; the ASM
routine copies it to a high-memory scratch buffer (via `LDIR`) before banking VRAM,
since the source data in low memory is hidden during VRAM access.

Two tiles are pre-built:

- **Gem tile** — white highlight (top + left), bright colour body, dark shade
  (bottom + right), with reflection details. Rebuilt when colour scheme changes.
- **Black tile** — all zeros, built once at game start.

## Gem Colour Schemes

6 colour schemes, each with a bright and dark shade. All use white highlights.
A random scheme is picked each level (never the same as the previous).

| Scheme | Bright | Dark |
|--------|--------|------|
| Cyan | BrCyan (11) | Cyan (3) |
| Yellow | Yellow (14) | Orange (6) |
| Purple | Pink (13) | Magenta (5) |
| Green | BrGreen (10) | Green (2) |
| Red | BrRed (12) | Red (4) |
| Blue | BrBlue (9) | Blue (1) |

## Game Rules

NES/Game Boy classic style:

- 10×20 visible board (+ 2 hidden rows)
- 7 standard tetrominoes (I, O, T, S, Z, J, L)
- No wall kicks, no hold, no ghost piece
- Immediate lock on landing (soft drop moves down without locking)
- Clockwise rotation only
- Simple PRNG (xorshift16, seeded from title screen wait time)
- Level increases every 10 lines
- Scoring: Single=40, Double=100, Triple=300, Tetris=1200 (× level+1)
- Hard drop adds 1 point per row dropped
- Soft drop adds 1 point per row
- 20-level speed table from 48 ticks/drop down to 1
- Top score persists across games (in memory, not saved to disk)

## Performance Considerations

- **No `sprintf`/`printf`** — custom `u32_to_str` avoids pulling in the huge
  printf library (saved ~1 KB of code)
- **Tile blit** — pointer passed to ASM, LDIR to high-mem scratch, 1 VRAM bank
  switch per cell vs 3–5 with fill_rect calls
- **Delta draw** — piece movement uses a diff algorithm (`delta_move`) that only
  redraws cells that actually change between old and new positions, eliminating
  flicker from the erase-then-draw approach
- **Minimal redraws** — only redraw full board on line clears, not every piece lock
- **Split static/dynamic drawing** — info labels ("SCORE", "LINES", etc.) and
  statistics mini-pieces are drawn once at game start; only values/counters update
- **No `vid_plot` in hot paths** — each plot does a full bank switch cycle;
  TETRIS logo uses `vid_vline` (1 bank switch per 4px column) instead
- **Score display** only updates on piece lock, not every frame
- **Border drawn once** at game start, never redrawn during play
- **Erase text widths** kept within screen bounds (x + width ≤ 256) to prevent
  hline wrap-around bug that overwrites unrelated VRAM

## Input

- Uses `<conio.h>`: `kbhit()` polls keyboard, `getch()` reads character
- Controls: A=left, D=right, W=rotate, S=soft drop, Space=hard drop
- P=pause (waits specifically for P to unpause), Q=quit, O=sound on/off
- No key repeat (DAS) — one action per keypress

## Sound

Sound is provided by the YM2149 / AY-3-8910 PSG chip, accessed via z88dk's
`<arch/tiki100.h>` library functions.

### Sound Effects

| Effect | Description |
|--------|-------------|
| `sfx_line_clear` | Rising two-note chirp (E5 → C6) |
| `sfx_level_up` | Rising arpeggio (C5 → E5 → G5 → C6) |
| `sfx_drop` | Short low thud (A3 → very low), descending |
| `sfx_move` | Tiny high click (C6, low volume, very short) |

### Title Music

Korobeiniki melody (the classic Tetris theme) plays on the title screen in
a loop until a key is pressed. Uses two PSG channels:

- **Channel A:** Melody at volume 12
- **Channel B:** Same melody one octave lower (period × 2) at volume 10

All note periods are pre-computed integers to avoid pulling in float math:

| Note | Period | Note | Period |
|------|--------|------|--------|
| A3 | 454 | A4 | 227 |
| B3 | 405 | B4 | 202 |
| C4 | 382 | C5 | 191 |
| D4 | 340 | E5 | 152 |
| E4 | 303 | G5 | 128 |
| F4 | 287 | C6 | 96 |
| G4 | 255 | | |

Duration units use delay ticks: D8=1600, D4=3200, D4D=4800, D2=6400.
Keypress is checked every 600 ticks within each note for responsive interruption.

Sound can be toggled on/off with the O key. When off, all `sfx_*` functions
return immediately and music notes play silently (timing loop still runs to
seed the PRNG).

## Title Screen

The title screen features:

- **St. Basil's Cathedral** — drawn procedurally using ~50 `vid_fill_rect` calls
  (5 onion domes with coloured stripes, towers with windows, base with arched windows)
- **TETRIS block logo** — 6 letters (18px wide × 20px tall each) defined as
  bitmap arrays (5 rows of 18-bit masks), rendered with a 5-band gradient:
  red → bright red → orange → yellow → green
- **Rotated branding** — "TIKI TETRIS BY ARCTIC RETRO" drawn vertically along
  the left side of the game board using the ASM rotated text routine
- **Controls help** and "PRESS ANY KEY" prompt

## Game Screen Layout

- **Left panel (x=30):** Statistics — mini piece previews with counters
- **Board (x=78, y=16):** 100×200px playfield with 2px white border + 3D shadow
- **Right panel (x=200):** Next piece preview, score, lines, level, top score
- **Below board (y=222):** TETRIS gradient logo
- **Left edge (x=3):** Rotated branding text

## Build & Deploy

```
.\build.ps1              # Build tikitet.com + tikitet.dsk
.\build.ps1 -Hello       # Build hello world test only
.\build.ps1 -Clean       # Clean first
.\deploy.ps1             # Build, copy to work.dsk, launch emulator
```

The deploy script uses a custom CP/M disk image writer in PowerShell — it
reads the base disk, finds free directory slots and blocks, writes the .COM
file data, and saves the modified image. No external CP/M disk tools needed.
