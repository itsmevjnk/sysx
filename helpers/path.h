#ifndef HELPERS_PATH_H
#define HELPERS_PATH_H

#include <stddef.h>
#include <stdint.h>

/*
 * const char* path_traverse(const char* path, size_t* len)
 *  Traverse the given path string, returning the pointer to the next
 *  element in the path string and optionally the last element's length
 *  via the len pointer.
 */
const char* path_traverse(const char* path, size_t* len);

#endif
