#include <fs/devfs.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <kernel/log.h>

struct dirent* devfs_vfs_readdir(vfs_node_t* node, uint64_t idx) {
    node = node->link.ptr; // 1st device node
    for(size_t i = 0; i < idx && node; i++) node = node->link.ptr; // traverse to next node
    if(!node) return NULL; // nothing to return
    struct dirent* result = vfs_alloc_dirent();
    strcpy(result->name, node->name);
    result->ino = node->inode;
    return result;
}

struct vfs_node* devfs_vfs_finddir(vfs_node_t* node, const char* name) {
    node = node->link.ptr; // 1st device node
    while(node) {
        if(!strcmp(node->name, name)) break; // we've found the node
        node = node->link.ptr; // traverse to next node
    }
    return node;
}

const vfs_hook_t devfs_hook_root = {
    NULL, // read
    NULL, // write
    NULL, // open
    NULL, // close
    (void*) &devfs_vfs_readdir,
    (void*) &devfs_vfs_finddir,
    NULL, // create
    NULL, // mkdir
    NULL, // remove
    NULL, // ioctl
};

vfs_node_t* devfs_mount(vfs_node_t* root) {
    if(!root) {
        /* create new node */
        root = kcalloc(1, sizeof(vfs_node_t));
        if(!root) return NULL;
        ksprintf(root->name, "devfs_%x", (uintptr_t) root);
    }
#ifndef DEVFS_OVERRIDE_TYPE
    else if(root->flags != VFS_DIRECTORY && root->flags != VFS_MOUNTPOINT) {
        kerror("cannot mount into a node that is not a directory nor an existing mountpoint - compile with -DDEVFS_OVERRIDE_TYPE to override this");
        return NULL;
    }
#endif
    
    root->flags = VFS_MOUNTPOINT;
    root->link.ptr = NULL; // no devices (yet)
    root->hook = (vfs_hook_t*) &devfs_hook_root;

    return root;
}

vfs_node_t* devfs_create(   
                            vfs_node_t* root,
                            uint64_t (*read)(vfs_node_t*, uint64_t, uint64_t, uint8_t*),
                            uint64_t (*write)(vfs_node_t*, uint64_t, uint64_t, const uint8_t*),
                            bool (*open)(vfs_node_t*, bool, bool),
                            void (*close)(vfs_node_t*),
                            void (*ioctl)(vfs_node_t*, size_t, void*),
                            bool block,
                            uint64_t size,
                            const char* name_fmt, ...
                        ) {
    if(!vfs_is_valid(root)) {
        kdebug("specified root node is not a valid devfs node");
        return NULL;
    }


    /* find last node */
    vfs_node_t* last_node = NULL; // null if there are no nodes yet
    vfs_hook_t* hook = NULL; // optimise by trying to find an identical hook
    if(root->link.ptr) {
        last_node = root->link.ptr; // first node
        while(1) {
            if(!hook &&  (uintptr_t) last_node->hook->read == (uintptr_t) read && 
                                (uintptr_t) last_node->hook->write == (uintptr_t) write && 
                                (uintptr_t) last_node->hook->open == (uintptr_t) open && 
                                (uintptr_t) last_node->hook->close == (uintptr_t) close && 
                                (uintptr_t) last_node->hook->ioctl == (uintptr_t) ioctl)
                hook = last_node->hook;
            if(last_node->link.ptr) last_node = last_node->link.ptr; // traverse to the next node
            else break; // we've reached the last node
        }
    }

    vfs_node_t* node = kcalloc(1, sizeof(vfs_node_t));
    if(!node) {
        kdebug("cannot create node");
        return NULL;
    }

    if(!hook) {
        /* new hook required */
        hook = kcalloc(1, sizeof(vfs_hook_t));
        if(!hook) {
            kdebug("cannot create hook struct");
            kfree(node);
            return NULL;
        }

        hook->read = (void*) read;
        hook->write = (void*) write;
        hook->open = (void*) open;
        hook->close = (void*) close;
        hook->ioctl = (void*) ioctl;
    }
    
    va_list arg; va_start(arg, name_fmt);
    kvsprintf(node->name, name_fmt, arg);
    va_end(arg);
    
    node->flags = (block) ? VFS_BLOCKDEV : VFS_CHARDEV;
    node->length = size;
    node->hook = (void*) hook;

    if(!last_node) root->link.ptr = node;
    else {
        node->inode = last_node->inode + 1;
        last_node->link.ptr = node;
    }

    return node;
}

void devfs_remove(vfs_node_t* root, vfs_node_t* node) {
    if(!vfs_is_valid(root)) {
        kdebug("specified root node is not a valid devfs node");
        return;
    }

    /* from this point root will be reused for the node before the node we are removing */
    while(root->link.ptr && root->link.ptr != node) root = root->link.ptr;

    if(root->link.ptr != node) {
        root->link.ptr = node->link.ptr;
        kfree(node->hook); kfree(node);
    } else kdebug("cannot find node");
}

bool vfs_is_valid(vfs_node_t* root) {
    return (root && root->hook == &devfs_hook_root);
}