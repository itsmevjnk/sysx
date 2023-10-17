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
 * void kfree(void* ptr)
 *  Deallocates the memory block previously allocated by
 *  kmalloc(_ext)/kcalloc/krealloc.
 *  This is implemented in mm/kheap.c.
 */
void kfree(void* ptr);

#endif