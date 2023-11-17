#ifndef ADDR_H
#define ADDR_H

#include <stddef.h>
#include <stdint.h>

/* pointers to starting and ending addresses of kernel sections */
extern uintptr_t kernel_start;
extern uintptr_t kernel_end;
extern uintptr_t text_start;
extern uintptr_t text_end;
extern uintptr_t rodata_start;
extern uintptr_t rodata_end;
extern uintptr_t data_start;
extern uintptr_t data_end;
extern uintptr_t bss_start;
extern uintptr_t bss_end;
extern uintptr_t kernel_stack_top;
extern uintptr_t kernel_stack_bottom;

#endif
