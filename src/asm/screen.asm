; TIKI TETRIS - screen.asm
; Low-level VRAM drawing routines for Tiki-100 mode 3 (256x256x16).
;
; All routines in SECTION code_graphics (high memory, above 0x8000)
; so they remain accessible when VRAM is banked into 0x0000-0x7FFF.
;
; Mode 3 VRAM layout:
;   128 bytes per scanline, 256 scanlines = 32768 bytes
;   2 pixels per byte: bits 3-0 = left (even x), bits 7-4 = right (odd x)
;   Byte address = y * 128 + x / 2

    SECTION code_graphics

    PUBLIC  _vid_clear_asm
    PUBLIC  _vid_plot_gfx
    PUBLIC  _vid_hline_gfx
    PUBLIC  _vid_vline_gfx
    PUBLIC  _vid_fill_rect_gfx
    PUBLIC  _vid_draw_text_gfx
    PUBLIC  _vid_draw_text_rotcw_gfx
    PUBLIC  _vid_blit_tile_gfx

    EXTERN  swapgfxbk
    EXTERN  swapgfxbk1

; ============================================================
; Global parameter block for C -> ASM communication
; ============================================================
    SECTION bss_graphics

    PUBLIC  _gfx_x1
    PUBLIC  _gfx_y1
    PUBLIC  _gfx_x2
    PUBLIC  _gfx_y2
    PUBLIC  _gfx_colour
    PUBLIC  _gfx_width
    PUBLIC  _gfx_height

_gfx_x1:       defs 2
_gfx_y1:       defs 2
_gfx_x2:       defs 2
_gfx_y2:       defs 2
_gfx_colour:   defs 1
_gfx_width:    defs 1
_gfx_height:   defs 1

; Scratch for hline
_hl_base:      defs 2
_hl_x1:        defs 1
_hl_x2:        defs 1
_hl_colbyte:   defs 1
_hl_colnib:    defs 1

; Text draw parameters
    PUBLIC  _txt_x
    PUBLIC  _txt_y
    PUBLIC  _txt_str
    PUBLIC  _txt_colour

_txt_x:        defs 2
_txt_y:        defs 2
_txt_str:      defs 2
_txt_colour:   defs 1

; Text scratch
_txt_colbyte:  defs 1
_txt_vram:     defs 2
_txt_glyph:    defs 2
_txt_rot:      defs 5

    SECTION code_graphics

; ============================================================
; vid_clear_asm — zero all 32K VRAM
; ============================================================
_vid_clear_asm:
    call    swapgfxbk
    ld      hl, 0
    ld      de, 1
    ld      bc, 32767
    ld      (hl), 0
    ldir
    call    swapgfxbk1
    ret

; ============================================================
; vid_plot_gfx — plot single pixel
; ============================================================
_vid_plot_gfx:
    ld      hl, (_gfx_y1)
    ld      a, h
    or      a
    ret     nz
    ld      d, l

    ld      hl, (_gfx_x1)
    ld      a, h
    or      a
    ret     nz
    ld      e, l

    ld      a, (_gfx_colour)
    ld      c, a

    ; VRAM address = y * 128 + x / 2
    ld      a, d
    srl     a
    ld      h, a
    ld      a, d
    rrca
    and     $80
    ld      l, a
    ld      a, e
    srl     a
    add     a, l
    ld      l, a
    jr      nc, plot_noc
    inc     h
plot_noc:
    call    swapgfxbk
    bit     0, e
    jr      nz, plot_odd
    ; Even pixel: low nibble
    ld      a, c
    and     $0F
    ld      b, a
    ld      a, (hl)
    and     $F0
    or      b
    ld      (hl), a
    call    swapgfxbk1
    ret
plot_odd:
    ; Odd pixel: high nibble
    ld      a, c
    rlca
    rlca
    rlca
    rlca
    and     $F0
    ld      b, a
    ld      a, (hl)
    and     $0F
    or      b
    ld      (hl), a
    call    swapgfxbk1
    ret

; ============================================================
; vid_hline_gfx — fast horizontal line
; ============================================================
_vid_hline_gfx:
    ld      hl, (_gfx_y1)
    ld      a, h
    or      a
    ret     nz

    ; Compute scanline base: y * 128
    ld      a, l
    srl     a
    ld      h, a
    ld      a, (_gfx_y1)
    rrca
    and     $80
    ld      l, a
    ld      (_hl_base), hl

    ; Sort x1, x2
    ld      a, (_gfx_x1)
    ld      b, a
    ld      a, (_gfx_x2)
    ld      c, a
    ld      a, b
    cp      c
    jr      c, hl_sorted
    jr      z, hl_sorted
    ld      b, c
    ld      c, a
hl_sorted:
    ld      a, b
    ld      (_hl_x1), a
    ld      a, c
    ld      (_hl_x2), a

    ; Build colour bytes
    ld      a, (_gfx_colour)
    and     $0F
    ld      (_hl_colnib), a
    ld      b, a
    rlca
    rlca
    rlca
    rlca
    or      b
    ld      (_hl_colbyte), a

    call    swapgfxbk

    ; Phase 1: Left partial byte (x1 odd)
    ld      a, (_hl_x1)
    bit     0, a
    jr      z, hl_no_left

    srl     a
    ld      hl, (_hl_base)
    add     a, l
    ld      l, a
    jr      nc, hl_lp_noc
    inc     h
hl_lp_noc:
    ld      a, (_hl_colbyte)
    and     $F0
    ld      b, a
    ld      a, (hl)
    and     $0F
    or      b
    ld      (hl), a

    ld      a, (_hl_x1)
    inc     a
    ld      (_hl_x1), a
    ld      b, a
    ld      a, (_hl_x2)
    cp      b
    jp      c, hl_done

hl_no_left:
    ; Phase 3: Right partial byte (x2 even)
    ld      a, (_hl_x2)
    bit     0, a
    jr      nz, hl_no_right

    srl     a
    ld      hl, (_hl_base)
    add     a, l
    ld      l, a
    jr      nc, hl_rp_noc
    inc     h
hl_rp_noc:
    ld      a, (_hl_colnib)
    ld      b, a
    ld      a, (hl)
    and     $F0
    or      b
    ld      (hl), a

    ld      a, (_hl_x2)
    dec     a
    ld      (_hl_x2), a
    ld      b, a
    ld      a, (_hl_x1)
    ld      c, a
    ld      a, b
    cp      c
    jp      c, hl_done

hl_no_right:
    ; Phase 2: Middle full bytes
    ld      a, (_hl_x1)
    srl     a
    ld      c, a

    ld      a, (_hl_x2)
    srl     a
    sub     c
    inc     a
    ld      b, a

    ld      hl, (_hl_base)
    ld      a, c
    add     a, l
    ld      l, a
    jr      nc, hl_mid_noc
    inc     h
hl_mid_noc:
    ld      a, (_hl_colbyte)

hl_fill:
    ld      (hl), a
    inc     hl
    djnz    hl_fill

hl_done:
    call    swapgfxbk1
    ret

; ============================================================
; vid_vline_gfx — fast vertical line
; ============================================================
_vid_vline_gfx:
    ld      hl, (_gfx_x1)
    ld      a, h
    or      a
    ret     nz

    ld      a, l
    ld      e, a
    srl     a

    ld      d, a
    ld      hl, (_gfx_y1)
    ld      a, l
    srl     a
    ld      h, a
    ld      a, (_gfx_y1)
    rrca
    and     $80
    add     a, d
    ld      l, a
    jr      nc, vl_noc
    inc     h
vl_noc:
    ld      a, (_gfx_colour)
    and     $0F
    bit     0, e
    jr      nz, vl_odd_prep

    ld      c, a
    ld      b, $F0
    jr      vl_prep_done

vl_odd_prep:
    rlca
    rlca
    rlca
    rlca
    ld      c, a
    ld      b, $0F

vl_prep_done:
    ld      a, (_gfx_y2)
    ld      d, a
    ld      a, (_gfx_y1)
    ld      e, a
    ld      a, d
    sub     e
    inc     a
    ld      d, a

    call    swapgfxbk

vl_loop:
    ld      a, (hl)
    and     b
    or      c
    ld      (hl), a

    ld      a, l
    add     a, 128
    ld      l, a
    jr      nc, vl_no_carry
    inc     h
vl_no_carry:
    dec     d
    jr      nz, vl_loop

    call    swapgfxbk1
    ret

; ============================================================
; vid_fill_rect_gfx — fast filled rectangle
; ============================================================
_vid_fill_rect_gfx:
    ld      hl, (_gfx_x1)
    ld      a, (_gfx_width)
    dec     a
    add     a, l
    ld      l, a
    ld      a, 0
    adc     a, h
    ld      h, a
    ld      (_gfx_x2), hl

    ld      a, (_gfx_height)
    ld      b, a
    or      a
    ret     z

fr_loop:
    push    bc
    call    _vid_hline_gfx
    pop     bc

    ld      hl, (_gfx_y1)
    inc     hl
    ld      (_gfx_y1), hl
    ld      a, h
    or      a
    ret     nz

    djnz    fr_loop
    ret

; ============================================================
; vid_blit_tile_gfx — fast 10x10 tile blit
;
; Copies a pre-rendered 50-byte tile (5 bytes/row x 10 rows)
; from _tile_data (high mem) to VRAM at pixel (tile_px, tile_py).
; Assumes tile_px is EVEN (which it is: BOARD_PX_X=78).
; Single VRAM bank switch for entire tile.
;
; Params: _tile_px (uint16), _tile_py (uint16), _tile_data (50 bytes in bss)
; ============================================================

    SECTION bss_graphics

    PUBLIC  _tile_px
    PUBLIC  _tile_py
    PUBLIC  _tile_data

_tile_px:      defs 2
_tile_py:      defs 2
_tile_data:    defs 50      ; 10 rows x 5 bytes (pre-rendered)

    SECTION code_graphics

_vid_blit_tile_gfx:
    ; Compute VRAM address: tile_py * 128 + tile_px / 2
    ld      hl, (_tile_py)
    ld      a, l
    srl     a
    ld      h, a
    ld      a, (_tile_py)
    rrca
    and     $80
    ld      l, a            ; HL = y * 128

    ld      a, (_tile_px)
    srl     a               ; x / 2 (x is even)
    add     a, l
    ld      l, a
    jr      nc, bt_noc
    inc     h
bt_noc:
    ; HL = VRAM destination address
    ; DE = source tile data (high mem buffer)
    push    hl
    call    swapgfxbk
    pop     hl

    ld      de, _tile_data
    ld      b, 10           ; 10 rows

bt_row:
    push    hl              ; save VRAM row base
    push    bc              ; save row counter

    ; Copy 5 bytes from tile data to VRAM
    ld      a, (de)
    ld      (hl), a
    inc     de
    inc     hl
    ld      a, (de)
    ld      (hl), a
    inc     de
    inc     hl
    ld      a, (de)
    ld      (hl), a
    inc     de
    inc     hl
    ld      a, (de)
    ld      (hl), a
    inc     de
    inc     hl
    ld      a, (de)
    ld      (hl), a
    inc     de

    pop     bc
    pop     hl

    ; Next scanline: +128
    ld      a, l
    add     a, 128
    ld      l, a
    jr      nc, bt_nc
    inc     h
bt_nc:
    djnz    bt_row

    call    swapgfxbk1
    ret

; ============================================================
; Font data — 5x7 pixel glyphs
; ============================================================
_font_gfx:
    ; A
    defb $04, $0A, $11, $11, $1F, $11, $11
    ; B
    defb $1E, $11, $11, $1E, $11, $11, $1E
    ; C
    defb $0E, $11, $10, $10, $10, $11, $0E
    ; D
    defb $1E, $11, $11, $11, $11, $11, $1E
    ; E
    defb $1F, $10, $10, $1E, $10, $10, $1F
    ; F
    defb $1F, $10, $10, $1E, $10, $10, $10
    ; G
    defb $0E, $11, $10, $17, $11, $11, $0F
    ; H
    defb $11, $11, $11, $1F, $11, $11, $11
    ; I
    defb $0E, $04, $04, $04, $04, $04, $0E
    ; J
    defb $07, $02, $02, $02, $02, $12, $0C
    ; K
    defb $11, $12, $14, $18, $14, $12, $11
    ; L
    defb $10, $10, $10, $10, $10, $10, $1F
    ; M
    defb $11, $1B, $15, $15, $11, $11, $11
    ; N
    defb $11, $11, $19, $15, $13, $11, $11
    ; O
    defb $0E, $11, $11, $11, $11, $11, $0E
    ; P
    defb $1E, $11, $11, $1E, $10, $10, $10
    ; Q
    defb $0E, $11, $11, $11, $15, $12, $0D
    ; R
    defb $1E, $11, $11, $1E, $14, $12, $11
    ; S
    defb $0E, $11, $10, $0E, $01, $11, $0E
    ; T
    defb $1F, $04, $04, $04, $04, $04, $04
    ; U
    defb $11, $11, $11, $11, $11, $11, $0E
    ; V
    defb $11, $11, $11, $11, $0A, $0A, $04
    ; W
    defb $11, $11, $11, $15, $15, $15, $0A
    ; X
    defb $11, $11, $0A, $04, $0A, $11, $11
    ; Y
    defb $11, $11, $0A, $04, $04, $04, $04
    ; Z
    defb $1F, $01, $02, $04, $08, $10, $1F
    ; 0
    defb $0E, $11, $13, $15, $19, $11, $0E
    ; 1
    defb $04, $0C, $04, $04, $04, $04, $0E
    ; 2
    defb $0E, $11, $01, $06, $08, $10, $1F
    ; 3
    defb $0E, $11, $01, $06, $01, $11, $0E
    ; 4
    defb $02, $06, $0A, $12, $1F, $02, $02
    ; 5
    defb $1F, $10, $1E, $01, $01, $11, $0E
    ; 6
    defb $06, $08, $10, $1E, $11, $11, $0E
    ; 7
    defb $1F, $01, $02, $04, $08, $08, $08
    ; 8
    defb $0E, $11, $11, $0E, $11, $11, $0E
    ; 9
    defb $0E, $11, $11, $0F, $01, $02, $0C
    ; space
    defb $00, $00, $00, $00, $00, $00, $00
    ; !
    defb $04, $04, $04, $04, $04, $00, $04
    ; .
    defb $00, $00, $00, $00, $00, $00, $04
    ; :
    defb $00, $04, $00, $00, $00, $04, $00
    ; -
    defb $00, $00, $00, $0E, $00, $00, $00

; ============================================================
; ASCII-to-glyph lookup table (128 entries)
; ============================================================
_char_map:
    ; 0x00-0x0F
    defb 36,36,36,36,36,36,36,36, 36,36,36,36,36,36,36,36
    ; 0x10-0x1F
    defb 36,36,36,36,36,36,36,36, 36,36,36,36,36,36,36,36
    ; 0x20-0x2F: ' ' ! " # $ % & ' ( ) * + , - . /
    defb 36,37,36,36,36,36,36,36, 36,36,36,36,36,40,38,36
    ; 0x30-0x3F: 0-9 : ; < = > ?
    defb 26,27,28,29,30,31,32,33, 34,35,39,36,36,36,36,36
    ; 0x40-0x4F: @ A-O
    defb 36, 0, 1, 2, 3, 4, 5, 6,  7, 8, 9,10,11,12,13,14
    ; 0x50-0x5F: P-Z
    defb 15,16,17,18,19,20,21,22, 23,24,25,36,36,36,36,36
    ; 0x60-0x6F: ` a-o (lowercase = uppercase)
    defb 36, 0, 1, 2, 3, 4, 5, 6,  7, 8, 9,10,11,12,13,14
    ; 0x70-0x7F: p-z
    defb 15,16,17,18,19,20,21,22, 23,24,25,36,36,36,36,36

; ============================================================
; vid_draw_text_gfx — fast text rendering
; ============================================================

    SECTION bss_graphics
_txt_strbuf:   defs 32
    SECTION code_graphics

_vid_draw_text_gfx:
    push    ix

    ; Copy string to high-memory buffer (max 31 chars + null)
    ld      hl, (_txt_str)
    ld      de, _txt_strbuf
    ld      b, 31
txt_strcpy:
    ld      a, (hl)
    ld      (de), a
    or      a
    jr      z, txt_strcpy_done
    inc     hl
    inc     de
    djnz    txt_strcpy
    xor     a
    ld      (de), a
txt_strcpy_done:

    ; Prepare colour byte
    ld      a, (_txt_colour)
    and     $0F
    ld      b, a
    rlca
    rlca
    rlca
    rlca
    or      b
    ld      (_txt_colbyte), a

    call    swapgfxbk

    ld      ix, _txt_strbuf

; Character loop
txt_char_loop:
    ld      a, (ix+0)
    or      a
    jp      z, txt_done
    inc     ix

    ; Map ASCII to glyph index
    and     $7F
    ld      e, a
    ld      d, 0
    ld      hl, _char_map
    add     hl, de
    ld      a, (hl)

    ; Compute glyph pointer: _font_gfx + index * 7
    ld      l, a
    ld      h, 0
    add     hl, hl
    add     hl, hl
    add     hl, hl
    ld      e, a
    ld      d, 0
    or      a
    sbc     hl, de
    ld      de, _font_gfx
    add     hl, de
    ld      (_txt_glyph), hl

    ; Compute VRAM address: y * 128 + x / 2
    ld      hl, (_txt_y)
    ld      a, l
    srl     a
    ld      h, a
    ld      a, (_txt_y)
    rrca
    and     $80
    ld      l, a

    ld      a, (_txt_x)
    srl     a
    add     a, l
    ld      l, a
    jr      nc, txt_noc1
    inc     h
txt_noc1:
    ; HL = VRAM address

    ld      b, 7            ; 7 rows

    ld      a, (_txt_x)
    and     $01
    ld      c, a            ; C = alignment

    ld      de, (_txt_glyph)

txt_row_loop:
    push    bc
    push    hl

    ld      a, (de)
    inc     de
    push    de

    ld      b, a
    ld      a, c
    or      a
    jr      nz, txt_row_odd

    ; EVEN alignment
    ; Byte 0: pixel 0 (bit4) -> low nibble, pixel 1 (bit3) -> high nibble
    ld      a, (hl)
    and     $F0

    bit     4, b
    jr      z, txt_e0_bg
    ld      c, a
    ld      a, (_txt_colbyte)
    and     $0F
    or      c
    jr      txt_e0_done
txt_e0_bg:
txt_e0_done:
    ld      c, a

    bit     3, b
    jr      z, txt_e1_bg
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      txt_e1_done
txt_e1_bg:
    ld      a, c
txt_e1_done:
    ld      (hl), a
    inc     hl

    ; Byte 1: pixel 2 (bit2) -> low, pixel 3 (bit1) -> high
    ld      a, 0

    bit     2, b
    jr      z, txt_e2_bg
    ld      a, (_txt_colbyte)
    and     $0F
txt_e2_bg:
    ld      c, a

    bit     1, b
    jr      z, txt_e3_bg
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      txt_e3_done
txt_e3_bg:
    ld      a, c
txt_e3_done:
    ld      (hl), a
    inc     hl

    ; Byte 2: pixel 4 (bit0) -> low nibble, preserve high nibble
    ld      a, (hl)
    and     $F0

    bit     0, b
    jr      z, txt_e4_bg
    ld      c, a
    ld      a, (_txt_colbyte)
    and     $0F
    or      c
    jr      txt_e4_done
txt_e4_bg:
txt_e4_done:
    ld      (hl), a

    jr      txt_row_end

    ; ODD alignment
txt_row_odd:
    ; Byte 0: preserve low nibble, pixel 0 -> high nibble
    ld      a, (hl)
    and     $0F

    bit     4, b
    jr      z, txt_o0_bg
    ld      c, a
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      txt_o0_done
txt_o0_bg:
txt_o0_done:
    ld      (hl), a
    inc     hl

    ; Byte 1: pixel 1 (bit3) -> low, pixel 2 (bit2) -> high
    ld      a, 0

    bit     3, b
    jr      z, txt_o1_bg
    ld      a, (_txt_colbyte)
    and     $0F
txt_o1_bg:
    ld      c, a

    bit     2, b
    jr      z, txt_o2_bg
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      txt_o2_done
txt_o2_bg:
    ld      a, c
txt_o2_done:
    ld      (hl), a
    inc     hl

    ; Byte 2: pixel 3 (bit1) -> low, pixel 4 (bit0) -> high
    ld      a, 0

    bit     1, b
    jr      z, txt_o3_bg
    ld      a, (_txt_colbyte)
    and     $0F
txt_o3_bg:
    ld      c, a

    bit     0, b
    jr      z, txt_o4_bg
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      txt_o4_done
txt_o4_bg:
    ld      a, c
txt_o4_done:
    ld      (hl), a

txt_row_end:
    pop     de
    pop     hl
    pop     bc

    ; Next scanline (+128)
    ld      a, l
    add     a, 128
    ld      l, a
    jr      nc, txt_row_nc
    inc     h
txt_row_nc:
    dec     b
    jp      nz, txt_row_loop

    ; Advance x by 6 (FONT_W + FONT_SPACING)
    ld      hl, (_txt_x)
    ld      de, 6
    add     hl, de
    ld      (_txt_x), hl

    jp      txt_char_loop

txt_done:
    call    swapgfxbk1
    pop     ix
    ret

; ============================================================
; vid_draw_text_rotcw_gfx — draw string rotated 90° clockwise
; Text runs downward, each glyph rotated CW.
; ============================================================
_vid_draw_text_rotcw_gfx:
    push    ix

    ; Copy string to high-memory buffer
    ld      hl, (_txt_str)
    ld      de, _txt_strbuf
    ld      b, 31
rot_strcpy:
    ld      a, (hl)
    ld      (de), a
    or      a
    jr      z, rot_strcpy_done
    inc     hl
    inc     de
    djnz    rot_strcpy
    xor     a
    ld      (de), a
rot_strcpy_done:

    ; Prepare colour byte (both nibbles)
    ld      a, (_txt_colour)
    and     $0F
    ld      b, a
    rlca
    rlca
    rlca
    rlca
    or      b
    ld      (_txt_colbyte), a

    call    swapgfxbk

    ld      ix, _txt_strbuf

rot_char_loop:
    ld      a, (ix+0)
    or      a
    jp      z, rot_done
    inc     ix

    ; Look up glyph
    and     $7F
    ld      e, a
    ld      d, 0
    ld      hl, _char_map
    add     hl, de
    ld      a, (hl)

    ; glyph ptr = _font_gfx + index * 7
    ld      l, a
    ld      h, 0
    add     hl, hl
    add     hl, hl
    add     hl, hl
    ld      e, a
    ld      d, 0
    or      a
    sbc     hl, de
    ld      de, _font_gfx
    add     hl, de
    ld      (_txt_glyph), hl

    ; Transpose 5x7 -> 7x5 (90 CW)
    ld      c, 5
    ld      b, $10          ; bit mask for col 4 (ny=0)
    ld      hl, _txt_rot
rot_transpose_row:
    push    hl
    push    bc
    ld      hl, (_txt_glyph)
    ld      de, 6
    add     hl, de
    ex      de, hl
    ld      c, 7
    ld      a, 0
rot_transpose_col:
    sla     a
    push    af
    ld      a, (de)
    dec     de
    and     b
    jr      z, rot_t_zero
    pop     af
    or      $01
    jr      rot_t_next
rot_t_zero:
    pop     af
rot_t_next:
    dec     c
    jr      nz, rot_transpose_col

    pop     bc
    pop     hl
    ld      (hl), a
    inc     hl
    srl     b
    dec     c
    jr      nz, rot_transpose_row

    ; Compute VRAM address: y * 128 + x / 2
    ld      hl, (_txt_y)
    ld      a, l
    srl     a
    ld      h, a
    ld      a, (_txt_y)
    rrca
    and     $80
    ld      l, a

    ld      a, (_txt_x)
    srl     a
    add     a, l
    ld      l, a
    jr      nc, rot_noc1
    inc     h
rot_noc1:

    ; Draw 5 rows of 7 pixels
    ld      de, _txt_rot
    ld      b, 5

    ld      a, (_txt_x)
    and     $01
    ld      c, a

rot_row_loop:
    push    bc
    push    hl

    ld      a, (de)
    inc     de
    push    de

    ld      b, a
    ld      a, c
    or      a
    jr      nz, rot_row_odd

    ; EVEN x alignment
    ld      a, 0
    bit     6, b
    jr      z, rot_e0s
    ld      a, (_txt_colbyte)
    and     $0F
rot_e0s:
    ld      c, a
    bit     5, b
    jr      z, rot_e1s
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      rot_e1d
rot_e1s:
    ld      a, c
rot_e1d:
    ld      (hl), a
    inc     hl

    ld      a, 0
    bit     4, b
    jr      z, rot_e2s
    ld      a, (_txt_colbyte)
    and     $0F
rot_e2s:
    ld      c, a
    bit     3, b
    jr      z, rot_e3s
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      rot_e3d
rot_e3s:
    ld      a, c
rot_e3d:
    ld      (hl), a
    inc     hl

    ld      a, 0
    bit     2, b
    jr      z, rot_e4s
    ld      a, (_txt_colbyte)
    and     $0F
rot_e4s:
    ld      c, a
    bit     1, b
    jr      z, rot_e5s
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      rot_e5d
rot_e5s:
    ld      a, c
rot_e5d:
    ld      (hl), a
    inc     hl

    ld      a, (hl)
    and     $F0
    bit     0, b
    jr      z, rot_e6d
    ld      c, a
    ld      a, (_txt_colbyte)
    and     $0F
    or      c
rot_e6d:
    ld      (hl), a
    jr      rot_row_end

    ; ODD x alignment
rot_row_odd:
    ld      a, (hl)
    and     $0F
    bit     6, b
    jr      z, rot_o0d
    ld      c, a
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
rot_o0d:
    ld      (hl), a
    inc     hl

    ld      a, 0
    bit     5, b
    jr      z, rot_o1s
    ld      a, (_txt_colbyte)
    and     $0F
rot_o1s:
    ld      c, a
    bit     4, b
    jr      z, rot_o2s
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      rot_o2d
rot_o2s:
    ld      a, c
rot_o2d:
    ld      (hl), a
    inc     hl

    ld      a, 0
    bit     3, b
    jr      z, rot_o3s
    ld      a, (_txt_colbyte)
    and     $0F
rot_o3s:
    ld      c, a
    bit     2, b
    jr      z, rot_o4s
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      rot_o4d
rot_o4s:
    ld      a, c
rot_o4d:
    ld      (hl), a
    inc     hl

    ld      a, 0
    bit     1, b
    jr      z, rot_o5s
    ld      a, (_txt_colbyte)
    and     $0F
rot_o5s:
    ld      c, a
    bit     0, b
    jr      z, rot_o6s
    ld      a, (_txt_colbyte)
    and     $F0
    or      c
    jr      rot_o6d
rot_o6s:
    ld      a, c
rot_o6d:
    ld      (hl), a

rot_row_end:
    pop     de
    pop     hl
    pop     bc

    ; Advance to next scanline (+128)
    ld      a, l
    add     a, 128
    ld      l, a
    jr      nc, rot_row_nc
    inc     h
rot_row_nc:
    dec     b
    jp      nz, rot_row_loop

    ; Advance _txt_y by 6
    ld      hl, (_txt_y)
    ld      de, 6
    add     hl, de
    ld      (_txt_y), hl

    jp      rot_char_loop

rot_done:
    call    swapgfxbk1
    pop     ix
    ret
