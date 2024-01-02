#ifndef ARCH_X86_BIOS_H
#define ARCH_X86_BIOS_H

#include <stddef.h>
#include <stdint.h>

extern uintptr_t bios_ebda_paddr; // EBDA physical address - initialized by bios_get_ebda_paddr

/*
 * void* bios_find_tab(const char* sig, size_t sig_len, uintptr_t range_paddr, size_t range_len, size_t boundary, uintptr_t va_start)
 *  Finds a table starting with the specified signature at a specified physical
 *  address range. The signature is expected to reside on a boundary specified by
 *  the boundary parameter, and the physical address range will be mapped to an
 *  unused virtual address range starting from va_start.
 */
void* bios_find_tab(const char* sig, size_t sig_len, uintptr_t range_paddr, size_t range_len, size_t boundary, uintptr_t va_start);

/*
 * uintptr_t bios_get_ebda_paddr()
 *  Returns the EBDA's physical address and sets the bios_ebda_paddr variable.
 */
uintptr_t bios_get_ebda_paddr();

#endif
