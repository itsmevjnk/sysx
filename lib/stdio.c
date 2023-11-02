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
#include <hal/terminal.h>

/* protostream for log output */
#ifndef TERM_NO_INPUT
static uint8_t ptlog_read(struct ptstream* stream) {
    (void) stream;
    return (uint8_t) term_getc_noecho();
}
#endif

static int ptlog_write(struct ptstream* stream, uint8_t c) {
    (void) stream;
    term_putc(c);
    return 0;
}

static ptstream_t pts_log = {
    NULL,
    (size_t) -1,
    0,
#ifdef TERM_NO_INPUT
    NULL,
#else
    &ptlog_read,
#endif
    &ptlog_write
};

#ifdef KSTDERR_SER

#ifdef NO_SERIAL
#error "Serial is disabled through the NO_SERIAL macro!"
#endif

#include <hal/serial.h>

#ifndef KSTDERR_SER_PORT
#define KSTDERR_SER_PORT            0
#endif

#ifndef KSTDERR_SER_DBIT
#define KSTDERR_SER_DBIT            8
#endif

#ifndef KSTDERR_SER_SBIT
#define KSTDERR_SER_SBIT            1
#endif

#ifndef KSTDERR_SER_PARITY
#define KSTDERR_SER_PARITY          SER_PARITY_NONE
#endif

#ifndef KSTDERR_SER_BAUD
#define KSTDERR_SER_BAUD            115200UL
#endif

static uint8_t kstderr_read(struct ptstream* stream) {
    (void) stream;
    return (uint8_t) ser_getc(KSTDERR_SER_PORT);
}

static int kstderr_write(struct ptstream* stream, uint8_t c) {
    (void) stream;
    ser_putc(KSTDERR_SER_PORT, (uint8_t) c);
    return 0;
}

static ptstream_t pts_kstderr = {
    NULL,
    (size_t) -1,
    0,
    &kstderr_read,
    &kstderr_write
};
#endif

ptstream_t* kstdin = NULL;
ptstream_t* kstdout = NULL;
ptstream_t* kstderr = NULL;

void stdio_init() {
    kstdout = &pts_log;

#ifndef TERM_NO_INPUT
    kstdin = &pts_log;
#endif

#ifdef KSTDERR_SER
    ser_init(KSTDERR_SER_PORT, KSTDERR_SER_DBIT, KSTDERR_SER_SBIT, KSTDERR_SER_PARITY, KSTDERR_SER_BAUD);
    kstderr = &pts_kstderr;
#else
    kstderr = &pts_log;
#endif
}

int kputc(int c, ptstream_t* stream) {
    if(stream != NULL && stream->write != NULL) stream->write(stream, (uint8_t) c);
    return c;
}

int kputchar(int c) {
    return kputc(c, kstdout);
}

int kputs(const char* str) {
    return kprintf("%s\n", str);
}

int kprintf(const char* fmt, ...) {
    if(kstdout == NULL) return 0; // don't even bother
    va_list arg; va_start(arg, fmt);
    int ret = kvfprintf(kstdout, fmt, arg);
    va_end(arg);
    return ret;
}

int kfprintf(ptstream_t* stream, const char* fmt, ...) {
    if(stream == NULL) return 0; // don't even bother
    va_list arg; va_start(arg, fmt);
    int ret = kvfprintf(stream, fmt, arg);
    va_end(arg);
    return ret;
}

/* functions for handling string protostreams */
static int ptstr_write(struct ptstream* stream, uint8_t c) {
    if(stream->off >= stream->sz) return -1;
    *(uint8_t*)((uintptr_t) stream->buf + stream->off++) = c;
    return 0;
}

int kvsprintf(char* str, const char* fmt, va_list arg) {
    /* create stream for string */
    ptstream_t stream = {
        str,
        (size_t) -1,
        0,
        NULL,
        &ptstr_write
    };
    int ret = kvfprintf(&stream, fmt, arg);
    str[ret] = 0; // terminate the string
    return ret;
}

int ksprintf(char* str, const char* fmt, ...) {
    va_list arg; va_start(arg, fmt);
    int ret = kvsprintf(str, fmt, arg);
    va_end(arg);
    return ret;
}
