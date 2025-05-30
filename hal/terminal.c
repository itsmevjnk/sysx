#include <hal/terminal.h>
#include <hal/intr.h>

term_hook_t* term_impl = NULL;

/* output guard functions */

/*
 * static bool term_outguard_start()
 *  Start guarding the output operation, which includes saving the
 *  interrupt state and disabling interrupt (if term_impl->out_irq
 *  is not set), followed by acquiring term_impl->mutex_out.
 *  This function returns the saved interrupt state (or false if
 *  term_impl->out_irq is set). This should be passed onto
 *  term_outguard_end() at the end of the guarded section.
 */
static bool term_outguard_start() {
    bool irq = false;
    if(!term_impl->out_irq) {
        irq = intr_test();
        intr_disable();
    }
    mutex_acquire(&term_impl->mutex_out);
    return irq;
}

/*
 * static void term_outguard_end(bool enable_irq)
 *  End guarding the output operaton, which includes releasing the 
 *  term_impl->mutex_out mutex, followed by enabling interrupts if
 *  enable_irq (returned from term_outguard_start()) is set.
 *  Note that term_outguard_start() will always return false if
 *  term_impl->out_irq is set.
 */
static void term_outguard_end(bool enable_irq) {
    mutex_release(&term_impl->mutex_out);
    if(enable_irq) intr_enable();
}

void term_putc(char c) {
    if(term_impl && term_impl->putc) {
        bool irq = term_outguard_start();
        term_impl->putc(term_impl, c);
        term_outguard_end(irq);
    }
}

size_t term_available() {
    if(term_impl && term_impl->available) {
        mutex_acquire(&term_impl->mutex_in);
        size_t ret = term_impl->available(term_impl);
        mutex_release(&term_impl->mutex_in);
        return ret;
    }
    return 1; // so that the getc implementation can handle waiting by itself
}

char term_getc_noecho() {
    if(!term_impl || !term_impl->getc) return 0;
    mutex_acquire(&term_impl->mutex_in); // acquire terminal for this character
    if(term_impl->available) {
        while(!term_impl->available(term_impl));
    }
    char c = term_impl->getc(term_impl);
    mutex_release(&term_impl->mutex_in);
    return c;
}

void term_clear() {
    if(term_impl && term_impl->clear) {
        bool irq = term_outguard_start();
        term_impl->clear(term_impl);
        term_outguard_end(irq);
    }
}

void term_get_dimensions(size_t* width, size_t* height) {
    if(term_impl && term_impl->get_dimensions) {
        bool irq = term_outguard_start();
        term_impl->get_dimensions(term_impl, width, height);
        term_outguard_end(irq);
    }
}

void term_set_xy(size_t x, size_t y) {
    if(term_impl && term_impl->set_xy) {
        bool irq = term_outguard_start();
        term_impl->set_xy(term_impl, x, y);
        term_outguard_end(irq);
    }
}

void term_get_xy(size_t* x, size_t* y) {
    if(term_impl && term_impl->get_xy) {
        bool irq = term_outguard_start();
        term_impl->get_xy(term_impl, x, y);
        term_outguard_end(irq);
    }
}

void term_puts(const char* s) {
    if(term_impl) {
        bool irq = term_outguard_start();
        if(term_impl->puts) term_impl->puts(term_impl, s);
        else if(term_impl->putc) {
            for(size_t i = 0; s[i]; i++) term_impl->putc(term_impl, s[i]);
        }
        term_outguard_end(irq);
    }
    
}

char term_getc() {
    if(!term_impl || !term_impl->getc) return 0;
    char c = term_getc_noecho();
    term_putc(c);
    return c;
}

void term_gets_noecho(char* s) {
    if(!term_impl || !term_impl->getc) return;
    size_t i = 0;
    while(1) {
        char c = term_getc_noecho();
        switch(c) {
            case TERM_LINE_TERMINATION:
                s[i] = '\0';
                return;
            case '\b':
                if(i) i--;
                break;
            default:
                s[i++] = c;
                break;
        }
    }
}

void term_gets(char* s) {
    if(!term_impl || !term_impl->getc) return;
    size_t i = 0;
    while(1) {
        char c = term_getc_noecho();
        switch(c) {
            case TERM_LINE_TERMINATION:
                s[i] = '\0';
                term_puts("\r\n");
                return;
            case '\b':
                if(i) i--;
                term_puts("\b \b"); // backspace, space, then backspace to clear character
                break;
            default:
                s[i++] = c;
                term_putc(c);
                break;
        }
    }
}

#ifndef TERM_NO_IDXCOLOR_EMULATION
static const uint32_t term_color_palette[] = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xC0C0C0,
    0x808080, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF,
    0x000000, 0x00002A, 0x000054, 0x00007E, 0x0000A8, 0x0000D2, 0x002A00, 0x002A2A, 
    0x002A54, 0x002A7E, 0x002AA8, 0x002AD2, 0x005400, 0x00542A, 0x005454, 0x00547E, 
    0x0054A8, 0x0054D2, 0x007E00, 0x007E2A, 0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 
    0x00A800, 0x00A82A, 0x00A854, 0x00A87E, 0x00A8A8, 0x00A8D2, 0x00D200, 0x00D22A, 
    0x00D254, 0x00D27E, 0x00D2A8, 0x00D2D2, 0x002A00, 0x002A2A, 0x002A54, 0x002A7E, 
    0x002AA8, 0x002AD2, 0x002A00, 0x002A2A, 0x002A54, 0x002A7E, 0x002AA8, 0x002AD2, 
    0x007E00, 0x007E2A, 0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 0x007E00, 0x007E2A, 
    0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 0x00AA00, 0x00AA2A, 0x00AA54, 0x00AA7E, 
    0x00AAA8, 0x00AAD2, 0x00FA00, 0x00FA2A, 0x00FA54, 0x00FA7E, 0x00FAA8, 0x00FAD2, 
    0x005400, 0x00542A, 0x005454, 0x00547E, 0x0054A8, 0x0054D2, 0x007E00, 0x007E2A, 
    0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 0x005400, 0x00542A, 0x005454, 0x00547E, 
    0x0054A8, 0x0054D2, 0x007E00, 0x007E2A, 0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 
    0x00FC00, 0x00FC2A, 0x00FC54, 0x00FC7E, 0x00FCA8, 0x00FCD2, 0x00D600, 0x00D62A, 
    0x00D654, 0x00D67E, 0x00D6A8, 0x00D6D2, 0x007E00, 0x007E2A, 0x007E54, 0x007E7E, 
    0x007EA8, 0x007ED2, 0x007E00, 0x007E2A, 0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 
    0x007E00, 0x007E2A, 0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 0x007E00, 0x007E2A, 
    0x007E54, 0x007E7E, 0x007EA8, 0x007ED2, 0x00FE00, 0x00FE2A, 0x00FE54, 0x00FE7E, 
    0x00FEA8, 0x00FED2, 0x00FE00, 0x00FE2A, 0x00FE54, 0x00FE7E, 0x00FEA8, 0x00FED2, 
    0x00A800, 0x00A82A, 0x00A854, 0x00A87E, 0x00A8A8, 0x00A8D2, 0x00AA00, 0x00AA2A, 
    0x00AA54, 0x00AA7E, 0x00AAA8, 0x00AAD2, 0x00FC00, 0x00FC2A, 0x00FC54, 0x00FC7E, 
    0x00FCA8, 0x00FCD2, 0x00FE00, 0x00FE2A, 0x00FE54, 0x00FE7E, 0x00FEA8, 0x00FED2, 
    0x00A800, 0x00A82A, 0x00A854, 0x00A87E, 0x00A8A8, 0x00A8D2, 0x00FA00, 0x00FA2A, 
    0x00FA54, 0x00FA7E, 0x00FAA8, 0x00FAD2, 0x00D200, 0x00D22A, 0x00D254, 0x00D27E, 
    0x00D2A8, 0x00D2D2, 0x00FA00, 0x00FA2A, 0x00FA54, 0x00FA7E, 0x00FAA8, 0x00FAD2, 
    0x00D600, 0x00D62A, 0x00D654, 0x00D67E, 0x00D6A8, 0x00D6D2, 0x00FE00, 0x00FE2A, 
    0x00FE54, 0x00FE7E, 0x00FEA8, 0x00FED2, 0x00FA00, 0x00FA2A, 0x00FA54, 0x00FA7E, 
    0x00FAA8, 0x00FAD2, 0x00D200, 0x00D22A, 0x00D254, 0x00D27E, 0x00D2A8, 0x00D2D2,
    0x000000, 0x0A0A0A, 0x141414, 0x1E1E1E, 0x282828, 0x323232, 0x3C3C3C, 0x464646, 
    0x505050, 0x5A5A5A, 0x646464, 0x6E6E6E, 0x787878, 0x828282, 0x8C8C8C, 0x969696, 
    0xA0A0A0, 0xAAAAAA, 0xB4B4B4, 0xBEBEBE, 0xC8C8C8, 0xD2D2D2, 0xDCDCDC, 0xE6E6E6,
};
#endif

bool term_setbg_indexed(size_t idx) {
    if(idx > 255 || !term_impl) return false;
    if(term_impl->setbg_indexed) {
        bool irq = term_outguard_start();
        bool ret = term_impl->setbg_indexed(term_impl, idx);
        term_outguard_end(irq);
        return ret;
    }
#ifndef TERM_NO_IDXCOLOR_EMULATION
    else if(term_impl->setbg_rgb) {
        bool irq = term_outguard_start();
        bool ret = term_impl->setbg_rgb(term_impl, term_color_palette[idx]);
        term_outguard_end(irq);
        return ret;
    }
#endif
    return false;
}

bool term_setfg_indexed(size_t idx) {
    if(idx > 255 || !term_impl) return false;
    if(term_impl->setfg_indexed) {
        bool irq = term_outguard_start();
        bool ret = term_impl->setfg_indexed(term_impl, idx);
        term_outguard_end(irq);
        return ret;
    }
#ifndef TERM_NO_IDXCOLOR_EMULATION
    else if(term_impl->setfg_rgb) {
        bool irq = term_outguard_start();
        bool ret = term_impl->setfg_rgb(term_impl, term_color_palette[idx]);
        term_outguard_end(irq);
        return ret;
    }
#endif
    return false;
}

size_t term_getbg_indexed() {
    if(!term_impl) return (size_t)-1;
    if(term_impl->getbg_indexed) {
        bool irq = term_outguard_start();
        size_t ret = term_impl->getbg_indexed(term_impl);
        term_outguard_end(irq);
        return ret;
    }
#ifndef TERM_NO_IDXCOLOR_EMULATION
    else if(term_impl->getbg_rgb) {
        bool irq = term_outguard_start();
        uint32_t color = term_impl->getbg_rgb(term_impl);
        term_outguard_end(irq);
        for(size_t i = 0; i < 256; i++) {
            if(term_color_palette[i] == color) return i;
        }
    }
#endif
    return (size_t)-1;
}

size_t term_getfg_indexed() {
    if(!term_impl) return (size_t)-1;
    if(term_impl->getfg_indexed) {
        bool irq = term_outguard_start();
        size_t ret = term_impl->getfg_indexed(term_impl);
        term_outguard_end(irq);
        return ret;
    }
#ifndef TERM_NO_IDXCOLOR_EMULATION
    else if(term_impl->getfg_rgb) {
        bool irq = term_outguard_start();
        uint32_t color = term_impl->getfg_rgb(term_impl);
        term_outguard_end(irq);
        for(size_t i = 0; i < 256; i++) {
            if(term_color_palette[i] == color) return i;
        }
    }
#endif
    return (size_t)-1;
}

bool term_setbg_rgb(uint32_t color) {
    if(color > 0xFFFFFF || !term_impl || !term_impl->setbg_rgb) return false;
    bool irq = term_outguard_start();
    bool ret = term_impl->setbg_rgb(term_impl, color);
    term_outguard_end(irq);
    return ret;
}

bool term_setfg_rgb(uint32_t color) {
    if(color > 0xFFFFFF || !term_impl || !term_impl->setfg_rgb) return false;
    bool irq = term_outguard_start();
    bool ret = term_impl->setfg_rgb(term_impl, color);
    term_outguard_end(irq);
    return ret;
}

uint32_t term_getbg_rgb() {
    if(!term_impl) return false;
    if(term_impl->getbg_rgb) {
        bool irq = term_outguard_start();
        uint32_t ret = term_impl->getbg_rgb(term_impl);
        term_outguard_end(irq);
        return ret;
    }
#ifndef TERM_NO_IDXCOLOR_EMULATION
    else if(term_impl->getbg_indexed) {
        bool irq = term_outguard_start();
        size_t idx = term_impl->getbg_indexed(term_impl);
        term_outguard_end(irq);
        if(idx <= 255) return term_color_palette[idx];
    }
#endif
    return (size_t)-1;
}

uint32_t term_getfg_rgb() {
    if(!term_impl) return false;
    if(term_impl->getfg_rgb) {
        bool irq = term_outguard_start();
        uint32_t ret = term_impl->getfg_rgb(term_impl);
        term_outguard_end(irq);
        return ret;
    }
#ifndef TERM_NO_IDXCOLOR_EMULATION
    else if(term_impl->getfg_indexed) {
        bool irq = term_outguard_start();
        size_t idx = term_impl->getfg_indexed(term_impl);
        term_outguard_end(irq);
        if(idx <= 255) return term_color_palette[idx];
    }
#endif
    return (size_t)-1;
}
