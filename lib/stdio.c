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

/* functions for handling protostreams */
static char ptread(ptstream_t* stream) {
  if(stream->read == NULL) return 0;
  return (char) stream->read((struct ptstream*) stream); /* since we'll be mostly dealing with char */
}

static int ptwrite(ptstream_t* stream, char c /* since we'll be mostly dealing with char */) {
  if(stream->write == NULL) return -1;
  return stream->write((struct ptstream*) stream, (uint8_t) c);
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
  *(uint8_t*)((uintptr_t) stream->buf + stream->off++) = c;
  return 0;
}

/*
 * int ksprintf(char* str, const char* fmt, va_list arg)
 *  Writes and formats the string pointed by fmt to the buffer
 *  str. Returns the number of characters printed.
 */
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

/* here comes the pain - stop scrolling if you value your sanity */

/* internal isdigit(), TODO: backport to ctype.h */
static inline int _isdigit(int c) {
  if(c >= '0' && c <= '9') return 1;
  return 0;
}

/* internal atoi(), TODO: backport to stdlib.h */
static intmax_t _atoi(const char* str) {
  intmax_t ret = 0, sign = 1;
  if(*str == '-') {
    sign = -1;
    str++;
  }
  if(*str == '+') str++; // skip
  while(_isdigit(*str)) {
    ret = ret * 10 + *str - '0';
    str++;
  }
  return (ret * sign);
}

/* internal itoa(), TODO: backport to stdlib.h */
static const char _itoa_lut[37] = "0123456789abcdefghijklmnopqrstuvwxyz";
static char* _itoa(intmax_t value, char* str, int base) {
  char* s = str;
  if(base == 10 && value < 0) {
    value = -value;
    *(s++) = '-';
  }
  size_t len = 0, v = (size_t) value;
  while(value) {
    s[len++] = _itoa_lut[v % base];
    v /= base;
  }
  s[len] = 0;
  for(size_t i = 0; i < (len / 2); i++) {
    char t = s[i];
    s[i] = s[len - i - 1];
    s[len - i - 1] = t;
  }
  return str;
}

/* internal flags */
#define FLG_LEFT        (1 << 0) // left justify if field width is given
#define FLG_SIGN        (1 << 1) // forces plus sign for positive numbers
#define FLG_HASH        (1 << 2) // add base prefix (o,x,X) or force decimal point (a,A,e,E,f,F,g,G)
#define FLG_BLANK       (1 << 3) // insert space before value if no sign is going to be written
/* length types */
#define LEN_NONE        0
#define LEN_HH          1
#define LEN_H           2
#define LEN_LL          3
#define LEN_L           4
#define LEN_J           5
#define LEN_Z           6
#define LEN_T           7
#define LEN_LCAP        8

/* PLEASE COME BACK AND DEVELOP FURTHER HHHHHHHH */
int kvfprintf(ptstream_t* stream, const char* fmt, va_list arg) {
  int written = 0; // for keeping track of the number of characters written
  char buf[128]; // buffer to work on

  while(*fmt) {
    /* read every character of the format string */
    if(*fmt != '%') {
      if(ptwrite(stream, *fmt)) return written; // write failure, end now
      written++; fmt++; continue; // move on to the next character
    }
    /* we've hit the % character, check the next one */
    fmt++;

    /* first of all, check flags */
    size_t flags = 0; char padding = ' ';
    while(*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#' || *fmt == '0') {
      switch(*fmt) {
        case '-': flags |= FLG_LEFT; break;
        case '+': flags |= FLG_SIGN; break;
        case '#': flags |= FLG_HASH; break;
        case ' ': flags |= FLG_BLANK; break;
        case '0': padding = '0'; break;
        default: break;
      }
      fmt++; // next flag (supposedly)
    }

    /* check width field */
    int width = -1;
    if(*fmt == '*') {
      /* width is given in arg */
      width = va_arg(arg, int); // TODO: is it int or size_t?
      fmt++;
    } else if(_isdigit(*fmt)) {
      /* width is given in fmt */
      width = _atoi(fmt);
      while(_isdigit(*fmt)) fmt++; // skip past the width field
    }

    /* check precision field */
    int precision = -1;
    if(*fmt == '.') {
      /* yep, we have a valid precision field */
      fmt++;
      if(*fmt == '*') {
        /* precision is given in arg */
        precision = va_arg(arg, int); // TODO: is it int or size_t?
        fmt++;
      } else if(_isdigit(*fmt)) {
        /* precision is given in fmt */
        precision = _atoi(fmt);
        while(_isdigit(*fmt)) fmt++; // skip past the width field
      }
    }

    /* check length field, TODO: find a better way of doing this */
    size_t len = LEN_NONE;
    switch(*fmt) {
      case 'h': /* hh or h */
        fmt++;
        if(*fmt == 'h') { /* hh */
          len = LEN_HH;
          fmt++;
        } else len = LEN_H; /* h */
        break;
      case 'l': /* ll or l */
        fmt++;
        if(*fmt == 'l') { /* ll */
          len = LEN_LL;
          fmt++;
        } else len = LEN_L; /* l */
        break;
      case 'j': len = LEN_J; break;
      case 'z': len = LEN_Z; break;
      case 't': len = LEN_T; break;
      case 'L': len = LEN_LCAP; break;
      default: break;
    }

    /* process the specifier and do all the formatting magic */
    if(*fmt == 's') {
      /* string of characters */
      const char* s = va_arg(arg, const char*);
      if(!(flags & FLG_LEFT) && (int) strlen(s) < width) {
        for(size_t i = 0; i < (width - strlen(s)); i++) {
          if(ptwrite(stream, ' ')) return written;
          written++;
        }
      }
      for(size_t i = 0; s[i]; i++) {
        if(ptwrite(stream, s[i])) return written;
        written++;
      }
      if((flags & FLG_LEFT) && (int) strlen(s) < width) {
        for(size_t i = 0; i < (width - strlen(s)); i++) {
          if(ptwrite(stream, ' ')) return written;
          written++;
        }
      }
    }

next_char:
    fmt++; // next!
  }

  return written;
}
