#ifndef EXEC_PROCESS_H
#define EXEC_PROCESS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <exec/mutex.h>

#ifndef PROC_PIDMAX
#define PROC_PIDMAX                 65535 // maximum PID that can be allocated
#endif

/* PROCESS CONTROL STRUCTURE */
typedef struct {
    size_t pid; // process ID
    size_t parent_pid; // the process ID of the process that spawned this
    void* vmm; // process' VMM configuration
    mutex_t mutex; // mutex for process control block data read/write operations
    size_t num_tasks; // number of entries (used + free) for tasks associated with the process
    void** tasks; // list of tasks
} __attribute__((packed)) proc_t;

extern proc_t* proc_kernel; // kernel process

/*
 * proc_t* proc_get(size_t pid)
 *  Retrieves a process, given its PID.
 *  Returns NULL if the PID is invalid.
 */
proc_t* proc_get(size_t pid);

/*
 * proc_t* proc_create(proc_t* parent, void* vmm)
 *  Creates a new process, optionally given the caller's process, optionally
 *  cloning the specified VMM config.
 *  Returns the process on success or NULL on failure.
 */
proc_t* proc_create(proc_t* parent, void* vmm);

/*
 * void proc_delete(proc_t* proc)
 *  Deallocates the specified process from memory, and deletes all of
 *  its associated tasks.
 */
void proc_delete(proc_t* proc);

/*
 * size_t proc_add_task(proc_t* proc, void* task)
 *  Adds the specified task to the process.
 *  Returns the task's index in proc->tasks, or -1 on failure.
 */
size_t proc_add_task(proc_t* proc, void* task);

/*
 * void proc_delete_task(proc_t* proc, void* task)
 *  Deletes the specified task from the process, and stages the task
 *  for deletion by the scheduler.
 */
void proc_delete_task(proc_t* proc, void* task);

/*
 * void proc_init()
 *  Creates the kernel task (via task_init()) and the corresponding
 *  kernel process.
 */
void proc_init();

#endif
