#include <fs/ftab.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <string.h>

static struct ftab* ftab = NULL; // file table
static size_t ftab_ents = 0; // number of allocated file table entries
static mutex_t ftab_mutex = {0}; // mutex for file table allocation

#ifndef FTAB_ALLOCSZ
#define FTAB_ALLOCSZ                4
#endif

struct ftab* ftab_open(vfs_node_t* node, bool read, bool write, bool excl) {
    size_t idx = 0;
    mutex_acquire(&ftab_mutex);
    for(; idx < ftab_ents; idx++) {
        if(!ftab[idx].node || ftab[idx].node == node) {
            break; // existing or empty entry
        }
    }
    if(idx == ftab_ents) {
        /* allocate more entries */
        struct ftab* new_ftab = krealloc(ftab, (ftab_ents + FTAB_ALLOCSZ) * sizeof(struct ftab));
        if(!new_ftab) {
            kerror("cannot allocate memory for file table");
            mutex_release(&ftab_mutex);
            return NULL;
        }
        ftab = new_ftab;
        memset(&ftab[ftab_ents], 0, FTAB_ALLOCSZ * sizeof(struct ftab));
        ftab_ents += FTAB_ALLOCSZ;
    }
    
    mutex_acquire(&ftab[idx].mutex);
    if(!ftab[idx].node) {
        /* empty entry - file is opened for the first time */
        if(!vfs_open(node, read, write)) {
            kerror("opening VFS node 0x%x (%s) for r=%u,w=%u access failed", node, node->name, (read)?1:0, (write)?1:0);
            return NULL;
        }
        ftab[idx].node = node;
        ftab[idx].read = (read) ? 1 : 0;
        ftab[idx].write = (write) ? 1 : 0;
        ftab[idx].refs = 1;
        // ftab[idx].excl = (excl) ? proc : NULL; // exclusive access
    } else {
        /* non-empty entry - file is already opened */
         // make sure that no one's working on it

        // if(ftab[idx].excl && ftab[idx].excl != proc) {
        //     kerror("attempting to access VFS node 0x%x (%s) exclusively held by another process", node, node->name);
        //     mutex_release(&ftab[idx].mutex);
        //     mutex_release(&ftab_mutex);
        //     return NULL;
        // }
        if(excl && ftab[idx].refs) {
            kerror("attempting to exclusively hold VFS node 0x%x (%s) which is already in use", node, node->name);
            mutex_release(&ftab[idx].mutex);
            mutex_release(&ftab_mutex);
            return NULL;
        }

        read |= ftab[idx].read; write |= ftab[idx].write;
        if(read != ftab[idx].read || write != ftab[idx].write) {
            /* reopen file for our desired access mode */
            if(!vfs_open(node, read, write)) {
                kerror("reopening VFS node 0x%x (%s) for r=%u,w=%u access failed", node, node->name, (read)?1:0, (write)?1:0);
                mutex_release(&ftab[idx].mutex);
                mutex_release(&ftab_mutex);
                return NULL;
            }
            ftab[idx].read = read; ftab[idx].write = write;
        }

        ftab[idx].refs++; // increment process counter
    }
    mutex_release(&ftab[idx].mutex);

    mutex_release(&ftab_mutex);
    return &ftab[idx];
}

void ftab_close(struct ftab* ent) {
    // if(ent->excl && ent->excl != proc) {
    //     kerror("non-accessing process attempting to close exclusively-accessed VFS node 0x%x (%s)", ent->node, ent->node->name);
    //     return;
    // }
    mutex_acquire(&ent->mutex);
    ent->refs--;
    if(!ent->refs) {
        /* no one's opening this file, so we'll close it */
        vfs_close(ent->node);
        memset(ent, 0, sizeof(struct ftab));
    }
    mutex_release(&ent->mutex);
}

uint64_t ftab_read(struct ftab* ent, uint64_t offset, uint64_t size, uint8_t* buf) {
    mutex_acquire(&ent->mutex);
    
    uint64_t ret = 0;
    // if((!ent->excl || ent->excl == proc) && ent->read) ret = vfs_read(ftab->node, offset, size, buf);

    mutex_release(&ent->mutex);
    return ret;
}

uint64_t ftab_write(struct ftab* ent, uint64_t offset, uint64_t size, const uint8_t* buf) {
    mutex_acquire(&ent->mutex);
    
    uint64_t ret = 0;
    // if((!ent->excl || ent->excl == proc) && ent->write) ret = vfs_write(ftab->node, offset, size, buf);

    mutex_release(&ent->mutex);
    return ret;
}