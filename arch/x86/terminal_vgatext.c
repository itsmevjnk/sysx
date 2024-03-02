#include <hal/terminal.h>
#include <hal/keyboard.h>
#include <mm/vmm.h>
#include <arch/x86cpu/asm.h>
#include <string.h>
#include <exec/task.h>
#include <helpers/mutex.h>

#define TERM_VGATEXT_ADDR                   0xC00B8000 // the base address to map the textmode framebuffer to (must be 4K aligned)

#define TERM_VGATEXT_CURSOR_START           14 // cursor starting scanline (0-15)
#define TERM_VGATEXT_CURSOR_END             15 // cursor ending scanline (0-15)

#define TERM_VGATEXT_HTAB_LENGTH            4 // horizontal tab length

#ifdef TERM_VGATEXT

#ifndef KINIT_MM_FIRST
#error "VGA textmode terminal requires PMM and VMM to be initialized first; add -DKINIT_MM_FIRST to CFLAGS."
#endif

static uint8_t vgaterm_x = 0, vgaterm_y = 0; // current cursor coordinates
static uint16_t* vgaterm_buffer = (uint16_t*) TERM_VGATEXT_ADDR;

#ifndef TERM_VGATEXT_NO_CURSOR // can be specified in CFLAGS
static void vgaterm_update_cursor() {
    uint16_t pos = vgaterm_y * 80 + vgaterm_x;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
#endif

#ifndef TERM_NO_COLOR
static uint8_t vgaterm_attr = 0x07; // light gray on black
static const uint8_t vgaterm_idxmap[16] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15}; // for both index to attr and vice versa

bool vgaterm_setfg_idx(const term_hook_t* impl, size_t idx) {
    (void) impl;
    if(idx >= 16) return false; // 16 color palette only
    vgaterm_attr = (vgaterm_attr & 0xF0) | vgaterm_idxmap[idx];
    return true;
}

bool vgaterm_setbg_idx(const term_hook_t* impl, size_t idx) {
    (void) impl;
    if(idx >= 8) return false; // textmode terminal only has 8 background colors; bit 7 is used for blinking
    vgaterm_attr = (vgaterm_attr & 0x0F) | ((vgaterm_idxmap[idx] & 0x7) << 4);
    return true;
}

size_t vgaterm_getfg_idx(const term_hook_t* impl) {
    (void) impl;
    return vgaterm_idxmap[vgaterm_attr & 0x0F];
}

size_t vgaterm_getbg_idx(const term_hook_t* impl) {
    (void) impl;
    return vgaterm_idxmap[(vgaterm_attr >> 4) & 0x7];
}
#endif

static void vgaterm_newline() {
    vgaterm_x = 0;
    vgaterm_y++;
    if(vgaterm_y == 25) {
        /* shift entire buffer upward */
        memmove(vgaterm_buffer, &vgaterm_buffer[1 * 80], 24 * 80 * 2);
        for(uint8_t x = 0; x < 80; x++)
#ifndef TERM_NO_COLOR
            vgaterm_buffer[24 * 80 + x] = ((uint16_t)vgaterm_attr << 8);
#else
            vgaterm_buffer[24 * 80 + x] = 0x0700;
#endif
        vgaterm_y = 24;
    }
}

static void vgaterm_putc_stub(char c) {
    switch(c) {
        case '\b': // backspace character
            if(vgaterm_x) vgaterm_x--;
            else if(vgaterm_y) {
                vgaterm_x = 79;
                vgaterm_y--;
            }
            break;
        case '\r': // carriage return
            vgaterm_x = 0;
            break;
        case '\n': // newline
            vgaterm_newline();
            break;
        case '\t': // horizontal tab
            do {
#ifndef TERM_NO_COLOR
                vgaterm_buffer[vgaterm_y * 80 + vgaterm_x] = ((uint16_t)vgaterm_attr << 8);
#else
                vgaterm_buffer[vgaterm_y * 80 + vgaterm_x] = 0x0700;
#endif
                vgaterm_x++;
            } while(vgaterm_x < 80 && (vgaterm_x % TERM_VGATEXT_HTAB_LENGTH));
            if(vgaterm_x == 80) vgaterm_newline();
            break;
        default: // normal character
#ifndef TERM_NO_COLOR
            vgaterm_buffer[vgaterm_y * 80 + vgaterm_x] = ((uint8_t)c) | ((uint16_t)vgaterm_attr << 8);
#else
            vgaterm_buffer[vgaterm_y * 80 + vgaterm_x] = ((uint8_t)c) | 0x0700; // light grey on black
#endif
            vgaterm_x++;
            if(vgaterm_x == 80) vgaterm_newline();
            break;
    }
}

void vgaterm_putc(const term_hook_t* impl, char c) {
    (void) impl;
    vgaterm_putc_stub(c);
#ifndef TERM_VGATEXT_NO_CURSOR
    vgaterm_update_cursor();
#endif
}

void vgaterm_puts(const term_hook_t* impl, const char* s) {
    (void) impl;
    for(size_t i = 0; s[i]; i++) vgaterm_putc_stub(s[i]);
#ifndef TERM_VGATEXT_NO_CURSOR
    vgaterm_update_cursor();
#endif
}

#ifndef TERM_NO_CLEAR
void vgaterm_clear(const term_hook_t* impl) {
    (void) impl;
    for(uint8_t y = 0; y < 25; y++) {
        for(uint8_t x = 0; x < 80; x++)
#ifndef TERM_NO_COLOR
            vgaterm_buffer[y * 80 + x] = ((uint16_t)vgaterm_attr << 8);
#else
            vgaterm_buffer[y * 80 + x] = 0x0700;
#endif
    }
    vgaterm_x = 0; vgaterm_y = 0; // also remember to reset coordinates
#ifndef TERM_VGATEXT_NO_CURSOR
    vgaterm_update_cursor();
#endif
}
#endif

#ifndef TERM_NO_XY
void vgaterm_get_dimensions(const term_hook_t* impl, size_t* width, size_t* height) {
    (void) impl;
    *width = 80;
    *height = 25;
}

void vgaterm_set_xy(const term_hook_t* impl, size_t x, size_t y) {
    (void) impl;
    vgaterm_x = x; vgaterm_y = y;
#ifndef TERM_VGATEXT_NO_CURSOR
    vgaterm_update_cursor();
#endif
}

void vgaterm_get_xy(const term_hook_t* impl, size_t* x, size_t* y) {
    (void) impl;
    *x = vgaterm_x; *y = vgaterm_y;
}
#endif

term_hook_t vgaterm_hook = {
    &vgaterm_putc,
    &vgaterm_puts,
#ifndef TERM_NO_INPUT
    &kbd_term_available,
    &kbd_term_getc,
#else
    NULL,
    NULL,
#endif

#ifndef TERM_NO_CLEAR
    &vgaterm_clear,
#else
    NULL,
#endif

#ifndef TERM_NO_XY
    &vgaterm_get_dimensions,
    &vgaterm_set_xy,
    &vgaterm_get_xy,
#else
    NULL,
    NULL,
    NULL,
#endif

#ifndef TERM_NO_COLOR
    &vgaterm_setfg_idx,
    &vgaterm_getfg_idx,
    &vgaterm_setbg_idx,
    &vgaterm_getbg_idx,
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
    vmm_map(vmm_current, 0xB8000, TERM_VGATEXT_ADDR, 80 * 25 * 2, 0, VMM_FLAGS_PRESENT | VMM_FLAGS_RW | VMM_FLAGS_GLOBAL | VMM_FLAGS_CACHE); // map textmode framebuffer
#ifndef TERM_VGATEXT_NO_CURSOR
    /* enable cursor */
    outb(0x3D4, 0x09); outb(0x3D5, 15); // set maximum scan line to 15
    outb(0x3D4, 0x0A); outb(0x3D5, (inb(0x3D5) & 0xC0) | TERM_VGATEXT_CURSOR_START);
    outb(0x3D4, 0x0B); outb(0x3D5, (inb(0x3D5) & 0xE0) | TERM_VGATEXT_CURSOR_END);
#ifdef TERM_NO_CLEAR
    vgaterm_update_cursor(); // update cursor position here since vgaterm_clear is disabled
#endif
#else
    /* disable cursor */
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
#endif

#ifndef TERM_NO_CLEAR
    vgaterm_clear(NULL);
#endif

    term_impl = &vgaterm_hook;
}

#endif
