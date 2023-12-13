#include <exec/task.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/addr.h>
#include <kernel/kernel.h>
#include <kernel/log.h>
#include <string.h>

void* task_kernel = NULL;
volatile void* task_current = NULL;

void task_set_ready(void* task, bool ready) {
    task_common(task)->ready = (ready) ? 1 : 0;
}

bool task_get_ready(void* task) {
    return (task_common(task)->ready != 0);
}

void task_insert(void* task, void* target) {
    task_yield_enable = false;
    task_common_t* common = task_common(task);
    task_common_t* common_tgt = task_common(target);
    common->prev = target;
    common->next = common_tgt->next;
    task_common(common->next)->prev = task;
    common_tgt->next = task;
    task_yield_enable = true;
}

void* task_create(bool user, struct proc* proc, size_t stack_sz, uintptr_t entry) {
    /* allocate memory for new task */
    void* task = task_create_stub();
    if(task == NULL) {
        kerror("task_create_stub() cannot create new task");
        return NULL;
    }
    size_t task_idx = proc_add_task(proc, task);
    if(task_idx == (size_t)-1) {
        kerror("proc_add_task() cannot add task to process");
        task_delete_stub(task);
        return NULL;
    }

    task_common_t* common = task_common(task);
    
    /* allocate memory for stack */
    if(user) stack_sz += TASK_KERNEL_STACK_SIZE; // allocate memory for kernel stack too
    size_t stack_frames = 0; // number of allocated stack frames
    size_t framesz = pmm_framesz();
    if(stack_sz % framesz) stack_sz += framesz - stack_sz % framesz; // frame-align stack size
    common->stack_bottom = vmm_first_free(proc->vmm, 0, kernel_start, stack_sz, 0, true) + stack_sz;
    if(common->stack_bottom == 0) {
        kerror("cannot allocate virtual address space for task");
        task_delete_stub(task);
        return NULL;
    }
    for(size_t i = 0; i < (stack_sz + framesz - 1) / framesz; i++) {
        size_t frame = pmm_alloc_free(1);
        if(frame == (size_t)-1) break; // cannot allocate more
        stack_frames++;
        // pmm_alloc(frame);
        vmm_pgmap(proc->vmm, frame * framesz, common->stack_bottom - stack_frames * framesz, 0, VMM_FLAGS_PRESENT | VMM_FLAGS_RW | VMM_FLAGS_CACHE | ((user) ? VMM_FLAGS_USER : 0));
    }
    if(stack_frames * framesz <= ((user) ? TASK_KERNEL_STACK_SIZE : 0)) {
        kerror("cannot allocate memory for task stack");
        task_delete_stub(task);
        return NULL;
    }
    common->stack_size = stack_frames * framesz;

    /* set instruction and stack pointers */
    task_set_iptr(task, entry);
    task_set_sptr(task, common->stack_bottom - ((user) ? TASK_KERNEL_STACK_SIZE : 0));

    /* set task type and activate task */
    common->type = (user) ? TASK_TYPE_USER : TASK_TYPE_KERNEL;
    if(entry != 0) common->ready = 1; // start task immediately if there's an instruction pointer ready

    /* insert this task after task_kernel */
    task_insert(task, task_kernel);

    return task;
}

static void task_do_delete(void* task) {
    task_common_t* common = task_common(task);
    
    task_yield_enable = false;

    struct proc* proc = proc_get(common->pid);
    if(proc != NULL) {
        /* de-allocate stack */
        size_t framesz = pmm_framesz();
        for(size_t i = 0; i < common->stack_size; i += framesz) {
            uintptr_t vaddr = common->stack_bottom - framesz - i;
            pmm_free(vmm_get_paddr(proc->vmm, vaddr) / framesz);
        }

        /* delete task from process list and count remaining tasks */
        size_t remaining_tasks = 0; // number of remaining tasks
        for(size_t i = 0; i < proc->num_tasks; i++) {
            if(proc->tasks[i] != NULL) {
                if(proc->tasks[i] == task) proc->tasks[i] = NULL;
                else remaining_tasks++;
            }
        }

        if(!remaining_tasks) proc_do_delete(proc); // delete the process if it no longer has any tasks
    } else kwarn("task 0x%x (PID %u) is possibly orphaned", task, common->pid);

    /* remove task from queue */
    task_common(common->prev)->next = common->next;
    task_common(common->next)->prev = common->prev;

    task_yield_enable = true;

    task_delete_stub(task); // finally purge the task
}

void task_delete(void* task) {
    task_common_t* common = task_common(task);
    // common->ready = 0;
    if(task_current == task) {
        common->type = TASK_TYPE_DELETE_PENDING;
        // while(1); // wait until we switch out of the task - then we'll delete it later
    } else task_do_delete(task); // delete right away
}

void task_init_stub() {
    void* task = task_create_stub(NULL);
    task_common_t* common = task_common(task);
    common->pid = 0; common->type = TASK_TYPE_KERNEL;
    common->prev = task; common->next = task;
    common->stack_bottom = kernel_stack_bottom;
    common->stack_size = kernel_stack_bottom - kernel_stack_top;
    task_set_iptr(task, (uintptr_t) &kmain);
    task_set_sptr(task, kernel_stack_bottom);
    common->ready = 0;
    task_kernel = task;
    // common->ready = 1;
}

volatile bool task_yield_enable = true;

volatile timer_tick_t task_switch_tick = 0;

void task_yield(void* context) {
    if(!task_yield_enable || task_kernel == NULL) return; // cannot switch yet
    vmm_do_cleanup(); // remove any VMM config that have been staged for deletion
    if(task_current == NULL) {
        if(task_get_ready(task_kernel)) {
            task_switch_tick = timer_tick;
            task_switch(task_kernel, context); // switch into kernel task
        }
    } else {
        void* task = (void*) task_current;
        do {
            task = task_common(task)->next;
            if(task_common(task)->type == TASK_TYPE_DELETE_PENDING) {
                /* this task is to be deleted - delete it now */
                void* next_task = task_common((void*) task_common(task)->next)->next; // go to the task after this one
                task_do_delete(task);
                task = next_task;
            }
        } while(!task_common(task)->ready && task != task_current); // find a ready task to switch to, while making sure that we don't get stuck in a loop if no tasks are ready
        if(task == task_current) return; // no tasks to switch to
        else {
            if(task_common((void*) task_current)->type == TASK_TYPE_DELETE_PENDING) {
                /* current task is waiting to be deleted - let's do it now */
                task_do_delete((void*) task_current);
                task_current = NULL;
            }
            task_switch_tick = timer_tick;
            task_switch(task, context);
        }
    }
}

void* task_fork_stub() {
    /* create blank task */
    task_common_t* common_current = task_common((void*) task_current);
    struct proc* proc_current = proc_get(common_current->pid);
    void* task = task_create((common_current->type == TASK_TYPE_USER || common_current->type == TASK_TYPE_USER_SYS), proc_current, common_current->stack_size, 0);
    if(task == NULL) return NULL;

    task_common_t* common = task_common(task);

    /* set up common parameters */
    common->type = (common_current->type == TASK_TYPE_KERNEL) ? TASK_TYPE_KERNEL : TASK_TYPE_USER_SYS;
    common->pid = common_current->pid;
    // stack has been allocated by task_create()
    common->ready = 0; // do not switch into this task as it's still being set up

    // size_t pgsz = vmm_pgsz();
    // void* stack_copy_dst = (void*) vmm_first_free(vmm_current, pgsz, kernel_start, pgsz, false);
    // if(stack_copy_dst == NULL) {
    //     kerror("cannot find unused virtual address space to map new task's stack page for copying");
    //     task_delete_stub(task);
    //     return NULL;
    // }
    // vmm_pgmap(vmm_current, 0, (uintptr_t) stack_copy_dst, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // we'll fill in the phys address later
    // for(size_t i = 0; i < common->stack_size; i += pgsz) {
    //     /* copy stack page - we may copy some of this function's garbage over, but it does not matter since task_fork will discard this stack frame anyway */
    //     vmm_set_paddr(vmm_current, (uintptr_t) stack_copy_dst, vmm_get_paddr(vmm_current, common->stack_bottom - common->stack_size + i));
    //     memcpy(stack_copy_dst, (void*) (common_current->stack_bottom - common_current->stack_size + i), pgsz);
    // }
    // vmm_pgunmap(vmm_current, (uintptr_t) stack_copy_dst);
    memcpy((void*) (common->stack_bottom - common->stack_size), (void*) (common_current->stack_bottom - common_current->stack_size), common->stack_size);

    return task;
}

size_t task_get_pid(void* task) {
    return task_common(task)->pid;
}
