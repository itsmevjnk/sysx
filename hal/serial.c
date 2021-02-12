/* architecture independent portion */
#include <hal/serial.h>

#ifndef NO_SERIAL

void ser_puts(size_t p, const char* s) {
  while(*s) {
    ser_putc(p, *s);
    s++;
  }
}

size_t ser_getbuf(size_t p, char* buf, char stop) {
  size_t read = 0;
  while(1) {
    char t = ser_getc(p);
    if(t == stop) break;
    buf[read++] = t;
  }
  return read;
}

char* ser_gets(size_t p, char* buf) {
  buf[ser_getbuf(p, buf, '\n')] = 0;
  return buf;
}

#endif
