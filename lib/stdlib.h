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

/*
 * uint64_t strtoull(const char* str, char** endptr, int base)
 *  Converts a base-n representation of an integer (whose base
 *  can be auto-detected if base = 0) into a 64-bit unsigned
 *  integer, and optionally store the pointer to the first non-
 *  numeric character of the string in endptr.
 *  Returns the converted integer on success, or UINT64_MAX if
 *  the integer is out of uint64_t range.
 */
uint64_t strtoull(const char* str, char** endptr, int base);

/*
 * int64_t strtoll(const char* str, char** endptr, int base)
 *  Converts a base-n representation of an integer (whose base
 *  can be auto-detected if base = 0) into a 64-bit signed
 *  integer, and optionally store the pointer to the first non-
 *  numeric character of the string in endptr.
 *  Returns the converted integer on success, or INT64_MAX/_MIN
 *  if the integer is out of int64_t range.
 */
int64_t strtoll(const char* str, char** endptr, int base);

/*
 * uint32_t strtoul(const char* str, char** endptr, int base)
 *  Converts a base-n representation of an integer (whose base
 *  can be auto-detected if base = 0) into a 32-bit unsigned
 *  integer, and optionally store the pointer to the first non-
 *  numeric character of the string in endptr.
 *  Returns the converted integer on success, or UINT64_MAX if
 *  the integer is out of uint32_t range.
 */
uint32_t strtoul(const char* str, char** endptr, int base);

/*
 * int32_t strtol(const char* str, char** endptr, int base)
 *  Converts a base-n representation of an integer (whose base
 *  can be auto-detected if base = 0) into a 32-bit signed
 *  integer, and optionally store the pointer to the first non-
 *  numeric character of the string in endptr.
 *  Returns the converted integer on success, or INT32_MAX/_MIN
 *  if the integer is out of int32_t range.
 */
int32_t strtol(const char* str, char** endptr, int base);

#define RAND_MAX                            32767

/*
 * void srand(unsigned int seed)
 *  Initializes the pseudorandom number generator.
 */
void srand(unsigned int seed);

/*
 * int rand()
 *  Generates a pseudorandom number.
 */
int rand();

#endif