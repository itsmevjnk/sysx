#include <arch/x86/multiboot.h>

struct multiboot_info* mb_info = NULL;

struct multiboot_mmap_entry* mb_traverse_mmap(struct multiboot_mmap_entry* prev) {
	if(mb_info == NULL) return NULL;
	if(prev == NULL) return (struct multiboot_mmap_entry*) mb_info->mmap_addr;
	struct multiboot_mmap_entry* ret = (struct multiboot_mmap_entry*) ((uintptr_t) prev + prev->size + sizeof(prev->size));
	if((uintptr_t) ret >= (mb_info->mmap_addr + mb_info->mmap_length)) return NULL;
	return ret;
}
