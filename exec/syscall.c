#include <exec/syscall.h>
#include <exec/task.h>
#include <exec/process.h>
#include <kernel/log.h>

void syscall_exit(size_t retval) {
    size_t pid = task_common(task_current)->pid;
    kdebug("PID %u exiting with return code %u", pid, retval);    
    proc_delete(proc_get(pid));
    while(1); // do not do anything else, or we may crash
}

bool syscall_handler_stub(size_t* func_ret, size_t* arg1, size_t* arg2, size_t* arg3, size_t* arg4, size_t* arg5) {
    switch(*func_ret) {
        case SYSCALL_EXIT:
            syscall_exit(*arg1);
            return true;
        default:
            *func_ret = (size_t)-1;
            return false;
    }
}
