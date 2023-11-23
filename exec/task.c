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
    task_common_t* common = task_common(task);
    task_common_t* common_tgt = task_common(target);
    common->prev = target;
    common->next = common_tgt->next;
    task_common(common->next)->prev = task;
    common_tgt->next = task;
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
        size_t frame = pmm_alloc_free(1);
        if(frame == (size_t)-1) break; // cannot allocate more
        stack_frames++;
        // pmm_alloc(frame);
        vmm_pgmap(common->vmm, frame * framesz, kernel_start - stack_frames * framesz, VMM_FLAGS_PRESENT | VMM_FLAGS_RW | VMM_FLAGS_CACHE | ((user) ? VMM_FLAGS_USER : 0));
        if(copy_stack) {
            /* copy stack */
            vmm_pgmap(vmm_current, vmm_get_paddr(common_src->vmm, common_src->stack_bottom - stack_frames * framesz), (uintptr_t) stack_copy_src, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
            vmm_pgmap(vmm_current, frame * framesz, (uintptr_t) stack_copy_dst, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
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
    task_insert(task, task_kernel);

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
        pmm_free(vmm_get_paddr(common->vmm, vaddr) / framesz);
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

void* task_fork_stub() {
    /* create blank task */
    void* task = task_create_stub(task_current);
    if(task == NULL) return NULL;

    task_common_t* common = task_common(task);
    task_common_t* common_current = task_common(task_current);

    /* set up common parameters */
    common->vmm = vmm_clone(common_current->vmm);
    common->type = (common_current->type == TASK_TYPE_KERNEL) ? TASK_TYPE_KERNEL : TASK_TYPE_USER_SYS;
    common->stack_bottom = common_current->stack_bottom;
    common->stack_size = common_current->stack_size;
    common->ready = 0; // do not switch into this task as it's still being set up

    size_t pgsz = vmm_pgsz();
    void* stack_copy_dst = (void*) vmm_first_free(vmm_current, pgsz, pgsz);
    if(stack_copy_dst == NULL) {
        kerror("cannot find unused virtual address space to map new task's stack page for copying");
        task_delete_stub(task);
        return NULL;
    }
    vmm_pgmap(vmm_current, 0, (uintptr_t) stack_copy_dst, VMM_FLAGS_PRESENT | VMM_FLAGS_RW); // we'll fill in the phys address later
    for(size_t i = 0; i < common->stack_size; i += pgsz) {
        uintptr_t addr = common->stack_bottom - common->stack_size + i;
        size_t frame = pmm_alloc_free(1);
        if(frame == (size_t)-1) {
            kerror("cannot allocate frame for new stack page at 0x%x", addr);
            task_delete_stub(task);
            vmm_pgunmap(vmm_current, (uintptr_t) stack_copy_dst);
            return NULL;
        }
        vmm_set_paddr(common->vmm, addr, frame * pgsz); // change the phys address out

        /* copy stack page - we may copy some of this function's garbage over, but it does not matter since task_fork will discard this stack frame anyway */
        vmm_set_paddr(vmm_current, (uintptr_t) stack_copy_dst, frame * pgsz);
        memcpy(stack_copy_dst, (void*) addr, pgsz);
    }
    vmm_pgunmap(vmm_current, (uintptr_t) stack_copy_dst);

    task_insert(task, task_current);

    return task;
}

size_t task_get_pid(void* task) {
    return task_common(task)->pid;
}

static void** task_pidtab = NULL; // array of PID to task struct mappings
static size_t task_pidtab_len = 0;

#ifndef TASK_PIDTAB_ALLOCSZ
#define TASK_PIDTAB_ALLOCSZ         4 // number of entries to be allocated at once
#endif

size_t task_pid_alloc(void* task) {
    size_t pid = 0;
    for(; pid < task_pidtab_len; pid++) {
        if(task_pidtab[pid] == NULL) break;
    }
    if(pid == task_pidtab_len) {
        if(task_pidtab_len >= (1 << TASK_PID_BITS) || task_pidtab_len > TASK_PIDMAX) {
            kerror("PID limit reached");
            return (size_t)-1;
        }
        void** new_pidtab = krealloc(task_pidtab, (task_pidtab_len + TASK_PIDTAB_ALLOCSZ) * sizeof(void*));
        if(new_pidtab == NULL) {
            kerror("insufficient memory for PID allocation");
            return (size_t)-1;
        }
        task_pidtab = new_pidtab;
        memset(&task_pidtab[pid + 1], 0, (TASK_PIDTAB_ALLOCSZ - 1) * sizeof(void*));
        task_pidtab_len += TASK_PIDTAB_ALLOCSZ;
    }
    task_pidtab[pid] = task;
    return pid;
}

void task_pid_free(size_t pid) {
    if(pid < task_pidtab_len) task_pidtab[pid] = NULL;
}
