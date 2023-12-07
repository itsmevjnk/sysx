#ifndef HAL_DEVTREE_H
#define HAL_DEVTREE_H

#include <stddef.h>
#include <stdint.h>
#include <helpers/mutex.h>

/* device tree header structure */
enum devtree_node_type {
    DEVTREE_NODE_ROOT, // root node
    DEVTREE_NODE_BUS, // device bus (e.g. PCI or USB)
    DEVTREE_NODE_DEVICE, // device
};

typedef struct devtree {
    size_t size; // total size (including header)
    char name[16]; // node name (e.g. the PCI device's geographic name i.e. bb:dd.f)
    enum devtree_node_type type; // node type
    struct devtree* parent; // parent node
    struct devtree* first_child; // first child node
    struct devtree* next_sibling; // next sibling node
    mutex_t in_use; // mutex for device driver to acquire full access to device
} devtree_t;
extern devtree_t devtree_root;

/*
 * void devtree_add_child(devtree_t* parent, devtree_t* child)
 *  Adds a child node to the specified parent node.
 */
void devtree_add_child(devtree_t* parent, devtree_t* child);

/*
 * void devtree_add_sibling(devtree_t* sib_existing, devtree_t* sib_new)
 *  Adds a new sibling node to the specified existing node in the
 *  device tree.
 */
void devtree_add_sibling(devtree_t* sib_existing, devtree_t* sib_new);

/*
 * devtree_t* devtree_traverse_path(devtree_t* curr_node, const char* path)
 *  Traverses the given device path and returns the resulting node, or
 *  NULL if such node cannot be found.
 *  curr_node specifies the node to start from (as the root node); if this
 *  is not specified, devtree_root will be used.
 */
devtree_t* devtree_traverse_path(devtree_t* curr_node, const char* path);

#endif
