#include <exec/process.h>
#include <exec/task.h>
#include <stdlib.h>
#include <kernel/log.h>
#include <mm/vmm.h>
#include <mm/pmm.h>

proc_t** proc_pidtab = NULL; // array of PID to process struct mappings
size_t proc_pidtab_len = 0;
static mutex_t proc_mutex = {0}; // mutex for PID table-related operations

proc_t* proc_kernel = NULL; // kernel process

#ifndef PROC_PIDTAB_ALLOCSZ
#define PROC_PIDTAB_ALLOCSZ         4 // number of process entries to be allocated at once
#endif

#ifndef PROC_TASK_ALLOCSZ
#define PROC_TASK_ALLOCSZ           4 // number of task entries to be allocated at once
#endif

/* PID ALLOCATION/DEALLOCATION */

static size_t proc_pid_alloc(proc_t* proc) {
    mutex_acquire(&proc_mutex);
    size_t pid = 0;
    for(; pid < proc_pidtab_len; pid++) {
        if(proc_pidtab[pid] == NULL) break;
    }
    if(pid == proc_pidtab_len) {
        if(proc_pidtab_len > PROC_PIDMAX || proc_pidtab_len >= (1 << TASK_PID_BITS)) {
            kerror("PID limit reached");
            mutex_release(&proc_mutex);
            return (size_t)-1;
        }
        void** new_pidtab = krealloc(proc_pidtab, (proc_pidtab_len + PROC_PIDTAB_ALLOCSZ) * sizeof(void*));
        if(new_pidtab == NULL) {
            kerror("insufficient memory for PID allocation");
            mutex_release(&proc_mutex);
            return (size_t)-1;
        }
        proc_pidtab = new_pidtab;
        memset(&proc_pidtab[pid + 1], 0, (PROC_PIDTAB_ALLOCSZ - 1) * sizeof(void*));
        proc_pidtab_len += PROC_PIDTAB_ALLOCSZ;
    }
    proc_pidtab[pid] = proc;
    proc->pid = pid;
    mutex_release(&proc_mutex);
    return pid;
}

static void proc_pid_free(size_t pid) {
    mutex_acquire(&proc_mutex);
    if(pid < proc_pidtab_len) proc_pidtab[pid] = NULL;
    mutex_release(&proc_mutex);
}

proc_t* proc_get(size_t pid) {
    if(pid < proc_pidtab_len) return proc_pidtab[pid];
    else return NULL;
}

proc_t* proc_create(proc_t* parent, void* vmm) {
    proc_t* proc = kcalloc(1, sizeof(proc_t));
    if(proc == NULL) {
        kerror("cannot allocate memory for new process");
        return NULL;
    }
    
    if(vmm != NULL) {
        proc->vmm = vmm_clone(vmm);
        if(proc->vmm == NULL) {
            kerror("cannot clone VMM for new process");
            kfree(proc);
            return NULL;
        }
    }

    if(proc_pid_alloc(proc) == (size_t)-1) {
        kerror("cannot give PID to new process");
        vmm_free(proc->vmm);
        kfree(proc);
        return NULL;
    }

    if(parent != NULL) proc->parent_pid = parent->pid;
    return proc;
}

void proc_delete(proc_t* proc) {
    mutex_acquire(&proc->mutex); // make sure nobody else is holding the process

    /* delete all tasks */
    for(size_t i = 0; i < proc->num_tasks; i++) {
        if(proc->tasks[i] != NULL) task_delete(proc->tasks[i]);
    }

    /* unmap ELF segments (if there's any) */
    if(proc->elf_segments != NULL) {
        size_t pgsz = vmm_pgsz();
        for(size_t i = 0; i < proc->num_elf_segments; i++) {
            for(size_t j = 0; j < proc->elf_segments[i].size; j += pgsz) {
                uintptr_t addr = proc->elf_segments[i].vaddr + j;
                pmm_free(vmm_get_paddr(proc->vmm, addr) / pgsz);
                vmm_pgunmap(proc->vmm, addr);
            }
        }
        kfree(proc->elf_segments);
    }

    vmm_free(proc->vmm); // delete VMM config
    
    proc_pid_free(proc->pid);
    kfree(proc);
}

size_t proc_add_task(proc_t* proc, void* task) {
    mutex_acquire(&proc->mutex);
    size_t i = 0;
    for(; i < proc->num_tasks; i++) {
        if(proc->tasks[i] == task) {
            /* attempting to insert an already existing task */
            mutex_release(&proc->mutex);
            return i;
        }
        if(proc->tasks[i] == NULL) break;
    }
    if(i == proc->num_tasks) {
        void** new_tasks = krealloc(proc->tasks, (proc->num_tasks + PROC_TASK_ALLOCSZ) * sizeof(void*));
        if(new_tasks == NULL) {
            kerror("insufficient memory to add task to process 0x%x", proc);
            mutex_release(&proc->mutex);
            return (size_t)-1;
        }
        proc->tasks = new_tasks;
        proc->num_tasks += PROC_TASK_ALLOCSZ;
        memset(&proc->tasks[i + 1], 0, (PROC_TASK_ALLOCSZ - 1) * sizeof(void*));
    }
    proc->tasks[i] = task;
    task_common(task)->pid = proc->pid;

    mutex_release(&proc->mutex);
    return i;
}

void proc_delete_task(proc_t* proc, void* task) {
    mutex_acquire(&proc->mutex);
    for(size_t i = 0; i < proc->num_tasks; i++) {
        if(proc->tasks[i] == task) {
            proc->tasks[i] = NULL;
            break;
        }
    }
    mutex_release(&proc->mutex);
    task_delete(task);
}

void proc_init() {
    proc_kernel = proc_create(NULL, NULL); // we'll set the VMM later
    kassert(proc_kernel != NULL);
    proc_kernel->vmm = vmm_kernel;
    task_init();
    proc_add_task(proc_kernel, task_kernel);
}
