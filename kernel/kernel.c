#include <kernel/log.h>

#include <hal/serial.h>
#include <stdio.h>

extern int ktgtinit(); // must be defined somewhere in the target specific code

void kinit() {
  ser_init(0, 8, 1, SER_PARITY_NONE, 115200);
  kinfo("SysX version 0.0.1 prealpha (compiled %s %s)", __DATE__, __TIME__);
  kinfo("Copyright <C> 2021 Vinh T. Nguyen");

  kinfo("invoking target-specific system initialization routine");
  if(ktgtinit()) {
    kinfo("ktgtinit() failed, committing suicide");
    return; // this should send us into an infinite loop prepared by the bootstrap code
  }
  while(1);
}
