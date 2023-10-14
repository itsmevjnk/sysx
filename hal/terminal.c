#include <hal/terminal.h>

#ifdef TERM_NO_CLEAR
void term_clear() {
    // do nothing
}
#endif

void term_puts(const char* s) {
    for(size_t i = 0; s[i] != 0; i++) term_putc(s[i]);
}

#ifndef TERM_NO_INPUT
char term_getc() {
    char c = term_getc_noecho();
    term_putc(c);
    return c;
}

void term_gets_noecho(char* s) {
    size_t i = 0;
    while(1) {
        char c = term_getc_noecho();
        switch(c) {
            case TERM_LINE_TERMINATION:
                s[i] = '\0';
                return;
            case '\b':
                if(i > 0) i--;
                break;
            default:
                s[i++] = c;
                break;
        }
    }
}

void term_gets(char* s) {
    size_t i = 0;
    while(1) {
        char c = term_getc_noecho();
        switch(c) {
            case TERM_LINE_TERMINATION:
                s[i] = '\0';
                term_puts("\r\n");
                return;
            case '\b':
                if(i > 0) i--;
                term_puts("\b \b"); // backspace, space, then backspace to clear character
                break;
            default:
                s[i++] = c;
                term_putc(c);
                break;
        }
    }
}
#endif