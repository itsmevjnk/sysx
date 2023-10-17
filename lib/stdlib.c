#include <stdlib.h>
#include <string.h>
#include <mm/kheap.h>

void* kmalloc(size_t size) {
    return kmalloc_ext(size, 1, NULL);
}

void* kcalloc(size_t nitems, size_t size) {
    void* ptr = kmalloc(nitems * size);
    if(ptr != NULL) memset(ptr, 0, nitems * size);
    return ptr;
}

void* krealloc(void* ptr, size_t size) {
    return krealloc_ext(ptr, size, 1, NULL);
}