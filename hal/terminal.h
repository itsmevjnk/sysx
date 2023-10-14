#ifndef _HAL_TERMINAL_H_
#define _HAL_TERMINAL_H_

#include <stddef.h>
#include <stdint.h>

#define TERM_LINE_TERMINATION                   '\n' // line termination character, used by term_gets

/* must be handled by architecture-specific code - generic serial implementation is available with the TERM_SER macro */

/*
 * void term_init()
 *  Initializes the terminal.
 *  Note that if the TERM_SER macro is defined, the TERM_SER_PORT,
 *  TERM_SER_DBIT, TERM_SER_SBIT, TERM_SER_PARITY and TERM_SER_BAUD
 *  macros can also be defined to change from the default of
 *  (115200,N,8,1).
 */
void term_init();

/*
 * void term_putc(char c)
 *  Outputs a character to the terminal.
 */
void term_putc(char c);

#ifndef TERM_NO_INPUT
/*
 * char term_getc()
 *  Gets a character from the terminal.
 *  This is only available if TERM_NO_INPUT is not defined.
 */
char term_getc_noecho();
#endif

/*
 * void term_clear()
 *  Clears the terminal.
 *  If TERM_NO_CLEAR is defined, this function will not do anything.
 */
void term_clear();

/* handled by hal/terminal.c */

/*
 * void term_puts(const char* s)
 *  Outputs a null-terminated string to the terminal.
 */
void term_puts(const char* s);

#ifndef TERM_NO_INPUT
/*
 * char term_getc()
 *  Gets a character from the terminal and echo it back.
 *  This is only available if TERM_NO_INPUT is not defined.
 */
char term_getc();

/*
 * void term_gets_noecho(char* s)
 *  Receives a string from the terminal (WITHOUT ECHOING), ending with
 *  the character specified in TERM_LINE_TERMINATION above.
 *  This is only available if TERM_NO_INPUT is not defined.
 */
void term_gets_noecho(char* s);

/*
 * void term_gets(char* s)
 *  Receives a string from the terminal, ending with the character
 *  specified in TERM_LINE_TERMINATION above.
 *  This is only available if TERM_NO_INPUT is not defined.
 */
void term_gets(char* s);
#endif

#endif