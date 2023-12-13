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
typedef struct {
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
} __attribute__((packed)) vmm_pde_t;

typedef struct {
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
} __attribute__((packed)) vmm_pde_pse_t; // for PSE (4MiB pages)

typedef struct {
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

void vmm_pgmap_small(void* vmm, uintptr_t pa, uintptr_t va, size_t flags) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW) : vmm_pd(&__rmap_start)); // page directory
	vmm_pte_t* pt = NULL; // page table
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	} else if(*((uint32_t*)&pd[pde]) && !pd[pde].pgsz) {
		/* there's a PT to access too */
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
	}

	bool invalidate_tlb = (vmm == vmm_current); // set if TLB invalidation is needed

	vmm_pde_t* pd_entry = &pd[pde]; // PD entry
	vmm_pde_pse_t pde_pse; *((uint32_t*) &pde_pse) = *((uint32_t*) pd_entry); // current PD entry (assuming it's PSE)
	bool pse = (pde_pse.present && pde_pse.pgsz); // set if this is a huge (PSE/4M) page
	if(pse) invalidate_tlb = invalidate_tlb || (pde_pse.global); // invalidate TLB if this was a global hugepage

	if(pt == NULL) {
		/* allocate page table */
		size_t frame = pmm_alloc_free(1); // ask for a single contiguous free frame
		if(frame == (size_t)-1) kerror("no more free frames, brace for impact");
		else {
			// pmm_alloc(frame);			
			*((uint32_t*) pd_entry) = (frame << 12) | (1 << 0) | (1 << 1); // all the other flags will be filled in for us, but we must have present and rw
			if(pse) { // transfer the flags over
				// pd_entry->present |= pde_pse.present;
				// pd_entry->rw |= pde_pse.rw;
				pd_entry->user |= pde_pse.user;
			}

			if(pd_map) {
				/* map PT */
				pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
				if(pt == NULL) {
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
			
			if(task_kernel != NULL && va >= kernel_start) {
				/* propagate kernel pages to all tasks' VMM configs */
				vmm_pde_t* task_vmm_pd = (vmm_pde_t*) vmm_alloc_map(vmm_current, 0, 4096, (pd_map) ? ((uintptr_t) pt + 4096) : 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
				if(task_vmm_pd == NULL) kerror("cannot map task VMM configurations for page table propagation");
				else {
					task_t* task = task_kernel;
					struct proc* proc = proc_get(task_common(task)->pid);
					do {
						if(proc->vmm != vmm) {
							/* map page directory and copy PD entry over */
							vmm_set_paddr(vmm_current, (uintptr_t) task_vmm_pd, (uintptr_t) proc->vmm);
							*((uint32_t*)&task_vmm_pd[pde]) = *((uint32_t*) pd_entry);
						}
						task = task->common.next;
					} while(task != task_kernel);
					vmm_pgunmap(vmm_current, (uintptr_t) task_vmm_pd, 0);
				}
			}
		}

		/* remap any PSE pages */
		if(pse) {
			size_t pa_frame = pde_pse.pa << 10;
			for(size_t i = 0; i < 1024; i++, pa_frame++) {
				if(i != pte) {
					pt[i].present = pde_pse.present;
					pt[i].user = pde_pse.user;
					pt[i].rw = pde_pse.rw;
					// pt[i].zero = 0;
					pt[i].global = pde_pse.global;
					pt[i].ncache = pde_pse.ncache;
					pt[i].wthru = pde_pse.wthru;
					pt[i].accessed = pde_pse.accessed;
					pt[i].dirty = pde_pse.dirty;
					pt[i].pa = pa_frame;
				}
			}
		}
	}

	/* set settings in page directory entry */
	pd_entry->present |= (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	pd_entry->rw |= (flags & VMM_FLAGS_RW) ? 1 : 0;
	pd_entry->user |= (flags & VMM_FLAGS_USER) ? 1 : 0;

	/* populate page table entry */
	vmm_pte_t* pt_entry = &pt[pte];
	invalidate_tlb = invalidate_tlb || (pt_entry->global) || (flags & VMM_FLAGS_GLOBAL); // set if the page was global, or will be global
	pt_entry->present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	pt_entry->user = (flags & VMM_FLAGS_USER) ? 1 : 0;
	pt_entry->rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
	// pt_entry->zero = 0;
	pt_entry->global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
	pt_entry->ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
	pt_entry->wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
	pt_entry->pa = pa >> 12;
	pt_entry->accessed = 0; pt[pte].dirty = 0;

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
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	}
	vmm_pde_pse_t* pd_entry = (vmm_pde_pse_t*) &pd[pde]; // PD entry

	bool invalidate_tlb = (vmm == vmm_current); // set if the TLB is to be invalidated

	if(*((uint32_t*) &pd_entry) && !pd_entry->pgsz) {
		/* there's a page table for this PDE - deallocate it to avoid confusion */
		pmm_free(((vmm_pde_t*)pd_entry)->pt);
		*((uint32_t*) pd_entry) = 0; // quick way to unmap page
		if(!pd_map) asm volatile("invlpg (%0)" : : "r"(vmm_pt(&__rmap_start, pde)) : "memory"); // invalidate TLB entry for the PT as a safety measure
	}
	
	invalidate_tlb = invalidate_tlb || (pd_entry->global) || (flags & VMM_FLAGS_GLOBAL); // set if the page was global, or will be global
	*((uint32_t*) pd_entry) = (1 << 7); // quickly clear PDE and set its PSE bit
	pd_entry->present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	pd_entry->user = (flags & VMM_FLAGS_USER) ? 1 : 0;
	pd_entry->rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
	pd_entry->global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
	pd_entry->ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
	pd_entry->wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
	pd_entry->pa = pa >> 22;
	pd_entry->accessed = 0; pd_entry->dirty = 0;

	if(task_kernel != NULL && va >= kernel_start) {
		/* propagate kernel pages to all tasks' VMM configs */
		vmm_pde_pse_t* task_vmm_pd = (vmm_pde_pse_t*) vmm_alloc_map(vmm_current, 0, 4096, (pd_map) ? ((uintptr_t) pd + 4096) : 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
		if(task_vmm_pd == NULL) kerror("cannot map task VMM configurations for page table propagation");
		else {
			task_t* task = task_kernel;
			struct proc* proc = proc_get(task_common(task)->pid);
			do {
				if(proc->vmm != vmm) {
					/* map page directory and copy PD entry over */
					vmm_set_paddr(vmm_current, (uintptr_t) task_vmm_pd, (uintptr_t) proc->vmm);
					*((uint32_t*)&task_vmm_pd[pde]) = *((uint32_t*) pd_entry);
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

void vmm_pgunmap_huge(void* vmm, uintptr_t va) {
	size_t pde = va >> 22; // page directory entry number for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW) : vmm_pd(&__rmap_start)); // page directory
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	}
	vmm_pde_pse_t* pd_entry = (vmm_pde_pse_t*) &pd[pde]; // PD entry
	if(!*((uint32_t*)&pd_entry)) goto done; // nothing to do

	size_t invalidate_tlb = (vmm == vmm_current);

	if(!pd_entry->pgsz) {
		/* there's a PT behind this - deallocate it. but first we'll need to invalidate the TLB of global pages if there's any (and if it's needed) */
		vmm_pte_t* pt = ((pd_map) ? (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pt(&__rmap_start, pde));
		if(!invalidate_tlb) {
			for(size_t i = 0; i < 1024; i++) {
				if(pt[i].global) asm volatile("invlpg (%0)" : : "r"(va | (i << 12)) : "memory");
			}
		}
		pmm_free(((vmm_pde_t*) pd_entry)->pt);
		if(!pd_map) asm volatile("invlpg (%0)" : : "r"(pt) : "memory");
		else vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

	*((uint32_t*) pd_entry) = 0;

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
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	} else if(*((uint32_t*)&pd[pde]) && !pd[pde].pgsz) {
		/* there's a PT to access too */
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
	}

	vmm_pde_t* pd_entry = &pd[pde]; // PD entry
	if(!*((uint32_t*)pd_entry)) goto done; // nothing to do

	bool invalidate_tlb = (vmm == vmm_current);
	if(!pd_entry->pgsz) {
		/* unmapping small page */
		invalidate_tlb = invalidate_tlb || (pt[pte].global);
		*((uint32_t*) &pt[pte]) = 0;
	} else {
		/* unmapping one small page in a huge page */
		uintptr_t pa = ((vmm_pde_pse_t*) pd_entry)->pa << 22;
		size_t flags = vmm_get_flags(vmm, va); // reuse existing func
		invalidate_tlb = invalidate_tlb || (flags & VMM_FLAGS_GLOBAL);
		*((uint32_t*) pd_entry) = 0;
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
		if(pt != NULL) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
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
	if(pd == NULL) {
		kerror("cannot map page directory");
		return (size_t)-1;
	}
	
	if(!(*((uint32_t*)&pd[pde]))) {
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
		return (size_t)-1; // 4M not mapped
	} else if(pd[pde].pgsz) {
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
		return 1; // 4M page
	} else {
		/* there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return (size_t)-1;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		bool mapped = (*((uint32_t*)&pt[pte]));
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
		return (mapped) ? 0 : (size_t)-1;
	}
}

uintptr_t vmm_get_paddr(void* vmm, uintptr_t va) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(pd == NULL) {
		kerror("cannot map page directory");
		return 0;
	}
	
	uintptr_t paddr = 0;
	if(!(*((uint32_t*)&pd[pde]))) goto done; // nothing to be done here

	if(pd[pde].pgsz) paddr = (((vmm_pde_pse_t*)&pd[pde])->pa << 22) | (va & 0x3FFFFF); // hugepage
	else {
		/* there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return 0;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(*((uint32_t*)&pt[pte])) paddr = (pt[pte].pa << 12) | (va & 0xFFF);
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
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	}

	if(!(*((uint32_t*)&pd[pde]))) goto done; // not mapped - nothing to be done here

	if(pd[pde].pgsz) {
		/* huge page */
		((vmm_pde_pse_t*)&pd[pde])->pa = pa >> 22;
		if(vmm == vmm_current || ((vmm_pde_pse_t*)&pd[pde])->global) {
			/* invalidate TLB */
			va &= 0xFFC00000;
			for(size_t i = 0; i < 1024; i++, va += 4096) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	} else {
		/* small page */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(*((uint32_t*)&pt[pte])) {
			pt[pte].pa = pa >> 12;
			if(vmm == vmm_current || pt[pte].global) asm volatile("invlpg (%0)" : : "r"(va & ~0xFFF) : "memory"); // invalidate TLB
		}
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
}

void vmm_switch(void* vmm) {
	asm volatile("mov %0, %%cr3" : : "r"(vmm) : "memory");
	vmm_current = vmm;
}

void vmm_init() {
	// do nothing - VMM initialization is done during bootstrapping
}

void* vmm_clone(void* src) {
	/* get source's PD */
	bool pd_map = (src != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd_src = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) src, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(pd_src == NULL) {
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
	if(pd_dst == NULL) {
		kerror("cannot map destination page directory");
		pmm_free(dst_frame);
		return NULL;
	}

	/* initialize destination PD */
	memcpy(pd_dst, pd_src, 4096);
	*((uint32_t*)&pd_dst[VMM_PD_PTE]) = (dst_frame << 12) | (1 << 0) | (1 << 1); // set up recursive mapping

	/* unmap source PD since we have a copy of it in pd_dst now */
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd_src, 0);

	/* clone non-kernel page tables */
	size_t tables = kernel_start >> 22;
	vmm_pte_t* pt_src = (vmm_pte_t*) vmm_alloc_map(vmm_current, 0, 4096 * 2, (pd_map) ? (uintptr_t)pd_src : 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // source PT (placeholder for now)
	if(pt_src == NULL) {
		kerror("cannot map source and destination page tables");
		vmm_pgunmap(vmm_current, (uintptr_t) pd_dst, 0); pmm_free(dst_frame); // deallocate destination PD
		return NULL;
	}
	vmm_pte_t* pt_dst = (vmm_pte_t*) ((uintptr_t) pt_src + 4096); // destination PT, also placeholder
	for(size_t i = 0; i < tables; i++) { // do not clone the kernel's top 1G space
		if((*((uint32_t*)&pd_dst[i])) && !pd_dst[i].pgsz) { // ignore hugepages
			/* map source PT */
			vmm_set_paddr(vmm_current, (uintptr_t) pt_src, pd_dst[i].pt << 12);
			
			/* allocate and map destination PT */
			size_t pt_dst_frame = pmm_alloc_free(1);
			if(pt_dst_frame == (size_t)-1) {
				kerror("cannot allocate destination page table %u", i);
				
				/* do cleanup */
				vmm_unmap(vmm_current, (uintptr_t) pt_src, 4096 * 2); // unmap PTs
				for(size_t j = 0; j < i; j++) {
					/* free existing PTs */
					if((*((uint32_t*)&pd_dst[j])) && !pd_dst[j].pgsz) pmm_free(pd_dst[j].pt);
				}
				vmm_pgunmap(vmm_current, (uintptr_t) pd_dst, 0); pmm_free(dst_frame); // deallocate destination PD
				return NULL;
			}
			vmm_set_paddr(vmm_current, (uintptr_t) pt_dst, pt_dst_frame << 12);

			/* replace the PT */
			memcpy(pt_dst, pt_src, 4096);
			pd_dst[i].pt = pt_dst_frame;
		}
	}
	vmm_unmap(vmm_current, (uintptr_t) pt_src, 4096 * 2); // unmap PTs

	return (void*) (dst_frame << 12); // return phys address of destination PD as VMM config address
}

void vmm_free(void* vmm) {
	if(vmm == vmm_kernel) return; // we can't free the kernel VMM config; however, this is not a fatal issue as we can just skip the deallocation
	
	if(vmm == vmm_current) {
		vmm_stage_free(vmm);
	}

	vmm_pde_t* pd = (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // map PD
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	}

	/* free PT frames */
	size_t tables = kernel_start >> 22; // only free tables up to the kernel space
	for(size_t i = 0; i < tables; i++) {
		if((*((uint32_t*)&pd[i])) && !pd[i].pgsz) pmm_free(pd[i].pt);
	}
	
	pmm_free((uintptr_t) vmm >> 12); // free the page directory's frame
	vmm_pgunmap(vmm_current, (uintptr_t) pd, 0); // unmap the PD we just mapped
}

size_t vmm_get_flags(void* vmm, uintptr_t va) {
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff; // page directory and page table entries for our virtual address

	bool pd_map = (vmm != vmm_current); // set if we need to map the page directory and page table to our VMM config
	vmm_pde_t* pd = ((pd_map) ? (vmm_pde_t*) vmm_alloc_map(vmm_current, (uintptr_t) vmm, 4096, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT) : vmm_pd(&__rmap_start)); // page directory
	if(pd == NULL) {
		kerror("cannot map page directory");
		return 0;
	}
	
	size_t flags = 0;
	if(!(*((uint32_t*)&pd[pde]))) goto done; // nothing to be done here

	if(pd[pde].pgsz) {
		/* hugepage */
		vmm_pde_pse_t* pde_pse = (vmm_pde_pse_t*) &pd[pde];
		if(pde_pse->present) flags |= VMM_FLAGS_PRESENT;
		if(pde_pse->rw) flags |= VMM_FLAGS_RW;
		if(pde_pse->user) flags |= VMM_FLAGS_USER;
		if(pde_pse->global) flags |= VMM_FLAGS_GLOBAL;
		if(!pde_pse->ncache) flags |= VMM_FLAGS_CACHE | ((pde_pse->wthru) ? VMM_FLAGS_CACHE_WTHRU : 0);
	} else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return 0;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(*((uint32_t*)&pt[pte])) {
			if(pt[pte].present) flags |= VMM_FLAGS_PRESENT;
			if(pt[pte].rw) flags |= VMM_FLAGS_RW;
			if(pt[pte].user) flags |= VMM_FLAGS_USER;
			if(pt[pte].global) flags |= VMM_FLAGS_GLOBAL;
			if(!pt[pte].ncache) flags |= VMM_FLAGS_CACHE | ((pt[pte].wthru) ? VMM_FLAGS_CACHE_WTHRU : 0);
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
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	}
	
	if(!(*((uint32_t*)&pd[pde]))) goto done; // nothing to be done here

	bool invalidate_tlb = (vmm == vmm_current) || (flags & VMM_FLAGS_GLOBAL);

	if(pd[pde].pgsz) {
		/* hugepage */
		vmm_pde_pse_t* pde_pse = (vmm_pde_pse_t*) &pd[pde];
		invalidate_tlb = invalidate_tlb || (pde_pse->global);
		pde_pse->present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
		pde_pse->user = (flags & VMM_FLAGS_USER) ? 1 : 0;
		pde_pse->rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
		pde_pse->global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
		pde_pse->ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
		pde_pse->wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
		if(invalidate_tlb) {
			va &= 0xFFC00000;
			for(size_t i = 0; i < 1024; i++, va += 4096) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	} else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(*((uint32_t*)&pt[pte])) {
			invalidate_tlb = invalidate_tlb || (pt[pte].global);
			pt[pte].present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
			pt[pte].user = (flags & VMM_FLAGS_USER) ? 1 : 0;
			pt[pte].rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
			pt[pte].global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
			pt[pte].ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
			pt[pte].wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
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
	if(pd == NULL) {
		kerror("cannot map page directory");
		return true; // assume page is dirty (TODO?)
	}
	
	bool dirty = false;
	if(!(*((uint32_t*)&pd[pde]))) goto done; // nothing to be done here

	if(pd[pde].pgsz) dirty = (((vmm_pde_pse_t*)&pd[pde])->dirty);
	else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return true;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		dirty = (pt[pte].dirty);
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
	if(pd == NULL) {
		kerror("cannot map page directory");
		return;
	}
	
	if(!(*((uint32_t*)&pd[pde]))) goto done; // nothing to be done here

	if(pd[pde].pgsz) ((vmm_pde_pse_t*)&pd[pde])->dirty = (dirty) ? 1 : 0;
	else {
		/* small page - there's a PT to access too */
		vmm_pte_t* pt = NULL; // page table
		if(pd_map) {
			/* map page table if needed */
			pt = (vmm_pte_t*) vmm_alloc_map(vmm_current, pd[pde].pt << 12, 4096, (uintptr_t) pd + 4096, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT); // speed up lookup by basing it off pd
			if(pt == NULL) {
				kerror("cannot map page table");
				vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
				return;
			}
		} else pt = vmm_pt(&__rmap_start, pde);
		if(*((uint32_t*)&pt[pte])) pt[pte].dirty = (dirty) ? 1 : 0;
		if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pt, 0);
	}

done:
	if(pd_map) vmm_pgunmap(vmm_current, (uintptr_t) pd, 0);
}
