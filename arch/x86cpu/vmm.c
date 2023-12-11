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

typedef struct {
	vmm_pte_t pt[1024];
} vmm_pt_t;

typedef struct {
	vmm_pde_t pd[1024];
	vmm_pt_t* pt[1024];
	uintptr_t cr3; // phys ptr to page directory
} __attribute__((packed)) vmm_t;

vmm_t vmm_default __attribute__((aligned(4096))); // for bootstrap code
vmm_pt_t vmm_krnlpt __attribute__((aligned(4096))); // kernel page table for mapping newly allocated page tables to EFC00000->F0000000

extern uintptr_t __krnlpt_start; // krnlpt start location (in link.ld)

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

void vmm_pgmap_small(void* vmm, uintptr_t pa, uintptr_t va, size_t flags) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;

	bool invalidate_tlb = (vmm == vmm_current); // set if TLB invalidation is needed

	if(cfg->pt[pde] == NULL) {
		vmm_pde_pse_t pde_pse; *((uint32_t*) &pde_pse) = *((uint32_t*) &cfg->pd[pde]); // current PD entry (assuming it's PSE)
		bool pse = (pde_pse.present && pde_pse.pgsz); // set if this is a huge (PSE/4M) page
		if(pse) invalidate_tlb = invalidate_tlb || (pde_pse.global); // invalidate TLB if this was a global hugepage

		/* allocate page table */
		size_t frame = pmm_alloc_free(1); // ask for a single contiguous free frame
		if(frame == (size_t)-1) kerror("no more free frames, brace for impact");
		else {
			// pmm_alloc(frame);
			bool allocated = false;
			for(size_t i = 0; i < 1024; i++) {
				/* find a space to map the new page table for accessing */
				if(!vmm_krnlpt.pt[i].present) {
					*((uint32_t*) &vmm_krnlpt.pt[i]) = (1 << 0) | (1 << 1) | (frame << 12); // present, rw, supervisor
					*((uint32_t*) &cfg->pd[pde]) = (frame << 12); // all the other flags will be filled in for us
					if(pse) { // transfer the flags over
						cfg->pd[pde].present |= pde_pse.present;
						cfg->pd[pde].rw |= pde_pse.rw;
						cfg->pd[pde].user |= pde_pse.user;
					}

					cfg->pt[pde] = (vmm_pt_t*) ((uintptr_t) &__krnlpt_start + (i << 12)); // 0xEFC00000 + i * 0x1000
					memset(cfg->pt[pde], 0, sizeof(vmm_pt_t)); // clear page table so we can be sure there's no junk in here

					if(task_kernel != NULL && va >= kernel_start) {
						/* map kernel pages to all tasks' VMM configs */
						task_t* task = task_kernel;
						struct proc* proc = proc_get(task_common(task)->pid);
						do {
							if(proc->vmm != vmm && ((vmm_t*)proc->vmm)->pt[pde] != cfg->pt[pde]) {
								/* copy PT pointer and PDE */
								((vmm_t*)proc->vmm)->pt[pde] = cfg->pt[pde];
								memcpy(&((vmm_t*)proc->vmm)->pd[pde], &cfg->pd[pde], sizeof(vmm_pde_t));
							}
							task = task->common.next;
						} while(task != task_kernel);
					}

					allocated = true;
					// kdebug("PT %u (VMM 0x%08x) @ 0x%08x (phys 0x%08x)", pde, (uintptr_t) vmm, (uintptr_t) (cfg->pt[pde]), frame << 12);
					break;
				}
			}

			if(!allocated) kerror("cannot allocate page table, brace for impact");
		}

		/* remap any PSE pages */
		if(pse) {
			size_t pa_frame = pde_pse.pa << 10;
			for(size_t i = 0; i < 1024; i++, pa_frame++) {
				if(i != pte) {
					cfg->pt[pde]->pt[i].present = pde_pse.present;
					cfg->pt[pde]->pt[i].user = pde_pse.user;
					cfg->pt[pde]->pt[i].rw = pde_pse.rw;
					// cfg->pt[pde]->pt[i].zero = 0;
					cfg->pt[pde]->pt[i].global = pde_pse.global;
					cfg->pt[pde]->pt[i].ncache = pde_pse.ncache;
					cfg->pt[pde]->pt[i].wthru = pde_pse.wthru;
					cfg->pt[pde]->pt[i].accessed = pde_pse.accessed;
					cfg->pt[pde]->pt[i].dirty = pde_pse.dirty;
					cfg->pt[pde]->pt[i].pa = pa_frame;
				}
			}
		}
	}

	/* set settings in page directory entry */
	cfg->pd[pde].present |= (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	cfg->pd[pde].rw |= (flags & VMM_FLAGS_RW) ? 1 : 0;
	cfg->pd[pde].user |= (flags & VMM_FLAGS_USER) ? 1 : 0;

	/* populate page table entry */
	invalidate_tlb = invalidate_tlb || (cfg->pt[pde]->pt[pte].global) || (flags & VMM_FLAGS_GLOBAL); // set if the page was global, or will be global
	cfg->pt[pde]->pt[pte].present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	cfg->pt[pde]->pt[pte].user = (flags & VMM_FLAGS_USER) ? 1 : 0;
	cfg->pt[pde]->pt[pte].rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
	// cfg->pt[pde]->pt[pte].zero = 0;
	cfg->pt[pde]->pt[pte].global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
	cfg->pt[pde]->pt[pte].ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
	cfg->pt[pde]->pt[pte].wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
	cfg->pt[pde]->pt[pte].pa = pa >> 12;
	cfg->pt[pde]->pt[pte].accessed = 0; cfg->pt[pde]->pt[pte].dirty = 0;

	if(invalidate_tlb) asm volatile("invlpg (%0)" : : "r"(va) : "memory"); // invalidate TLB if needed
}

void vmm_pgmap_huge(void* vmm, uintptr_t pa, uintptr_t va, size_t flags) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22;

	bool invalidate_tlb = (vmm == vmm_current); // set if the TLB is to be invalidated

	if(cfg->pt[pde] != NULL) {
		/* there's a page table for this PDE - deallocate it to avoid confusion */
		size_t pte = ((uintptr_t)cfg->pt[pde] >> 12) & 0xFFF; // PTE of vmm_krnlpt
		pmm_free(vmm_krnlpt.pt[pte].pa);
		*((uint32_t*) &vmm_krnlpt.pt[pte]) = 0; // quick way to unmap page
		asm volatile("invlpg (%0)" : : "r"(cfg->pt[pde]) : "memory");
		cfg->pt[pde] = NULL;
	}
	
	vmm_pde_pse_t* pd_entry = (vmm_pde_pse_t*) &cfg->pd[pde];
	invalidate_tlb = invalidate_tlb || (pd_entry->global) || (flags & VMM_FLAGS_GLOBAL); // set if the page was global, o will be global
	*((uint32_t*) pd_entry) = (1 << 7); // quickly clear PDE and set its PSE bit
	pd_entry->present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
	pd_entry->user = (flags & VMM_FLAGS_USER) ? 1 : 0;
	pd_entry->rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
	pd_entry->global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
	pd_entry->ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
	pd_entry->wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
	pd_entry->pa = pa >> 22;
	pd_entry->accessed = 0; pd_entry->dirty = 0;

	if(invalidate_tlb) {
		/* invalidate TLB if needed */
		for(size_t i = 0; i < 1024; i++, va += 1024) {
			asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	}
}

void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, size_t pgsz_idx, size_t flags) {
	switch(pgsz_idx) {
		case 0: vmm_pgmap_small(vmm, pa, va, flags); break;
		case 1: vmm_pgmap_huge(vmm, pa, va, flags); break;
		default:
			kerror("invalid page size index %u", pgsz_idx);
			break;
	}
}

void vmm_pgunmap_huge(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22;
	size_t invalidate_tlb = (vmm == vmm_current);
	if(cfg->pt[pde] != NULL) {
		/* there's a PT behind this - deallocate it. but first we'll need to invalidate the TLB of global pages if there's any (and if it's needed) */
		if(!invalidate_tlb) {
			for(size_t i = 0; i < 1024; i++) {
				if(cfg->pt[pde]->pt[i].global) asm volatile("invlpg (%0)" : : "r"(va | (i << 12)) : "memory");
			}
		}
		size_t pte = ((uintptr_t)cfg->pt[pde] >> 12) & 0xFFF; // PTE of vmm_krnlpt
		pmm_free(vmm_krnlpt.pt[pte].pa);
		*((uint32_t*) &vmm_krnlpt.pt[pte]) = 0; // quick way to unmap page
		asm volatile("invlpg (%0)" : : "r"(cfg->pt[pde]) : "memory");
		cfg->pt[pde] = NULL;
	}
	*((uint32_t*) &cfg->pd[pde]) = 0;
	if(invalidate_tlb) {
		for(size_t i = 0; i < 1024; i++, va += 4096) {
			asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	}
}


void vmm_pgunmap_small(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	bool invalidate_tlb = (vmm == vmm_current);
	if(cfg->pt[pde] != NULL) {
		/* unmapping small page */
		invalidate_tlb = invalidate_tlb || (cfg->pt[pde]->pt[pte].global);
		*((uint32_t*) &cfg->pt[pde]->pt[pte]) = 0;
	} else if(cfg->pd[pde].present && cfg->pd[pde].pgsz) {
		/* unmapping one small page in a huge page */
		uintptr_t pa = ((vmm_pde_pse_t*) &cfg->pd[pde])->pa << 22;
		size_t flags = vmm_get_flags(vmm, va); // reuse existing func
		invalidate_tlb = invalidate_tlb || (flags & VMM_FLAGS_GLOBAL);
		*((uint32_t*) &cfg->pd[pde]) = 0;
		uintptr_t va_map = va & 0xFFC00000;
		for(size_t i = 0; i < 1024; i++, pa += 4096, va_map += 4096) {
			if(va_map != va) vmm_pgmap_small(vmm, pa, va_map, flags);
		}
	}
	if(invalidate_tlb) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

void vmm_pgunmap(void* vmm, uintptr_t va, size_t pgsz_idx) {
	switch(pgsz_idx) {
		case 0: vmm_pgunmap_small(vmm, va); break;
		case 1: vmm_pgunmap_huge(vmm, va); break;
		default:
			kerror("invalid page size index %u", pgsz_idx);
			break;
	}
}

size_t vmm_get_pgsz(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	if(!cfg->pd[pde].present) return (size_t)-1; // entire 4M region not mapped
	if(cfg->pd[pde].pgsz) return 1; // hugepage
	else return (cfg->pt[pde]->pt[pte].present) ? 0 : (size_t)-1;
}

uintptr_t vmm_get_paddr(void* vmm, uintptr_t va) {
	if(vmm_get_pgsz(vmm, va) == (size_t)-1) return 0; // address is not mapped
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	if(cfg->pd[pde].pgsz) return (((vmm_pde_pse_t*) &cfg->pd[pde])->pa << 22) | (va & 0x3FFFFF); // huge page
	else return ((cfg->pt[pde]->pt[pte].pa << 12) | (va & 0xFFF)); // small page
}

void vmm_set_paddr(void* vmm, uintptr_t va, uintptr_t pa) {
	if(vmm_get_pgsz(vmm, va) == (size_t)-1) return; // address is not mapped - don't do anything
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	if(cfg->pd[pde].pgsz) {
		/* huge page */
		((vmm_pde_pse_t*) &cfg->pd[pde])->pa = pa >> 22;
		if(vmm == vmm_current || ((vmm_pde_pse_t*) &cfg->pd[pde])->global) {
			va &= 0xFFC00000;
			for(size_t i = 0; i < 1024; i++, va += 4096) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
		}
	} else {
		/* small page */
		cfg->pt[pde]->pt[pte].pa = pa >> 12;
		if(vmm == vmm_current || cfg->pt[pde]->pt[pte].global) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
	}
}

void vmm_switch(void* vmm) {
	vmm_t* cfg = vmm;
	asm volatile("mov %0, %%cr3" : : "r"(cfg->cr3) : "memory");
	vmm_current = vmm;
}

void vmm_init() {
	// do nothing - VMM initialization is done during bootstrapping
}

void* vmm_clone(void* src) {
	vmm_t* cfg_src = src;

	uintptr_t cr3;
	vmm_t* cfg_dest = kmalloc_ext(sizeof(vmm_t), 4096, (void**)&cr3);
	if(cfg_dest == NULL) {
		kerror("cannot allocate new VMM config");
		return NULL;
	}
	memcpy(cfg_dest, cfg_src, sizeof(vmm_t));

	/* clone page tables */
	for(size_t i = 0; i < 768; i++) { // do not clone the kernel's top 1G space
		if(cfg_src->pt[i] != NULL) {
			size_t frame = pmm_alloc_free(1);
			
			bool allocated = false;

			if(frame != (size_t)-1) {
				// pmm_alloc(frame);
				for(size_t j = 0; j < 1024; j++) {
					/* find a space to map the new page table for accessing */
					if(!vmm_krnlpt.pt[j].present) {
						*((uint32_t*) &vmm_krnlpt.pt[j]) = (frame << 12) | (1 << 0) | (1 << 1); // present, rw, supervisor
						cfg_dest->pd[i].pt = frame;

						cfg_dest->pt[i] = (vmm_pt_t*) ((uintptr_t) &__krnlpt_start + (j << 12)); // 0xEFC00000 + i * 0x1000

						allocated = true;
						break;
					}
				}
			}

			if(!allocated) {
				kerror("cannot allocate memory for PT #%u", i);
				if(frame != (size_t)-1) pmm_free(frame);
				for(size_t j = 0; j < i; j++) {
					pmm_free(cfg_dest->pd[j].pt);
					*((uint32_t*) &vmm_krnlpt.pt[((uintptr_t)cfg_dest->pt[j] >> 12) & 0xFFF]) = 0;
				}
				kfree(cfg_dest);
				return NULL;
			} else memcpy(cfg_dest->pt[i], cfg_src->pt[i], sizeof(vmm_pt_t)); // copy PT
		}
	}

	cfg_dest->cr3 = cr3;
	return cfg_dest;
}

void vmm_free(void* vmm) {
	kassert((uintptr_t)vmm != (uintptr_t)vmm_current); // we cannot free a configuration that is in use

	if((uintptr_t)vmm == (uintptr_t)&vmm_default) return; // and we cannot free the default one either; however, this is not a fatal issue as we can just skip the deallocation

	vmm_t* cfg = vmm;
	for(size_t i = 0; i < 768; i++) {
		if(cfg->pt[i] != NULL) {
			pmm_free(cfg->pd[i].pt);
			*((uint32_t*) &vmm_krnlpt.pt[((uintptr_t)cfg->pt[i] >> 12) & 0xFFF]) = 0;
		}
	}
	kfree(cfg);
}

size_t vmm_get_flags(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	size_t flags = 0;
	vmm_pde_pse_t* pde_pse = (vmm_pde_pse_t*) &cfg->pd[pde];
	switch(vmm_get_pgsz(vmm, va)) {
		case 0: // small page
			if(cfg->pt[pde]->pt[pte].present) flags |= VMM_FLAGS_PRESENT;
			if(cfg->pt[pde]->pt[pte].rw) flags |= VMM_FLAGS_RW;
			if(cfg->pt[pde]->pt[pte].user) flags |= VMM_FLAGS_USER;
			if(cfg->pt[pde]->pt[pte].global) flags |= VMM_FLAGS_GLOBAL;
			if(!cfg->pt[pde]->pt[pte].ncache) flags |= VMM_FLAGS_CACHE | ((cfg->pt[pde]->pt[pte].wthru) ? VMM_FLAGS_CACHE_WTHRU : 0);
			break;
		case 1: // huge page
			if(pde_pse->present) flags |= VMM_FLAGS_PRESENT;
			if(pde_pse->rw) flags |= VMM_FLAGS_RW;
			if(pde_pse->user) flags |= VMM_FLAGS_USER;
			if(pde_pse->global) flags |= VMM_FLAGS_GLOBAL;
			if(!pde_pse->ncache) flags |= VMM_FLAGS_CACHE | ((pde_pse->wthru) ? VMM_FLAGS_CACHE_WTHRU : 0);
			break;
		default: break;
	}
	return flags;
}

void vmm_set_flags(void* vmm, uintptr_t va, size_t flags) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	vmm_pde_pse_t* pde_pse = (vmm_pde_pse_t*) &cfg->pd[pde];
	bool invalidate_tlb = (vmm == vmm_current) || (flags & VMM_FLAGS_GLOBAL);
	size_t i = 0;
	switch(vmm_get_pgsz(vmm, va)) {
		case 0: // small page
			invalidate_tlb = invalidate_tlb || (cfg->pt[pde]->pt[pte].global);
			cfg->pt[pde]->pt[pte].present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
			cfg->pt[pde]->pt[pte].user = (flags & VMM_FLAGS_USER) ? 1 : 0;
			cfg->pt[pde]->pt[pte].rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
			cfg->pt[pde]->pt[pte].global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
			cfg->pt[pde]->pt[pte].ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
			cfg->pt[pde]->pt[pte].wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
			if(invalidate_tlb) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
			break;
		case 1: // huge page
			invalidate_tlb = invalidate_tlb || (pde_pse->global);
			pde_pse->present = (flags & VMM_FLAGS_PRESENT) ? 1 : 0;
			pde_pse->user = (flags & VMM_FLAGS_USER) ? 1 : 0;
			pde_pse->rw = (flags & VMM_FLAGS_RW) ? 1 : 0;
			pde_pse->global = (flags & VMM_FLAGS_GLOBAL) ? 1 : 0;
			pde_pse->ncache = (flags & VMM_FLAGS_CACHE) ? 0 : 1;
			pde_pse->wthru = (flags & VMM_FLAGS_CACHE_WTHRU) ? 1 : 0;
			if(invalidate_tlb) {
				va &= 0xFFC00000;
				for(; i < 1024; i++, va += 4096) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
			}
			break;
		default: break;
	}
}

bool vmm_get_dirty(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	switch(vmm_get_pgsz(vmm, va)) {
		case 0: return (cfg->pt[pde]->pt[pte].dirty);
		case 1: return (cfg->pd[pde].zero); // dirty for PSE
		default: return false; // not mapped
	}
}

void vmm_set_dirty(void* vmm, uintptr_t va, bool dirty) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	size_t i = 0;
	switch(vmm_get_pgsz(vmm, va)) {
		case 0:
			cfg->pt[pde]->pt[pte].dirty = (dirty) ? 1 : 0;
			if(vmm == vmm_current || cfg->pt[pde]->pt[pte].global) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
			break;
		case 1:
			cfg->pd[pde].zero = (dirty) ? 1 : 0; // dirty for PSE
			if(vmm == vmm_current || (cfg->pd[pde].avail & 1)) { // global for PSE
				va &= 0xFFC00000;
				for(; i < 1024; i++, va += 4096) asm volatile("invlpg (%0)" : : "r"(va) : "memory");
			}
			break;
		default: break;
	}
}
