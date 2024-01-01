#ifndef _STDIO_H_
#define _STDIO_H_

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

/* WARNING:
 *  This library is nowhere near compliant with current
 *  C standards! To separate its functions from the
 *  userspace counterparts (which may appear alongside
 *  this as kernel modules are implemented), the functions'
 *  names are appended with a k prefix (e.g. kputchar instead
 *  of putchar).
 */

/*
 * ptstream_t
 *  Describes a "proto"stream (that is, a very simple
 *  stream-like structure).
 */
typedef struct ptstream {
    void* buf; // buffer pointer
    size_t sz; // buffer size (one may want to set it to -1 for unlimited streams)
    size_t off; // offset for operations
    uint8_t (*read)(struct ptstream*); // reads a byte from stream
    int (*write)(struct ptstream*, uint8_t); // writes a byte into stream and returns 0 on success
} ptstream_t;

/*
 * extern ptstream_t* kstdin
 *  The kernel's stdin stream pointer.
 */
extern ptstream_t* kstdin;

/*
 * extern ptstream_t* kstdout
 *  The kernel's stdout stream pointer.
 */
extern ptstream_t* kstdout;

/*
 * extern ptstream_t* kstderr
 *  The kernel's stderr stream pointer.
 *  By default, this is set to the same stream as stdout;
 *  however, the KSTDERR_SER macro (as well as its parameter macros
 *  KSTDERR_SER_PORT, KSTDERR_SER_DBIT, KSTDERR_SER_SBIT,
 *  KSTDERR_SER_PARITY and KSTDERR_SER_BAUD) can be used to redirect
 *  stderr to a serial interface.
 */
extern ptstream_t* kstderr;

/*
 * void stdio_stderr_init()
 *  Initializes the stderr stream based on kernel cmdline
 *  arguments.
 */
void stdio_stderr_init();

/*
 * void stdio_init()
 *  Initializes the stdio system.
 *  If KSTDERR_INIT_SEPARATE is not defined, this function
 *  will also call stdio_stderr_init.
 */
void stdio_init();

/*
 * int kputc(int c, ptstream_t* stream)
 *  Writes the character c to a specified stream then
 *  returns it.
 */
int kputc(int c, ptstream_t* stream);

/*
 * int kputchar(int c)
 *  Prints the character c to stdout then returns it.
 */
int kputchar(int c);

/*
 * int kputs(const char* str)
 *  Prints the null terminated string str appended with
 *  an LF character to the stdout stream, then returns
 *  a non-negative value on success.
 */
int kputs(const char* str);

/*
 * int kvfprintf(ptstream_t* stream, const char* fmt, va_list arg)
 *  Writes and formats the string pointed by fmt to the stream
 *  protostream. Returns the number of characters written.
 */
int kvfprintf(ptstream_t* stream, const char* fmt, va_list arg);

/*
 * int kfprintf(ptstream_t* stream, const char* fmt, ...)
 *  Writes and formats the string pointed by fmt to the stream
 *  protostream. Returns the number of characters written.
 */
int kfprintf(ptstream_t* stream, const char* fmt, ...);

/*
 * int kprintf(const char* fmt, ...)
 *  Prints and formats the string pointed by fmt to the stdout
 *  stream. Returns the number of characters printed.
 */
int kprintf(const char* fmt, ...);

/*
 * int ksprintf(char* str, const char* fmt, va_list arg)
 *  Writes and formats the string pointed by fmt to the buffer
 *  str. Returns the number of characters printed.
 */
int ksprintf(char* str, const char* fmt, ...);

/*
 * int kvsprintf(char* str, const char* fmt, va_list arg)
 *  Writes and formats the string pointed by fmt to the buffer
 *  str. Returns the number of characters printed.
 */
int kvsprintf(char* str, const char* fmt, va_list arg);

#endif
