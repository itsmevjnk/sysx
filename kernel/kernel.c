#include <kernel/log.h>

#include <hal/terminal.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <stdio.h>

#include <stdlib.h>
#include <mm/kheap.h>
#include <string.h>

#include <fs/vfs.h>

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

    vfs_dirlist(vfs_root, 0);

    while(1);
}
