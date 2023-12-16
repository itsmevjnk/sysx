#ifndef _HAL_TERMINAL_H_
#define _HAL_TERMINAL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <helpers/mutex.h>

#define TERM_LINE_TERMINATION                   '\n' // line termination character, used by term_gets

/* hooks for terminal implementation */
typedef struct term_hook {
    void (*putc)(const struct term_hook*, char);
    void (*puts)(const struct term_hook*, const char*); // accelerated puts implementation
    size_t (*available)(const struct term_hook*); // get number of characters available to be read
    char (*getc)(const struct term_hook*); // get character from terminal without echoing
    void (*clear)(const struct term_hook*);
    void (*get_dimensions)(const struct term_hook*, size_t*, size_t*);
    void (*set_xy)(const struct term_hook*, size_t, size_t);
    void (*get_xy)(const struct term_hook*, size_t*, size_t*);
    bool (*setfg_indexed)(const struct term_hook*, size_t); // set indexed foreground color
    size_t (*getfg_indexed)(const struct term_hook*); // get indexed foreground color
    bool (*setbg_indexed)(const struct term_hook*, size_t); // set indexed background color
    size_t (*getbg_indexed)(const struct term_hook*); // get indexed background color
    bool (*setfg_rgb)(const struct term_hook*, uint32_t); // set RGB foreground color
    uint32_t (*getfg_rgb)(const struct term_hook*); // get RGB foreground color
    bool (*setbg_rgb)(const struct term_hook*, uint32_t); // set RGB background color
    uint32_t (*getbg_rgb)(const struct term_hook*); // get RGB background color
    mutex_t mutex_in; // mutex for input operations (available/getc)
    mutex_t mutex_out; // mutex for output operations (putc/puts/clear/get_dimensions/set_xy/get_xy/set(get)bg(fg))
    void* data; // other data if needed
} term_hook_t;
extern term_hook_t* term_impl; // terminal implementation

/*
 * void term_init()
 *  Initializes the kernel's built-in terminal. Note that a built-in
 *  terminal implementation must be present.
 *  If the TERM_SER macro is defined, the TERM_SER_PORT, TERM_SER_DBIT,
 *  TERM_SER_SBIT, TERM_SER_PARITY and TERM_SER_BAUD macros can also
 *  be defined to change from the default of (9600,N,8,1).
 */
void term_init();

/*
 * void term_putc(char c)
 *  Outputs a character to the terminal.
 */
void term_putc(char c);

/*
 * size_t term_available()
 *  Gets the minimum number of characters available to be read from the
 *  terminal.
 */
size_t term_available();

/*
 * char term_getc()
 *  Gets a character from the terminal.
 *  If the terminal implementation does not support character input,
 *  this will return 0.
 */
char term_getc_noecho();

/*
 * void term_clear()
 *  Clears the terminal.
 *  If the terminal implementation does not support screen clearing,
 *  this function will not do anything.
 */
void term_clear();

/*
 * void term_get_dimensions(size_t* width, size_t* height)
 *  Retrieves the terminal's display dimensions, and save them to the
 *  variables referenced by width and height.
 *  If this feature is not implemented by the terminal implementation,
 *  this function will not do anything.
 */
void term_get_dimensions(size_t* width, size_t* height);

/*
 * void term_set_xy(size_t x, size_t y)
 *  Sets the terminal's cursor coordinates.
 *  If this feature is not implemented by the terminal implementation,
 *  this function will not do anything.
 */
void term_set_xy(size_t x, size_t y);

/*
 * void term_get_xy(size_t* x, size_t* y)
 *  Gets the terminal's current cursor coordinates.
 *  If this feature is not implemented by the terminal implementation,
 *  this function will not do anything.
 */
void term_get_xy(size_t* x, size_t* y);

/* handled by hal/terminal.c */

/*
 * void term_puts(const char* s)
 *  Outputs a null-terminated string to the terminal.
 */
void term_puts(const char* s);

/*
 * char term_getc()
 *  Gets a character from the terminal and echo it back.
 *  If the terminal implementation does not support character input,
 *  this will return 0.
 */
char term_getc();

/*
 * void term_gets_noecho(char* s)
 *  Receives a string from the terminal (WITHOUT ECHOING), ending with
 *  the character specified in TERM_LINE_TERMINATION above.
 *  If the terminal implementation does not support character input,
 *  this will not do anything.
 */
void term_gets_noecho(char* s);

/*
 * void term_gets(char* s)
 *  Receives a string from the terminal, ending with the character
 *  specified in TERM_LINE_TERMINATION above.
 *  If the terminal implementation does not support character input,
 *  this will not do anything.
 */
void term_gets(char* s);

/*
 * bool term_setbg_indexed(size_t idx)
 *  Set the background color to the specified indexed (256 color palette)
 *  color.
 *  Returns whether the operation is successful (i.e. if it's supported
 *  and the provided color is valid).
 */
bool term_setbg_indexed(size_t idx);

/*
 * bool term_setfg_indexed(size_t idx)
 *  Set the foreground color to the specified indexed (256 color palette)
 *  color.
 *  Returns whether the operation is successful (i.e. if it's supported
 *  and the provided color is valid).
 */
bool term_setfg_indexed(size_t idx);

/*
 * size_t term_getbg_indexed()
 *  Gets the background indexed (256 color palette) color.
 *  Returns the color index, or -1 if the operation is unsupported or
 *  the color has been set to an RGB value.
 */
size_t term_getbg_indexed();

/*
 * size_t term_getfg_indexed()
 *  Gets the foreground indexed (256 color palette) color.
 *  Returns the color index, or -1 if the operation is unsupported or
 *  the color has been set to an RGB value.
 */
size_t term_getfg_indexed();

/*
 * bool term_setbg_rgb(uint32_t color)
 *  Set the background color to the specified RGB (0x00RRGGBB) color.
 *  Returns whether the operation is successful (i.e. if it's supported
 *  and the provided color is valid).
 */
bool term_setbg_rgb(uint32_t color);

/*
 * bool term_setfg_rgb(uint32_t color)
 *  Set the background color to the specified RGB (0x00RRGGBB) color.
 *  Returns whether the operation is successful (i.e. if it's supported
 *  and the provided color is valid).
 */
bool term_setfg_rgb(uint32_t color);

/*
 * uint32_t term_getbg_rgb()
 *  Gets the background RGB (0x00RRGGBB) color.
 *  Returns the color, or -1 if the operation is unsupported. If the
 *  terminal's color has been set to an indexed color, the function will
 *  convert the color to RGB for returning.
 */
uint32_t term_getbg_rgb();

/*
 * uint32_t term_getfg_rgb()
 *  Gets the foreground RGB (0x00RRGGBB) color.
 *  Returns the color, or -1 if the operation is unsupported. If the
 *  terminal's color has been set to an indexed color, the function will
 *  convert the color to RGB for returning.
 */
uint32_t term_getfg_rgb();

#endif