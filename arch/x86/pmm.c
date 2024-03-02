#include <mm/pmm.h>
#include <mm/addr.h>
#include <arch/x86/multiboot.h>
#include <kernel/log.h>

extern uintptr_t __pre_start;

void pmm_init() {
	/* mark free frames according to memory map */
	if(mb_info->flags & (1 << 6)) {
		struct multiboot_mmap_entry* entry = mb_traverse_mmap(NULL);
		for(size_t i = 0; entry; i++) {
			kdebug("mmap entry %u: base %08x%08x len %08x%08x type %u", i, entry->addr_h, entry->addr_l, entry->len_h, entry->len_l, entry->type);
			if(!entry->addr_h && !entry->len_h && entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
				uint64_t frame_hi = (entry->addr_l + entry->len_l) >> 12;
				for(size_t i = entry->addr_l >> 12; i < (size_t) frame_hi; i++) {
					if(i >= ((uintptr_t)&__pre_start >> 12)) pmm_free(i); // reserve the memory space before the kernel (i.e. the first 16M) as we may need access to it later
				}
			}
			entry = mb_traverse_mmap(entry);
		}
	}

	/* mark kernel frames as used */
	for(size_t frame = ((uintptr_t)&__pre_start >> 12); frame < ((kernel_end - 0xC0000000) >> 12); frame++)
		pmm_alloc(frame);

	kdebug("bitmap @ 0x%08x, %u frame(s) in total", (uintptr_t) pmm_bitmap, pmm_frames);
}
