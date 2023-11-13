#ifndef EXEC_USERMODE_H
#define EXEC_USERMODE_H

#include <stddef.h>
#include <stdint.h>

/*
 * void usermode_switch(uintptr_t iptr, uintptr_t sptr)
 *  Switch to user mode, returning to the caller (if iptr = 0) or
 *  to a specified non-zero instruction pointer, and set the stack
 *  pointer to sptr (if sptr != 0).
 *  This function is to be supplied by the architecture-specific
 *  implementation.
 */
void usermode_switch(uintptr_t iptr, uintptr_t sptr);

#endif