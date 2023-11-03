#include <fs/memfs.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

uint64_t memfs_vfs_read(struct vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if(offset + size > node->length) {
        if(offset >= node->length) return 0; // nothing to be read
        else size = node->length - offset;
    }

    memcpy(buffer, (void*)((uintptr_t)node->link.ptr + (size_t)offset), size);
    return size;
}

uint64_t memfs_vfs_write(struct vfs_node* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    if(offset + size > node->length) {
        if(offset >= node->length) return 0; // nothing to be written
        else size = node->length - offset;
    }

    memcpy((void*)((uintptr_t)node->link.ptr + (size_t)offset), buffer, size);
    return size;
}

const vfs_hook_t memfs_hook_ro = {
    (void*) &memfs_vfs_read,
    NULL, // write
    NULL, // open
    NULL, // close
    NULL, // readdir
    NULL, // finddir
    NULL, // create
    NULL, // mkdir
    NULL, // remove
    NULL, // ioctl
};

const vfs_hook_t memfs_hook_rw = {
    (void*) &memfs_vfs_read,
    (void*) &memfs_vfs_write,
    NULL, // open
    NULL, // close
    NULL, // readdir
    NULL, // finddir
    NULL, // create
    NULL, // mkdir
    NULL, // remove
    NULL, // ioctl
};

vfs_node_t* memfs_mount(vfs_node_t* node, void* ptr, size_t size, bool rw) {
    if(node == NULL) {
        /* create new node */
        node = kcalloc(1, sizeof(vfs_node_t));
        node->flags = VFS_FILE;
        ksprintf(node->name, "memfs_%x", (uintptr_t) ptr);
    }
    else if(node->flags != VFS_FILE) {
#ifndef MEMFS_OVERRIDE_TYPE
        kerror("cannot mount into a node that is not a file - compile with -DMEMFS_OVERRIDE_FLAGS to override this");
        return NULL;
#else
        node->flags = VFS_FILE;
#endif
    }
    node->length = size;
    node->link.ptr = ptr;
    node->hook = (vfs_hook_t*)((rw) ? &memfs_hook_rw : &memfs_hook_ro);
    return node;
}