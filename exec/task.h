#ifndef EXEC_TASK_H
#define EXEC_TASK_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <hal/timer.h>
#include <exec/process.h>

/* kernel task description structure pointer */
extern void* task_kernel;

/* current task pointer */
extern volatile void* task_current;

/* task switch timestamp */
extern volatile timer_tick_t task_switch_tick;

/* COMMON TASK DESCRIPTION FIELDS */
#if UINTPTR_MAX == UINT64_MAX
#define TASK_PID_BITS               60 // number of bits reserved for PID field in task_common_t
#else
#define TASK_PID_BITS               28
#endif

typedef struct {
    size_t type : 3; // task type
    size_t ready : 1; // set when the task is ready to be switched to
    size_t pid : TASK_PID_BITS; // task's process ID
    uintptr_t stack_bottom;
    size_t stack_size;
    void* prev; // previous task
    void* next; // next task - our task description structure is a cyclic doubly linked list
} __attribute__((packed)) task_common_t;

/* user field values */
#define TASK_TYPE_KERNEL                    0 // kernel task
#define TASK_TYPE_USER                      1 // user task running user code
#define TASK_TYPE_USER_SYS                  2 // user task running kernel code (e.g. syscall in progress)
#define TASK_TYPE_DELETE_PENDING            3 // pending deletion

/* task kernel stack size (only allocated for user tasks) */
#ifndef TASK_KERNEL_STACK_SIZE
#define TASK_KERNEL_STACK_SIZE              2048
#endif

/* initial task stack size */
#ifndef TASK_INITIAL_STACK_SIZE
#define TASK_INITIAL_STACK_SIZE             4096
#endif

/* task quantum (minimum number of ticks between yield calls) */
#ifndef TASK_QUANTUM
#define TASK_QUANTUM                        1000
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
 * void task_yield_block()
 *  Blocks task yielding.
 *  This function increments an internal block counter (similar to a semaphore).
 */
void task_yield_block();

/*
 * void task_yield_unblock()
 *  Unblocks task yielding.
 *  This function decrements the aforementioned internal counter, whose value
 *  of zero indicates the enabling of task yielding.
 */
void task_yield_unblock();

/*
 * void task_init_stub()
 *  Initializes the kernel task with the entry point at kmain (see
 *  kernel/kernel.c). The kernel task is not active upon creation, and
 *  its state must be set to ready using task_set_ready() by kinit()
 *  before returning.
 *  This is a common-defined function.
 */
void task_init_stub();

/*
 * void task_init()
 *  Initializes multitasking facilities specific to the target
 *  architecture, before calling task_init_stub().
 *  This is an architecture-specific function and is called in ring 0.
 */
void task_init();

/*
 * void* task_create_stub()
 *  Allocates space for a new task.
 *  This is an architecture-specific function and is called in ring 0.
 */
void* task_create_stub();

/*
 * void* task_create(bool user, struct proc* proc, size_t stack_sz, uintptr_t entry, uintptr_t stack_bottom)
 *  Creates a new kernel/user task belonging to the specified process,
 *  then set the task's entry point to the specified pointer and allocate
 *  stack space for it.
 *  If stack_bottom is 0, new task's stack will be located at the end of
 *  the user address space (start of kernel address space); otherwise, 
 *  stack_bottom specifies the new task's stack bottom address.
 *  This is a common-defined function to be called in ring 0.
 */
void* task_create(bool user, struct proc* proc, size_t stack_sz, uintptr_t entry, uintptr_t stack_bottom);

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
 * task_common_t* task_common(volatile void* task)
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

/*
 * void task_yield_noirq()
 *  Yields to the next task on demand (i.e. without IRQs).
 *  This is an architecture-specific function.
 */
void task_yield_noirq();

/*
 * void task_insert(void* task, void* target)
 *  Inserts the specified task after the target task in the
 *  task queue.
 */
void task_insert(void* task, void* target);

/*
 * void* task_fork_stub(struct proc* proc)
 *  Creates a new child (forked) task of the current task and copies
 *  the current task's stack to it. The newly-created task will not
 *  have a context yet; this is to be saved by task_fork.
 *  The proc parameter specifies the process to associate the new task
 *  with; if it is NULL, the current process will be used.
 */
void* task_fork_stub(struct proc* proc);

/*
 * void* task_fork(struct proc* proc)
 *  Forks the current task and returns the child task.
 *  The proc parameter specifies the process to associate the new task
 *  with; if it is NULL, the current process will be used.
 *  This is an architecture-specific function, preferably implemented
 *  in Assembly, due to differences in ABIs and the fact that GCC
 *  does not guarantee the preservation of certain registers (such as
 *  EBX on x86) during execution.
 */
void* task_fork(struct proc* proc);

/*
 * size_t task_get_pid(void* task)
 *  Retrieves the specified task's process ID (PID).
 */
size_t task_get_pid(void* task);

#endif