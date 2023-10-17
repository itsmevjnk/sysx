#include <fs/vfs.h>
#include <kernel/log.h>

const vfs_node_t* vfs_root = NULL;

static vfs_node_t* vfs_traverse_symlink(vfs_node_t* node) {
    while(node != NULL && node->flags & VFS_SYMLINK) {
        if(node->link.target == node) {
            kerror("VFS node at 0x%x is a symlink to itself", (uintptr_t) node);
            break;
        }
        node = node->link.target;
    }
    if(node == NULL) kdebug("null VFS node encountered");
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

bool vfs_open(vfs_node_t* node, bool write) {
    node = vfs_traverse_symlink(node);
    if(node != NULL && node->hook->open != NULL) return node->hook->open((void*)node, write);
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