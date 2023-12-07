#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/addr.h>
#include <stdlib.h>
#include <kernel/log.h>
#include <helpers/mutex.h>
#include <string.h>

void* vmm_current = NULL;
void* vmm_kernel = NULL;

uintptr_t vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, size_t flags) {
	size_t delta = 0;
	if(pa % vmm_pgsz()) {
		delta = pa;
		pa = (pa / vmm_pgsz()) * vmm_pgsz();
		delta -= pa;
	}
	if(va % vmm_pgsz()) {
		size_t d = va;
		va = (va / vmm_pgsz()) * vmm_pgsz();
		d = delta - va;
		if(delta < d) delta = d;
	}
	sz += delta;
	for(size_t i = 0; i < (sz + vmm_pgsz() - 1) / vmm_pgsz(); i++)
		vmm_pgmap(vmm, pa + i * vmm_pgsz(), va + i * vmm_pgsz(), flags);
	
	return (va + delta);
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

uintptr_t vmm_first_free(void* vmm, uintptr_t va_start, uintptr_t va_end, size_t sz, bool reverse) {
	/* convert virtual address to VMM page number */
	size_t pgsz = vmm_pgsz();
	if(va_start == 0) va_start = 1; // avoid null
	else va_start = (va_start + pgsz - 1) / pgsz;
	va_end /= pgsz;
	sz = (sz + pgsz - 1) / pgsz; // sz now stores the number of pages required
	uintptr_t result = (!reverse) ? va_start : (va_end - sz); size_t blk_sz = 0;
	while(result >= va_start && result + sz <= va_end) {
		if(vmm_get_paddr(vmm, (result + blk_sz) * pgsz) != 0) {
			/* block end */
			if(reverse) result -= blk_sz + 1;
			else result += blk_sz + 1;
			blk_sz = 0;
			continue;
		}
		/* this page is free */
		blk_sz++;
		if(blk_sz == sz) return result * pgsz; // we've reached our target
	}
	return 0; // cannot find block
}

uintptr_t vmm_alloc_map(void* vmm, uintptr_t pa, size_t sz, uintptr_t va_start, uintptr_t va_end, bool reverse, size_t flags) {
	size_t off = pa % vmm_pgsz();
	pa -= off; sz += off;
	uintptr_t vaddr = vmm_first_free(vmm, va_start, va_end, sz, reverse);
	if(!vaddr) return 0; // cannot find space
	vmm_map(vmm, pa, vaddr, sz, flags);
	return (vaddr + off);
}

static vmm_trap_t* vmm_traps = NULL;
static size_t vmm_traps_maxlen = 0;
static mutex_t vmm_traps_mutex = {0};

#ifndef VMM_TRAP_ALLOCSZ
#define VMM_TRAP_ALLOCSZ			4 // number of entries to be allocated at once
#endif

vmm_trap_t* vmm_new_trap(void* vmm, uintptr_t vaddr, enum vmm_trap_type type) {
	vmm_trap_t* trap = NULL;
	mutex_acquire(&vmm_traps_mutex);
	for(size_t i = 0; i < vmm_traps_maxlen; i++) {
		if(vmm_traps[i].type == VMM_TRAP_NONE) {
			trap = &vmm_traps[i];
			break;
		}
	}
	if(trap == NULL) {
		/* create new trap */
		vmm_trap_t* new_trap = krealloc(vmm_traps, (vmm_traps_maxlen + VMM_TRAP_ALLOCSZ) * sizeof(vmm_trap_t));
		if(new_trap == NULL) {
			kerror("cannot allocate memory for new trap (type %u, vmm 0x%x, vaddr 0x%x)", type, vmm, vaddr);
		} else {
			trap = &new_trap[vmm_traps_maxlen];
			vmm_traps = new_trap;
			memset(&new_trap[vmm_traps_maxlen + 1], 0, (VMM_TRAP_ALLOCSZ - 1) * sizeof(vmm_trap_t));
			vmm_traps_maxlen += VMM_TRAP_ALLOCSZ;
		}
	}
	if(trap != NULL) {
		trap->type = type;
		trap->vmm = vmm;
		trap->vaddr = vaddr;
	}
	mutex_release(&vmm_traps_mutex);
	return trap; // cannot create new trap
}

void vmm_delete_trap(vmm_trap_t* trap) {
	if(trap == NULL) return;
	trap->type = VMM_TRAP_NONE;
}

size_t vmm_cow_setup(void* vmm_src, uintptr_t vaddr_src, void* vmm_dst, uintptr_t vaddr_dst, size_t size) {
	/* page-align addresses */
	size_t size_delta = 0;
	if(vaddr_src % vmm_pgsz()) {
		size_delta = vaddr_src % vmm_pgsz();
		vaddr_src -= size_delta;
	}
	if(vaddr_dst % vmm_pgsz()) {
		size_t d = vaddr_dst % vmm_pgsz();
		if(d > size_delta) size_delta = d;
		vaddr_dst -= d;
	}
	size += size_delta;

	size_t done_sz = 0;
	for(done_sz = 0; done_sz < size; done_sz += vmm_pgsz()) {
		if(vmm_get_paddr(vmm_src, vaddr_src + done_sz) == 0 || vmm_get_paddr(vmm_dst, vaddr_dst + done_sz) != 0) break; // source is not mapped, or destination is mapped already
		vmm_pgmap(vmm_dst, vmm_get_paddr(vmm_src, vaddr_src + done_sz), vaddr_dst + done_sz, vmm_get_flags(vmm_src, vaddr_src + done_sz) & ~VMM_FLAGS_RW);
		vmm_trap_t* src = vmm_new_trap(vmm_src, vaddr_src + done_sz, VMM_TRAP_COW);
		vmm_trap_t* dst = vmm_new_trap(vmm_dst, vaddr_dst + done_sz, VMM_TRAP_COW);
		if(src == NULL || dst == NULL) {
			/* cannot place COW order */
			vmm_delete_trap(src);
			vmm_delete_trap(dst);
			break;
		}
		dst->info = src;
		src->info = dst;
		vmm_set_flags(vmm_src, vaddr_src + done_sz, vmm_get_flags(vmm_src, vaddr_src + done_sz) & ~VMM_FLAGS_RW); // disable RW so that we get page faults
		// vmm_set_flags(vmm_dst, vaddr_dst + done_sz, vmm_get_flags(vmm_dst, vaddr_dst + done_sz) & ~VMM_FLAGS_RW);
	}

	return done_sz;
}

bool vmm_cow_duplicate(void* vmm, uintptr_t vaddr) {
	size_t pgsz = vmm_pgsz();
	vaddr -= vaddr % pgsz; // page-align address
	
	/* find the page's COW trap index */
	size_t idx_dst = 0;
	for(; idx_dst < vmm_traps_maxlen; idx_dst++) {
		if(vmm_traps[idx_dst].type == VMM_TRAP_COW && vmm_traps[idx_dst].vmm == vmm && vmm_traps[idx_dst].vaddr == vaddr) break;
	}
	if(idx_dst == vmm_traps_maxlen) return false; // cannot find COW trap entry
	vmm_trap_t* dst = &vmm_traps[idx_dst];
	vmm_trap_t* src = dst->info;

	/* find virtual address space to map memory to for copying */
    void* copy_src = (void*) vmm_first_free(vmm_current, pgsz, kernel_start, 2 * pgsz, false);
	if(copy_src == NULL) {
		kerror("cannot find virtual address space to map for copying");
		return false;
	}

	/* allocate memory for the new page */
	size_t frame = pmm_alloc_free(1);
	if(frame == (size_t)-1) {
		kerror("cannot allocate memory for COW");
		return false;
	}

	/* map memory and perform copy */
	vmm_pgmap(vmm_current, vmm_get_paddr(src->vmm, src->vaddr), (uintptr_t) copy_src, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
	vmm_pgmap(vmm_current, frame * pgsz, (uintptr_t) copy_src + pgsz, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
	memcpy((void*)((uintptr_t) copy_src + pgsz), copy_src, pgsz);
	vmm_unmap(vmm_current, (uintptr_t) copy_src, 2 * pgsz);

	/* change destination's physical address to the allocated frame */
	vmm_set_paddr(vmm, vaddr, frame * pgsz);
	vmm_set_flags(vmm, vaddr, vmm_get_flags(vmm, vaddr) | VMM_FLAGS_RW);

	/* check if the source will not be referenced */
	size_t idx_src_ref = 0;
	for(; idx_src_ref < vmm_traps_maxlen; idx_src_ref++) {
		if(src == &vmm_traps[idx_src_ref]) continue;
		if(vmm_traps[idx_src_ref].type == VMM_TRAP_COW && vmm_traps[idx_src_ref].vmm == vmm && vmm_traps[idx_src_ref].vaddr == vaddr) break;
	}
	if(idx_src_ref != vmm_traps_maxlen) {
		vmm_set_flags(src->vmm, src->vaddr, vmm_get_flags(src->vmm, src->vaddr) | VMM_FLAGS_RW);
	}

	/* delete traps */
	vmm_delete_trap(src);
	vmm_delete_trap(dst);
	
	return true;
}

bool vmm_handle_fault(uintptr_t vaddr, size_t flags) {
	kdebug("page fault on vaddr 0x%x (vmm_current = 0x%x), flags 0x%x", vaddr, vmm_current, flags);
	if(flags & VMM_FLAGS_RW) {
		/* write access caused this fault */
		if(vmm_cow_duplicate(vmm_current, vaddr)) return true; // COW?
	}
	return false;
}
