#include <mm/pmm.h>
#include <arch/x86/multiboot.h>
#include <kernel/log.h>

void pmm_init() {
	if(mb_info->flags & (1 << 6)) {
		struct multiboot_mmap_entry* entry = mb_traverse_mmap(NULL);
		for(size_t i = 0; entry != NULL; i++) {
			kdebug("mmap entry %u: base %08x%08x len %08x%08x type %u", i, entry->addr_h, entry->addr_l, entry->len_h, entry->len_l, entry->type);
			if(!entry->addr_h && !entry->len_h && entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
				uint64_t frame_hi = (entry->addr_l + entry->len_l) >> 12;
				for(size_t i = entry->addr_l >> 12; i < (size_t) frame_hi; i++) {
					pmm_free(i);
				}
			}
			entry = mb_traverse_mmap(entry);
		}
	}
	kdebug("bitmap @ 0x%08x, %u frame(s) in total", (uintptr_t) pmm_bitmap, pmm_frames);
}
