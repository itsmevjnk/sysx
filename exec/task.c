#include <exec/task.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/addr.h>
#include <kernel/kernel.h>
#include <kernel/log.h>
#include <string.h>
#include <stdatomic.h>

void* task_kernel = NULL;
volatile void* task_current = NULL;

void task_set_ready(void* task, bool ready) {
    task_common(task)->ready = (ready) ? 1 : 0;
}

bool task_get_ready(void* task) {
    return (task_common(task)->ready);
}

void task_insert(void* task, void* target) {
    task_yield_block();
    task_common_t* common = task_common(task);
    task_common_t* common_tgt = task_common(target);
    common->prev = target;
    common->next = common_tgt->next;
    task_common(common->next)->prev = task;
    common_tgt->next = task;
    task_yield_unblock();
}

void* task_create(bool user, struct proc* proc, size_t stack_sz, uintptr_t entry, uintptr_t stack_bottom) {
    /* allocate memory for new task */
    void* task = task_create_stub(user);
    if(!task) {
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
    size_t stack_frames = 0; // number of allocated stack frames
    size_t framesz = pmm_framesz();
    if(stack_sz % framesz) stack_sz += framesz - stack_sz % framesz; // frame-align stack size
    common->stack_bottom = (stack_bottom) ? stack_bottom : (vmm_first_free(proc->vmm, 0, kernel_start, stack_sz, 0, true) + stack_sz);
    if(!common->stack_bottom) {
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

    common->t_switch = timer_tick; // give the new task an equal chance to be started later

    /* set task type and activate task */
    common->type = (user) ? TASK_TYPE_USER : TASK_TYPE_KERNEL;
    if(entry) common->ready = 1; // start task immediately if there's an instruction pointer ready

    /* insert this task after task_kernel */
    task_insert(task, task_kernel);

    return task;
}

static void task_do_delete(void* task) {
    task_common_t* common = task_common(task);
    
    task_yield_block();

    struct proc* proc = proc_get(common->pid);
    if(proc) {
        /* de-allocate stack */
        size_t framesz = pmm_framesz();
        for(size_t i = 0; i < common->stack_size; i += framesz) {
            uintptr_t vaddr = common->stack_bottom - framesz - i;
            pmm_free(vmm_get_paddr(proc->vmm, vaddr) / framesz);
        }

        /* delete task from process list and count remaining tasks */
        size_t remaining_tasks = 0; // number of remaining tasks
        for(size_t i = 0; i < proc->num_tasks; i++) {
            if(proc->tasks[i]) {
                if(proc->tasks[i] == task) proc->tasks[i] = NULL;
                else remaining_tasks++;
            }
        }

        if(!remaining_tasks) proc_do_delete(proc); // delete the process if it no longer has any tasks
    } else kwarn("task 0x%x (PID %u) is possibly orphaned", task, common->pid);

    /* remove task from queue */
    task_common(common->prev)->next = common->next;
    task_common(common->next)->prev = common->prev;

    task_yield_unblock();

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
    void* task = task_create_stub(false); // kernel task
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

static volatile atomic_size_t task_yield_block_cnt = 0;

volatile timer_tick_t task_yield_tick = 0;

void task_yield(void* context) {
    if(!task_kernel || atomic_load(&task_yield_block_cnt)) return; // cannot switch yet
    vmm_do_cleanup(); // remove any VMM config that have been staged for deletion
    if(!task_current) {
        if(task_get_ready(task_kernel)) {
            task_yield_tick = task_common(task_kernel)->t_switch = timer_tick;
            task_switch(task_kernel, context); // switch into kernel task
        }
    } else {
        void* task_selected = (void*) task_current; // the task we'll switch to next
        timer_tick_t tdelta_max = 0; // maximum duration between now and the selected task's switch-out - we'll select a ready task that has been waiting for longest
        void* task = (void*) task_current;
        while(1) {
            task = task_common(task)->next;
            kassert((uintptr_t)task >= (uintptr_t)&kernel_start);
            if(task == task_current) break;
            task_common_t* common = task_common(task);
            if(!common->ready) continue; // task is not ready - discard this one
            timer_tick_t tdelta = timer_tick - common->t_switch; // figure out how long this task has been suspended for
            if(tdelta > tdelta_max) {
                tdelta_max = tdelta;
                task_selected = task;
            }
        }
        if(task_selected == task_current) return; // no tasks to switch to
        else {
            if(task_common((void*) task_current)->type == TASK_TYPE_DELETE_PENDING) {
                /* current task is waiting to be deleted - let's do it now */
                task_do_delete((void*) task_current);
                task_current = NULL;
            }
            task_yield_tick = task_common(task_selected)->t_switch = timer_tick;
            task_switch(task_selected, context);
        }
    }
}

void task_yield_block() {
    atomic_fetch_add_explicit(&task_yield_block_cnt, 1, memory_order_relaxed);
}

void task_yield_unblock() {
    size_t expected = atomic_load_explicit(&task_yield_block_cnt, memory_order_relaxed);
    while (expected > 0 && !atomic_compare_exchange_weak_explicit(&task_yield_block_cnt, &expected, expected - 1, memory_order_acquire, memory_order_relaxed)) {
        // expected is updated by atomic_compare_exchange_weak
    }
}

void* task_fork_stub(struct proc* proc) {
    /* create blank task */
    task_common_t* common_current = task_common((void*) task_current);
    bool same_proc = (!proc || proc == proc_get(common_current->pid)); // set if we're cloning into the same process
    if(!proc) proc = proc_get(common_current->pid); // current process
    bool user = (common_current->type == TASK_TYPE_USER || common_current->type == TASK_TYPE_USER_SYS);
    void* task = task_create(user, proc, common_current->stack_size - ((user) ? TASK_KERNEL_STACK_SIZE : 0), 0, (same_proc) ? 0 : common_current->stack_bottom);
    if(!task) return NULL;

    task_common_t* common = task_common(task);

    /* set up common parameters */
    common->type = (common_current->type == TASK_TYPE_KERNEL) ? TASK_TYPE_KERNEL : TASK_TYPE_USER_SYS;
    common->pid = proc->pid;
    // stack has been allocated by task_create()
    common->ready = 0; // do not switch into this task as it's still being set up

    /* copy stack */
    uintptr_t stack_top = common->stack_bottom - common->stack_size;
    if(proc->vmm != vmm_current) {
        uintptr_t stack_top_src = stack_top;
        stack_top = vmm_alloc_map(vmm_current, 0, common->stack_size, 0, kernel_start, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
        if(!stack_top) {
            kerror("cannot map new task's stack for copying");
            task_delete(task);
            return NULL;
        }
        size_t framesz = pmm_framesz();
        for(size_t off = 0; off < common->stack_size; off += framesz, stack_top_src += framesz) {
            vmm_set_paddr(vmm_current, stack_top + off, vmm_get_paddr(proc->vmm, stack_top_src));
        }
    }
    // kdebug("copying %u bytes (current = %u) of stack from 0x%x to 0x%x", common->stack_size, common_current->stack_size, common_current->stack_bottom - common_current->stack_size, stack_top);
    memcpy((void*) stack_top, (void*) (common_current->stack_bottom - common_current->stack_size), common->stack_size);
    if(proc->vmm != vmm_current) vmm_unmap(vmm_current, stack_top, common->stack_size); // unmap stack that we just had to map to copy data over

    return task;
}

size_t task_get_pid(void* task) {
    return task_common(task)->pid;
}
