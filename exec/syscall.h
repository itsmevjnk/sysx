#ifndef EXEC_SYSCALL_H
#define EXEC_SYSCALL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * void syscall_init()
 *  Initializes syscall capabilities for the target architecture.
 *  This is an architecture-specific function.
 */
void syscall_init();

/*
 * bool syscall_handler_stub(size_t* func_ret, size_t* arg1, size_t* arg2, size_t* arg3, size_t* arg4, size_t* arg5)
 *  Common syscall handler, to be called by the architecture-specific
 *  syscall handler.
 *  Returns true if the syscall has been handled succesfully, or false
 *  otherwise.
 */
bool syscall_handler_stub(size_t* func_ret, size_t* arg1, size_t* arg2, size_t* arg3, size_t* arg4, size_t* arg5);

/* syscall function numbers */
#define SYSCALL_EXIT                            0 // arg1 = return code
#define SYSCALL_ARCH_LO                         0xF0000000 // start of architecture-specific syscall functions
#define SYSCALL_ARCH_HI                         0xFFFFFFFF

#endif
