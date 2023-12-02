#include <hal/fbuf.h>
#include <hal/keyboard.h>

#define TERM_FBUF_HTAB_LENGTH                   4 // horizontal tab length

static size_t fbterm_x = 0, fbterm_y = 0;
static uint32_t fbterm_fg = 0xC0C0C0, fbterm_bg = 0x000000;

static void fbterm_newline() {
    fbterm_x = 0;
    fbterm_y++;
    if((int)fbuf_impl->height - (int)fbterm_y * fbuf_font->height < fbuf_font->height) {
        /* shift entire buffer upward */
        fbterm_y--;
        fbuf_scroll_up(fbuf_font->height, fbterm_bg);
    }
}

static void fbterm_putc_stub(char c) {
    switch(c) {
        case '\b': // backspace character
            if(fbterm_x > 0) fbterm_x--;
            else if(fbterm_y > 0) {
                fbterm_x = fbuf_impl->width / fbuf_font->width - 1;
                fbterm_y--;
            }
            break;
        case '\r': // carriage return
            fbterm_x = 0;
            break;
        case '\n': // newline
            fbterm_newline();
            break;
        case '\t': // horizontal tab
            do {
                fbuf_putc_stub(fbterm_x * fbuf_font->width, fbterm_y * fbuf_font->height, ' ', fbterm_fg, fbterm_bg, false);
                fbterm_x++;
            } while((int)fbuf_impl->width - (int)fbterm_x * fbuf_font->width >= fbuf_font->width && fbterm_x % TERM_FBUF_HTAB_LENGTH != 0);
            if((int)fbuf_impl->width - (int)fbterm_x * fbuf_font->width < fbuf_font->width) fbterm_newline();
            break;
        default: // normal character
            fbuf_putc_stub(fbterm_x * fbuf_font->width, fbterm_y * fbuf_font->height, c, fbterm_fg, fbterm_bg, false);
            fbterm_x++;
            if((int)fbuf_impl->width - (int)fbterm_x * fbuf_font->width < fbuf_font->width) fbterm_newline();
            break;
    }
}

void fbterm_putc(term_hook_t* impl, char c) {
    (void) impl;
    if(fbuf_impl == NULL || fbuf_font == NULL) return;
    fbterm_putc_stub(c);
    fbuf_commit();
}

void fbterm_puts(term_hook_t* impl, const char* s) {
    (void) impl;
    if(fbuf_impl == NULL || fbuf_font == NULL) return;
    while(*s != '\0') {
        fbterm_putc_stub(*s);
        s++;
    }
    fbuf_commit();
}

#ifndef TERM_NO_CLEAR
void fbterm_clear(term_hook_t* impl) {
    (void) impl;
    fbterm_x = 0; fbterm_y = 0;
    fbuf_fill(fbterm_bg);
    fbuf_commit();
}
#endif

#ifndef TERM_NO_XY
void fbterm_get_dimensions(term_hook_t* impl, size_t* width, size_t* height) {
    (void) impl;
    if(fbuf_impl != NULL && fbuf_font != NULL) {
        *width = fbuf_impl->width / fbuf_font->width;
        *height = fbuf_impl->height / fbuf_font->height;
    } else {
        *width = 0;
        *height = 0;
    }
}

void fbterm_set_xy(term_hook_t* impl, size_t x, size_t y) {
    (void) impl;
    fbterm_x = x; fbterm_y = y;
}

void fbterm_get_xy(term_hook_t* impl, size_t* x, size_t* y) {
    (void) impl;
    *x = fbterm_x; *y = fbterm_y;
}
#endif

#ifndef TERM_NO_COLOR
bool fbterm_setfg_rgb(term_hook_t* impl, uint32_t color) {
    (void) impl;
    fbterm_fg = color;
    return true;
}

bool fbterm_setbg_rgb(term_hook_t* impl, uint32_t color) {
    (void) impl;
    fbterm_bg = color;
    return true;
}

uint32_t fbterm_getfg_rgb(term_hook_t* impl) {
    (void) impl;
    return fbterm_fg;
}

uint32_t fbterm_getbg_rgb(term_hook_t* impl) {
    (void) impl;
    return fbterm_bg;
}
#endif

term_hook_t fbterm_hook = {
    &fbterm_putc,
    &fbterm_puts,

#ifndef TERM_NO_INPUT
    &kbd_term_available,
    &kbd_term_getc,
#else
    NULL,
    NULL,
#endif

#ifndef TERM_NO_CLEAR
    &fbterm_clear,
#else
    NULL,
#endif

#ifndef TERM_NO_XY
    &fbterm_get_dimensions,
    &fbterm_set_xy,
    &fbterm_get_xy,
#else
    NULL,
    NULL,
    NULL,
#endif

    NULL,
    NULL,
    NULL,
    NULL,

#ifndef TERM_NO_COLOR
    &fbterm_setfg_rgb,
    &fbterm_getfg_rgb,
    &fbterm_setbg_rgb,
    &fbterm_getbg_rgb,
#else
    NULL,
    NULL,
    NULL,
    NULL,
#endif

    {0},
    {0},

    NULL
};