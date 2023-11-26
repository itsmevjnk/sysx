#include <arch/x86cpu/gdt.h>
#include <arch/x86cpu/idt.h>
#include <kernel/log.h>
#include <kernel/cmdline.h>
#include <arch/x86/i8259.h>
#include <arch/x86/i8253.h>
#include <arch/x86/multiboot.h>
#include <string.h>

#include <fs/vfs.h>
#include <fs/tarfs.h>
#include <fs/memfs.h>

#define INITRD_PATH                             "/initrd.tar"

extern uint16_t x86ext_on;

int ktgtinit() {
    kdebug("x86ext_on = 0x%x", x86ext_on);

    kdebug("Multiboot information structure address: 0x%08x", mb_info);

    if(mb_info->flags & MULTIBOOT_INFO_MEMORY) kdebug("memory size: lo %uK, hi %uK", mb_info->mem_lower, mb_info->mem_upper);
    if(mb_info->flags & MULTIBOOT_INFO_CMDLINE) {
        cmdline_init((char*)mb_info->cmdline);
        kdebug("cmdline: %s (argc = %u)", (char*)mb_info->cmdline, kernel_argc);
    }

    kinfo("initializing GDT"); gdt_init();
    kinfo("initializing IDT"); idt_init();
    kinfo("initializing PIC"); pic_init();
    kinfo("initializing PIT as system timer source"); pit_systimer_init();

    if(mb_info->flags & MULTIBOOT_INFO_MODS && mb_info->mods_count > 0) {
        kdebug("module information @ 0x%08x:", mb_info->mods_addr);
        struct multiboot_mod_list* modules = (struct multiboot_mod_list*) mb_info->mods_addr;
        for(size_t i = 0; i < mb_info->mods_count; i++) {
            kdebug(" - entry %u: 0x%08x - 0x%08x (%u bytes), cmdline %s", i, modules[i].mod_start, modules[i].mod_end - 1, modules[i].mod_end - modules[i].mod_start, (char*)modules[i].cmdline);
            if(!strcmp((char*)modules[i].cmdline, INITRD_PATH)) {
                /* we've found the initrd file - let's load it as VFS root */
                vfs_root = tar_init((void*) modules[i].mod_start, modules[i].mod_end - modules[i].mod_start, NULL);
                memfs_mount(vfs_traverse_path("/boot/initrd.tar"), (void*) modules[i].mod_start, modules[i].mod_end - modules[i].mod_start, false);
            }
        }
    } else kwarn("kernel has been loaded without any modules");

    kinfo("initialization for target x86 completed successfully");
    return 0;
}
