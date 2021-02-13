#ifndef X86CPU_GDT_H
#define X86CPU_GDT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * void gdt_add_entry(uint8_t entry, uintptr_t base, uintptr_t limit, bool rw, bool dc,
 *                    bool exec, bool type, uint8_t privl, bool size, bool granularity)
 *  Adds an entry to the GDT with the specified properties.
 */
void gdt_add_entry(uint8_t entry, uintptr_t base, uintptr_t limit, bool rw, bool dc, bool exec, bool type, uint8_t privl, bool size, bool granularity);

/*
 * void gdt_init()
 *  Initializes and loads the GDT.
 */
void gdt_init();

#endif
