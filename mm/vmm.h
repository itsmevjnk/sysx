#ifndef MMU_H
#define MMU_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* to be implemented by CPU-specific code */

/*
 * size_t vmm_pgsz_num()
 *  Returns the number of page sizes available.
 */
size_t vmm_pgsz_num();

/*
 * size_t vmm_pgsz(size_t idx)
 *  Returns the specified supported page size index.
 *  idx is a number ranging from 0 to vmm_pgsz_num() - 1,
 *  and specifies a page size in an ascending list.
 */
size_t vmm_pgsz(size_t idx);

/*
 * void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, size_t pgsz_idx, size_t flags)
 *  Maps a single page from pa to va. pgsz_idx specifies the page's
 *  size, and is the index passed into vmm_pgsz().
 *  pa and va are assumed to be page-aligned addresses.
 */
void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, size_t pgsz_idx, size_t flags);

/* flags for vmm_pgmap and vmm_map */
#define VMM_FLAGS_PRESENT           (1 << 0) // set if page is present
#define VMM_FLAGS_RW                (1 << 1) // set if page is writable
#define VMM_FLAGS_USER              (1 << 2) // set if page is accessible in userland
#define VMM_FLAGS_GLOBAL            (1 << 3) // set if mapping is global (TLB cached across VMM switches)
#define VMM_FLAGS_CACHE             (1 << 4) // set if page can be cached
#define VMM_FLAGS_CACHE_WTHRU       (1 << 5) // set if page is write-through instead of write-back cached
#define VMM_FLAGS_TRAPPED           (1 << 6) // set if there's a trap set up for the page

/*
 * void vmm_pgunmap(void* vmm, uintptr_t va, size_t pgsz_idx)
 *  Unmaps a single page corresponding to virtual address va.
 *  Similarly to vmm_pgunmap, the page's size is specified via
 *  pgsz_idx.
 *  pa and va are assumed to be page-aligned addresses.
 */
void vmm_pgunmap(void* vmm, uintptr_t va, size_t pgsz_idx);

/*
 * size_t vmm_get_pgsz(void* vmm, uintptr_t va)
 *  Gets the size of the page containing the specified virtual address.
 *  Returns the size index, or -1 if the page is not mapped.
 */
size_t vmm_get_pgsz(void* vmm, uintptr_t va);

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
 *  Sets the physical memory address of a given mapped virtual
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
 * void* vmm_clone(void* src, bool cow)
 *  Creates a new VMM configuration from the specified source.
 *  The cow parameter specifies whether frames mapped in userland
 *  address space shall be copy-on-write instead of being referenced
 *  in the new VMM configuration.
 *  Returns NULL on failure.
 */
void* vmm_clone(void* src, bool cow);

/*
 * void vmm_free(void* vmm)
 *  Deallocates the specified VMM configuration.
 */
void vmm_free(void* vmm);

/*
 * bool vmm_get_dirty(void* vmm, uintptr_t va)
 *  Checks whether the page corresponding to the specified virtual
 *  address is dirty (i.e. written to).
 */
bool vmm_get_dirty(void* vmm, uintptr_t va);

/*
 * void vmm_set_dirty(void* vmm, uintptr_t va, bool dirty)
 *  Sets or reset the dirty (write accessed) flag for the page
 *  corresponding to the specified virtual address.
 */
void vmm_set_dirty(void* vmm, uintptr_t va, bool dirty);

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
 * uintptr_t vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, size_t pgsz_max_idx, size_t flags)
 *  Maps sz byte(s) starting from linear address pa to virtual
 *  address va with the specified properties. The maximum page size
 *  to be used is specified via pgsz_max_idx.
 *  Returns the mapped address for pa.
 */
uintptr_t vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, size_t pgsz_max_idx, size_t flags);

/*
 * void vmm_unmap(void* vmm, uintptr_t va, size_t sz)
 *  Unmaps sz byte(s) starting from linear address va.
 */
void vmm_unmap(void* vmm, uintptr_t va, size_t sz);

/*
 * uintptr_t vmm_first_free(void* vmm, uintptr_t va_start, uintptr_t va_end, size_t sz, size_t align, bool reverse)
 *  Finds the first unmapped contiguous address space of size sz
 *  between va_start and va_end inclusive in the specified VMM
 *  configuration. If reverse is set, the function will attempt
 *  to find a space towards va_end. The virtual address' boundary
 *  alignment (which must be a multiple of the minimum page size)
 *  can also be specified; if not, align must be set to 0.
 *  Returns the space's starting virtual address, or 0 if none
 *  can be found.
 */
uintptr_t vmm_first_free(void* vmm, uintptr_t va_start, uintptr_t va_end, size_t sz, size_t align, bool reverse);

/*
 * uintptr_t vmm_alloc_map(void* vmm, uintptr_t pa, size_t sz, uintptr_t va_start, uintptr_t va_end, size_t va_align, size_t pgsz_max_idx, bool reverse, size_t flags) 
 *  Finds a contiguous address space sufficient for mapping the
 *  physical memory range specified by pa and sz and maps the
 *  range. The virtual address' boundary alignment (which must be
 *  a multiple of the minimum page size) can also be specified; if
 *  not, va_align is to be set to 0.
 *  Returns the space's starting virtual address, corresponding
 *  to pa, or 0 if such a space cannot be found.
 */
uintptr_t vmm_alloc_map(void* vmm, uintptr_t pa, size_t sz, uintptr_t va_start, uintptr_t va_end, size_t va_align, size_t pgsz_max_idx, bool reverse, size_t flags);

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
 * bool vmm_cow_duplicate(void* vmm, uintptr_t vaddr, size_t pgsz)
 *  Resolves the COW order on the specified page if it has one.
 *  The pgsz parameter allows the caller to pre-specify the page's
 *  size index (see vmm_pgsz); if this is set to -1, the function will 
 *  call vmm_get_pgsz() to find out the actual page size.
 */
bool vmm_cow_duplicate(void* vmm, uintptr_t vaddr, size_t pgsz);

/*
 * bool vmm_trap_remove(void* vmm)
 *  Removes the specified VMM structure's relations in the traps list.
 *  This includes the following:
 *   - Resolving the first CoW order of each address whose source is vmm
 *     and pointing any subsequent order to that VMM structure.
 *   - Deleting traps that apply on vmm.
 *  This function is to be called by vmm_free before deallocating vmm,
 *  and wil return true on success.
 */
bool vmm_trap_remove(void* vmm);

/*
 * vmm_trap_t* vmm_is_cow(void* vmm, uintptr_t vaddr, bool validated)
 *  Checks if the specified virtual address is mapped as CoW.
 *  The validated flag indicates whether the address is guaranteed
 *  to be mapped and aligned to its page size.
 *  Returns the first CoW trap entry found, or NULL if there's none.
 */
vmm_trap_t* vmm_is_cow(void* vmm, uintptr_t vaddr, bool validated);

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

/*
 * void vmm_stage_free(void* vmm)
 *  Stages the specified VMM configuration for deletion after
 *  switching out of it.
 */
void vmm_stage_free(void* vmm);

/*
 * void vmm_do_cleanup()
 *  Deallocates all VMM configurations that have been staged
 *  and are eligible for deletion.
 */
void vmm_do_cleanup();

#endif
