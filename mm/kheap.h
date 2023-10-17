#ifndef MM_KHEAP_H
#define MM_KHEAP_H

#include <stddef.h>
#include <stdint.h>

/*
 * void* kmalloc_ext(size_t size, size_t align, void** phys)
 *  An extended version of kmalloc that allows for the memory
 *  block to be specified as aligned to a specific boundary
 *  as well as its physical address to be retrieved.
 */
void* kmalloc_ext(size_t size, size_t align, void** phys);

/*
 * void* krealloc_ext(void* ptr, size_t size, size_t align, void** phys)
 *  An extended version of krealloc that allows for alignment
 *  requirements to be required and the new physical address
 *  to be retrieved.
 *  This is implemented in lib/kheap.c.
 */
void* krealloc_ext(void* ptr, size_t size, size_t align, void** phys);

/*
 * void kheap_dump()
 *  Dumps the kernel heap to the debug terminal.
 */
void kheap_dump();

#endif
