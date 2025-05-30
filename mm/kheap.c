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

static size_t kheap_size = 0;
void* kmorecore(intptr_t incr) {
    if(kheap_size + incr < 0 || kheap_size + incr > KHEAP_MAX_SIZE) return (void*)UINTPTR_MAX; // cannot expand/trim further

    void* prev_brk = (void*)(KHEAP_BASE_ADDRESS + kheap_size);

    if(incr) {
        size_t old_size = kheap_size; kheap_size += incr;
        size_t framesz = pmm_framesz(); 
        size_t old_frames = (old_size + framesz - 1) / framesz, new_frames = (kheap_size + framesz - 1) / framesz; // to get difference between old and new frames
        if(old_frames < new_frames) { // expand
            uintptr_t vaddr = KHEAP_BASE_ADDRESS + old_frames * framesz; // virtual address of end of heap
            for(size_t i = 0; i < new_frames - old_frames; i++, vaddr += framesz) {
                size_t frame = pmm_alloc_free(1);
                if(frame == (size_t)-1) return (void*)UINTPTR_MAX; // out of memory
                vmm_pgmap(vmm_current, frame * framesz, vaddr, 0, VMM_FLAGS_PRESENT | VMM_FLAGS_RW | VMM_FLAGS_GLOBAL | VMM_FLAGS_CACHE); // map new frame to heap memory space
            }
        } else { // trim
            uintptr_t vaddr = KHEAP_BASE_ADDRESS + (old_frames - 1) * framesz; // virtual address of last page of heap
            for(size_t i = 0; i < old_frames - new_frames; i++, vaddr -= framesz) {
                uintptr_t paddr = vmm_get_paddr(vmm_current, vaddr);
                if(!paddr) {
                    kerror("virtual address 0x%x is not mapped", vaddr);
                    continue;
                }
                vmm_pgunmap(vmm_current, vaddr, 0); // unmap from VMM
                pmm_free(paddr / framesz); // free this frame
            }
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

