#include <hal/terminal.h>
#include <hal/serial.h>

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

#endif