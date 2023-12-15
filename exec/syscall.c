#include <exec/syscall.h>
#include <exec/task.h>
#include <exec/process.h>
#include <kernel/log.h>

void syscall_exit(proc_t* proc, size_t retval) {
    kdebug("PID %u exiting with return code %u", proc->pid, retval); 
    proc_delete(proc);
    while(1); // do not do anything else, or we may crash
}

size_t syscall_fork() {
    proc_t* proc_child = proc_fork();
    if(task_common((void*) task_current)->pid == proc_child->pid) return 0; // we're on the child process now
    kdebug("PID %u forked into child PID %u", proc_child->parent_pid, proc_child->pid);
    return proc_child->pid;
}

bool syscall_handler_stub(size_t* func_ret, size_t* arg1, size_t* arg2, size_t* arg3, size_t* arg4, size_t* arg5) {
    (void) arg4; (void) arg5; // unused args (as of now)
    size_t pid = task_common((void*) task_current)->pid; proc_t* proc = proc_get(pid);
    switch(*func_ret) {
        case SYSCALL_EXIT:
            syscall_exit(proc, *arg1);
            return true;
        case SYSCALL_READ:
            *func_ret = proc_fd_read(proc, *arg3, *arg1, (uint8_t*)*arg2);
            return true;
        case SYSCALL_WRITE:
            *func_ret = proc_fd_write(proc, *arg3, *arg1, (const uint8_t*)*arg2);
            return true;
        case SYSCALL_FORK:
            *func_ret = syscall_fork();
            return true;
        default:
            *func_ret = (size_t)-1;
            return false;
    }
}
