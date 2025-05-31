#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/addr.h>
#include <stdlib.h>
#include <kernel/log.h>
#include <helpers/mutex.h>
#include <string.h>

void* vmm_current = NULL;
void* vmm_kernel = NULL;

uintptr_t vmm_map(void* vmm, uintptr_t pa, uintptr_t va, size_t sz, size_t pgsz_max_idx, size_t flags) {
	size_t pgsz_num = vmm_pgsz_num();
	if(pgsz_max_idx >= pgsz_num) pgsz_max_idx = pgsz_num - 1;

	size_t delta = 0;
	size_t pgsz_min = vmm_pgsz(0);
	if(pa % pgsz_min) {
		delta = pa;
		pa = (pa / pgsz_min) * pgsz_min;
		delta -= pa;
	}
	if(va % pgsz_min) {
		size_t d = va;
		va = (va / pgsz_min) * pgsz_min;
		d = delta - va;
		if(delta < d) delta = d;
	}
	sz += delta;
	if(sz % pgsz_min) sz += pgsz_min - sz % pgsz_min;
	while(pgsz_max_idx && vmm_pgsz(pgsz_max_idx) > sz) pgsz_max_idx--; // reduce to most suitable maximum size
	size_t va_end = va + sz; // ending virtual address
	while(va < va_end) {
		for(int i = pgsz_max_idx; i >= 0; i--) {
			size_t pgsz = vmm_pgsz(i);
			if(va + pgsz > va_end || pa % pgsz || va % pgsz) continue; // use something else!
			vmm_pgmap(vmm, pa, va, i, flags);
			pa += pgsz; va += pgsz;
			break; // exit from the for loop
		}
	}
	
	return (va - sz + delta);
}

void vmm_unmap(void* vmm, uintptr_t va, size_t sz) {
	size_t pgsz_min = vmm_pgsz(0);
	if(va % pgsz_min) {
		size_t delta = va;
		va = (va / pgsz_min) * pgsz_min;
		delta -= va;
		sz += delta;
	}
	if(sz % pgsz_min) sz += pgsz_min - sz % pgsz_min;
	size_t va_end = va + sz;
	size_t pgsz_max_idx = vmm_pgsz_num() - 1;
	while(va < va_end) {
		for(int i = pgsz_max_idx; i >= 0; i--) {
			size_t pgsz = vmm_pgsz(i);
			if(va + pgsz > va_end || va % pgsz) continue; // use something else!
			vmm_pgunmap(vmm, va, i);
			va += pgsz;
		}
	}
}

uintptr_t vmm_first_free(void* vmm, uintptr_t va_start, uintptr_t va_end, size_t sz, size_t align, bool reverse) {
	/* convert virtual address to VMM page number */
	size_t pgsz = vmm_pgsz(0); // minimum page size
	if(!va_start) va_start++; // avoid null
	else va_start = (va_start + pgsz - 1) / pgsz;
	va_end /= pgsz;
	sz = (sz + pgsz - 1) / pgsz; // sz now stores the number of pages required
	align /= sz; // convert to pages
	uintptr_t result = (!reverse) ? va_start : (va_end - sz); size_t blk_sz = 0;
	while(result >= va_start && result + sz <= va_end) {
		if(vmm_get_pgsz(vmm, (result + blk_sz) * pgsz) != (size_t) -1) {
			/* block end */
			if(reverse) {
				result -= blk_sz + 1;
				if(align && result % align) result -= result % align;
			} else {
				result += blk_sz + 1;
				if(align && result % align) result += align - result % align;
			}
			blk_sz = 0;
			continue;
		}
		/* this page is free */
		blk_sz++;
		if(blk_sz == sz) return result * pgsz; // we've reached our target
	}
	return 0; // cannot find block
}

uintptr_t vmm_alloc_map(void* vmm, uintptr_t pa, size_t sz, uintptr_t va_start, uintptr_t va_end, size_t va_align, size_t pgsz_max_idx, bool reverse, size_t flags) {
	size_t off = pa % vmm_pgsz(0);
	pa -= off; sz += off;
	uintptr_t vaddr = vmm_first_free(vmm, va_start, va_end, sz, va_align, reverse);
	if(!vaddr) return 0; // cannot find space
	vmm_map(vmm, pa, vaddr, sz, pgsz_max_idx, flags);
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
	if(!trap) {
		/* create new trap */
		vmm_trap_t* new_trap = krealloc(vmm_traps, (vmm_traps_maxlen + VMM_TRAP_ALLOCSZ) * sizeof(vmm_trap_t));
		if(!new_trap) {
			kerror("cannot allocate memory for new trap (type %u, vmm 0x%x, vaddr 0x%x)", type, vmm, vaddr);
		} else {
			trap = &new_trap[vmm_traps_maxlen];
			vmm_traps = new_trap;
			memset(&new_trap[vmm_traps_maxlen + 1], 0, (VMM_TRAP_ALLOCSZ - 1) * sizeof(vmm_trap_t));
			vmm_traps_maxlen += VMM_TRAP_ALLOCSZ;
		}
	}
	if(trap) {
		trap->type = type;
		trap->vmm = vmm;
		trap->vaddr = vaddr;
	}
	mutex_release(&vmm_traps_mutex);
	return trap; // cannot create new trap
}

void vmm_delete_trap(vmm_trap_t* trap) {
	if(!trap) return;
	trap->type = VMM_TRAP_NONE;
}

size_t vmm_cow_setup(void* vmm_src, uintptr_t vaddr_src, void* vmm_dst, uintptr_t vaddr_dst, size_t size) {
	/* page-align addresses */
	size_t size_delta = 0;
	size_t pgsz_min = vmm_pgsz(0); // minimum page size
	if(vaddr_src % pgsz_min) {
		size_delta = vaddr_src % pgsz_min;
		vaddr_src -= size_delta;
	}
	if(vaddr_dst % pgsz_min) {
		size_t d = vaddr_dst % pgsz_min;
		if(d > size_delta) size_delta = d;
		vaddr_dst -= d;
	}
	size += size_delta;
	if(size % pgsz_min) size += size - size % pgsz_min; // round up size to the nearest minimum page boundary
	
	size_t done_sz = 0;
	while(done_sz < size) {
		size_t pgsz_src = vmm_get_pgsz(vmm_src, vaddr_src); // source's page size
		if(pgsz_src == (size_t)-1 || vmm_get_pgsz(vmm_dst, vaddr_dst) != (size_t)-1) break; // source is not mapped, or destination is mapped already
		if(pgsz_src && vmm_pgsz(pgsz_src) > size - done_sz) {
			/* page is too big - subdivide the page here */
			size_t pgsz_src_new = pgsz_src; while(pgsz_src_new && vmm_pgsz(pgsz_src_new) > size - done_sz) pgsz_src_new--; // suitable size
			size_t flags = vmm_get_flags(vmm_src, vaddr_src); // store the flags before we proceed with unmapping and remapping
			uintptr_t va_start = vaddr_src - vaddr_src % vmm_pgsz(pgsz_src);
			uintptr_t pa_start = vmm_get_paddr(vmm_src, va_start);
			uintptr_t va_end = va_start + vmm_pgsz(pgsz_src);
			vmm_map(vmm_src, pa_start, va_start, vaddr_src - va_start, pgsz_src, flags); // space before our page
			size_t new_sz = vmm_pgsz(pgsz_src_new);
			vmm_map(vmm_src, pa_start + vaddr_src - va_start, vaddr_src, new_sz, pgsz_src_new, flags); // our page
			vmm_map(vmm_src, pa_start + vaddr_src - va_start + new_sz, vaddr_src + new_sz, va_end - (vaddr_src + new_sz), pgsz_src, flags); // space after our page
			pgsz_src = pgsz_src_new;
		}
		vmm_pgmap(vmm_dst, vmm_get_paddr(vmm_src, vaddr_src), vaddr_dst + done_sz, pgsz_src, (vmm_get_flags(vmm_src, vaddr_src + done_sz) & ~VMM_FLAGS_RW) | VMM_FLAGS_TRAPPED);
		vmm_trap_t* src = vmm_new_trap(vmm_src, vaddr_src, VMM_TRAP_COW);
		vmm_trap_t* dst = vmm_new_trap(vmm_dst, vaddr_dst, VMM_TRAP_COW);
		if(!src || !dst) {
			/* cannot place COW order */
			vmm_delete_trap(src);
			vmm_delete_trap(dst);
			break;
		}
		dst->info = src;
		src->info = dst;
		vmm_set_flags(vmm_src, vaddr_src, (vmm_get_flags(vmm_src, vaddr_src) & ~VMM_FLAGS_RW) | VMM_FLAGS_TRAPPED); // disable RW so that we get page faults
		// vmm_set_flags(vmm_dst, vaddr_dst, vmm_get_flags(vmm_dst, vaddr_dst) & ~VMM_FLAGS_RW);

		size_t pgsz = vmm_pgsz(pgsz_src);
		done_sz += pgsz; vaddr_src += pgsz; vaddr_dst += pgsz;
	}

	return done_sz;
}

bool vmm_cow_duplicate(void* vmm, uintptr_t vaddr, size_t pgsz) {
	if(pgsz == (size_t)-1) pgsz = vmm_get_pgsz(vmm, vaddr);
	if(pgsz == (size_t)-1) {
		// kdebug("page fault is caused by accessing an non-existant page, exiting");
		return false;
	}
	pgsz = vmm_pgsz(pgsz); // resolve to size in bytes

	vaddr -= vaddr % pgsz; // page-align address
	
	/* find the page's COW trap index */
	size_t idx_dst = 0;
	mutex_acquire(&vmm_traps_mutex);
	for(; idx_dst < vmm_traps_maxlen; idx_dst++) {
		if(vmm_traps[idx_dst].type == VMM_TRAP_COW && vmm_traps[idx_dst].vmm == vmm && vmm_traps[idx_dst].vaddr == vaddr) break;
	}
	if(idx_dst == vmm_traps_maxlen) {
		mutex_release(&vmm_traps_mutex);
		return false; // cannot find COW trap entry
	}
	vmm_trap_t* dst = &vmm_traps[idx_dst];
	vmm_trap_t* src = dst->info;

	/* find virtual address space to map memory to for copying */
	size_t framesz = pmm_framesz(); // PMM frame size
    void* copy_src = (void*) vmm_alloc_map(vmm_current, 0, 2 * framesz, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
	if(!copy_src) {
		kerror("cannot find virtual address space to map for copying");
		mutex_release(&vmm_traps_mutex);
		return false;
	}

	/* allocate memory for the new page */
	size_t rq_frames = pgsz / framesz; // number of frames we'll be requesting
	size_t frame = pmm_alloc_free(rq_frames);
	if(frame == (size_t)-1) {
		kerror("cannot allocate memory for COW");
		mutex_release(&vmm_traps_mutex);
		return false;
	}

	/* map memory and perform copy */
	for(size_t i = 0; i < rq_frames; i++) {
		vmm_set_paddr(vmm_current, (uintptr_t) copy_src, vmm_get_paddr(src->vmm, src->vaddr) + i * framesz);
		vmm_set_paddr(vmm_current, (uintptr_t) copy_src + framesz, (frame + i) * framesz);
		memcpy((void*)((uintptr_t) copy_src + pgsz), copy_src, framesz);
	}

	/* change destination's physical address to the allocated frame */
	vmm_set_paddr(vmm, vaddr, frame * framesz);
	vmm_set_flags(vmm, vaddr, vmm_get_flags(vmm, vaddr) | VMM_FLAGS_RW);
	kdebug("resolved CoW: vaddr 0x%x (VMM 0x%x) mapped to paddr 0x%x", vaddr, (uintptr_t) vmm, frame * framesz);

	/* check if the source will not be referenced */
	size_t idx_src_ref = 0;
	for(; idx_src_ref < vmm_traps_maxlen; idx_src_ref++) {
		if(src == &vmm_traps[idx_src_ref]) continue;
		if(vmm_traps[idx_src_ref].type == VMM_TRAP_COW && vmm_traps[idx_src_ref].vmm == vmm && vmm_traps[idx_src_ref].vaddr == vaddr) break;
	}
	if(idx_src_ref != vmm_traps_maxlen) {
		vmm_set_flags(src->vmm, src->vaddr, (vmm_get_flags(src->vmm, src->vaddr) & ~VMM_FLAGS_TRAPPED) | VMM_FLAGS_RW);
	}

	/* delete traps */
	vmm_delete_trap(src);
	vmm_delete_trap(dst);

	mutex_release(&vmm_traps_mutex);
	
	return true;
}

bool vmm_trap_remove(void* vmm) {
	mutex_acquire(&vmm_traps_mutex);

	size_t resolved_cnt = 0, resolved_maxcnt = 0; // TODO: something like a hashmap would be more appropriate here
	uintptr_t* resolved = NULL; // resolved[3k] = vaddr, resolved[3k+1] = corresponding new source vmm, resolved[3k+2] = corresponding new source vaddr

	for(size_t i = 0; i < vmm_traps_maxlen; i++) {
		vmm_trap_t* trap = &vmm_traps[i];
		if(trap->type == VMM_TRAP_NONE) continue;

		/* resolve CoW orders */
		if(trap->type == VMM_TRAP_COW && ((vmm_trap_t*)trap->info)->vmm == vmm) { // CoW order sourcing from vmm
			bool done = false;
			for(size_t j = 0; j < resolved_cnt; j++) { // check if it has been resolved before
				if(resolved[j * 3] == trap->vaddr) {
					void* dst_vmm = trap->vmm; uintptr_t dst_vaddr = trap->vaddr;
					void* src_vmm = (void*)resolved[j * 3 + 1]; uintptr_t src_vaddr = resolved[j * 3 + 2];
					size_t pgsz = vmm_get_pgsz(dst_vmm, dst_vaddr);
					vmm_delete_trap((vmm_trap_t*)trap->info); vmm_delete_trap(trap); // delete old relation
					if(pgsz == (size_t)-1) { // unmapped - nothing to do here
						done = true;
						break;
					} else pgsz = vmm_pgsz(pgsz); // convert to bytes
					if(!vmm_cow_setup(src_vmm, src_vaddr, dst_vmm, dst_vaddr, pgsz)) {
						kerror("cannot set up new CoW relation: vmm:vaddr 0x%x:0x%x <-> 0x%x:0x%x", (uintptr_t)src_vmm, src_vaddr, (uintptr_t)dst_vmm, dst_vaddr);
						mutex_release(&vmm_traps_mutex);
						kfree(resolved);
						return false;
					}
					done = true;
					break;
				}
			}
			if(done) continue;

			/* first occurrence */
			if(resolved_maxcnt == resolved_cnt) {
				resolved_maxcnt += VMM_TRAP_ALLOCSZ;
				resolved = krealloc(resolved, resolved_maxcnt * 3 * sizeof(uintptr_t));
				if(!resolved) {
					kerror("cannot allocate CoW resolution tracking table");
					mutex_release(&vmm_traps_mutex);
					return false;
				}
			}
			void* dst_vmm = trap->vmm; uintptr_t dst_vaddr = trap->vaddr;
			vmm_set_flags(dst_vmm, dst_vaddr, vmm_get_flags(dst_vmm, dst_vaddr) | VMM_FLAGS_RW); // destination now owns the frame
			vmm_delete_trap((vmm_trap_t*)trap->info); vmm_delete_trap(trap);
			resolved[resolved_cnt * 3] = trap->vaddr;
			resolved[resolved_cnt * 3 + 1] = (uintptr_t)dst_vmm;
			resolved[resolved_cnt * 3 + 2] = dst_vaddr;
			resolved_cnt++;
		}
		else if(trap->vmm == vmm) { // traps targeting this vmm in general
			vmm_delete_trap(trap);
		}
	}

	kfree(resolved);

	mutex_release(&vmm_traps_mutex);
	return true;
}

bool vmm_handle_fault(uintptr_t vaddr, size_t flags) {
	kdebug("page fault on vaddr 0x%x (vmm_current = 0x%x), flags 0x%x", vaddr, vmm_current, flags);
	if(flags & VMM_FLAGS_RW) {
		/* write access caused this fault */
		if(vmm_cow_duplicate(vmm_current, vaddr, (size_t)-1)) return true; // COW?
	}
	return false;
}

vmm_trap_t* vmm_is_cow(void* vmm, uintptr_t vaddr, bool validated) {
	if(!validated) {
		size_t pgsz_idx = vmm_get_pgsz(vmm, vaddr);
		if(pgsz_idx == (size_t)-1) return NULL; // page isn't mapped
		vaddr -= vaddr % vmm_pgsz(pgsz_idx);
	}

	mutex_acquire(&vmm_traps_mutex);

	for(size_t i = 0; i < vmm_traps_maxlen; i++) {
		if(vmm_traps[i].type == VMM_TRAP_COW && vmm_traps[i].vmm == vmm && vmm_traps[i].vaddr == vaddr) {
			mutex_release(&vmm_traps_mutex);
			return &vmm_traps[i];
		}
	}

	mutex_release(&vmm_traps_mutex);
	return NULL;
}

/* deallocation staging */

static mutex_t vmm_frstage_mutex = {0};
static void** vmm_frstage = NULL;
static size_t vmm_frstage_len = 0;

#ifndef VMM_FRSTAGE_ALLOCSZ
#define VMM_FRSTAGE_ALLOCSZ							4
#endif

void vmm_stage_free(void* vmm) {
	mutex_acquire(&vmm_frstage_mutex);
	size_t i = 0;
	for(; i < vmm_frstage_len; i++) {
		if(!vmm_frstage[i]) break;
	}
	if(i == vmm_frstage_len) {
		void** new_frstage = krealloc(vmm_frstage, (vmm_frstage_len + VMM_FRSTAGE_ALLOCSZ) * sizeof(void*));
		if(!new_frstage) {
			kerror("cannot extend deallocation staging queue");
			mutex_release(&vmm_frstage_mutex);
			return;
		}
		vmm_frstage_len += VMM_FRSTAGE_ALLOCSZ;
		vmm_frstage = new_frstage;
		memset(&vmm_frstage[i + 1], 0, (VMM_FRSTAGE_ALLOCSZ - 1) * sizeof(void*));
	}
	kdebug("staging VMM 0x%x for deletion", vmm);
	vmm_frstage[i] = vmm;
	mutex_release(&vmm_frstage_mutex);
}

void vmm_do_cleanup() {
	if(mutex_test(&vmm_frstage_mutex)) return; // someone else is doing our work
	mutex_acquire(&vmm_frstage_mutex);
	for(size_t i = 0; i < vmm_frstage_len; i++) {
		if(vmm_frstage[i] && vmm_frstage[i] != vmm_current) {
			kdebug("deleting staged VMM 0x%x", vmm_frstage[i]);
			vmm_free(vmm_frstage[i]);
			vmm_frstage[i] = NULL;
		}
	}
	mutex_release(&vmm_frstage_mutex);
}
