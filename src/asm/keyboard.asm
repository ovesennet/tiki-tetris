; TIKI TETRIS - keyboard.asm
; Direct keyboard matrix scanning via port $00.
;
; TIKI-100 keyboard: 8 rows x 12 columns on port $00.
;   - Write to port $00 to reset scan to column 1
;   - Read port $00 returns one column per read (columns 1-12)
;   - Bit SET = key NOT pressed, bit CLEAR = key pressed
;
; Key positions from hardware docs:
;   'a'     = col 2, bit 3   (left)
;   'd'     = col 3, bit 6   (right)
;   'w'     = col 3, bit 1   (rotate)
;   's'     = col 3, bit 2   (soft drop)
;   space   = col 1, bit 4   (hard drop / MLMROM)
;   'q'     = col 2, bit 6   (quit)
;   'p'     = col 7, bit 1   (pause)
;   'o'     = col 6, bit 5   (sound)

    SECTION code_driver

    PUBLIC  _kbd_init
    PUBLIC  _kbd_shutdown
    PUBLIC  _kbd_scan

; Result bitmask bits
KBIT_LEFT   equ 0       ; 'a'
KBIT_RIGHT  equ 1       ; 'd'
KBIT_ROT    equ 2       ; 'w'
KBIT_DOWN   equ 3       ; 's'
KBIT_DROP   equ 4       ; space
KBIT_QUIT   equ 5       ; 'q'
KBIT_PAUSE  equ 6       ; 'p'
KBIT_SOUND  equ 7       ; 'o'

_kbd_init:
    ret

_kbd_shutdown:
    ret

; ------------------------------------------------
; uint8_t kbd_scan(void)
; Returns bitmask of currently held keys
; ------------------------------------------------
_kbd_scan:
    ld      e, 0            ; result bitmask
    ld      d, 0            ; anykey accumulator

    ; Reset scan to column 1
    out     ($00), a

    ; --- Column 1 ---
    in      a, ($00)
    ld      c, a
    cpl
    or      d
    ld      d, a
    bit     4, c            ; space: bit 4, 0=pressed
    jr      nz, ks_no_spc
    set     KBIT_DROP, e
ks_no_spc:

    ; --- Column 2 ---
    in      a, ($00)
    ld      c, a
    ld      a, c
    cpl
    or      d
    ld      d, a
    bit     3, c            ; 'a': bit 3
    jr      nz, ks_no_a
    set     KBIT_LEFT, e
ks_no_a:
    bit     6, c            ; 'q': bit 6
    jr      nz, ks_no_q
    set     KBIT_QUIT, e
ks_no_q:

    ; --- Column 3 ---
    in      a, ($00)
    ld      c, a
    ld      a, c
    cpl
    or      d
    ld      d, a
    bit     6, c            ; 'd': bit 6
    jr      nz, ks_no_d
    set     KBIT_RIGHT, e
ks_no_d:
    bit     1, c            ; 'w': bit 1
    jr      nz, ks_no_w
    set     KBIT_ROT, e
ks_no_w:
    bit     2, c            ; 's': bit 2
    jr      nz, ks_no_s
    set     KBIT_DOWN, e
ks_no_s:

    ; --- Columns 4-5 (skip, accumulate any) ---
    in      a, ($00)        ; col 4
    cpl
    or      d
    ld      d, a

    in      a, ($00)        ; col 5
    cpl
    or      d
    ld      d, a

    ; --- Column 6 ---
    in      a, ($00)
    ld      c, a
    ld      a, c
    cpl
    or      d
    ld      d, a
    bit     5, c            ; 'o': bit 5
    jr      nz, ks_no_o
    set     KBIT_SOUND, e
ks_no_o:

    ; --- Column 7 ---
    in      a, ($00)
    ld      c, a
    ld      a, c
    cpl
    or      d
    ld      d, a
    bit     1, c            ; 'p': bit 1
    jr      nz, ks_no_p
    set     KBIT_PAUSE, e
ks_no_p:

    ; --- Columns 8-12 (accumulate any) ---
    in      a, ($00)        ; col 8
    cpl
    or      d
    ld      d, a

    in      a, ($00)        ; col 9
    cpl
    or      d
    ld      d, a

    in      a, ($00)        ; col 10
    cpl
    or      d
    ld      d, a

    in      a, ($00)        ; col 11
    cpl
    or      d
    ld      d, a

    in      a, ($00)        ; col 12
    cpl
    or      d
    ld      d, a

    ld      l, e
    ret
