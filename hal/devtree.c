#include <hal/devtree.h>
#include <helpers/path.h>
#include <string.h>

devtree_t devtree_root = {
    sizeof(devtree_t),
    "",
    DEVTREE_NODE_ROOT,
    &devtree_root, // because why not ;)
    NULL,
    NULL, // must be NULL for root!
    {1} // just to be safe here
};

void devtree_add_child(devtree_t* parent, devtree_t* child) {
    child->next_sibling = parent->first_child; // make whichever node's the first child of the parent the next sibling of this new child node
    parent->first_child = child; // make this child the first child node
    child->parent = parent;
}

void devtree_add_sibling(devtree_t* sib_existing, devtree_t* sib_new) {
    sib_new->next_sibling = sib_existing->next_sibling;
    sib_existing->next_sibling = sib_new;
    sib_new->parent = sib_existing->parent;
}

devtree_t* devtree_traverse_path(devtree_t* curr_node, const char* path) {
    devtree_t* ret = (curr_node) ? curr_node : &devtree_root;
    while(*path != '\0') {
        const char* e = path; // current element
        size_t len = 0; // e's length
        path = path_traverse(path, &len);
        if(!len || (len == 1 && *e == '.')) continue; // nothing to parse here
        else if(!strncmp(e, "..", len)) ret = ret->parent; // back to parent
        else {
            ret = ret->first_child; // enter child node
            if(!ret) return NULL; // nothing to look for here
            while(1) {
                if(!ret) return NULL; // we've reached the end, and we couldn't find the node
                if(!strncmp(ret->name, e, len)) break; // we've found the next node
                ret = ret->next_sibling;
            }
        }
    }
    return ret;
}
