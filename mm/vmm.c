#include <mm/vmm.h>

void* vmm_current = NULL;
void* vmm_kernel = NULL;

void vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, bool present, bool user, bool rw) {
	size_t delta = 0;
	if(pa % vmm_pgsz()) {
		delta = pa;
		pa = (pa / vmm_pgsz()) * vmm_pgsz();
		delta = pa - delta;
	}
	if(va % vmm_pgsz()) {
		size_t d = va;
		va = (va / vmm_pgsz()) * vmm_pgsz();
		d = va - delta;
		if(delta < d) delta = d;
	}
	sz += delta;
	for(size_t i = 0; i < (sz + vmm_pgsz() - 1) / vmm_pgsz(); i++)
		vmm_pgmap(vmm, pa + i * vmm_pgsz(), va + i * vmm_pgsz(), present, user, rw);
}

void vmm_unmap(void* vmm, uintptr_t va, size_t sz) {
	if(va % vmm_pgsz()) {
		size_t delta = va;
		va = (va / vmm_pgsz()) * vmm_pgsz();
		delta = va - delta;
		sz += delta;
	}
	for(size_t i = 0; i < (sz + vmm_pgsz() - 1) / vmm_pgsz(); i++)
		vmm_pgunmap(vmm, va + i * vmm_pgsz());
}

uintptr_t vmm_first_free(void* vmm, uintptr_t va, size_t sz) {
	/* convert virtual address to VMM page number */
	if(va == 0) va = 1; // avoid null
	else va = (va + vmm_pgsz() - 1) / vmm_pgsz();
	sz = (sz + vmm_pgsz() - 1) / vmm_pgsz(); // sz now stores the number of pages required
	uintptr_t result = va; size_t blk_sz = 0;
	while((result + sz) * vmm_pgsz() != 0) {
		if(vmm_physaddr(vmm, (result + blk_sz) * vmm_pgsz()) != 0) {
			/* block end */
			result += blk_sz + 1;
			blk_sz = 0;
			continue;
		}
		/* this page is free */
		blk_sz++;
		if(blk_sz == sz) return result * vmm_pgsz(); // we've reached our target
	}
	return 0; // cannot find block
}