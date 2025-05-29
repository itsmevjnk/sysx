#ifndef MM_KHEAP_H
#define MM_KHEAP_H

#include <stddef.h>
#include <stdint.h>

// /*
//  * void* kheap_alloc(size_t size, size_t align)
//  *  An extended version of kmalloc that allows for the memory
//  *  block to be specified as aligned to a specific boundary.
//  */
// void* kheap_alloc(size_t size, size_t align);

// /*
//  * void* kheap_realloc(void* ptr, size_t size, size_t align)
//  *  An extended version of krealloc that allows for alignment
//  *  requirements to be required.
//  *  This is implemented in lib/kheap.c.
//  */
// void* kheap_realloc(void* ptr, size_t size, size_t align);

// /*
//  * void kheap_dump()
//  *  Dumps the kernel heap to the debug terminal.
//  */
// void kheap_dump();

/*
 * void* kmorecore(intptr_t incr)
 *  Implementation of the sbrk function for dlmalloc.
 */
void* kmorecore(intptr_t incr);

#endif
