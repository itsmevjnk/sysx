#ifndef FS_TARFS_H
#define FS_TARFS_H

#include <stddef.h>
#include <stdint.h>
#include <fs/vfs.h>

/*
 * vfs_node_t* tar_init(void* buffer, size_t size, vfs_node_t* root)
 *  Parses a TAR file at the specified memory location, optionally
 *  with an existing root node, and returns its root node.
 */
vfs_node_t* tar_init(void* buffer, size_t size, vfs_node_t* root);

#endif