/* TIKI TETRIS — hello.c
 * Phase 1 test: prove the z88dk toolchain and CP/M output work.
 */

#include <stdio.h>
#include <conio.h>

void main(void)
{
    printf("TIKI TETRIS v0.1\r\n");
    printf("Hello from z88dk + C!\r\n");
    printf("Press any key to exit...\r\n");
    while (!kbhit()) ;
    getch();
}
