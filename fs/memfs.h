#ifndef FS_MEMFS_H
#define FS_MEMFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <fs/vfs.h>

/*
 * vfs_node_t* memfs_mount(vfs_node_t* node, void* ptr, size_t size, bool rw)
 *  Attach a memory space to be accessed via the specified node.
 *  Returns the node on success, or NULL otherwise.
 */
vfs_node_t* memfs_mount(vfs_node_t* node, void* ptr, size_t size, bool rw);

#endif