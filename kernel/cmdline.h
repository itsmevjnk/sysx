#ifndef KERNEL_CMDLINE_H
#define KERNEL_CMDLINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern size_t kernel_argc; // number of kernel_argv elements
extern char** kernel_argv;

/*
 * void cmdline_init(const char* str)
 *  Parses the specified cmdline string and set kernel_argc and
 *  kernel_argv accordingly.
 */
void cmdline_init(const char* str);

/*
 * bool cmdline_find_flag(const char* key)
 *  Finds a specified flag in the kernel cmdline and returns whether
 *  such flag exists.
 */
bool cmdline_find_flag(const char* key);

/*
 * const char* cmdline_find_kvp(const char* key)
 *  Finds a specified key=value pair in the kernel cmdline and returns
 *  the value's pointer if it's found, or NULL if such key doesn't
 *  exist.
 */
const char* cmdline_find_kvp(const char* key);

#endif
