#include <fs/vfs.h>
#include <kernel/log.h>
#include <string.h>
#include <stdlib.h>

const vfs_node_t* vfs_root = NULL;

vfs_node_t* vfs_traverse_symlink(vfs_node_t* node) {
    while(node != NULL && (node->flags & VFS_SYMLINK)) {
        if(node->link.target == node) {
            kerror("VFS node at 0x%x is a symlink to itself", (uintptr_t) node);
            break;
        }
        node = node->link.target;
    }
    if(node == NULL) kdebug("null VFS node encountered");
    return node;
}

vfs_node_t* vfs_traverse_path(const char* path) {
    if(path == NULL) {
        kerror("path is NULL");
        return NULL;
    }

    if(path[0] != '/') {
        kerror("first character of path is not /, indicating relative path");
        return NULL;
    }

    vfs_node_t* node = vfs_root; // start from the root node
    char *name = kmalloc(256); // path element
    while(node != NULL) {
        path++; // skip past the preceding slash
        size_t name_len = 0;
        for(; *path != '/' && *path != '\0'; path++, name_len++)
            name[name_len] = *path; // copy file/directory name
        name[name_len] = '\0'; // null terminate name
        node = vfs_finddir(node, name); // try to find next node
        if(*path == '\0') break; // we've reached the end of the path, exit now
    }
    kfree(name);

    return node;
}

uint64_t vfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && (node->flags & VFS_FILE) && node->hook->read != NULL) return node->hook->read((void*)node, offset, size, buffer);
    return 0;
}

uint64_t vfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && (node->flags & VFS_FILE) && node->hook->write != NULL) return node->hook->write((void*)node, offset, size, buffer);
    return 0;
}

bool vfs_open(vfs_node_t* node, bool read, bool write) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && node->hook->open != NULL) return node->hook->open((void*)node, read, write);
    return true;
}

void vfs_close(vfs_node_t* node) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && node->hook->close != NULL) node->hook->close((void*)node);
}

struct dirent* vfs_readdir(vfs_node_t* node, uint64_t idx) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->readdir != NULL) return node->hook->readdir((void*)node, idx);
    return NULL;
}

struct vfs_node* vfs_finddir(vfs_node_t* node, const char* name) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->finddir != NULL) return node->hook->finddir((void*)node, name);
    return NULL;
}

struct vfs_node* vfs_create(vfs_node_t* node, const char* name, uint16_t mask) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->create != NULL) return node->hook->create((void*)node, name, mask);
    return NULL;
}

struct vfs_node* vfs_mkdir(vfs_node_t* node, const char* name, uint16_t mask) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && (node->flags == VFS_DIRECTORY || node->flags == VFS_MOUNTPOINT) && node->hook->mkdir != NULL) return node->hook->mkdir((void*)node, name, mask);
    return NULL;
}

bool vfs_remove(vfs_node_t* node) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && node->hook->remove != NULL) return node->hook->remove((void*)node);
    return false;
}

void vfs_ioctl(vfs_node_t* node, size_t request, void* buf) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && node->hook->ioctl != NULL) return node->hook->ioctl((void*)node, request, buf);
}