#include <kernel/kernel.h>
#include <kernel/log.h>
#include <kernel/settings.h>

#include <hal/terminal.h>
#include <hal/serial.h>
#include <hal/timer.h>
#include <hal/keyboard.h>
#include <hal/fbuf.h>
#include <hal/fonts/font8x16.h>

#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdio.h>

#include <stdlib.h>
#include <mm/kheap.h>
#include <string.h>

#include <fs/vfs.h>
#include <fs/devfs.h>

#include <exec/elf.h>
#include <exec/syms.h>
#include <exec/usermode.h>
#include <exec/task.h>
#include <exec/syscall.h>

#include <drivers/pci.h>
#include <drivers/acpi.h>

int ktgt_preinit(); // must be defined somewhere in the target specific code
int ktgt_init();

/* VFS directory listing */
void vfs_dirlist(const vfs_node_t* node, size_t tab) {
    char tab_str[10]; // should be enough
    memset(tab_str, '\t', tab);
    tab_str[tab] = 0;

    for(size_t i = 0; ; i++) {
        struct dirent* de = vfs_readdir(node, i);
        if(!de) return;
        vfs_node_t* n = vfs_finddir(node, de->name);
        kinfo("ino %llu:%s%s (0x%02x), size %llu", de->ino, tab_str, de->name, n->flags, n->length);
        if((n->flags & ~VFS_SYMLINK) == VFS_DIRECTORY || (n->flags & ~VFS_SYMLINK) == VFS_MOUNTPOINT) vfs_dirlist(n, tab + 1);
    }
}

void kinit() {
#ifdef KINIT_MM_FIRST // initialize MM first
    pmm_init();
    vmm_init();
#endif
    
    fbuf_font = &font8x16;
    term_init();
    stdio_init();

    kinfo("SysX version 0.0.1 prealpha (compiled %s %s)", __DATE__, __TIME__);
    kinfo("Copyright <C> 2023 Thanh Vinh Nguyen (itsmevjnk)");

#ifndef KINIT_MM_FIRST
    kinfo("initializing physical memory management");
    pmm_init();
    kinfo("initializing virtual memory management");
    vmm_init();
#endif

    kinfo("invoking target-specific system pre-initialization routine");
    if(ktgt_preinit()) {
        kerror("ktgt_preinit() failed");
        return; // this should send us into an infinite loop prepared by the bootstrap code
    }

#ifndef TERM_NO_XY
    /* display terminal dimensions */
    size_t term_width, term_height;
    term_get_dimensions(&term_width, &term_height);
    kinfo("terminal size: %u x %u", term_width, term_height);
#endif

    vfs_node_t* devfs_root = vfs_traverse_path(NULL, DEVFS_ROOT);
    if(!devfs_root) kerror("cannot find " DEVFS_ROOT " for devfs mounting");
    else {
        kinfo("mounting devfs at " DEVFS_ROOT);
        devfs_mount(devfs_root);
        devfs_std_init(devfs_root);
#ifndef NO_SERIAL
        ser_devfs_init(devfs_root);
#endif
    }

    vfs_dirlist(vfs_root, 0);

    /* load kernel symbols */
    kinfo("creating kernel symbol table");
    kernel_syms = sym_new_table(KSYM_INITIAL_CNT);
    kinfo("locating kernel symbols file at " KSYM_PATH);
    vfs_node_t* ksym_node = vfs_traverse_path(NULL, KSYM_PATH);
    if(!ksym_node) kerror("cannot find kernel symbols file");
    else {
        kinfo("loading kernel symbols");
        elf_load_ksym(ksym_node);
        kinfo("%u symbol(s) have been added to kernel symbol pool", kernel_syms->count);
    }

    kinfo("seeding PRNG using timer tick"); // for architectures without hardware pseudorandom number generation capabilities and additional entropy for those that do
    srand(timer_tick);

    kinfo("creating kernel process and task");
    proc_init();

    kinfo("initializing syscall");
    syscall_init();

#ifdef FEAT_PCI
    kinfo("initializing PCI");
    pci_init();
#endif

#ifdef FEAT_ACPI
    kinfo("initializing ACPI");
    acpi_init();
#endif

    kinfo("invoking target-specific system initialization routine");
    if(ktgt_init()) {
        kerror("ktgt_init() failed");
        return; // this should send us into an infinite loop prepared by the bootstrap code
    }

    /* load kernel modules */
    kinfo("loading kernel modules from " KMOD_PATH);
    vfs_node_t* kmods_node = vfs_traverse_path(NULL, KMOD_PATH);
    if(!kmods_node) kerror("cannot find kernel modules directory");
    else for(size_t i = 0; ; i++) {
        struct dirent* ent = vfs_readdir(kmods_node, i);
        if(!ent) break;
        vfs_node_t* mod = vfs_finddir(kmods_node, ent->name);
        if(!mod) {
            kerror("module %s shows up in readdir result but cannot be found using finddir", ent->name);
            continue;
        }
        kinfo("loading %s (ino %llu)", mod->name, mod->inode);
        int32_t (*kmod_init)(elf_prgload_t*, size_t) = NULL;
        elf_prgload_t* load_result; size_t load_result_len;
        elf_load_kmod(mod, &load_result, &load_result_len, (uintptr_t*) &kmod_init);
        if(kmod_init) {
            kinfo(" - calling module's " ELF_KMOD_INIT_FUNC " function at 0x%x", (uintptr_t)kmod_init);
            int32_t ret = (*kmod_init)(load_result, load_result_len);
            kinfo(" - " ELF_KMOD_INIT_FUNC " returned %d", ret);
            if(ret < 0) {
                kinfo("   unloading module");
                elf_unload_prg(vmm_kernel, load_result, load_result_len);
            } else kfree(load_result); // save kernel heap and free the loading result (since we'll discard it anyway)
        } else kwarn(" - module does not have an init function");
    }

    kinfo("kernel init finished, current timer tick: %llu", (uint64_t)timer_tick);

    /* create another task for testing */
    // kinfo("creating another task");
    // task_create(false, NULL, (uintptr_t) &kmain); // NOTE: we cannot create an user task since kernel code is not mapped as user-accessible (for security reasons)

    kinfo("starting kernel task");
    task_set_ready(task_kernel, true);
}

void kmain() {
    kinfo("kernel task (kmain), PID %u", task_get_pid((void*) task_current));
    vfs_node_t* bin_node = vfs_traverse_path(NULL, BIN_ROOT);
    if(!bin_node) {
        kerror(BIN_ROOT " not found");
        while(1);
    }

    kprintf("Available binaries: ");
    for(size_t i = 0; ; i++) {
        struct dirent* ent = vfs_readdir(bin_node, i);
        if(!ent) {
            kprintf(".\n");
            break;
        }
        if(i) kprintf(", ");
        kprintf("%s", ent->name);
    }

    while(1) {
        kputchar('>');
        char buf[100]; term_gets(buf);
        if(buf[0] == '\0') continue; // quicker than strlen(buf) == 0
        
        vfs_node_t* file = vfs_finddir(bin_node, buf);
        if(!file) {
            kprintf("Binary %s not found\n\n");
            continue;
        }
        
        // kputs(buf);

        kinfo("creating new process");
        proc_t* proc = proc_create(proc_kernel, vmm_kernel, false);
        if(!proc) {
            kprintf("Cannot create process\n\n");
            continue;
        }

        kinfo("loading ELF executable");
        elf_prgload_t* load_result; size_t load_result_len; uintptr_t entry;
        enum elf_load_result result = elf_load_exec(file, true, proc->vmm, &load_result, &load_result_len, &entry);
        if(result != LOAD_OK) {
            kprintf("Cannot load file (elf_load_exec() returned %d)", result);
            proc_delete(proc);
            continue;
        } else kinfo("loaded %u segment(s), entry point: 0x%x", load_result_len, entry);
        proc->elf_segments = load_result; proc->num_elf_segments = load_result_len;

        kinfo("creating new task");
        void* task = task_create(true, proc, TASK_INITIAL_STACK_SIZE, entry, 0);
        if(!task) {
            kprintf("Cannot create task");
            proc_delete(proc);
            continue;
        }
    }
}
