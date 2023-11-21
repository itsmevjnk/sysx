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
 * void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, bool present, bool user, bool rw, uint8_t caching, bool global)
 *  Maps a single page from pa to va.
 */
void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, bool present, bool user, bool rw, uint8_t caching, bool global);

/* caching configuration constants (for vmm_pgmap and vmm_map) */
#define VMM_CACHE_NONE                      0 // no caching
#define VMM_CACHE_WTHRU                     1 // write-through caching (ensures consistency at the cost of performance)
#define VMM_CACHE_WBACK                     2 // write-back caching (consistency not guaranteed, does not block CPU)

/*
 * void vmm_pgunmap(void* vmm, uintptr_t va)
 *  Unmaps a single page corresponding to virtual address va.
 */
void vmm_pgunmap(void* vmm, uintptr_t va);

/*
 * uintptr_t vmm_physaddr(uintptr_t va)
 *  Retrieves the physical memory address of a given virtual
 *  address.
 *  Returns 0 if the virtual address is unmapped.
 */
uintptr_t vmm_physaddr(void* vmm, uintptr_t va);

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

/*
 * void* vmm_clone(void* src)
 *  Creates a new VMM configuration from the specified source.
 *  Returns NULL on failure.
 */
void* vmm_clone(void* src);

/*
 * void vmm_free(void* vmm)
 *  Deallocates the specified VMM configuration.
 */
void vmm_free(void* vmm);

/* generic code */

/*
 * void* vmm_current
 *  Points to the current MMU configuration.
 */
extern void* vmm_current;

/*
 * void* vmm_kernel
 *  Points to the kernel's MMU configuration.
 */
extern void* vmm_kernel;

/*
 * void vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, bool present, bool user, bool rw, uint8_t caching, bool global)
 *  Maps sz byte(s) starting from linear address pa to virtual
 *  address va with the specified properties.
 */
void vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, bool present, bool user, bool rw, uint8_t caching, bool global);

/*
 * void vmm_unmap(void* vmm, uintptr_t va, size_t sz)
 *  Unmaps sz byte(s) starting from linear address va.
 */
void vmm_unmap(void* vmm, uintptr_t va, size_t sz);

/*
 * uintptr_t vmm_first_free(void* vmm, uintptr_t va, size_t sz)
 *  Finds the first unmapped contiguous address space of size sz
 *  starting from the virtual address va in the given VMM config.
 *  Returns the space's starting virtual address, or 0 if none
 *  can be found.
 */
uintptr_t vmm_first_free(void* vmm, uintptr_t va, size_t sz);

#endif
