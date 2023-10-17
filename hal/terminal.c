#include <hal/terminal.h>

const term_hook_t* term_impl = NULL;

void term_putc(char c) {
    if(term_impl != NULL && term_impl->putc != NULL) term_impl->putc(term_impl, c);
}

size_t term_available() {
    if(term_impl != NULL && term_impl->available != NULL) return term_impl->available(term_impl);
    return 1; // so that the getc implementation can handle waiting by itself
}

char term_getc_noecho() {
    if(term_impl == NULL || term_impl->getc == NULL) return 0;
    if(term_impl->available != NULL) {
        while(term_impl->available(term_impl) == 0);
    }
    return term_impl->getc(term_impl);
}

void term_clear() {
    if(term_impl != NULL && term_impl->clear != NULL) term_impl->clear(term_impl);
}

void term_get_dimensions(size_t* width, size_t* height) {
    if(term_impl != NULL && term_impl->get_dimensions != NULL) term_impl->get_dimensions(term_impl, width, height);
}

void term_set_xy(size_t x, size_t y) {
    if(term_impl != NULL && term_impl->set_xy != NULL) term_impl->set_xy(term_impl, x, y);
}

void term_get_xy(size_t* x, size_t* y) {
    if(term_impl != NULL && term_impl->get_xy != NULL) term_impl->get_xy(term_impl, x, y);
}

void term_puts(const char* s) {
    if(term_impl != NULL) {
        if(term_impl->puts != NULL) term_impl->puts(term_impl, s);
        else if(term_impl->putc != NULL) {
            for(size_t i = 0; s[i] != 0; i++) term_impl->putc(term_impl, s[i]);
        }
    }
    
}

char term_getc() {
    if(term_impl == NULL || term_impl->getc == NULL) return 0;
    char c = term_getc_noecho();
    term_putc(c);
    return c;
}

void term_gets_noecho(char* s) {
    if(term_impl == NULL || term_impl->getc == NULL) return;
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
    if(term_impl == NULL || term_impl->getc == NULL) return;
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