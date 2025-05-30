#ifndef MM_KHEAP_H
#define MM_KHEAP_H

#include <stddef.h>
#include <stdint.h>

/*
 * void* kmorecore(intptr_t incr)
 *  Implementation of the sbrk function for dlmalloc.
 */
void* kmorecore(intptr_t incr);

/*
 * size_t kheap_get_size()
 *  Get the size allocated for the kernel heap in bytes.
 *  Throughout the kernel's lifespan, this size may increase from
 *  positive kmorecore calls, or decrease from negative kmorecore
 *  (trim) calls.
 */
size_t kheap_get_size();

#endif
