#ifndef MMU_H
#define MMU_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* to be implemented by CPU-specific code */

/*
 * size_t vmm_pgsz()
 *  Returns the size of a MMU page.
 */
size_t vmm_pgsz();

/*
 * void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, bool present, bool user, bool rw)
 *  Maps a single page from pa to va.
 */
void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, bool present, bool user, bool rw);

/*
 * void vmm_pgunmap(void* vmm, uintptr_t va)
 *  Unmaps a single page corresponding to virtual address va.
 */
void vmm_pgunmap(void* vmm, uintptr_t va);

/*
 * void vmm_switch(void* vmm)
 *  Switches to the specified MMU configuration.
 */
void vmm_switch(void* vmm);

/*
 * void vmm_init()
 *  Initializes the MMU.
 */
void vmm_init();

/* generic code */

/*
 * void* vmm_current
 *  Points to the current MMU configuration.
 */
extern void* vmm_current;

/*
 * void vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, bool present, bool user, bool rw)
 *  Maps sz byte(s) starting from linear address pa to virtual
 *  address va with the specified properties.
 */
void vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, bool present, bool user, bool rw);

/*
 * void vmm_unmap(void* vmm, uintptr_t va, size_t sz)
 *  Unmaps sz byte(s) starting from linear address va.
 */
void vmm_unmap(void* vmm, uintptr_t va, size_t sz);

#endif
