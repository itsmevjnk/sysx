#ifndef FS_DEVFS_H
#define FS_DEVFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include <fs/vfs.h>

/*
 * vfs_node_t* devfs_mount(vfs_node_t* root)
 *  Specifies a node to be used as the devfs root, or create a new one
 *  to be used for this purpose.
 *  Returns the new root node, or NULL if one cannot be created/used.
 */
vfs_node_t* devfs_mount(vfs_node_t* root);

/*
 * vfs_node_t* devfs_create(   
 *                             vfs_node_t* root,
 *                             uint64_t (*read)(vfs_node_t*, uint64_t, uint64_t, uint8_t*),
 *                             uint64_t (*write)(vfs_node_t*, uint64_t, uint64_t, const uint8_t*),
 *                             bool (*open)(vfs_node_t*, bool),
 *                             void (*close)(vfs_node_t*),
 *                             void (*ioctl)(vfs_node_t*, size_t, void*),
 *                             bool block,
 *                             uint64_t size,
 *                             const char* name_fmt, ...
 *                         )
 *  Creates a device with the specified access functions and a sprintf-like
 *  name format in the specified devfs root node.
 *  Returns the device's node, or NULL if one cannot be created.
 */
vfs_node_t* devfs_create(   
                            vfs_node_t* root,
                            uint64_t (*read)(vfs_node_t*, uint64_t, uint64_t, uint8_t*),
                            uint64_t (*write)(vfs_node_t*, uint64_t, uint64_t, const uint8_t*),
                            bool (*open)(vfs_node_t*, bool),
                            void (*close)(vfs_node_t*),
                            void (*ioctl)(vfs_node_t*, size_t, void*),
                            bool block,
                            uint64_t size,
                            const char* name_fmt, ...
                        );

/*
 * void devfs_remove(vfs_node_t* root, vfs_node_t* node)
 *  Removes a device from the specified devfs root node.
 */
void devfs_remove(vfs_node_t* root, vfs_node_t* node);

/*
 * bool vfs_is_valid(vfs_node_t* root)
 *  Returns whether the given node is a valid devfs root node.
 */
bool vfs_is_valid(vfs_node_t* root);

/*
 * void devfs_std_init(vfs_node_t* root)
 *  Attempts to populate the specified devfs root with standard
 *  devices (/dev/null, /dev/console and /dev/zero).
 *  This is defined in devfs_std.c.
 */
void devfs_std_init(vfs_node_t* root);

/* standard device nodes */
extern vfs_node_t* dev_console;
extern vfs_node_t* dev_zero;
extern vfs_node_t* dev_null;
extern vfs_node_t* dev_random;
extern vfs_node_t* dev_urandom;

#endif