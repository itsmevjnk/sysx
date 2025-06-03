#ifndef EXEC_PROCESS_H
#define EXEC_PROCESS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* process control block */
struct proc {
    void* vmm; // VMM configuration (virtual address space) - null if node is empty
    
    /* radix tree */
    struct proc* parent; // for back-traversal
    struct proc* child0; // left (xxx0) child - assuming that the PID of this node is xxx in binary
    struct proc* child1; // right (xxx1) child

    /* TODO: tasks list */
} __attribute__((packed));

/*
 * void proc_init()
 *  Creates a process (PID 1) for the kernel.
 */
void proc_init();

/*
 * struct proc* proc_create(void* vmm, size_t* pid)
 *  Creates a process given its VMM configuration, and returns the
 *  process control block structure. Additionally, if pid is non-null,
 *  the new process' PID will be written into this pointer.
 */
struct proc* proc_create(void* vmm, size_t* pid);

/*
 * void proc_delete(struct proc* proc)
 *  Deletes a process given its control block.
 */
void proc_delete(struct proc* proc);

/*
 * bool proc_delete_pid(size_t pid)
 *  Deletes a process given its PID.
 */
bool proc_delete_pid(size_t pid);

/*
 * struct proc* proc_get_pcb(size_t pid)
 *  Retrieves the process control block of a given PID.
 */
struct proc* proc_get_pcb(size_t pid);

/*
 * size_t proc_get_pid(struct proc* proc)
 *  Retrieves the PID of a given process control block.
 */
size_t proc_get_pid(struct proc* proc);

#endif