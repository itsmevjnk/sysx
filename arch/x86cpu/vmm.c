#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/addr.h>
#include <mm/kheap.h>
#include <kernel/log.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <arch/x86cpu/task.h>
#include <exec/process.h>

/* MMU data types */
typedef union {
	struct {
		size_t present : 1;
		size_t rw : 1;
		size_t user : 1;
		size_t wthru : 1;
		size_t ncache : 1;
		size_t accessed : 1;
		size_t zero : 1;
		size_t pgsz : 1;
		size_t avail : 4;
		uintptr_t pt : 20; // page table ptr >> 12
	} __attribute__((packed)) entry;
	struct {
		size_t present : 1;
		size_t rw : 1;
		size_t user : 1;
		size_t wthru : 1;
		size_t ncache : 1;
		size_t accessed : 1;
		size_t dirty : 1;
		size_t pgsz : 1;
		size_t global : 1;
		size_t avail : 3;
		size_t pat : 1;
		uintptr_t pa_ext : 8; // bits 32-39 of address
		size_t zero : 1;
		uintptr_t pa : 10; // bits 22-31 of address
	} __attribute__((packed)) entry_pse; // for 4M pages
	uint32_t dword;
} __attribute__((packed)) vmm_pde_t;

typedef union {
	struct {
		size_t present : 1;
		size_t rw : 1;
		size_t user : 1;
		size_t wthru : 1;
		size_t ncache : 1;
		size_t accessed : 1;
		size_t dirty : 1;
		size_t zero : 1;
		size_t global : 1;
		size_t avail : 3;
		uintptr_t pa : 20; // phys addr >> 12
	} __attribute__((packed)) entry;
	uint32_t dword;
} __attribute__((packed)) vmm_pte_t;

vmm_pde_t vmm_default_pd[1024] __attribute__((aligned(4096))); // for bootstrap code

extern uintptr_t __rmap_start; // recursive mapping region start address (in link.ld)
extern uintptr_t __rmap_end;

size_t vmm_pgsz_num() {
	return 2;
}

size_t vmm_pgsz(size_t idx) {
	switch(idx) {
		case 0: return 4096;
		case 1: return 4194304;
		default: return 0; // invalid
	}
}

#define VMM_PD_PTE								((uintptr_t)&__rmap_start >> 22) // page table entry for accessing the page directory
#define vmm_pt(base, pde)						((vmm_pte_t*) ((uintptr_t)(base) + ((pde) << 12))) // retrieve page table given the base ptr and PDE
#define vmm_pd(base)							((vmm_pde_t*) vmm_pt(base, VMM_PD_PTE)) // retrieve page directory virtual address given the base ptr

#define VMM_AVAIL_TRAPPED						(1 << 0)

void vmm_pgmap_small(void* vmm, uintptr_t pa, uintptr_t va, size_t flags) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW) : vmm_pd(&__rmap_start)); // page directory
	vmm_pte_t* pt = NULL; // page table
	if(!pd) {
		kerror("cannot map page directory");
		return;
	} else if(pd[pde].dword && !pd[pde].entry.pgsz) {
		/* there's a PT to access too */
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
	}

	bool invalidate_tlb = (vmm == vmm_current); // set if TLB invalidation is needed

	vmm_pde_t* pd_entry = &pd[pde]; // PD entry
	vmm_pde_t pde_orig; pde_orig.dword = pd_entry->dword; // current PD entry
	bool pse = (pde_orig.entry.present && pde_orig.entry.pgsz); // set if this is a huge (PSE/4M) page
	if(pse) invalidate_tlb = invalidate_tlb || (pde_orig.entry_pse.global); // invalidate TLB if this was a global hugepage

	if(!pt) {
		/* allocate page table */
		size_t frame = pmm_alloc_free(1); // ask for a single contiguous free frame
		if(frame == (size_t)-1) kerror("no more free frames, brace for impact");
		else {
			// pmm_alloc(frame);			
			pd_entry->dword = (frame << 12) | (1 << 0) | (1 << 1); // all the other flags will be filled in for us, but we must have present and rw
			if(pse) { // transfer the flags over
				// pd_entry->entry.present |= pde_orig.entry.present;
				// pd_entry->entry.rw |= pde_orig.entry.rw;
				pd_entry->entry.user |= pde_orig.entry.user;
			}

			if(pd_map) {
				/* map PT */
				pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
				if(!pt) {
					kerror("cannot map page table");
					vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
					return;
				}
			} else {
				/* make use of recursive mapping */
				pt = vmm_pt(&__rmap_start, pde);
				asm volatile("invlpg (%0)" : : "r"(pt) : "memory"); // invalidate TLB entry for our PT so we don't end up with wrong page faults
			}
			memset(pt, 0, 4096); // clear out the newly allocated page table
			
			if(task_kernel && va >= kernel_start) {
				/* propagate kernel pages to all tasks' VMM configs */
				vmm_pde_t* task_vmm_pd = (vmm_pde_t*) vmm_alloc_map(vmm_current, 0, 4096, (pd_map) ? ((uintptr_t) pt + 4096) : 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
				if(!task_vmm_pd) kerror("cannot map task VMM configurations for page table propagation");
				else {
					task_t* task = task_kernel;
					struct proc* proc = proc_get(task_common(task)->pid);
					do {
						if(proc->vmm != vmm) {
							/* map page directory and copy PD entry over */
							vmm_set_paddr(vmm_current, (uintptr_t) task_vmm_pd, (uintptr_t) proc->vmm);
							task_vmm_pd[pde].dword = pd_entry->dword;
						}
						task = task->common.next;
					} while(task != task_kernel);
					vmm_pgunmap(vmm_current, (uintptr_t) task_vmm_pd, 0);
				}
			}
		}

		/* remap any PSE pages */
		if(pse) {
			size_t pa_frame = pde_orig.entry_pse.pa << 10;
			for(size_t i = 0; i < 1024; i++, pa_frame++) {
				if(i != pte) {
					pt[i].entry.present = pde_orig.entry_pse.present;
					pt[i].entry.user = pde_orig.entry_pse.user;
					pt[i].entry.rw = pde_orig.entry_pse.rw;
					// pt[i].entry.zero = 0;
					pt[i].entry.global = pde_orig.entry_pse.global;
					pt[i].entry.ncache = pde_orig.entry_pse.ncache;
					pt[i].entry.wthru = pde_orig.entry_pse.wthru;
					pt[i].entry.accessed = pde_orig.entry_pse.accessed;
					pt[i].entry.dirty = pde_orig.entry_pse.dirty;
					pt[i].entry.avail = pde_orig.entry_pse.avail;
					pt[i].entry.pa = pa_frame;
				}
			}
		}
	}

	/* set settings in page directory entry */
	pd_entry->entry.present |= (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	pd_entry->entry.rw |= (flags & VMM_FLAGS_RW) ? 1 : 0;
	pd_entry->entry.user |= (flags & VMM_FLAGS_USER) ? 1 : 0;

	/* populate page table entry */
	vmm_pte_t* pt_entry = &pt[pte];
	invalidate_tlb = invalidate_tlb || (pt_entry->entry.global) || (flags & VMM_FLAGS_GLOBAL); // set if the page was global, or will be global
	pt_entry->entry.present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	pt_entry->entry.user = (flags & VMM_FLAGS_USER) ? 1 : 0;
	pt_entry->entry.rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
	// pt_entry->entry.zero = 0;
	pt_entry->entry.global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
	pt_entry->entry.ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
	pt_entry->entry.wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
	pt_entry->entry.pa = pa >> 12;
	pt_entry->entry.accessed = 0; pt_entry->entry.dirty = 0;
	pt_entry->entry.avail = (flags & VMM_FLAGS_TRAPPED) ? VMM_AVAIL_TRAPPED : 0;

	if(invalidate_tlb) asm volatile("invlpg (%0)" : : "r"(va) : "memory"); // invalidate TLB if needed

	if(pd_map) {
		/* unmap PD and PT */
		vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
		vmm_pgunmap(vmm_current, (uintptr_t) pt, 0); // PT is supposed to be non-NULL
	}
}

void vmm_pgmap_huge(void* vmm, uintptr_t pa, uintptr_t va, size_t flags) {
	size_t pde = va >> 22; // page directory entry number for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return;
	}
	vmm_pde_t* pd_entry = &pd[pde]; // PD entry

	bool invalidate_tlb = (vmm == vmm_current); // set if the TLB is to be invalidated

	if(pd_entry->dword && !pd_entry->entry.pgsz) {
		/* there's a page table for this PDE - deallocate it to avoid confusion */
		pmm_free(((vmm_pde_t*)pd_entry)->entry.pt);
		pd_entry->dword = 0; // quick way to unmap page
		if(!pd_map) asm volatile("invlpg (%0)" : : "r"(vmm_pt(&__rmap_start, pde)) : "memory"); // invalidate TLB entry for the PT as a safety measure
	}
	
	invalidate_tlb = invalidate_tlb || (pd_entry->entry_pse.global) || (flags & VMM_FLAGS_GLOBAL); // set if the page was global, or will be global
	pd_entry->dword = (1 << 7); // quickly clear PDE and set its PSE bit
	pd_entry->entry_pse.present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	pd_entry->entry_pse.user = (flags & VMM_FLAGS_USER) ? 1 : 0;
	pd_entry->entry_pse.rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
	pd_entry->entry_pse.global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
	pd_entry->entry_pse.ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
	pd_entry->entry_pse.wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
	pd_entry->entry_pse.pa = pa >> 22;
	pd_entry->entry_pse.accessed = 0; pd_entry->entry_pse.dirty = 0;
	pd_entry->entry_pse.avail = (flags & VMM_FLAGS_TRAPPED) ? VMM_AVAIL_TRAPPED : 0;

	if(task_kernel && va >= kernel_start) {
		/* propagate kernel pages to all tasks' VMM configs */
		vmm_pde_t* task_vmm_pd = (vmm_pde_t*) vmm_alloc_map(vmm_current, 0, 4096, (pd_map) ? ((uintptr_t) pd + 4096) : 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
		if(!task_vmm_pd) kerror("cannot map task VMM configurations for page table propagation");
		else {
			task_t* task = task_kernel;
			struct proc* proc = proc_get(task_common(task)->pid);
			do {
				if(proc->vmm != vmm) {
					/* map page directory and copy PD entry over */
					vmm_set_paddr(vmm_current, (uintptr_t) task_vmm_pd, (uintptr_t) proc->vmm);
					task_vmm_pd[pde].dword = pd_entry->dword;
				}
				task = task->common.next;
			} while(task != task_kernel);
			vmm_pgunmap(vmm_current, (uintptr_t) task_vmm_pd, 0);
		}
	}

	if(invalidate_tlb) {
		/* invalidate TLB if needed */
		for(size_t i = 0; i < 1024; i++, va += 1024) {
			asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	}

	if(pd_map) {
		vmm_pgunmap(vmm_current, (uintptr_t) pd, 0); // unmap PD
	}
}

void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, size_t pgsz_idx, size_t flags) {
	if(va >= (uintptr_t)&__rmap_start && va < (uintptr_t)&__rmap_end) {
		kerror("cannot map into recursive mapping region");
		return;
	}

	switch(pgsz_idx) {
		case 0: vmm_pgmap_small(vmm, pa, va, flags); break;
		case 1: vmm_pgmap_huge(vmm, pa, va, flags); break;
		default:
			kerror("invalid page size index %u", pgsz_idx);
			break;
	}
}

static void vmm_unmap_resolve_cow(void* vmm, uintptr_t va, size_t pgsz) {
	/* resolve any remaining CoW traps */
	vmm_trap_t* cow = vmm_is_cow(vmm, va, true);
	while(cow) {
		vmm_trap_t* src = (vmm_trap_t*) cow->info;
		// kdebug("resolving CoW: 0x%x:0x%x <-- 0x%x:0x%x", src->vmm, src->vaddr, cow->vmm, cow->vaddr);
		vmm_cow_duplicate(src->vmm, src->vaddr, pgsz);
		cow = vmm_is_cow(vmm, va, true);
	}
}

void vmm_pgunmap_huge(void* vmm, uintptr_t va) {
	size_t pde = va >> 22; // page directory entry number for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return;
	}
	vmm_pde_t* pd_entry = &pd[pde]; // PD entry
	if(!pd_entry->dword) goto done; // nothing to do

	size_t invalidate_tlb = (vmm == vmm_current);

	if(!pd_entry->entry.pgsz) {
		/* there's a PT behind this - deallocate it. but first we'll need to invalidate the TLB of global pages if there's any (and if it's needed) */
		vmm_pte_t* pt = ((pd_map) ? (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pt(&__rmap_start, pde));
		for(size_t i = 0; i < 1024; i++) {
			if(!invalidate_tlb && pt[i].entry.global) asm volatile("invlpg (%0)" : : "r"(va | (i << 12)) : "memory");
			if(pt[i].entry.avail & VMM_AVAIL_TRAPPED) vmm_unmap_resolve_cow(vmm, va | (i << 12), 0);
		}
		pmm_free(pd_entry->entry.pt);
		if(!pd_map) asm volatile("invlpg (%0)" : : "r"(pt) : "memory");
		else vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	} else if(pd_entry->entry_pse.avail & VMM_AVAIL_TRAPPED) vmm_unmap_resolve_cow(vmm, va, 1); // resolve CoW if needed

	pd_entry->dword = 0;

	if(invalidate_tlb) {
		for(size_t i = 0; i < 1024; i++, va += 4096) {
			asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	}

done:
	if(pd_map) {
		vmm_pgunmap(vmm_current, (uintptr_t) pd, 0); // unmap PD
	}
}

void vmm_pgunmap_small(void* vmm, uintptr_t va) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW) : vmm_pd(&__rmap_start)); // page directory
	vmm_pte_t* pt = NULL; // page table
	if(!pd) {
		kerror("cannot map page directory");
		return;
	} else if(pd[pde].dword && !pd[pde].entry.pgsz) {
		/* there's a PT to access too */
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
	}

	vmm_pde_t* pd_entry = &pd[pde]; // PD entry
	if(!pd_entry->dword) goto done; // nothing to do

	bool invalidate_tlb = (vmm == vmm_current);
	if(!pd_entry->entry.pgsz) {
		/* unmapping small page */
		invalidate_tlb = invalidate_tlb || (pt[pte].entry.global);
		if(pt[pte].entry.avail & VMM_AVAIL_TRAPPED) vmm_unmap_resolve_cow(vmm, va, 0);
		pt[pte].dword = 0;
	} else {
		/* unmapping one small page in a huge page */
		uintptr_t pa = pd_entry->entry_pse.pa << 22;
		size_t flags = 0;
		if(pd_entry->entry_pse.present) flags |= VMM_FLAGS_PRESENT;
		if(pd_entry->entry_pse.rw) flags |= VMM_FLAGS_RW;
		if(pd_entry->entry_pse.user) flags |= VMM_FLAGS_USER;
		if(pd_entry->entry_pse.global) flags |= VMM_FLAGS_GLOBAL;
		if(!pd_entry->entry_pse.ncache) flags |= VMM_FLAGS_CACHE | ((pd_entry->entry_pse.wthru) ? VMM_FLAGS_CACHE_WTHRU : 0);
		if(pd_entry->entry_pse.avail & VMM_AVAIL_TRAPPED) vmm_unmap_resolve_cow(vmm, pde << 22, 1); // resolve CoW here (and discard the trap flag)
		invalidate_tlb = invalidate_tlb || (flags & VMM_FLAGS_GLOBAL);
		pd_entry->dword = 0;
		uintptr_t va_map = va & 0xFFC00000;
		for(size_t i = 0; i < 1024; i++, pa += 4096, va_map += 4096) {
			if(va_map != va) vmm_pgmap_small(vmm, pa, va_map, flags);
		}
	}

	if(invalidate_tlb) asm volatile("invlpg (%0)" : : "r"(va) : "memory");

done:
	if(pd_map) {
		/* unmap PD and PT */
		vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
		if(pt) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}
}

void vmm_pgunmap(void* vmm, uintptr_t va, size_t pgsz_idx) {
	if(va >= (uintptr_t)&__rmap_start && va < (uintptr_t)&__rmap_end) {
		kerror("cannot unmap recursive mapping region");
		return;
	}

	switch(pgsz_idx) {
		case 0: vmm_pgunmap_small(vmm, va); break;
		case 1: vmm_pgunmap_huge(vmm, va); break;
		default:
			kerror("invalid page size index %u", pgsz_idx);
			break;
	}
}

size_t vmm_get_pgsz(void* vmm, uintptr_t va) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return (size_t)-1;
	}
	
	if(!pd[pde].dword) {
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
		return (size_t)-1; // 4M not mapped
	} else if(pd[pde].entry.pgsz) {
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
		return 1; // 4M page
	} else {
		/* there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return (size_t)-1;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		bool mapped = (pt[pte].dword);
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
		return (mapped) ? 0 : (size_t)-1;
	}
}

uintptr_t vmm_get_paddr(void* vmm, uintptr_t va) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return 0;
	}
	
	uintptr_t paddr = 0;
	if(!pd[pde].dword) goto done; // nothing to be done here

	if(pd[pde].entry.pgsz) paddr = (pd[pde].entry_pse.pa << 22) | (va & 0x3FFFFF); // hugepage
	else {
		/* there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return 0;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(pt[pte].dword) paddr = (pt[pte].entry.pa << 12) | (va & 0xFFF);
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
	return paddr;
}

void vmm_set_paddr(void* vmm, uintptr_t va, uintptr_t pa) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return;
	}

	if(!pd[pde].dword) goto done; // not mapped - nothing to be done here

	if(pd[pde].entry.pgsz) {
		/* huge page */
		pd[pde].entry_pse.pa = pa >> 22;
		if(vmm == vmm_current || pd[pde].entry_pse.global) {
			/* invalidate TLB */
			va &= 0xFFC00000;
			for(size_t i = 0; i < 1024; i++, va += 4096) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	} else {
		/* small page */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(pt[pte].dword) {
			pt[pte].entry.pa = pa >> 12;
			if(vmm == vmm_current || pt[pte].entry.global) asm volatile("invlpg (%0)" : : "r"(va & ~0xFFF) : "memory"); // invalidate TLB
		}
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
}

void vmm_switch(void* vmm) {
	if(vmm_current == vmm) return; // no need to do anything
	asm volatile("mov %0, %%cr3" : : "r"(vmm) : "memory");
	vmm_current = vmm;
}

void vmm_init() {
	// do nothing - VMM initialization is done during bootstrapping
}

void* vmm_clone(void* src, bool cow) {
	/* get source's PD */
	bool pd_map = (src != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd_src = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) src, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(!pd_src) {
		kerror("cannot map source page directory");
		return NULL;
	}

	/* allocate and map destination PD */
	size_t dst_frame = pmm_alloc_free(1);
	if(dst_frame == (size_t)-1) {
		kerror("cannot allocate destination page directory");
		return NULL;
	}
	vmm_pde_t* pd_dst = (vmm_pde_t*) vmm_alloc_map(vmm_current, dst_frame << 12, 4096, (pd_map) ? ((uintptr_t) pd_src + 4096) : 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
	if(!pd_dst) {
		kerror("cannot map destination page directory");
		pmm_free(dst_frame);
		return NULL;
	}

	/* initialize destination PD */
	size_t tables = kernel_start >> 22; // number of non-kernel tables
	if(cow) {
		memset(pd_dst, 0, tables << 2); // wipe out non-kernel table space (so we can CoW map later)
		memcpy(&pd_dst[tables], &pd_src[tables], (1024 - tables) << 2); // only copy kernel tables over
	} else memcpy(pd_dst, pd_src, 4096); // copy the entire page directory
	pd_dst[VMM_PD_PTE].dword = (dst_frame << 12) | (1 << 0) | (1 << 1); // set up recursive mapping

	/* unmap source PD since we have a copy of it in pd_dst now */
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd_src, 0);

	/* clone non-kernel page tables */
	vmm_pte_t* pt_src = (vmm_pte_t*) vmm_alloc_map(vmm_current, 0, (cow) ? 4096 : 8192, (pd_map) ? (uintptr_t)pd_src : 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // source PT (placeholder for now)
	if(!pt_src) {
		kerror("cannot map source and destination page tables");
		vmm_pgunmap(vmm_current, (uintptr_t) pd_dst, 0); pmm_free(dst_frame); // deallocate destination PD
		return NULL;
	}
	vmm_pte_t* pt_dst = (vmm_pte_t*) ((uintptr_t) pt_src + 4096); // destination PT, also placeholder (this will not be used in CoW mode)
	for(size_t i = 0; i < tables; i++) { // do not clone the kernel's top 1G space
		if(pd_src[i].dword) {
			/* ignore empty page directory entries */
			if(cow) {
				/* set up copy on write */
				if(pd_dst[i].entry.pgsz) vmm_cow_setup(src, (i << 22), (void*)(dst_frame << 12), (i << 22), 4194304); // hugepage
				else {
					vmm_set_paddr(vmm_current, (uintptr_t) pt_src, pd_dst[i].entry.pt << 12);
					for(size_t j = 0; j < 1024; j++) {
						if(pt_src[j].dword) vmm_cow_setup(src, (i << 22) | (j << 12), (void*)(dst_frame << 12), (i << 22) | (j << 12), 4096); // small page - we ignore any empty entries here
					}
				}
			} else {
				/* clone references */
				if(!pd_dst[i].entry.pgsz) { // ignore hugepages
					/* map source PT */
					vmm_set_paddr(vmm_current, (uintptr_t) pt_src, pd_dst[i].entry.pt << 12);
					
					/* allocate and map destination PT */
					size_t pt_dst_frame = pmm_alloc_free(1);
					if(pt_dst_frame == (size_t)-1) {
						kerror("cannot allocate destination page table %u", i);
						
						/* do cleanup */
						vmm_unmap(vmm_current, (uintptr_t) pt_src, 4096 * 2); // unmap PTs
						for(size_t j = 0; j < i; j++) {
							/* free existing PTs */
							if(pd_dst[j].dword && !pd_dst[j].entry.pgsz) pmm_free(pd_dst[j].entry.pt);
						}
						vmm_pgunmap(vmm_current, (uintptr_t) pd_dst, 0); pmm_free(dst_frame); // deallocate destination PD
						return NULL;
					}
					vmm_set_paddr(vmm_current, (uintptr_t) pt_dst, pt_dst_frame << 12);

					/* replace the PT */
					memcpy(pt_dst, pt_src, 4096);
					pd_dst[i].entry.pt = pt_dst_frame;
				}
			}
		}
	}
	vmm_unmap(vmm_current, (uintptr_t) pt_src, (cow) ? 4096 : 8192); // unmap PTs
	vmm_pgunmap(vmm_current, (uintptr_t) pd_dst, 0); // unmap our new PD

	return (void*) (dst_frame << 12); // return phys address of destination PD as VMM config address
}

void vmm_free(void* vmm) {
	if(vmm == vmm_kernel) return; // we can't free the kernel VMM config; however, this is not a fatal issue as we can just skip the deallocation
	
	if(vmm == vmm_current) {
		vmm_stage_free(vmm);
	}

	vmm_pde_t* pd = (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // map PD
	if(!pd) {
		kerror("cannot map page directory");
		return;
	}

	/* free PT frames */
	size_t tables = kernel_start >> 22; // only free tables up to the kernel space
	for(size_t i = 0; i < tables; i++) {
		if(pd[i].dword && !pd[i].entry.pgsz) pmm_free(pd[i].entry.pt);
	}
	
	pmm_free((uintptr_t) vmm >> 12); // free the page directory's frame
	vmm_pgunmap(vmm_current, (uintptr_t) pd, 0); // unmap the PD we just mapped
}

size_t vmm_get_flags(void* vmm, uintptr_t va) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return 0;
	}
	
	size_t flags = 0;
	if(!pd[pde].dword) goto done; // nothing to be done here

	if(pd[pde].entry.pgsz) {
		/* hugepage */
		if(pd[pde].entry_pse.present) flags |= VMM_FLAGS_PRESENT;
		if(pd[pde].entry_pse.rw) flags |= VMM_FLAGS_RW;
		if(pd[pde].entry_pse.user) flags |= VMM_FLAGS_USER;
		if(pd[pde].entry_pse.global) flags |= VMM_FLAGS_GLOBAL;
		if(!pd[pde].entry_pse.ncache) flags |= VMM_FLAGS_CACHE | ((pd[pde].entry_pse.wthru) ? VMM_FLAGS_CACHE_WTHRU : 0);
		if(pd[pde].entry_pse.avail & VMM_AVAIL_TRAPPED) flags |= VMM_FLAGS_TRAPPED;
	} else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return 0;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(pt[pte].dword) {
			if(pt[pte].entry.present) flags |= VMM_FLAGS_PRESENT;
			if(pt[pte].entry.rw) flags |= VMM_FLAGS_RW;
			if(pt[pte].entry.user) flags |= VMM_FLAGS_USER;
			if(pt[pte].entry.global) flags |= VMM_FLAGS_GLOBAL;
			if(!pt[pte].entry.ncache) flags |= VMM_FLAGS_CACHE | ((pt[pte].entry.wthru) ? VMM_FLAGS_CACHE_WTHRU : 0);
			if(pt[pte].entry.avail & VMM_AVAIL_TRAPPED) flags |= VMM_FLAGS_TRAPPED;
		}
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
	return flags;
}

void vmm_set_flags(void* vmm, uintptr_t va, size_t flags) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return;
	}
	
	if(!pd[pde].dword) goto done; // nothing to be done here

	bool invalidate_tlb = (vmm == vmm_current) || (flags & VMM_FLAGS_GLOBAL);

	if(pd[pde].entry.pgsz) {
		/* hugepage */
		invalidate_tlb = invalidate_tlb || (pd[pde].entry_pse.global);
		pd[pde].entry_pse.present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
		pd[pde].entry_pse.user = (flags & VMM_FLAGS_USER) ? 1 : 0;
		pd[pde].entry_pse.rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
		pd[pde].entry_pse.global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
		pd[pde].entry_pse.ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
		pd[pde].entry_pse.wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
		pd[pde].entry_pse.avail = (flags & VMM_FLAGS_TRAPPED) ? VMM_AVAIL_TRAPPED : 0;
		if(invalidate_tlb) {
			va &= 0xFFC00000;
			for(size_t i = 0; i < 1024; i++, va += 4096) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	} else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(pt[pte].dword) {
			invalidate_tlb = invalidate_tlb || (pt[pte].entry.global);
			pt[pte].entry.present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
			pt[pte].entry.user = (flags & VMM_FLAGS_USER) ? 1 : 0;
			pt[pte].entry.rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
			pt[pte].entry.global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
			pt[pte].entry.ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
			pt[pte].entry.wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
			pt[pde].entry.avail = (flags & VMM_FLAGS_TRAPPED) ? VMM_AVAIL_TRAPPED : 0;
			if(invalidate_tlb) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
}

bool vmm_get_dirty(void* vmm, uintptr_t va) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return true; // assume page is dirty (TODO?)
	}
	
	bool dirty = false;
	if(!pd[pde].dword) goto done; // nothing to be done here

	if(pd[pde].entry.pgsz) dirty = (pd[pde].entry_pse.dirty);
	else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return true;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		dirty = (pt[pte].entry.dirty);
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
	return dirty;
}

void vmm_set_dirty(void* vmm, uintptr_t va, bool dirty) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(!pd) {
		kerror("cannot map page directory");
		return;
	}
	
	if(!pd[pde].dword) goto done; // nothing to be done here

	if(pd[pde].entry.pgsz) pd[pde].entry_pse.dirty = (dirty) ? 1 : 0;
	else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].entry.pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(!pt) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(pt[pte].dword) pt[pte].entry.dirty = (dirty) ? 1 : 0;
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
}
