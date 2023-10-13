/* WARNING:
 *  This library is nowhere near compliant with current
 *  C standards! To separate its functions from the
 *  userspace counterparts (which may appear alongside
 *  this as kernel modules are implemented), the functions'
 *  names are appended with a k prefix (e.g. kputchar instead
 *  of putchar).
 */

#include <stdio.h>
#include <string.h>
#include <hal/serial.h>

int kputchar(int c) {
    // TODO: allow the log output to be redirected
    ser_putc(0, (char) c);
    return c;
}

int kputs(const char* str) {
    return kprintf("%s\n", str);
}

/* protostream for log output */
    static uint8_t ptlog_read(struct ptstream* stream) {
    (void) stream;
    return (uint8_t) ser_getc(0);
}

static int ptlog_write(struct ptstream* stream, uint8_t c) {
    (void) stream;
    ser_putc(0, (char) c);
    return 0;
}

static ptstream_t pts_log = {
    NULL,
    (size_t) -1,
    0,
    &ptlog_read,
    &ptlog_write
};

int kprintf(const char* fmt, ...) {
    va_list arg; va_start(arg, fmt);
    int ret = kvfprintf(&pts_log, fmt, arg);
    va_end(arg);
    return ret;
}

/* functions for handling string protostreams */
static int ptstr_write(struct ptstream* stream, uint8_t c) {
    if(stream->off >= stream->sz) return -1;
    *(uint8_t*)((uintptr_t) stream->buf + stream->off++) = c;
    return 0;
}

int ksprintf(char* str, const char* fmt, ...) {
    /* create stream for string */
    ptstream_t stream = {
        str,
        (size_t) -1,
        0,
        NULL,
        &ptstr_write
    };
    va_list arg; va_start(arg, fmt);
    int ret = kvfprintf(&stream, fmt, arg);
    str[ret] = 0; // terminate the string
    va_end(arg);
    return ret;
}
