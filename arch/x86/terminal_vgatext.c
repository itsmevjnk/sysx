#include <hal/terminal.h>
#include <mm/vmm.h>
#include <arch/x86cpu/asm.h>
#include <string.h>

#define TERM_VGATEXT_ADDR                   0xB8000 // the base address to map the textmode framebuffer to (must be 4K aligned)

#define TERM_VGATEXT_CURSOR_START           14 // cursor starting scanline (0-15)
#define TERM_VGATEXT_CURSOR_END             15 // cursor ending scanline (0-15)

#ifdef TERM_VGATEXT

static uint8_t term_x = 0, term_y = 0; // current cursor coordinates
static uint16_t* term_buffer = (uint16_t*) TERM_VGATEXT_ADDR;

#ifndef TERM_VGATEXT_NO_CURSOR // can be specified in CFLAGS
static void term_update_cursor() {
    uint16_t pos = term_y * 80 + term_x;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
#endif

void term_init() {
    vmm_map(vmm_current, 0xB8000, TERM_VGATEXT_ADDR, 80 * 25 * 2, true, false, true); // map textmode framebuffer
#ifndef TERM_VGATEXT_NO_CURSOR
    /* enable cursor */
    outb(0x3D4, 0x09); outb(0x3D5, 15); // set maximum scan line to 15
    outb(0x3D4, 0x0A); outb(0x3D5, (inb(0x3D5) & 0xC0) | TERM_VGATEXT_CURSOR_START);
    outb(0x3D4, 0x0B); outb(0x3D5, (inb(0x3D5) & 0xE0) | TERM_VGATEXT_CURSOR_END);
#ifdef TERM_NO_CLEAR
    term_update_cursor(); // update cursor position here since term_clear is disabled
#endif
#else
    /* disable cursor */
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
#endif

#ifndef TERM_NO_CLEAR
    term_clear();
#endif
}

static void term_newline() {
    term_x = 0;
    term_y++;
    if(term_y == 25) {
        /* shift entire buffer upward */
        memmove(term_buffer, &term_buffer[1 * 80], 24 * 80 * 2);
        for(uint8_t x = 0; x < 80; x++) term_buffer[24 * 80 + x] = 0x0700;
        term_y = 24;
    }
}

void term_putc(char c) {
    switch(c) {
        case '\b': // backspace character
            if(term_x > 0) term_x--;
            else if(term_y > 0) {
                term_x = 79;
                term_y--;
            }
            break;
        case '\r': // carriage return - ignore this
            break;
        case '\n': // newline
            term_newline();
            break;
        default: // normal character
            term_buffer[term_y * 80 + term_x] = ((uint8_t)c) | 0x0700; // light grey on black
            term_x++;
            if(term_x == 80) term_newline();
            break;
    }
#ifndef TERM_VGATEXT_NO_CURSOR
    term_update_cursor();
#endif
}

#ifndef TERM_NO_INPUT
#error "VGA textmode terminal does NOT support input yet!"
#endif

#ifndef TERM_NO_CLEAR
void term_clear() {
    for(uint8_t y = 0; y < 25; y++) {
        for(uint8_t x = 0; x < 80; x++) term_buffer[y * 80 + x] = 0x0700;
    }
    term_x = 0; term_y = 0; // also remember to reset coordinates
#ifndef TERM_VGATEXT_NO_CURSOR
    term_update_cursor();
#endif
}
#endif

#ifndef TERM_NO_XY
void term_get_dimensions(size_t* width, size_t* height) {
    *width = 80;
    *height = 25;
}

void term_set_xy(size_t x, size_t y) {
    term_x = x; term_y = y;
#ifndef TERM_VGATEXT_NO_CURSOR
    term_update_cursor();
#endif
}

void term_get_xy(size_t* x, size_t* y) {
    *x = term_x; *y = term_y;
}

#endif

#endif
