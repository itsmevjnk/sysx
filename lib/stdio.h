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
 * int kputchar(int c)
 *  Prints the character c to the current log output
 *  then returns it.
 */
int kputchar(int c);

/*
 * int kputs(const char* str)
 *  Prints the null terminated string str appended with
 *  an LF character to the current log output, then
 *  returns a non-negative value on success.
 */
int kputs(const char* str);

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
 * int kvfprintf(ptstream_t* stream, const char* fmt, va_list arg)
 *  Writes and formats the string pointed by fmt to the stream
 *  protostream. Returns the number of characters written.
 */
int kvfprintf(ptstream_t* stream, const char* fmt, va_list arg);

/*
 * int kprintf(const char* fmt, va_list arg)
 *  Prints and formats the string pointed by fmt to the current
 *  log output. Returns the number of characters printed.
 */
int kprintf(const char* fmt, ...);

/*
 * int ksprintf(char* str, const char* fmt, va_list arg)
 *  Writes and formats the string pointed by fmt to the buffer
 *  str. Returns the number of characters printed.
 */
int ksprintf(char* str, const char* fmt, ...);

#endif
