#include <exec/process.h>
#include <exec/task.h>
#include <stdlib.h>
#include <kernel/log.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <fs/devfs.h>
#include <string.h>

struct proc** proc_pidtab = NULL; // array of PID to process struct mappings
size_t proc_pidtab_len = 0;
static mutex_t proc_mutex = {0}; // mutex for PID table-related operations

struct proc* proc_kernel = NULL; // kernel process

#ifndef PROC_PIDTAB_ALLOCSZ
#define PROC_PIDTAB_ALLOCSZ         4 // number of process entries to be allocated at once
#endif

#ifndef PROC_TASK_ALLOCSZ
#define PROC_TASK_ALLOCSZ           4 // number of task entries to be allocated at once
#endif

#ifndef PROC_FDS_ALLOCSZ
#define PROC_FDS_ALLOCSZ            4 // number of file descriptor table entries to be allocated at once
#endif

/* PID ALLOCATION/DEALLOCATION */

static size_t proc_pid_alloc(struct proc* proc) {
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
        proc_t** new_pidtab = krealloc(proc_pidtab, (proc_pidtab_len + PROC_PIDTAB_ALLOCSZ) * sizeof(void*));
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

struct proc* proc_get(size_t pid) {
    if(pid < proc_pidtab_len) return proc_pidtab[pid];
    else return NULL;
}

struct proc* proc_create(struct proc* parent, void* vmm) {
    struct proc* proc = kcalloc(1, sizeof(struct proc));
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

    /* open stdin, stdout and stderr */
    size_t fd = proc_fd_open(proc, dev_console, true, true, false, false, false);
    if(fd != 0) kerror("cannot open stdin (proc_fd_open() returned unexpected value %u)", fd);
    fd = proc_fd_open(proc, dev_console, true, false, true, false, false);
    if(fd != 1) kerror("cannot open stdout (proc_fd_open() returned unexpected value %u)", fd);
    fd = proc_fd_open(proc, dev_console, true, false, true, false, false);
    if(fd != 2) kerror("cannot open stderr (proc_fd_open() returned unexpected value %u)", fd);

    if(parent != NULL) proc->parent_pid = parent->pid;
    return proc;
}

void proc_delete(struct proc* proc) {
    mutex_acquire(&proc->mu_tasks); mutex_acquire(&proc->mu_fds); // make sure nobody else is holding the process

    /* delete all tasks */
    bool deleting_current = false; // set if deleting current task
    for(size_t i = 0; i < proc->num_tasks; i++) {
        if(proc->tasks[i] != NULL) {
            if(proc->tasks[i] == task_current) deleting_current = true; // save for later
            else task_delete(proc->tasks[i]);
        }
    }

    /* unmap ELF segments (if there's any) */
    if(proc->elf_segments != NULL) elf_unload_prg(proc->vmm, proc->elf_segments, proc->num_elf_segments); // we can do this since we're in kernel space and therefore have no need to return to the calling code if it's being deleted

    if(deleting_current) task_delete((void*) task_current); // stage current task for deletion too
}

void proc_do_delete(struct proc* proc) {
    vmm_free(proc->vmm); // delete VMM config (or stage it for deletion)
    proc_pid_free(proc->pid);
    kfree(proc);
}

size_t proc_add_task(struct proc* proc, void* task) {
    mutex_acquire(&proc->mu_tasks);
    size_t i = 0;
    for(; i < proc->num_tasks; i++) {
        if(proc->tasks[i] == task) {
            /* attempting to insert an already existing task */
            mutex_release(&proc->mu_tasks);
            return i;
        }
        if(proc->tasks[i] == NULL) break;
    }
    if(i == proc->num_tasks) {
        void** new_tasks = krealloc(proc->tasks, (proc->num_tasks + PROC_TASK_ALLOCSZ) * sizeof(void*));
        if(new_tasks == NULL) {
            kerror("insufficient memory to add task to process 0x%x", proc);
            mutex_release(&proc->mu_tasks);
            return (size_t)-1;
        }
        proc->tasks = new_tasks;
        proc->num_tasks += PROC_TASK_ALLOCSZ;
        memset(&proc->tasks[i + 1], 0, (PROC_TASK_ALLOCSZ - 1) * sizeof(void*));
    }
    proc->tasks[i] = task;
    task_common(task)->pid = proc->pid;

    mutex_release(&proc->mu_tasks);
    return i;
}

void proc_delete_task(struct proc* proc, void* task) {
    mutex_acquire(&proc->mu_tasks);
    for(size_t i = 0; i < proc->num_tasks; i++) {
        if(proc->tasks[i] == task) {
            proc->tasks[i] = NULL;
            break;
        }
    }
    mutex_release(&proc->mu_tasks);
    task_delete(task);
}

void proc_init() {
    proc_kernel = proc_create(NULL, NULL); // we'll set the VMM later
    kassert(proc_kernel != NULL);
    proc_kernel->vmm = vmm_kernel;
    task_init();
    proc_add_task(proc_kernel, task_kernel);
}

size_t proc_fd_open(struct proc* proc, vfs_node_t* node, bool duplicate, bool read, bool write, bool append, bool excl) {
    /* open the file in the file table (or return its entry in the file table) */
    struct ftab* ftab = ftab_open(node, proc, read, write, excl);
    if(ftab == NULL) {
        kerror("ftab_open() failed, cannot open file");
        return (size_t)-1;
    }

    mutex_acquire(&proc->mu_fds);
    size_t i = 0;
    for(; i < proc->num_fds; i++) {
        if(proc->fds[i].ftab == NULL || (!duplicate && proc->fds[i].ftab == ftab)) break;
    }
    if(i == proc->num_fds) {
        fd_t* new_fds = krealloc(proc->fds, (proc->num_fds + PROC_FDS_ALLOCSZ) * sizeof(fd_t));
        if(new_fds == NULL) {
            kerror("insufficient memory to add fd entry to process 0x%x", proc);
            mutex_release(&proc->mu_fds);
            return (size_t)-1;
        }
        proc->fds = new_fds;
        proc->num_fds += PROC_FDS_ALLOCSZ;
        memset(&proc->fds[i], 0, PROC_TASK_ALLOCSZ * sizeof(fd_t));
    }
    mutex_release(&proc->mu_fds);

    mutex_acquire(&proc->fds[i].mutex);
    proc->fds[i].ftab = ftab;
    proc->fds[i].read = (read) ? 1 : 0;
    proc->fds[i].write = (write) ? 1 : 0;
    proc->fds[i].append = (append) ? 1 : 0;
    proc->fds[i].offset = 0; // reset offset
    mutex_release(&proc->fds[i].mutex);

    return i;
}

bool proc_fd_check(struct proc* proc, size_t fd) {
    mutex_acquire(&proc->mu_fds);
    bool ret = (fd < proc->num_fds);
    mutex_release(&proc->mu_fds);
    if(ret) {
        mutex_acquire(&proc->fds[fd].mutex);
        ret = (proc->fds[fd].ftab != NULL);
        mutex_release(&proc->fds[fd].mutex);
    }
    return ret;
}

void proc_fd_close(struct proc* proc, size_t fd) {
    if(!proc_fd_check(proc, fd)) return;

    mutex_acquire(&proc->fds[fd].mutex);
    ftab_close(proc->fds[fd].ftab, proc);
    memset(&proc->fds[fd], 0, sizeof(fd_t));
    mutex_release(&proc->fds[fd].mutex); // this might not be necessary
}

uint64_t proc_fd_read(struct proc* proc, size_t fd, uint64_t size, uint8_t* buf) {
    if(!proc_fd_check(proc, fd)) return 0; // invalid fd

    mutex_acquire(&proc->fds[fd].mutex);

    uint64_t ret = 0;
    if(proc->fds[fd].read) {
        ret = ftab_read(proc->fds[fd].ftab, proc, proc->fds[fd].offset, size, buf);
        proc->fds[fd].offset += ret;
    }

    mutex_release(&proc->fds[fd].mutex);
    return ret;
}

uint64_t proc_fd_write(struct proc* proc, size_t fd, uint64_t size, const uint8_t* buf) {
    if(!proc_fd_check(proc, fd)) return 0; // invalid fd

    mutex_acquire(&proc->fds[fd].mutex);

    uint64_t ret = 0;
    if(proc->fds[fd].write) {
        if(proc->fds[fd].append) proc->fds[fd].offset = proc->fds[fd].ftab->node->length; // seek to end before writing
        ret = ftab_write(proc->fds[fd].ftab, proc, proc->fds[fd].offset, size, buf);
        proc->fds[fd].offset += ret;
    }

    mutex_release(&proc->fds[fd].mutex);
    return ret;
}
