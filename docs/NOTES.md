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
| `main.c` | Game loop, state machine, rendering, input, scoring |
| `video.c` | Video init/shutdown, drawing primitive wrappers |
| `font.c` | Text rendering helpers (centred text, erase) |
| `board.c` | 10×22 board array, line clearing with row shifting |
| `piece.c` | 7 tetrominoes as 16-bit 4×4 bitmasks, collision detection |
| `hello.c` | Toolchain test (standalone CP/M hello world) |

### Headers (`src/c/`)

| File | Purpose |
|------|---------|
| `defs.h` | Board geometry, palette indices, key bindings, game constants |
| `video.h` | Drawing primitive declarations |
| `font.h` | Font metrics and text function declarations |
| `board.h` | Board operations API |
| `piece.h` | Tetromino data and collision API |

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
| `vid_blit_tile_gfx` | Fast 10×10 tile blit — copies 50 pre-rendered bytes to VRAM |
| `vid_draw_text_gfx` | Fast string rendering with single VRAM bank switch |

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
with one fast block copy. Two tiles are pre-built:

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
- Immediate lock on landing
- Clockwise rotation only
- Simple PRNG (`rand() % 7`)
- Level increases every 10 lines
- Scoring: Single=40, Double=100, Triple=300, Tetris=1200 (× level+1)
- 20-level speed table from 48 ticks/drop down to 1

## Performance Considerations

- **No `sprintf`/`printf`** — custom `u32_to_str` avoids pulling in the huge
  printf library (saved ~1 KB of code)
- **Tile blit** — 1 VRAM bank switch per cell vs 3–5 with fill_rect calls
- **Minimal redraws** — only redraw full board on line clears, not every piece lock
- **No `vid_plot` in hot paths** — each plot does a full bank switch cycle
- **Score display** only updates on line clears, not every frame
- **Border drawn once** at game start, never redrawn during play
- **Erase text widths** kept within screen bounds (x + width ≤ 256) to prevent
  hline wrap-around bug that overwrites unrelated VRAM

## Input

- Uses `<conio.h>`: `kbhit()` polls keyboard, `getch()` reads character
- Controls: A=left, D=right, W=rotate, S=soft drop, Space=hard drop
- P=pause, Q=quit
- No key repeat (DAS) — one action per keypress

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
