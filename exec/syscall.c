#include <exec/syscall.h>
#include <exec/task.h>
#include <exec/process.h>
#include <kernel/log.h>

void syscall_exit(size_t retval) {
    kdebug("PID %u exiting with return code %u", task_common((void*) task_current)->pid, retval); 
    proc_abort();
}

size_t syscall_fork() {
    proc_t* proc_child = proc_fork();
    if(!proc_child) return SIZE_MAX; // fork failed
    if(task_common((void*) task_current)->pid == proc_child->pid) return 0; // we're on the child process now
    kdebug("PID %u forked into child PID %u", proc_child->parent_pid, proc_child->pid);
    return proc_child->pid;
}

bool syscall_handler_stub(size_t* func_ret, size_t* arg1, size_t* arg2, size_t* arg3, size_t* arg4, size_t* arg5) {
    (void) arg4; (void) arg5; // unused args (as of now)
    switch(*func_ret) {
        case SYSCALL_EXIT:
            syscall_exit(*arg1);
            return true;
        case SYSCALL_READ:
            *func_ret = proc_fd_read(proc_get(task_common((void*) task_current)->pid), *arg3, *arg1, (uint8_t*)*arg2);
            return true;
        case SYSCALL_WRITE:
            *func_ret = proc_fd_write(proc_get(task_common((void*) task_current)->pid), *arg3, *arg1, (const uint8_t*)*arg2);
            return true;
        case SYSCALL_FORK:
            *func_ret = syscall_fork();
            return true;
        default:
            *func_ret = (size_t)-1;
            return false;
    }
}
