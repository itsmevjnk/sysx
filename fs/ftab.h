#ifndef FS_FTAB_H
#define FS_FTAB_H

#include <stddef.h>
#include <stdint.h>
#include <fs/vfs.h>
#include <helpers/mutex.h>
// #include <exec/process.h>

// struct proc;

/* file table entry */
#if UINTPTR_MAX == UINT64_MAX
#define FTAB_REFS_BITS                          62
#else
#define FTAB_REFS_BITS                          30
#endif
struct ftab {
    vfs_node_t* node; // the file's VFS node, or NULL if the entry is not in use
    size_t read : 1; // set if the file has been opened for read access
    size_t write : 1; // set if the file has been opened for write access
    size_t refs : FTAB_REFS_BITS; // number of processes opening the file
    // struct proc* excl; // the process that is exclusively holding the file, or NULL if the file is not being held exclusively
    mutex_t mutex; // mutex for FS operations on the file table entry
};
typedef struct ftab ftab_t;

/*
 * struct ftab* ftab_open(vfs_node_t* node, struct proc* proc, bool read, bool write, bool excl)
 *  Opens (or reopens) the specified node non-exclusively or exclusively.
 *  Returns NULL on failure, or the file table entry otherwise.
 */
struct ftab* ftab_open(vfs_node_t* node, bool read, bool write, bool excl);

/*
 * void ftab_close(struct ftab* ent, struct proc* proc)
 *  Closes the specified node.
 */
void ftab_close(struct ftab* ent);

/*
 * uint64_t ftab_read(struct ftab* ent, struct proc* proc, uint64_t offset, uint64_t size, uint8_t* buf)
 *  Reads the specified file and returns the number of bytes read.
 */
uint64_t ftab_read(struct ftab* ent, uint64_t offset, uint64_t size, uint8_t* buf);

/*
 * uint64_t ftab_write(struct ftab* ent, struct proc* proc, uint64_t offset, uint64_t size, const uint8_t* buf)
 *  Reads the specified file and returns the number of bytes written.
 */
uint64_t ftab_write(struct ftab* ent, uint64_t offset, uint64_t size, const uint8_t* buf);

#endif