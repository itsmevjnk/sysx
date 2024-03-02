#include <fs/vfs.h>
#include <kernel/log.h>
#include <string.h>
#include <stdlib.h>
#include <helpers/path.h>

const vfs_node_t* vfs_root = NULL;

vfs_node_t* vfs_traverse_symlink(const vfs_node_t* node) {
    while(node && (node->flags & VFS_SYMLINK)) {
        if(node->link.target == node) {
            kerror("VFS node at 0x%x is a symlink to itself", (uintptr_t) node);
            break;
        }
        node = node->link.target;
    }
    if(!node) kdebug("null VFS node encountered");
    return (vfs_node_t*) node;
}

vfs_node_t* vfs_traverse_path(vfs_node_t* start_node, const char* path) {
    if(!path) {
        kerror("path is NULL");
        return NULL;
    }

    const vfs_node_t* node;
    if(!start_node) {
        if(path[0] != '/') {
            kerror("first character of path is not /, indicating relative path");
            return NULL;
        }
        node = vfs_root; // start from the root node
    } else node = start_node;
    
    char* name = kmalloc(256);
    if(!name) {
        kerror("cannot allocate temporary space for path element");
        return NULL;
    }
    while(node && *path != '\0') {
        size_t len; const char* e = path; // current path element and its length
        path = path_traverse(path, &len);
        if(!len || (len == 1 && *e == '.')) continue; // nothing to do
        // TODO: support going back to the parent
        memcpy(name, e, len); name[len] = '\0'; // copy element and terminate it
        node = vfs_finddir((vfs_node_t*) node, name); // try to find next node
    }
    kfree(name);

    return (vfs_node_t*) node;
}

uint64_t vfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    node = vfs_traverse_symlink(node);
    if(node && (node->flags & VFS_FILE) && node->hook->read) return node->hook->read((void*)node, offset, size, buffer);
    return 0;
}

uint64_t vfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    node = vfs_traverse_symlink(node);
    if(node && (node->flags & VFS_FILE) && node->hook->write) return node->hook->write((void*)node, offset, size, buffer);
    return 0;
}

bool vfs_open(vfs_node_t* node, bool read, bool write) {
    node = vfs_traverse_symlink(node);
    if(node && node->hook->open) return node->hook->open((void*)node, read, write);
    return true;
}

void vfs_close(vfs_node_t* node) {
    node = vfs_traverse_symlink(node);
    if(node && node->hook->close) node->hook->close((void*)node);
}

struct dirent* vfs_readdir(const vfs_node_t* node, uint64_t idx) {
    node = vfs_traverse_symlink(node);
    if(node && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->readdir) return node->hook->readdir((void*)node, idx);
    return NULL;
}

struct vfs_node* vfs_finddir(const vfs_node_t* node, const char* name) {
    node = vfs_traverse_symlink(node);
    if(node && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->finddir) return node->hook->finddir((void*)node, name);
    return NULL;
}

struct vfs_node* vfs_create(const vfs_node_t* node, const char* name, uint16_t mask) {
    node = vfs_traverse_symlink(node);
    if(node && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->create) return node->hook->create((void*)node, name, mask);
    return NULL;
}

struct vfs_node* vfs_mkdir(const vfs_node_t* node, const char* name, uint16_t mask) {
    node = vfs_traverse_symlink(node);
    if(node && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->mkdir) return node->hook->mkdir((void*)node, name, mask);
    return NULL;
}

bool vfs_remove(vfs_node_t* node) {
    node = vfs_traverse_symlink(node);
    if(node && node->hook->remove) return node->hook->remove((void*)node);
    return false;
}

void vfs_ioctl(vfs_node_t* node, size_t request, void* buf) {
    node = vfs_traverse_symlink(node);
    if(node && node->hook->ioctl) return node->hook->ioctl((void*)node, request, buf);
}