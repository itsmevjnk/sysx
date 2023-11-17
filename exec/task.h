#ifndef EXEC_TASK_H
#define EXEC_TASK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* kernel task description structure pointer */
extern void* task_kernel;

/* current task pointer */
extern volatile void* task_current;

/* task yielding (automatic switching) enable/disable */
extern volatile bool task_yield_enable;

/* COMMON TASK DESCRIPTION FIELDS */
typedef struct {
    void* vmm; // VMM configuration pointer
    size_t type; // task type
    size_t ready; // set when the task is ready to be switched to
    size_t delete_pending; // set if the task is to be deleted
    size_t pid; // process ID
    uintptr_t stack_bottom;
    size_t stack_size;
    void* prev; // previous task
    void* next; // next task - our task description structure is a cyclic doubly linked list
} __attribute__((packed)) task_common_t;

/* user field values */
#define TASK_TYPE_KERNEL                    0 // kernel task
#define TASK_TYPE_USER                      1 // user task running user code
#define TASK_TYPE_USER_SYS                  2 // user task running kernel code (e.g. syscall in progress)

/* initial task stack size */
#ifndef TASK_INITIAL_STACK_SIZE
#define TASK_INITIAL_STACK_SIZE             4096
#endif

/*
 * void task_switch(void* task, void* context)
 *  Performs a context switch to the specified task, given the current
 *  task's context when the timer interrupt is triggered.
 *  The current context will be discarded if task_current is NULL;
 *  otherwise, the current context will be saved to task_current, before
 *  it is set to the switching task.
 *  This is an architecture-specific function and is called in ring 0
 *  by the timer interrupt handler.
 */
void task_switch(void* task, void* context);

/*
 * void task_yield(void* context)
 *  Yields to the next task.
 *  This is a common-defined function to be called in ring 0 by the timer
 *  interrupt handler.
 */
void task_yield(void* context);

/*
 * void task_init()
 *  Initializes the kernel task with the entry point at kmain (see
 *  kernel/kernel.c). The kernel task is not active upon creation, and
 *  its state must be set to ready using task_set_ready() by kinit()
 *  before returning.
 *  This is a common-defined function.
 */
void task_init();

/*
 * void* task_create_stub(void* src_task)
 *  Creates a new task, optionally cloning a non-NULL source task.
 *  This is an architecture-specific function and is called in ring 0.
 */
void* task_create_stub(void* src_task);

/*
 * void* task_create(bool user, void* src_task, uintptr_t entry)
 *  Creates a new kernel/user task, then set the task's entry point to 
 *  the specified pointer and allocate stack space for it.
 *  The new task's stack will be located at the end of the user address
 *  space (start of kernel address space), copying from the source task
 *  if it uses the same stack space.
 *  This is a common-defined function to be called in ring 0.
 */
void* task_create(bool user, void* src_task, uintptr_t entry);

/*
 * void task_delete_stub(void* task)
 *  Deallocates the task structure.
 *  This is an architecture-specific function and is called in ring 0.
 */
void task_delete_stub(void* task);

/*
 * void task_delete(void* task)
 *  Removes the specified task.
 *  This is a common-defined function.
 */
void task_delete(void* task);

/*
 * void* task_get_vmm(void* task)
 *  Retrieves the VMM configuration (mapped to the kernel's VMM) of the
 *  specified task.
 *  This is an architecture-specific function and is called in ring 0.
 */
void* task_get_vmm(void* task);

/*
 * uintptr_t task_get_iptr(void* task)
 *  Retrieves the specified task's current instruction pointer.
 *  This is an architecture-specific function and is called in ring 0.
 */
uintptr_t task_get_iptr(void* task);

/*
 * void task_set_iptr(void* task, uintptr_t iptr)
 *  Sets the specified task's instruction pointer.
 *  This is an architecture-specific function and is called in ring 0.
 */
void task_set_iptr(void* task, uintptr_t iptr);

/*
 * uintptr_t task_get_sptr(void* task)
 *  Retrieves the specified task's current stack pointer.
 *  This is an architecture-specific function and is called in ring 0.
 */
uintptr_t task_get_sptr(void* task);

/*
 * void task_set_sptr(void* task, uintptr_t sptr)
 *  Sets the specified task's stack pointer.
 *  This is an architecture-specific function and is called in ring 0.
 */
void task_set_sptr(void* task, uintptr_t sptr);

/*
 * task_common_t* task_common(void* task)
 *  Returns the pointer to the common task description fields struct of
 *  the given task.
 *  This is an architecture-specific function.
 */
task_common_t* task_common(void* task);

/*
 * void task_set_ready(void* task)
 *  Sets or clears the ready state of the specified task.
 */
void task_set_ready(void* task, bool ready);

/*
 * bool task_get_ready(void* task)
 *  Gets the ready state of the specified task.
 */
bool task_get_ready(void* task);

#endif