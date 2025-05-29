#include <mm/kheap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/malloc.h>
#include <stdlib.h>
#include <kernel/log.h>
#include <stdbool.h>
#include <string.h>

#ifndef KHEAP_BASE_ADDRESS
extern uintptr_t __kheap_start;
#define KHEAP_BASE_ADDRESS                          (uintptr_t)&__kheap_start // base address for the kernel heap - use config from linker script
#endif

#ifndef KHEAP_MAX_SIZE
extern uintptr_t __kheap_end;
#define KHEAP_MAX_SIZE                              ((size_t)&__kheap_end - (size_t)&__kheap_start) // kernel heap's maximum size in bytes - use config from linker script
#endif

#ifndef KHEAP_BLOCK_MIN_SIZE
#define KHEAP_BLOCK_MIN_SIZE                        4 // minimum size of a memory block (to prevent fragmentation)
#endif

static uintptr_t kheap_brk = KHEAP_BASE_ADDRESS;
void* kmorecore(intptr_t incr) {
    if(incr < 0) return (void*)UINTPTR_MAX; // MORECORE does not need to handle negative

    intptr_t size = kheap_brk - KHEAP_BASE_ADDRESS; // previous heap size
    if(size + incr < 0 || size + incr > KHEAP_MAX_SIZE) return (void*)UINTPTR_MAX; // cannot expand further

    void* prev_brk = (void*) kheap_brk;

    if(incr) {
        size_t framesz = pmm_framesz(); incr = (incr + framesz - 1) / framesz; // convert incr to frames
        for(size_t i = 0; i < incr; i++, kheap_brk += framesz) {
            size_t frame = pmm_alloc_free(1);
            if(frame == (size_t)-1) return (void*)UINTPTR_MAX; // out of memory
            vmm_pgmap(vmm_current, frame * framesz, kheap_brk, 0, VMM_FLAGS_PRESENT | VMM_FLAGS_RW | VMM_FLAGS_GLOBAL | VMM_FLAGS_CACHE); // map new frame to heap memory space
        }
    }

    return prev_brk;
}

void* kmalloc(size_t size) {
    return dlmalloc(size);
}

void* krealloc(void* ptr, size_t size) {
    return dlrealloc(ptr, size);
}

void* kmemalign(size_t alignment, size_t size) {
    return dlmemalign(alignment, size);
}

void kfree(void* ptr) {
    dlfree(ptr);
}

