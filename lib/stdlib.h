#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stddef.h>
#include <stdint.h>

/*
 * void* kmalloc(size_t size)
 *  Attempts to allocate a contiguous memory block of the
 *  specified size and returns it, or return NULL on failure.
 */
void* kmalloc(size_t size);

/*
 * void* kmalloc_ext(size_t size, size_t align, void** phys)
 *  An extended version of kmalloc that allows for the memory
 *  block to be specified as aligned to a specific boundary
 *  as well as its physical address to be retrieved.
 *  This is implemented in lib/kheap.c.
 */
void* kmalloc_ext(size_t size, size_t align, void** phys);

/*
 * void* kcalloc(size_t nitems, size_t size)
 *  Attempts to allocate a contiguous memory block of size
 *  nitems * size, then zeroes it out and returns it, or
 *  return NULL on failure.
 */
void* kcalloc(size_t nitems, size_t size);

/*
 * void* krealloc(void* ptr, size_t size)
 *  Attempts to resize a memory block allocated by
 *  kmalloc(_ext)/kcalloc/krealloc to a new size and return
 *  its new pointer; otherwise, return NULL.
 */
void* krealloc(void* ptr, size_t size);

/*
 * void* krealloc_ext(void* ptr, size_t size, size_t align, void** phys)
 *  An extended version of krealloc that allows for alignment
 *  requirements to be required and the new physical address
 *  to be retrieved.
 *  This is implemented in lib/kheap.c.
 */
void* krealloc_ext(void* ptr, size_t size, size_t align, void** phys);

/*
 * void kfree(void* ptr)
 *  Deallocates the memory block previously allocated by
 *  kmalloc(_ext)/kcalloc/krealloc.
 *  This is implemented in lib/kheap.c.
 */
void kfree(void* ptr);

/*
 * void kheap_dump()
 *  Dumps the kernel heap to the debug terminal.
 */
void kheap_dump();

#endif