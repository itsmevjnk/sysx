#include <fs/tarfs.h>
#include <stdlib.h>
#include <string.h>
#include <kernel/log.h>
#include <helpers/path.h>

#define TAR_INFO_MAGIC                  0x52415453 // 'RATS' on big-endian, 'STAR' on little-endian

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char type; // '0'/NUL = file, '1' = hard link, '2' = symlink, '3' = chardev, '4' = blockdev, '5' = directory, '6' = pipe
    char linked_file[100];
    char ustar_sig[6]; // 'ustar' + NUL. 7Zip produces ustar + space instead.
    char ustar_ver[2]; // '00'
    char uname[32];
    char gname[32];
    char dev_maj[8];
    char dev_min[8];
    char name_prefix[155];
} __attribute__((packed)) tar_header_t;

typedef struct {
    vfs_node_t* node;
    uint8_t* data; // ptr to data (if exists)
    vfs_node_t* parent; // NULL for root
} tar_hierarchy_t;

typedef struct {
    uint32_t magic;
    tar_header_t* first_hdr;
    size_t node_count;
    tar_hierarchy_t* hierarchy;
} tar_info_t;

uint64_t tar_vfs_read(struct vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    tar_info_t* info = node->link.ptr;
    kassert(info);
    if(!info->hierarchy[node->inode].data) return 0;
    if(offset >= node->length) return 0;
    if(offset + size > node->length) size = node->length - offset;
    memcpy(buffer, &info->hierarchy[node->inode].data[offset], size);
    return size;
}

struct dirent* tar_vfs_readdir(struct vfs_node* node, uint64_t idx) {
    tar_info_t* info = node->link.ptr;
    kassert(info);
    uint64_t n = 0;
    for(size_t i = node->inode + 1; i < info->node_count; i++) {
        if((uintptr_t) info->hierarchy[i].parent == (uintptr_t) node) {
            if(n == idx) {
                /* found the entry we need */
                struct dirent* result = kmalloc(sizeof(struct dirent));
                if(result) {
                    strcpy(result->name, info->hierarchy[i].node->name);
                    result->ino = info->hierarchy[i].node->inode;
                } else kerror("cannot allocate space for dirent");
                return result;
            } else n++;
        }
    }
    return NULL; // cannot find anything
}

struct vfs_node* tar_vfs_finddir(struct vfs_node* node, const char* name) {
    tar_info_t* info = node->link.ptr;
    kassert(info);
    for(size_t i = node->inode + 1; i < info->node_count; i++) {
        if((uintptr_t) info->hierarchy[i].parent == (uintptr_t) node && !strcmp(info->hierarchy[i].node->name, name))
            return info->hierarchy[i].node;
    }
    return NULL;
}

const vfs_hook_t tar_hooks = {
    (void*) &tar_vfs_read,
    NULL, // write
    NULL, // open
    NULL, // close
    (void*) &tar_vfs_readdir,
    (void*) &tar_vfs_finddir,
    NULL, // create
    NULL, // mkdir
    NULL, // remove
    NULL, // ioctl
};

#ifndef TAR_HIERARCHY_ITEM_INCREMENT
#define TAR_HIERARCHY_ITEM_INCREMENT        4 // number of items in TAR hierarchy struct to be allocated at once
#endif

vfs_node_t* tar_init(void* buffer, size_t size, vfs_node_t* root) {
    tar_info_t* root_info = NULL;

    if(!root) { // caller wants us to allocate our own root node
        root = kmalloc(sizeof(vfs_node_t));
        if(!root) {
            kerror("cannot allocate space for root node");
            goto fail;
        }
        root->name[0] = 0; // no name (yet?)
        root->uid = 0; root->gid = 0;
    }

    root_info = kmalloc(sizeof(tar_info_t));
    if(!root_info) {
        kerror("cannot allocate space for TAR information structure");
        goto fail;
    }

    root_info->hierarchy = kmalloc(TAR_HIERARCHY_ITEM_INCREMENT * sizeof(tar_hierarchy_t));
    if(!root_info->hierarchy) {
        kerror("cannot allocate space for TAR hierarchy structure");
        goto fail;
    }

    root_info->magic = TAR_INFO_MAGIC;
    root_info->first_hdr = buffer;

    root_info->node_count = 1; // first node will be the root itself
    root_info->hierarchy[0].node = root;
    root_info->hierarchy[0].data = NULL;
    root_info->hierarchy[0].parent = NULL;

    root->flags = VFS_MOUNTPOINT;
    root->inode = 0; // inode in this case is the index in the hierarchy struct
    root->hook = (vfs_hook_t*)&tar_hooks;
    root->link.ptr = root_info; // this information will be available on all nodes

    /* traverse the file, adding new items as we go */
    tar_header_t* header = buffer;
    char *name = kmalloc(256); // full path
    while((uintptr_t) header < (uintptr_t) buffer + size) {
        if(!header->name[0] && !header->size[0]) {
            /* probably empty sector - skip this one */
            kwarn("empty header encountered at 0x%x", (uintptr_t) header);
            header = (tar_header_t*) ((uintptr_t) header + 512);
            continue;
        }

        bool is_ustar = (!memcmp(header->ustar_sig, "ustar", 6));

        name[0] = 0; // quick and dirty way to clear string
        if(is_ustar) strcpy(name, header->name_prefix); // copy name prefix
        strcpy(&name[strlen(name)], header->name); // append it with the rest of the path
        // name[strlen(name) + 1] = 0; // this will be needed later
        
        vfs_node_t* node = kmalloc(sizeof(vfs_node_t)); // new node
        if(!node) {
            kerror("cannot allocate space for node #%u", root_info->node_count + 1);
            return root; // stop parsing
        }

        node->inode = root_info->node_count;
        if(!(root_info->node_count % TAR_HIERARCHY_ITEM_INCREMENT)) {
            /* allocate more space for hierarchy */
            root_info->hierarchy = krealloc(root_info->hierarchy, (root_info->node_count + TAR_HIERARCHY_ITEM_INCREMENT) * sizeof(tar_hierarchy_t));
            if(!root_info->hierarchy) {
                kerror("cannot allocate more space for TAR hierarchy structure, stopping parsing");
                kfree(node);
                return root; // stop parsing
            }
        }
        root_info->node_count++;
        root_info->hierarchy[node->inode].node = node;

        /* load information */
        node->uid = strtoul(header->uid, NULL, 8);
        node->gid = strtoul(header->gid, NULL, 8);
        node->length = strtoull(header->size, NULL, 8);
        node->mtime = strtoul(header->mtime, NULL, 8);
        node->hook = (vfs_hook_t*)&tar_hooks;
        node->link.ptr = root_info;
        switch(header->type) {
            case 0: node->flags = VFS_FILE; break;
            case '0': node->flags = VFS_FILE; break;
            case '1': node->flags = VFS_SYMLINK; break; // TODO: add hard link to VFS types?
            case '2': node->flags = VFS_SYMLINK; break;
            case '3': node->flags = VFS_CHARDEV; break;
            case '4': node->flags = VFS_BLOCKDEV; break;
            case '5': node->flags = VFS_DIRECTORY; break;
            case '6': node->flags = VFS_PIPE; break;
            default:
                kwarn("unknown file type 0x%02x, treating as file", header->type);
                node->flags = VFS_FILE;
                break;
        }
        if(node->flags == VFS_FILE && node->length) root_info->hierarchy[node->inode].data = (uint8_t*) ((uintptr_t) header + 512);

        /* traverse path */
        vfs_node_t* parent = root; // parent node
        char* path = name;
        while(1) {
            size_t len = 0; // path element length
            const char* path_element = path;
            path = (char*) path_traverse(path, &len);

            if(*path == '\0') {
                /* end of path */
                memcpy(node->name, path_element, len); node->name[len] = 0; // copy name
                root_info->hierarchy[node->inode].parent = parent;
                break; // done
            } else {
                /* find next parent */
                bool found = false;
                for(size_t i = 1; i < node->inode; i++) { // ignore first (root) node and this node
                    if((uintptr_t) root_info->hierarchy[i].parent == (uintptr_t) parent && !strncmp(root_info->hierarchy[i].node->name, path_element, len) && root_info->hierarchy[i].node->name[len] == '\0') {
                        /* found it */
                        found = true;
                        parent = root_info->hierarchy[i].node;
                        break;
                    }
                }
                if(!found)
                kerror("cannot find child node of %s (inode %llu) with name %s", parent->name, parent->inode, path);
            }
        }

        kdebug("inode %llu (node @ 0x%x): name %s, flags 0x%02x, size %llu, child of inode %u", node->inode, (uintptr_t) node, node->name, node->flags, node->length, parent->inode);

        header = (tar_header_t*) ((uintptr_t) header + 512 + (((size_t) node->length + 511) / 512) * 512); // next header
    }

    kfree(name);

    return root; // all done

fail: // cleanup on failure
    if(root_info) {
        if(root_info->hierarchy) {
            for(size_t i = 1; i < root_info->node_count; i++)
                kfree(root_info->hierarchy[i].node);
        }
        kfree(root_info);
    }
    if(root) kfree(root);
    return NULL;
}
