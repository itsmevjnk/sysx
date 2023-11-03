#ifndef FS_VFS_H
#define FS_VFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* node types */
#define VFS_FILE                    (1 << 0)
#define VFS_DIRECTORY               (1 << 1)
#define VFS_CHARDEV                 (VFS_FILE | VFS_DIRECTORY) // mutually exclusive with file/directory, but still behaves like a file
#define VFS_BLOCKDEV                (VFS_CHARDEV | (1 << 2)) // mutually exlusive with file, directory and chardev, and behaves like a file
#define VFS_PIPE                    (VFS_BLOCKDEV | (1 << 3)) // mutually exclusive with file, directory and devices, and behaves like a file
#define VFS_MOUNTPOINT              (VFS_DIRECTORY | (1 << 4)) // a mountpoint is technically still a directory
#define VFS_SYMLINK                 (1 << 5) // can be ORed with anything

struct dirent {
    char name[256]; // file name
    uint64_t ino; // inode number
};

typedef struct vfs_hook {
    uint64_t (*read)(struct vfs_node*, uint64_t, uint64_t, uint8_t*); // node, offset, size, buffer -> num of bytes read
    uint64_t (*write)(struct vfs_node*, uint64_t, uint64_t, const uint8_t*); // node, offset, size, buffer -> num of bytes written
    bool (*open)(struct vfs_node*, bool); // node, write access enable -> true on success, false on failure
    void (*close)(struct vfs_node*); // node -> void
    struct dirent* (*readdir)(struct vfs_node*, uint64_t); // node, index -> ptr to child node's dirent struct
    struct vfs_node* (*finddir)(struct vfs_node*, const char*); // node, file name -> ptr to file's node
    struct vfs_node* (*create)(struct vfs_node*, const char*, uint16_t); // node, file name, mask -> ptr to new file's node if created successfully
    struct vfs_node* (*mkdir)(struct vfs_node*, const char*, uint16_t); // node, directory name, mask -> ptr to new directory's node if created successfully
    bool (*remove)(struct vfs_node*); // node -> true on success, false on failure
    void (*ioctl)(struct vfs_node*, size_t, void*); // node, request, buffer -> void
} vfs_hook_t;

typedef struct vfs_node {
    char name[256]; // file name
    uint16_t mask; // permissions mask
    uint8_t flags; // node type
    uint32_t uid; // owner user ID
    uint32_t gid; // owner group ID
    uint64_t inode; // inode number
    uint64_t length; // file size in bytes
    size_t impl; // implementation-defined number; not sure what to use this for right now
    vfs_hook_t* hook; // handler functions
    union {
        void* ptr;
        struct vfs_node* target;
    } link; // mountpoint pointer/symlink target node
    uint64_t atime; // timestamp of last access
    uint64_t mtime; // timestamp of last modification
    uint64_t ctime; // creation timestamp
} vfs_node_t;

extern const vfs_node_t* vfs_root; // root node

/*
 * vfs_node_t* vfs_traverse_symlink(vfs_node_t* node)
 *  Traverses a symlink VFS node to the final target node.
 */
vfs_node_t* vfs_traverse_symlink(vfs_node_t* node);

/*
 * vfs_node_t* vfs_traverse_path(const char* path)
 *  Traverses an absolute path and returns the final node if one exists.
 */
vfs_node_t* vfs_traverse_path(const char* path);

/*
 * uint64_t vfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer)
 *  Reads a number of bytes from a node, starting from a specified offset.
 *  Returns the number of read bytes.
 */
uint64_t vfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer);

/*
 * uint64_t vfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer)
 *  Writes a number of bytes from a node, starting from a specified offset.
 *  Returns the number of written bytes.
 */
uint64_t vfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer);

/*
 * bool vfs_open(vfs_node_t* node, bool write)
 *  Opens a node for read or read+write access.
 *  Returns true on success, or false otherwise.
 *  Note that if the open hook is not provided, this will return true.
 *  This is so that simpler file systems can omit the implementation of
 *  this feature if not needed.
 */
bool vfs_open(vfs_node_t* node, bool write);

/*
 * void vfs_close(vfs_node_t* node)
 *  Closes a node, thus ending access to it.
 */
void vfs_close(vfs_node_t* node);

/*
 * struct dirent* vfs_readdir(vfs_node_t* node, uint64_t idx)
 *  Obtains information on a directory node's child item given its index.
 *  Returns a POSIX-compliant dirent struct pointer if such an item
 *  exists.
 */
struct dirent* vfs_readdir(vfs_node_t* node, uint64_t idx);

/*
 * struct dirent* vfs_finddir(vfs_node_t* node, uint64_t idx)
 *  Finds a child node of a directory given its name.
 *  Returns the node on success, or NULL otherwise.
 */
struct vfs_node* vfs_finddir(vfs_node_t* node, const char* name);

/*
 * struct vfs_node* vfs_create(vfs_node_t* node, const char* name, uint16_t mask)
 *  Creates a file node with the specified name and permissions
 *  in a directory node.
 *  Returns the node on success, or NULL otherwise.
 */
struct vfs_node* vfs_create(vfs_node_t* node, const char* name, uint16_t mask);

/*
 * struct vfs_node* vfs_mkdir(vfs_node_t* node, const char* name, uint16_t mask)
 *  Creates a directory node with the specified name and
 *  permissions in a directory node.
 *  Returns the node on success, or NULL otherwise.
 */
struct vfs_node* vfs_mkdir(vfs_node_t* node, const char* name, uint16_t mask);

/*
 * bool vfs_remove(vfs_node_t* node)
 *  Removes (deletes) a node from the storage.
 *  Returns true on success, or false otherwise.
 *  Note that if the open hook is not provided, this will return false.
 *  This is so that simpler file systems can omit the implementation of
 *  this feature if not needed.
 */
bool vfs_remove(vfs_node_t* node);

/*
 * void vfs_ioctl(vfs_node_t* node, size_t request, void* buf)
 *  Performs an IOCTL request to the device mounted to the specified
 *  node.
 */
void vfs_ioctl(vfs_node_t* node, size_t request, void* buf);

#endif