#include <kernel/log.h>

#include <hal/terminal.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

extern int ktgtinit(); // must be defined somewhere in the target specific code

/* test kernel heap */
void test_kernel_heap() {
    kinfo("testing kernel heap, check debug terminal for information");

    /* malloc */
    void* ptr1 = kmalloc(32);
    memset(ptr1, 0x55, 32); // for data integrity check later
    kdebug("kmalloc 32 bytes = 0x%x", (uintptr_t) ptr1);
    kheap_dump();

    /* malloc_ext */
    void* ptr2_phys;
    void* ptr2 = kmalloc_ext(42, 512, &ptr2_phys);
    kdebug("kmalloc_ext 42 bytes w/ 512-byte align = 0x%x (phys 0x%x)", (uintptr_t) ptr2, (uintptr_t) ptr2_phys);
    kheap_dump();

    /* realloc */
    uint8_t* ptr1_new = krealloc(ptr1, 53);
    kdebug("krealloc memory @ 0x%x to 53 bytes = 0x%x", (uintptr_t) ptr1, (uintptr_t) ptr1_new);
    for(size_t i = 0; i < 32; i++) {
        if(ptr1_new[i] != 0x55) kdebug("   data mismatch @ idx %u (0x%x)", i, (uintptr_t) &ptr1_new[i]);
    }
    kheap_dump();
    ptr1 = ptr1_new;

    /* realloc_ext approach 2 */
    void* ptr1_phys;
    ptr1_new = krealloc_ext(ptr1, 16, 4096, &ptr1_phys);
    kdebug("krealloc_ext memory @ 0x%x to 16 bytes w/ 4096-byte align = 0x%x (phys 0x%x)", (uintptr_t) ptr1, (uintptr_t) ptr1_new, (uintptr_t) ptr1_phys);
    for(size_t i = 0; i < 16; i++) {
        if(ptr1_new[i] != 0x55) kdebug("   data mismatch @ idx %u (0x%x)", i, (uintptr_t) &ptr1_new[i]);
    }
    kheap_dump();
    ptr1 = ptr1_new;

    /* kfree */
    kfree(ptr1);
    kdebug("freed memory @ 0x%x", (uintptr_t) ptr1);
    kheap_dump();
    // we don't free ptr2 so there's a bit more variety

    kinfo("kernel heap testing complete");
}

void kinit() {
#ifdef KINIT_MM_FIRST // initialize MM first
    pmm_init();
    vmm_init();
#endif
    
    term_init();
    stdio_init();

    kinfo("SysX version 0.0.1 prealpha (compiled %s %s)", __DATE__, __TIME__);
    kinfo("Copyright <C> 2023 Thanh Vinh Nguyen (itsmevjnk)");

#ifndef KINIT_MM_FIRST
    kinfo("initializing physical memory management");
    pmm_init();
    kinfo("initializing virtual memory management");
    vmm_init();
#endif

    kinfo("invoking target-specific system initialization routine");
    if(ktgtinit()) {
        kinfo("ktgtinit() failed, committing suicide");
        return; // this should send us into an infinite loop prepared by the bootstrap code
    }

#ifndef TERM_NO_XY
    /* display terminal dimensions */
    size_t term_width, term_height;
    term_get_dimensions(&term_width, &term_height);
    kinfo("terminal size: %u x %u", term_width, term_height);
#endif
    
    test_kernel_heap();

    while(1);
}
