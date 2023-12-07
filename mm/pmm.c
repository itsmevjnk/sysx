#include <mm/pmm.h>
#include <mm/vmm.h>
#include <kernel/log.h>
#include <helpers/mutex.h>

uintptr_t* pmm_bitmap = NULL;
size_t pmm_frames = 0;

size_t pmm_framesz() {
	return vmm_pgsz();
}

int pmm_alloc(size_t frame) {
	size_t off = frame / (sizeof(uintptr_t) * 8);
	size_t bit = frame % (sizeof(uintptr_t) * 8);
	if(pmm_bitmap[off] & (1 << bit)) return -1;
	pmm_bitmap[off] |= 1 << bit;
	return 0;
}

void pmm_free(size_t frame) {
	size_t off = frame / (sizeof(uintptr_t) * 8);
	size_t bit = frame % (sizeof(uintptr_t) * 8);
	pmm_bitmap[off] &= ~(1 << bit);
}

size_t pmm_first_free(size_t sz) {
	size_t frame = 0;
	while(frame < pmm_frames) {
		size_t off = frame / (sizeof(uintptr_t) * 8);
		if(pmm_bitmap[off] == (uintptr_t)-1) goto next;
		size_t bit = frame % (sizeof(uintptr_t) * 8);
		if(!(pmm_bitmap[off] & (1 << bit))) {
			size_t fail = (size_t) -1;
			for(size_t i = 0; i < sz; i++) {
				size_t of2 = (frame + i) / (sizeof(uintptr_t) * 8);
				if(pmm_bitmap[of2] == (uintptr_t)-1) goto fail;
				size_t bi2 = (frame + i) % (sizeof(uintptr_t) * 8);
				if(pmm_bitmap[of2] & (1 << bi2)) {
fail:
					fail = frame + i;
					break;
				}
			}
			if(fail != (size_t)-1) frame = fail + 1;
			else return frame;
		}
next:
		frame++;
	}
	kerror("out of memory");
	return (size_t) -1; // out of memory
}

static mutex_t pmm_alloc_mutex = {0};

size_t pmm_alloc_free(size_t sz) {
	mutex_acquire(&pmm_alloc_mutex);
	size_t frame = pmm_first_free(sz);
	if(frame != (size_t)-1) {
		for(size_t i = 0; i < sz; i++) pmm_alloc(frame + i);
	}
	mutex_release(&pmm_alloc_mutex);
	return frame;
}
