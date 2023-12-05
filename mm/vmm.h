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
 * void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, size_t flags)
 *  Maps a single page from pa to va.
 *  pa and va are assumed to be page-aligned addresses.
 */
void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, size_t flags);

/* flags for vmm_pgmap and vmm_map */
#define VMM_FLAGS_PRESENT           (1 << 0) // set if page is present
#define VMM_FLAGS_RW                (1 << 1) // set if page is writable
#define VMM_FLAGS_USER              (1 << 2) // set if page is accessible in userland
#define VMM_FLAGS_GLOBAL            (1 << 3) // set if mapping is global (TLB cached across VMM switches)
#define VMM_FLAGS_CACHE             (1 << 4) // set if page can be cached
#define VMM_FLAGS_CACHE_WTHRU       (1 << 5) // set if page is write-through instead of write-back cached

/*
 * void vmm_pgunmap(void* vmm, uintptr_t va)
 *  Unmaps a single page corresponding to virtual address va.
 *  pa and va are assumed to be page-aligned addresses.
 */
void vmm_pgunmap(void* vmm, uintptr_t va);

/*
 * size_t vmm_get_flags(void* vmm, uintptr_t va)
 *  Retrieves the flags for the specified page.
 *  Returns 0 if the address is not mapped.
 */
size_t vmm_get_flags(void* vmm, uintptr_t va);

/*
 * void vmm_set_flags(void* vmm, uintptr_t va, size_t flags)
 *  Sets the flags for the specified page if it's mapped.
 */
void vmm_set_flags(void* vmm, uintptr_t va, size_t flags);

/*
 * uintptr_t vmm_get_paddr(void* vmm, uintptr_t va)
 *  Retrieves the physical memory address of a given virtual
 *  address.
 *  Returns 0 if the virtual address is unmapped.
 */
uintptr_t vmm_get_paddr(void* vmm, uintptr_t va);

/*
 * void vmm_set_paddr(void* vmm, uintptr_t va, uintptr_t pa)
 *  Sets the physical memory address of a given virtual
 *  address.
 */
void vmm_set_paddr(void* vmm, uintptr_t va, uintptr_t pa);

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
 * uintptr_t vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, size_t flags)
 *  Maps sz byte(s) starting from linear address pa to virtual
 *  address va with the specified properties.
 *  Returns the mapped address for pa.
 */
uintptr_t vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, size_t flags);

/*
 * void vmm_unmap(void* vmm, uintptr_t va, size_t sz)
 *  Unmaps sz byte(s) starting from linear address va.
 */
void vmm_unmap(void* vmm, uintptr_t va, size_t sz);

/*
 * uintptr_t vmm_first_free(void* vmm, uintptr_t va_start, uintptr_t va_end, size_t sz, bool reverse)
 *  Finds the first unmapped contiguous address space of size sz
 *  between va_start and va_end inclusive in the specified VMM
 *  configuration. If reverse is set, the function will attempt
 *  to find a space towards va_end.
 *  Returns the space's starting virtual address, or 0 if none
 *  can be found.
 */
uintptr_t vmm_first_free(void* vmm, uintptr_t va_start, uintptr_t va_end, size_t sz, bool reverse);

/*
 * uintptr_t vmm_alloc_map(void* vmm, uintptr_t pa, size_t sz, uintptr_t va_start, uintptr_t va_end, bool reverse, size_t flags)
 *  Finds a contiguous address space sufficient for mapping the
 *  physical memory range specified by pa and sz and maps the
 *  range.
 *  Returns the space's starting virtual address, corresponding
 *  to pa, or 0 if such a space cannot be found.
 */
uintptr_t vmm_alloc_map(void* vmm, uintptr_t pa, size_t sz, uintptr_t va_start, uintptr_t va_end, bool reverse, size_t flags);

/* list of page traps (pages destined to cause a fault and to be handled by the kernel) */
enum vmm_trap_type {
    VMM_TRAP_NONE = 0,
    VMM_TRAP_COW // copy-on-write page - info points to the page's source entry
};
typedef struct {
    enum vmm_trap_type type;
    void* vmm;
    uintptr_t vaddr;
    void* info;
} vmm_trap_t;

/*
 * size_t vmm_cow_setup(void* vmm_src, uintptr_t vaddr_src, void* vmm_dst, uintptr_t vaddr_dst, size_t size)
 *  Sets up copy-on-write (COW) between the two specified address
 *  ranges.
 *  Returns the set-up address range size.
 */
size_t vmm_cow_setup(void* vmm_src, uintptr_t vaddr_src, void* vmm_dst, uintptr_t vaddr_dst, size_t size);

/*
 * bool vmm_cow_duplicate(void* vmm, uintptr_t vaddr)
 *  Resolves the COW order on the specified page if it has one.
 */
bool vmm_cow_duplicate(void* vmm, uintptr_t vaddr);

/*
 * bool vmm_handle_fault(uintptr_t vaddr, size_t flags)
 *  Handles a page fault exception occurring on the specified
 *  virtual address, with access details (whether the page is
 *  present, writable or user-accessible) in the flags
 *  parameter (using the same bits as vmm_map).
 *  Returns true if the fault is gracefully handled, or false
 *  otherwise.
 */
bool vmm_handle_fault(uintptr_t vaddr, size_t flags);

#endif
