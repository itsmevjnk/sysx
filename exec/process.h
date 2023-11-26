#ifndef EXEC_PROCESS_H
#define EXEC_PROCESS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <exec/mutex.h>
#include <exec/elf.h>
#include <fs/ftab.h>

#ifndef PROC_PIDMAX
#define PROC_PIDMAX                 65535 // maximum PID that can be allocated
#endif

struct ftab;

/* file descriptor entry */
typedef struct {
    struct ftab* ftab; // corresponding file table entry
    size_t read : 1; // set if the file has been opened by the process for read access
    size_t write : 1; // set if the file has been opened by the process for write access
    size_t append : 1; // set if the file has been opened with O_APPEND (i.e. seek to the end before each write)
    uint64_t offset; // file offset
    mutex_t mutex; // mutex for the entry
} fd_t;

/* PROCESS CONTROL STRUCTURE */
struct proc {
    size_t pid; // process ID
    size_t parent_pid; // the process ID of the process that spawned this
    void* vmm; // process' VMM configuration

    size_t num_elf_segments; // number of program segments (from loading ELF file)
    elf_prgload_t* elf_segments; // list of ELF program segments

    mutex_t mu_tasks; // mutex for adding/deleting tasks
    size_t num_tasks; // number of entries (used + free) for tasks associated with the process
    void** tasks; // list of tasks

    mutex_t mu_fds; // mutex for adding/deleting file descriptors
    size_t num_fds; // number of entries (used + free) for file descriptors
    fd_t* fds; // file descriptor table (maps to file table)
} __attribute__((packed));
typedef struct proc proc_t;

extern struct proc* proc_kernel; // kernel process

/*
 * struct proc* proc_get(size_t pid)
 *  Retrieves a process, given its PID.
 *  Returns NULL if the PID is invalid.
 */
struct proc* proc_get(size_t pid);

/*
 * struct proc* proc_create(struct proc* parent, void* vmm)
 *  Creates a new process, optionally given the caller's process, optionally
 *  cloning the specified VMM config.
 *  Returns the process on success or NULL on failure.
 */
struct proc* proc_create(struct proc* parent, void* vmm);

/*
 * void proc_delete(struct proc* proc)
 *  Deallocates the specified process from memory, and deletes all of
 *  its associated tasks.
 */
void proc_delete(struct proc* proc);

/*
 * size_t proc_add_task(struct proc* proc, void* task)
 *  Adds the specified task to the process.
 *  Returns the task's index in proc->tasks, or -1 on failure.
 */
size_t proc_add_task(struct proc* proc, void* task);

/*
 * void proc_delete_task(struct proc* proc, void* task)
 *  Deletes the specified task from the process, and stages the task
 *  for deletion by the scheduler.
 */
void proc_delete_task(struct proc* proc, void* task);

/*
 * void proc_init()
 *  Creates the kernel task (via task_init()) and the corresponding
 *  kernel process.
 */
void proc_init();

/*
 * size_t proc_fd_open(struct proc* proc, vfs_node_t* node, bool read, bool write, bool append, bool excl)
 *  Opens (or reopens) a file for the specified process, either allowing or disallowing
 *  duplicate file descriptors.
 *  Returns the file descriptor number for the file on success, or -1 on failure.
 */
size_t proc_fd_open(struct proc* proc, vfs_node_t* node, bool duplicate, bool read, bool write, bool append, bool excl);

/*
 * void proc_fd_close(struct proc* proc, size_t fd)
 *  Closes the specified file.
 */
void proc_fd_close(struct proc* proc, size_t fd);

/*
 * uint64_t proc_fd_read(struct proc* proc, size_t fd, uint64_t size, uint8_t* buf)
 *  Reads from the specified file and returns the number of bytes read.
 */
uint64_t proc_fd_read(struct proc* proc, size_t fd, uint64_t size, uint8_t* buf);

/*
 * uint64_t proc_fd_write(struct proc* proc, size_t fd, uint64_t size, const uint8_t* buf)
 *  Writes to the specified file and returns the number of bytes written.
 */
uint64_t proc_fd_write(struct proc* proc, size_t fd, uint64_t size, const uint8_t* buf);

#endif
