#include <hal/terminal.h>
#include <hal/serial.h>
#include <stdio.h>

#ifdef TERM_SER

#ifdef NO_SERIAL
#error "Serial is disabled through the NO_SERIAL macro!"
#endif

#ifndef TERM_SER_PORT
#define TERM_SER_PORT               0
#endif

#ifndef TERM_SER_DBIT
#define TERM_SER_DBIT               8
#endif

#ifndef TERN_SER_SBIT
#define TERM_SER_SBIT               1
#endif

#ifndef TERM_SER_PARITY
#define TERM_SER_PARITY             SER_PARITY_NONE
#endif

#ifndef TERM_SER_BAUD
#define TERM_SER_BAUD               115200UL
#endif

void term_init() {
    ser_init(TERM_SER_PORT, TERM_SER_DBIT, TERM_SER_SBIT, TERM_SER_PARITY, TERM_SER_BAUD);
#ifndef TERM_NO_CLEAR // to save space and time
    term_clear();
#endif
}

void term_putc(char c) {
    ser_putc(TERM_SER_PORT, c);
}

#ifndef TERM_NO_INPUT
char term_getc_noecho() {
    return ser_getc(TERM_SER_PORT);
}
#endif

#ifndef TERM_NO_CLEAR
void term_clear() {
    ser_puts(TERM_SER_PORT, "\x1B[2J\x1B[H"); // clear entire screen, then move back to home position (ANSI)
}
#endif

#ifndef TERM_NO_XY
void term_get_dimensions(size_t* width, size_t* height) {
    size_t x, y; // for saving the current cursor position
    term_get_xy(&x, &y);
    term_set_xy(1000, 1000); // move the cursor to some outrageous position
    term_get_xy(width, height); // then retrieve the bottom right corner's coordinates
    (*width)++; (*height)++;
    term_set_xy(x, y); // restore cursor position
}

void term_set_xy(size_t x, size_t y) {
    kprintf("\x1B[%u;%uH", y, x); // since kstdout is set to the terminal, we can use kprintf normally
}

void term_get_xy(size_t* x, size_t* y) {
    ser_puts(TERM_SER_PORT, "\x1B[6n"); // Device Status Report: returns cursor position
    
    *x = 0; *y = 0; // so we can start writing the results straight into them
    
    uint8_t state = 0; // 0 - waiting for ESC, 1 - waiting for [, 2 - reading y, 3 - reading x
    while(1) {
        /* begin reading results */
        char c = ser_getc(TERM_SER_PORT); // get next character from serial
        switch(state) {
            case 0:
                if(c == '\x1B') state++; // we've got the ESC
                break;
            case 1:
                if(c == '[') state++; // we've got the [
                break;
            case 2:
                if(c == ';') state++; // separator character
                else *y = *y * 10 + (c - '0');
                break;
            case 3:
                if(c == 'R') return; // we're done at this point
                else *x = *x * 10 + (c - '0');
                break;
            default:
                break;
        }
    }
}
#endif

#endif