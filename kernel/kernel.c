#include <kernel/log.h>
#include <kernel/settings.h>

#include <hal/terminal.h>
#include <hal/serial.h>
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

#ifndef KSYM_INITIAL_CNT
#define KSYM_INITIAL_CNT    8
#endif

extern int ktgtinit(); // must be defined somewhere in the target specific code

/* VFS directory listing */
void vfs_dirlist(vfs_node_t* node, size_t tab) {
    char tab_str[10]; // should be enough
    memset(tab_str, '\t', tab);
    tab_str[tab] = 0;

    for(size_t i = 0; ; i++) {
        struct dirent* de = vfs_readdir(node, i);
        if(de == NULL) return;
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

    kinfo("invoking target-specific system initialization routine");
    if(ktgtinit()) {
        kinfo("ktgtinit() failed, committing suicide");
        return; // this should send us into an infinite loop prepared by the bootstrap code
    }

#ifndef TERM_NO_XY
    /* display terminal dimensions */
    size_t term_width, term_height;
    term_get_dimensions(&term_width, &term_height);
    kinfo("terminal size: %u x %u", term_width, term_height);
#endif

    vfs_node_t* devfs_root = vfs_traverse_path(DEVFS_ROOT);
    if(devfs_root == NULL) kerror("cannot find " DEVFS_ROOT " for devfs mounting");
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
    vfs_node_t* ksym_node = vfs_traverse_path(KSYM_PATH);
    if(ksym_node == NULL) kerror("cannot find kernel symbols file");
    else {
        kinfo("loading kernel symbols");
        elf_load(ksym_node, NULL, NULL, NULL, NULL, NULL);
        kinfo("%u symbol(s) have been added to kernel symbol pool", kernel_syms->count);
    }

    /* load kernel modules */
    kinfo("loading kernel modules from " KMOD_PATH);
    vfs_node_t* kmods_node = vfs_traverse_path(KMOD_PATH);
    if(kmods_node == NULL) kerror("cannot find kernel modules directory");
    else for(size_t i = 0; ; i++) {
        struct dirent* ent = vfs_readdir(kmods_node, i);
        if(ent == NULL) break;
        vfs_node_t* mod = vfs_finddir(kmods_node, ent->name);
        if(mod == NULL) {
            kerror("module %s shows up in readdir result but cannot be found using finddir", ent->name);
            continue;
        }
        kinfo("loading %s (ino %llu)", mod->name, mod->inode);
        int32_t (*kmod_init)() = NULL;
        elf_load(mod, vmm_current, NULL, NULL, KMOD_INIT_FUNC, (uintptr_t*) &kmod_init);
        if(kmod_init != NULL) {
            kinfo(" - calling module's " KMOD_INIT_FUNC " function at 0x%x", (uintptr_t)kmod_init);
            int32_t ret = (*kmod_init)();
            kinfo(" - " KMOD_INIT_FUNC " returned %d", ret);
        } else kwarn(" - module does not have an init function");
    }

    while(1);
}
