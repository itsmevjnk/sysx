#include <hal/terminal.h>

term_hook_t* term_impl = NULL;

void term_putc(char c) {
    if(term_impl != NULL && term_impl->putc != NULL) {
        mutex_acquire(&term_impl->mutex_out);
        term_impl->putc(term_impl, c);
        mutex_release(&term_impl->mutex_out);
    }
}

size_t term_available() {
    if(term_impl != NULL && term_impl->available != NULL) {
        mutex_acquire(&term_impl->mutex_in);
        size_t ret = term_impl->available(term_impl);
        mutex_release(&term_impl->mutex_in);
        return ret;
    }
    return 1; // so that the getc implementation can handle waiting by itself
}

char term_getc_noecho() {
    if(term_impl == NULL || term_impl->getc == NULL) return 0;
    if(term_impl->available != NULL) {
        mutex_acquire(&term_impl->mutex_in);
        while(term_impl->available(term_impl) == 0);
        mutex_release(&term_impl->mutex_in);
    }
    mutex_acquire(&term_impl->mutex_in);
    char c = term_impl->getc(term_impl);
    mutex_release(&term_impl->mutex_in);
    return c;
}

void term_clear() {
    if(term_impl != NULL && term_impl->clear != NULL) {
        mutex_acquire(&term_impl->mutex_out);
        term_impl->clear(term_impl);
        mutex_release(&term_impl->mutex_out);
    }
}

void term_get_dimensions(size_t* width, size_t* height) {
    if(term_impl != NULL && term_impl->get_dimensions != NULL) {
        mutex_acquire(&term_impl->mutex_out);
        term_impl->get_dimensions(term_impl, width, height);
        mutex_release(&term_impl->mutex_out);
    }
}

void term_set_xy(size_t x, size_t y) {
    if(term_impl != NULL && term_impl->set_xy != NULL) {
        mutex_acquire(&term_impl->mutex_out);
        term_impl->set_xy(term_impl, x, y);
        mutex_release(&term_impl->mutex_out);
    }
}

void term_get_xy(size_t* x, size_t* y) {
    if(term_impl != NULL && term_impl->get_xy != NULL) {
        mutex_acquire(&term_impl->mutex_out);
        term_impl->get_xy(term_impl, x, y);
        mutex_release(&term_impl->mutex_out);
    }
}

void term_puts(const char* s) {
    if(term_impl != NULL) {
        mutex_acquire(&term_impl->mutex_out);
        if(term_impl->puts != NULL) term_impl->puts(term_impl, s);
        else if(term_impl->putc != NULL) {
            for(size_t i = 0; s[i] != 0; i++) term_impl->putc(term_impl, s[i]);
        }
        mutex_release(&term_impl->mutex_out);
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