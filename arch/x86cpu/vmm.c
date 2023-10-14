#include <mm/vmm.h>

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

size_t vmm_pgsz() {
	return 4096;
}

void vmm_pgmap(void* vmm, uintptr_t pa, uintptr_t va, bool present, bool user, bool rw) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	if(cfg->pt[pde] == NULL) {
		/* create page table */

	}
	cfg->pt[pde]->pt[pte].present = (present) ? 1 : 0;
	cfg->pt[pde]->pt[pte].user = (user) ? 1 : 0;
	cfg->pt[pde]->pt[pte].rw = (rw) ? 1 : 0;
	cfg->pt[pde]->pt[pte].zero = 0;
	cfg->pt[pde]->pt[pte].pa = pa >> 12;
	asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

void vmm_pgunmap(void* vmm, uintptr_t va) {
	vmm_t* cfg = vmm;
	size_t pde = va >> 22, pte = (va >> 12) & 0x3ff;
	cfg->pt[pde]->pt[pte].present = 0;
	asm volatile("invlpg (%0)" : : "r"(va) : "memory");
}

void vmm_switch(void* vmm) {
	vmm_t* cfg = vmm;
	asm volatile("mov %0, %%cr3" : : "r"(cfg->cr3) : "memory");
	vmm_current = vmm;
}

void vmm_init() {
	// do nothing - VMM initialization is done during bootstrapping
}
