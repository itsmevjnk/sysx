#include <kernel/log.h>

#include <hal/terminal.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdio.h>

extern int ktgtinit(); // must be defined somewhere in the target specific code

void kinit() {
    term_init();
    stdio_init();

    kinfo("SysX version 0.0.1 prealpha (compiled %s %s)", __DATE__, __TIME__);
    kinfo("Copyright <C> 2023 Thanh Vinh Nguyen (itsmevjnk)");

    kinfo("initializing physical memory management");
    pmm_init();
    kinfo("initializing virtual memory management");
    vmm_init();

    kinfo("invoking target-specific system initialization routine");
    if(ktgtinit()) {
        kinfo("ktgtinit() failed, committing suicide");
        return; // this should send us into an infinite loop prepared by the bootstrap code
    }
    while(1);
}
