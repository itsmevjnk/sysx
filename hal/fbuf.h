#ifndef HAL_FBUF_H
#define HAL_FBUF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <helpers/mutex.h>
#include <hal/terminal.h>
#include <exec/elf.h>
#include <hal/timer.h>

enum fbuf_pixel_type {
    FBUF_15BPP_RGB555,          // 0RRRRRGG GGGBBBBB, same endianness
    FBUF_15BPP_RGB555_RE,       // 0RRRRRGG GGGBBBBB, different endianness 
    FBUF_15BPP_BGR555,          // 0BBBBBGG GGGRRRRR
    FBUF_15BPP_BGR555_RE,       
    FBUF_16BPP_RGB565,          // RRRRRGGG GGGBBBBB
    FBUF_16BPP_RGB565_RE,   
    FBUF_16BPP_BGR565,          // BBBBBGGG GGGRRRRR
    FBUF_16BPP_BGR565_RE,
    FBUF_24BPP_RGB888,          // RRRRRRRR GGGGGGGG BBBBBBBB (as it is ordered in the memory)
    FBUF_24BPP_BGR888,          // BBBBBBBB GGGGGGGG RRRRRRRR (as it is ordered in the memory)
    FBUF_32BPP_RGB888,          // 00000000 RRRRRRRR GGGGGGGG BBBBBBBB
    FBUF_32BPP_RGB888_RE,
    FBUF_32BPP_BGR888,          // 00000000 BBBBBBBB GGGGGGGG RRRRRRRR
    FBUF_32BPP_BGR888_RE,
};

typedef struct fbuf {
    size_t width;
    size_t height;
    enum fbuf_pixel_type type;
    size_t pitch;
    void* framebuffer;

    /* double buffering */
    void* backbuffer; // NULL if not using double buffer
    timer_tick_t tick_flip; // the timer tick when the framebuffer was last flipped
    bool flip_all; // set to copy the entire backbuffer instead of only pages that have been modified
    bool dbuf_direct_write; // set to write directly to framebuffer (i.e. without waiting for commit) and read from backbuffer

    /* optional accelerated functions */
    void (*flip)(struct fbuf*); // double buffer flipping function
    void (*scroll_up)(struct fbuf*, size_t); // scroll up function (without filling bottom lines)
    void (*scroll_down)(struct fbuf*, size_t); // scroll down function (without filling top lines)

    /* associated driver information */
    elf_prgload_t* elf_segments; // ELF segments list - if the driver is a kernel module
    size_t num_elf_segments;
    bool (*unload)(struct fbuf*); // function to be called prior to unloading (optional) - returns false if the ELF segments are to be ignored (i.e. no unloading from memory)
} fbuf_t;
extern fbuf_t* fbuf_impl;

typedef struct fbuf_font {
    size_t width;
    size_t height;
    void* data;
    void (*draw)(struct fbuf_font*, size_t, size_t, char, uint32_t, uint32_t, bool); // font, x, y, character, fg color, bg color, transparent
} fbuf_font_t;
extern fbuf_font_t* fbuf_font;

extern term_hook_t fbterm_hook; // framebuffer terminal hooks

#define FBUF_RGB(r, g, b)                       (((r) << 16) | ((g) << 8) | ((b) << 0)) // RGB layout: 0xRRGGBB
#define FBUF_R(color)                           (((color) >> 16) & 0xFF)
#define FBUF_G(color)                           (((color) >> 8) & 0xFF)
#define FBUF_B(color)                           (((color) >> 0) & 0xFF)

#define FBUF_FLIP_PERIOD                        20000 // framebuffer flipping period (in timer ticks)

/*
 * size_t fbuf_process_color(uint32_t* color)
 *  Translates the given color for the current implementation's color data type
 *  and returns the number of bytes per pixel.
 */
size_t fbuf_process_color(uint32_t* color);

/*
 * void fbuf_putpixel(size_t x, size_t y, size_t n, uint32_t color)
 *  Draws n pixel(s) on the framebuffer, starting at the specified coordinate.
 *  The pixels will be drawn from left to right, then top to bottom.
 */
void fbuf_putpixel(size_t x, size_t y, size_t n, uint32_t color);

/*
 * void fbuf_getpixel(size_t x, size_t y, size_t n, uint32_t* color)
 *  Gets the color of n pixel(s) on the framebuffer, starting at the specified
 *  coordinate.
 *  The pixels will be retrieved from left to right, then top to bottom.
 */
void fbuf_getpixel(size_t x, size_t y, size_t n, uint32_t* color);

/*
 * void fbuf_fill(uint32_t color)
 *  Fills the framebuffer with the specified color.
 */
void fbuf_fill(uint32_t color);

/*
 * void fbuf_commit()
 *  Commits all changes on the backbuffer to the framebuffer (if double
 *  buffering is used).
 */
void fbuf_commit();

/*
 * void fbuf_putc_stub(size_t x, size_t y, char c, uint32_t fg, uint32_t bg, bool transparent)
 *  Draws the specified character on the framebuffer without committing
 *  it to the framebuffer (if double buffering is used).
 *  If transparent is set, the function will not overwrite blank spaces
 *  in the character with the background color.
 */
void fbuf_putc_stub(size_t x, size_t y, char c, uint32_t fg, uint32_t bg, bool transparent);

/*
 * void fbuf_putc(size_t x, size_t y, char c, uint32_t fg, uint32_t bg, bool transparent)
 *  Draws the specified character on the buffer and commit it to the
 *  framebuffer immediately (if double buffering is used).
 */
void fbuf_putc(size_t x, size_t y, char c, uint32_t fg, uint32_t bg, bool transparent);

/*
 * void fbuf_puts_stub(size_t x, size_t y, char* s, uint32_t fg, uint32_t bg, bool transparent)
 *  Draws the specified string on the framebuffer without committing it
 *  to the framebuffer (if double buffering is used).
 *  Text will be wrapped back to the first row (X = 0) at the end of the
 *  line.
 *  If transparent is set, the function will not overwrite blank spaces
 *  in the character with the background color.
 */
void fbuf_puts_stub(size_t x, size_t y, char* s, uint32_t fg, uint32_t bg, bool transparent);

/*
 * void fbuf_puts(size_t x, size_t y, char* s, uint32_t fg, uint32_t bg, bool transparent)
 *  Draws the specified string on the buffer and commit it to the
 *  framebuffer immediately (if double buffering is used).
 */
void fbuf_puts(size_t x, size_t y, char* s, uint32_t fg, uint32_t bg, bool transparent);

/*
 * void fbuf_scroll_up(size_t lines, uint32_t color)
 *  Scrolls the framebuffer up by the specified number of lines and fills
 *  the empty space with the specified color.
 */
void fbuf_scroll_up(size_t lines, uint32_t color);

/*
 * void fbuf_scroll_down(size_t lines, uint32_t color)
 *  Scrolls the framebuffer down by the specified number of lines and fills
 *  the empty space with the specified color.
 */
void fbuf_scroll_down(size_t lines, uint32_t color);

/*
 * void fbuf_unload()
 *  Unloads the current framebuffer implementation.
 */
void fbuf_unload();

#endif