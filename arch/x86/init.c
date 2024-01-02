#include <arch/x86cpu/gdt.h>
#include <arch/x86cpu/idt.h>
#include <kernel/log.h>
#include <kernel/cmdline.h>
#include <arch/x86/i8259.h>
#include <arch/x86/i8253.h>
#include <arch/x86/rtc.h>
#include <arch/x86/multiboot.h>
#include <string.h>
#include <arch/x86cpu/int32.h>
#include <arch/x86cpu/apic.h>
#include <arch/x86/mptab.h>
#include <arch/x86/bios.h>

#include <fs/vfs.h>
#include <fs/tarfs.h>
#include <fs/memfs.h>

#define INITRD_PATH                             "/initrd.tar"

extern uint16_t x86ext_on;

int ktgt_preinit() {
    if(mb_info->flags & MULTIBOOT_INFO_CMDLINE) {
        cmdline_init((char*)mb_info->cmdline);
        // kdebug("cmdline: %s (argc = %u)", (char*)mb_info->cmdline, kernel_argc);
    }
    stdio_stderr_init();

    kdebug("x86ext_on = 0x%x", x86ext_on);

    kdebug("Multiboot information structure address: 0x%08x", mb_info);

    if(mb_info->flags & MULTIBOOT_INFO_MEMORY) kdebug("memory size: lo %uK, hi %uK", mb_info->mem_lower, mb_info->mem_upper);

    kinfo("initializing GDT"); gdt_init();
    kinfo("initializing IDT"); idt_init();
    kinfo("initializing PIC"); pic_init();
    kinfo("initializing int32 capabilities"); int32_init();
    kinfo("initializing PIT as system timer source"); pit_systimer_init();
    // kinfo("initializing RTC as system timer source"); rtc_timer_enable(); rtc_init();

    kinfo("acquiring EBDA address"); kdebug("EBDA is located at 0x%08X", bios_get_ebda_paddr());

#ifdef DEBUG
    if(mb_info->flags & MULTIBOOT_INFO_MEM_MAP) {
        kdebug("memory map information: ");
		struct multiboot_mmap_entry* entry = mb_traverse_mmap(NULL);
		for(size_t i = 0; entry != NULL; i++) {
			kdebug(" - entry %u: base %08x%08x len %08x%08x type %u", i, entry->addr_h, entry->addr_l, entry->len_h, entry->len_l, entry->type);
			entry = mb_traverse_mmap(entry);
		}
	}
#endif

    if(mb_info->flags & MULTIBOOT_INFO_MODS && mb_info->mods_count > 0) {
        kdebug("module information @ 0x%08x:", mb_info->mods_addr);
        struct multiboot_mod_list* modules = (struct multiboot_mod_list*) mb_info->mods_addr;
        for(size_t i = 0; i < mb_info->mods_count; i++) {
            kdebug(" - entry %u: 0x%08x - 0x%08x (%u bytes), cmdline %s", i, modules[i].mod_start, modules[i].mod_end - 1, modules[i].mod_end - modules[i].mod_start, (char*)modules[i].cmdline);
            if(!strcmp((char*)modules[i].cmdline, INITRD_PATH)) {
                /* we've found the initrd file - let's load it as VFS root */
                vfs_root = tar_init((void*) modules[i].mod_start, modules[i].mod_end - modules[i].mod_start, NULL);
                memfs_mount(vfs_traverse_path(NULL, "/boot/initrd.tar"), (void*) modules[i].mod_start, modules[i].mod_end - modules[i].mod_start, false);
            }
        }
    } else kwarn("kernel has been loaded without any modules");

    kinfo("initialization for target x86 completed successfully");
    return 0;
}

int ktgt_init() {
    kinfo("initializing MP specification support");
    mp_init();

    kinfo("initializing APIC");
    if(apic_init()) {
        // kinfo("calibrating APIC timer");
        // apic_timer_calibrate();

        // kinfo("enabling APIC timer as new system timer source");
        // pit_systimer_stop();
        // apic_timer_enable();
    }
    // rtc_irq_reset();

    return 0;
}
