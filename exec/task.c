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

void* task_create(bool user, void* src_task, uintptr_t entry) {
    /* allocate memory for new task */
    void* task = task_create_stub(src_task);
    if(task == NULL) {
        kerror("task_create_stub() cannot create new task");
        return NULL;
    }
    task_common_t* common = task_common(task);
    task_common_t* common_src = (src_task == NULL) ? NULL : task_common(src_task);

    /* set task type */
    if(src_task == NULL) common->type = (user) ? TASK_TYPE_USER : TASK_TYPE_KERNEL;
    else {
        if(common_src->type != TASK_TYPE_KERNEL && !user) kwarn("attempting to create kernel task from non-kernel source task");
        common->type = (user) ? ((common_src->type == TASK_TYPE_KERNEL) ? TASK_TYPE_USER : common_src->type) : TASK_TYPE_KERNEL; // since the source task may be in the middle of a syscall or something
    }

    /* set VMM configuration */
    if(src_task != NULL) common->vmm = vmm_clone(common_src->vmm);
    else common->vmm = vmm_current;
    
    /* allocate memory for stack */
    size_t rq_stack_size = TASK_INITIAL_STACK_SIZE; // requested stack size
    bool copy_stack = (src_task != NULL && common_src->stack_bottom != 0);
    if(copy_stack) {
        size_t src_stack_size = common_src->stack_size;
        if(rq_stack_size < src_stack_size) rq_stack_size = src_stack_size;
    }
    size_t stack_frames = 0; // number of allocated stack frames
    size_t framesz = pmm_framesz();
    void* stack_copy_src = (void*) vmm_first_free(vmm_current, framesz, 2 * framesz);
    if(copy_stack && stack_copy_src == NULL) {
        kwarn("cannot allocate virtual memory space for copying stack");
        copy_stack = false;
    }
    void* stack_copy_dst = (void*) ((uintptr_t) stack_copy_src + framesz);
    common->stack_bottom = kernel_start;
    for(size_t i = 0; i < (rq_stack_size + framesz - 1) / framesz; i++) {
        size_t frame = pmm_first_free(1);
        if(frame == (size_t)-1) break; // cannot allocate more
        stack_frames++;
        pmm_alloc(frame);
        vmm_pgmap(common->vmm, frame * framesz, kernel_start - stack_frames * framesz, true, user, true, VMM_CACHE_WTHRU, false);
        if(copy_stack) {
            /* copy stack */
            vmm_pgmap(vmm_current, vmm_physaddr(common_src->vmm, common_src->stack_bottom - stack_frames * framesz), (uintptr_t) stack_copy_src, true, false, true, VMM_CACHE_NONE, false);
            vmm_pgmap(vmm_current, frame * framesz, (uintptr_t) stack_copy_dst, true, false, true, VMM_CACHE_NONE, false);
            memcpy(stack_copy_dst, stack_copy_src, framesz);
            vmm_pgunmap(vmm_current, (uintptr_t) stack_copy_src);
            vmm_pgunmap(vmm_current, (uintptr_t) stack_copy_dst);
        }
    }
    if(!stack_frames) {
        kerror("cannot allocate memory for task stack");
        task_delete_stub(task);
        return NULL;
    }
    common->stack_size = stack_frames * framesz;

    /* set instruction and stack pointers */
    task_set_iptr(task, entry);
    task_set_sptr(task, kernel_start - ((copy_stack) ? (common_src->stack_bottom - task_get_sptr(src_task)) : 0));

    /* set task type and activate task */
    common->type = (user) ? TASK_TYPE_USER : TASK_TYPE_KERNEL;
    if(entry != 0) common->ready = 1; // start task immediately if there's an instruction pointer ready

    /* insert this task after task_kernel */
    common->prev = task_kernel;
    common->next = task_common(task_kernel)->next;
    task_common(common->next)->prev = task;
    task_common(task_kernel)->next = task;

    return task;
}

static void task_do_delete(void* task) {
    task_common_t* common = task_common(task);
    
    /* remove task from queue */
    task_common(common->prev)->next = common->next;
    task_common(common->next)->prev = common->prev;

    /* check if VMM configuration is to be deleted */
    bool delete_vmm = true;
    void* chk_task = task;
    do {
        chk_task = task_common(chk_task)->next; // move to next task
        if(task_common(chk_task)->vmm == common->vmm) {
            // something else is using the VMM configuration
            delete_vmm = false;
            break;
        }
    } while(chk_task != task);
    if(delete_vmm) vmm_free(common->vmm); // delete VMM configuration

    /* de-allocate stack */
    size_t framesz = pmm_framesz();
    for(size_t i = 0; i < common->stack_size; i += framesz) {
        uintptr_t vaddr = common->stack_bottom - framesz - i;
        pmm_free(vmm_physaddr(common->vmm, vaddr) / framesz);
        if(!delete_vmm) vmm_pgunmap(common->vmm, vaddr);
    }

    task_delete_stub(task); // finally purge the task
}

void task_delete(void* task) {
    task_common_t* common = task_common(task);
    common->ready = 0;
    if(task_current == task) {
        common->type = TASK_TYPE_DELETE_PENDING;
        while(1); // wait until we switch out of the task - then we'll delete it later
    } else task_do_delete(task); // delete right away
}

void task_init_stub() {
    void* task = task_create_stub(NULL);
    task_common_t* common = task_common(task);
    common->pid = 0; common->type = TASK_TYPE_KERNEL;
    common->prev = task; common->next = task;
    common->vmm = vmm_kernel;
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
    if(task_current == NULL) {
        if(task_get_ready(task_kernel)) {
            task_switch_tick = timer_tick;
            task_switch(task_kernel, context); // switch into kernel task
        }
    } else {
        void* task = task_current;
        do {
            task = task_common(task)->next;
            if(task_common(task)->type == TASK_TYPE_DELETE_PENDING) {
                /* this task is to be deleted - delete it now */
                void* next_task = task_common(task_common(task)->next)->next; // go to the task after this one
                task_do_delete(task);
                task = next_task;
            }
        } while(!task_common(task)->ready && task != task_current); // find a ready task to switch to, while making sure that we don't get stuck in a loop if no tasks are ready
        if(task == task_current) return; // no tasks to switch to
        else {
            if(task_common(task_current)->type == TASK_TYPE_DELETE_PENDING) {
                /* current task is waiting to be deleted - let's do it now */
                task_do_delete(task_current);
                task_current = NULL;
            }
            task_switch_tick = timer_tick;
            task_switch(task, context);
        }
    }
}
