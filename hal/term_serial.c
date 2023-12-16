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
#define TERM_SER_BAUD               9600UL
#endif

void serterm_putc(term_hook_t* impl, char c) {
#ifndef TERM_SER_NO_CRNL
    // if(c == '\r') return; // ignore CR (since we'll be converting NL to CR+NL anyway)
    if(c == '\n') ser_putc(TERM_SER_PORT, '\r'); // add CR to NL
#endif
    ser_putc(TERM_SER_PORT, c);
}

#ifndef TERM_NO_INPUT
size_t serterm_available(term_hook_t* impl) {
    return (ser_avail_write(TERM_SER_PORT)) ? 1 : 0;
}

char serterm_getc(term_hook_t* impl) {
    return ser_getc(TERM_SER_PORT);
}
#endif

#ifndef TERM_NO_CLEAR
void serterm_clear(term_hook_t* impl) {
    ser_puts(TERM_SER_PORT, "\x1B[2J\x1B[H"); // clear entire screen, then move back to home position (ANSI)
}
#endif

#ifndef TERM_NO_XY
void serterm_get_dimensions(term_hook_t* impl, size_t* width, size_t* height) {
    size_t x, y; // for saving the current cursor position
    serterm_get_xy(impl, &x, &y);
    ser_puts(TERM_SER_PORT, "\x1B[1000;1000H"); // move the cursor to some outrageous position
    serterm_get_xy(impl, width, height); // then retrieve the bottom right corner's coordinates
    (*width)++; (*height)++;
    char buf[16]; ksprintf(buf, "\x1B[%u;%uH", y, x); ser_puts(TERM_SER_PORT, buf); // restore cursor position
}

void serterm_set_xy(term_hook_t* impl, size_t x, size_t y) {
    char buf[16]; ksprintf(buf, "\x1B[%u;%uH", y, x); ser_puts(TERM_SER_PORT, buf); // we already have exclusive access
}

void serterm_get_xy(term_hook_t* impl, size_t* x, size_t* y) {
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

#ifndef TERM_NO_COLOR
static size_t serterm_fgcolor = 7, serterm_bgcolor = 0; // light gray on black
bool serterm_setfg_idx(term_hook_t* impl, size_t idx) {
    char buf[16];
    if(idx < 16) ksprintf(buf, "\x1B[%um", (idx < 8) ? (idx + 30) : (idx - 8 + 90)); // use 4-bit color for maximum compatibility
    else ksprintf(buf, "\x1B[38;5;%um", idx);
    ser_puts(TERM_SER_PORT, buf);
    serterm_fgcolor = idx;
    return true;
}

bool serterm_setbg_idx(term_hook_t* impl, size_t idx) {
    char buf[16];
    if(idx < 16) ksprintf(buf, "\x1B[%um", (idx < 8) ? (idx + 40) : (idx - 8 + 100)); // use 4-bit color for maximum compatibility
    else ksprintf(buf, "\x1B[48;5;%um", idx);
    ser_puts(TERM_SER_PORT, buf);
    serterm_bgcolor = idx;
    return true;
}

size_t serterm_getfg_idx(term_hook_t* impl) {
    return serterm_fgcolor;
}

size_t serterm_getbg_idx(term_hook_t* impl) {
    return serterm_bgcolor;
}
#endif

term_hook_t serterm_hook = {
    &serterm_putc,
    NULL,
#ifndef TERM_NO_INPUT
    &serterm_available,
    &serterm_getc,
#else
    NULL,
    NULL,
#endif

#ifndef TERM_NO_CLEAR
    &serterm_clear,
#else
    NULL,
#endif

#ifndef TERM_NO_XY
    &serterm_get_dimensions,
    &serterm_set_xy,
    &serterm_get_xy,
#else
    NULL,
    NULL,
    NULL,
#endif

#ifndef TERM_NO_COLOR
    &serterm_setfg_idx,
    &serterm_getfg_idx,
    &serterm_setbg_idx,
    &serterm_getbg_idx,
#else
    NULL,
    NULL,
    NULL,
    NULL,
#endif

    NULL,
    NULL,
    NULL,
    NULL,

    {0},
    {0},
    
    NULL
};

void term_init() {
    ser_init(TERM_SER_PORT, TERM_SER_DBIT, TERM_SER_SBIT, TERM_SER_PARITY, TERM_SER_BAUD);
#ifndef TERM_NO_CLEAR // to save space and time
    serterm_clear(NULL);
#endif
#ifndef TERM_NO_COLOR
    serterm_setfg_idx(NULL, serterm_fgcolor);
    serterm_setbg_idx(NULL, serterm_bgcolor);
#endif

    term_impl = &serterm_hook;
}

#endif