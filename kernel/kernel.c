#include <hal/serial.h>
#include <stdio.h>

void kinit() {
  ser_init(0, 8, 1, SER_PARITY_NONE, 115200);
  kputs("Hello, World!");
  while(1);
}
