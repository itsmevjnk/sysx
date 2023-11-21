#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/addr.h>
#include <mm/kheap.h>
#include <kernel/log.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <arch/x86cpu/task.h>

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
	size_t zero : 1;
	size_t global : 1;
	size_t avail : 3;
	uintptr_t pa : 20; // phys addr >> 12
} __attribute__((packed)) vmm_pte_t;

typedef struct {
	vmm_pte_t pt[1024];
} __attribute__((packed)) vmm_pt_t;

typedef struct {
	vmm_pde_t pd[1024];
	vmm_pt_t* pt[1024];
	uintptr_t cr3; // phys ptr to page directory
} __attribute__((packed)) vmm_t;

vmm_t vmm_default __attribute__((aligned(4096))); // for bootstrap code
vmm_pt_t vmm_krnlpt __attribute__((aligned(4096))); // kernel page table for mapping newly allocated page tables to EFC00000->F0000000

extern uintptr_t __krnlpt_start; // krnlpt start location (in link.ld)

size_t vmm_pgsz() {
	return 4096;
}

void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, bool present, bool user, bool rw, uint8_t caching, bool global) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	if(cfg->pt[pde] == NULL) {
		/* allocate page table */
		size_t frame = pmm_first_free(1); // ask for a single contiguous free frame
		if(frame == (size_t)-1) kerror("no more free frames, brace for impact");
		else {
			pmm_alloc(frame);
			bool allocated = false;
			for(size_t i = 0; i < 1024; i++) {
				/* find a space to map the new page table for accessing */
				if(!vmm_krnlpt.pt[i].present) {

					vmm_krnlpt.pt[i].present = 1;
					vmm_krnlpt.pt[i].rw = 1;
					vmm_krnlpt.pt[i].user = 0;
					vmm_krnlpt.pt[i].pa = frame;

					cfg->pd[pde].present = 0; // to be filled later
					cfg->pd[pde].rw = 0;
					cfg->pd[pde].user = 0;
					cfg->pd[pde].wthru = 0;
					cfg->pd[pde].ncache = 1; // TODO: VMM caching
					cfg->pd[pde].accessed = 0; // just in case this is used
					cfg->pd[pde].zero = 0;
					cfg->pd[pde].pgsz = 0; // no PSE (yet)
					cfg->pd[pde].pt = vmm_krnlpt.pt[i].pa;

					cfg->pt[pde] = (vmm_pt_t*) ((uintptr_t) &__krnlpt_start + (i << 12)); // 0xEFC00000 + i * 0x1000

					if(task_kernel != NULL && va >= kernel_start) {
						/* map kernel pages to all tasks' VMM configs */
						task_t* task = task_kernel;
						do {
							if(task->common.vmm != vmm && ((vmm_t*)task->common.vmm)->pt[pde] != cfg->pt[pde]) {
								/* copy PT pointer and PDE */
								((vmm_t*)task->common.vmm)->pt[pde] = cfg->pt[pde];
								memcpy(&((vmm_t*)task->common.vmm)->pd[pde], &cfg->pd[pde], sizeof(vmm_pde_t));
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
	}

	/* set settings in page directory entry */
	cfg->pd[pde].present |= (present) ? 1 : 0;
	cfg->pd[pde].rw |= (rw) ? 1 : 0;
	cfg->pd[pde].user |= (user) ? 1 : 0;

	/* populate page table entry */
	cfg->pt[pde]->pt[pte].present = (present) ? 1 : 0;
	cfg->pt[pde]->pt[pte].user = (user) ? 1 : 0;
	cfg->pt[pde]->pt[pte].rw = (rw) ? 1 : 0;
	cfg->pt[pde]->pt[pte].zero = 0;
	cfg->pt[pde]->pt[pte].global = (global) ? 1 : 0;
	switch(caching) {
		case VMM_CACHE_NONE: cfg->pt[pde]->pt[pte].ncache = 1; break;
		case VMM_CACHE_WTHRU: cfg->pt[pde]->pt[pte].ncache = 0; cfg->pt[pde]->pt[pte].wthru = 1; break;
		case VMM_CACHE_WBACK: cfg->pt[pde]->pt[pte].ncache = 0; cfg->pt[pde]->pt[pte].wthru = 0; break;
		default: break;
	}
	cfg->pt[pde]->pt[pte].pa = pa >> 12;
	asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

void vmm_pgunmap(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	cfg->pt[pde]->pt[pte].present = 0;
	asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

uintptr_t vmm_physaddr(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	if(!cfg->pd[pde].present || cfg->pt[pde] == NULL || !cfg->pt[pde]->pt[pte].present) return 0; // address is not mapped
	return ((cfg->pt[pde]->pt[pte].pa << 12) | (va & 0xFFF));
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
			size_t frame = pmm_first_free(1);
			
			bool allocated = false;

			if(frame != (size_t)-1) {
				pmm_alloc(frame);
				for(size_t j = 0; j < 1024; j++) {
					/* find a space to map the new page table for accessing */
					if(!vmm_krnlpt.pt[j].present) {
						vmm_krnlpt.pt[j].present = 1;
						vmm_krnlpt.pt[j].rw = 1;
						vmm_krnlpt.pt[j].user = 0;
						vmm_krnlpt.pt[j].pa = frame;

						cfg_dest->pd[i].pt = vmm_krnlpt.pt[j].pa;

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
					vmm_pgunmap(vmm_current, (uintptr_t) cfg_dest->pt[j]);
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
			vmm_pgunmap(vmm_current, (uintptr_t) cfg->pt[i]);
		}
	}
	kfree(cfg);
}
