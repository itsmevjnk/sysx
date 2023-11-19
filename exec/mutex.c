#include <exec/mutex.h>
#include <exec/task.h>

#ifndef ARCH_MUTEX

void mutex_acquire(mutex_t* m) {
    while(m->locked) task_yield_noirq(); // mutex is being held - yield to the next task.
    m->locked = 1;
}

void mutex_release(mutex_t* m) {
    m->locked = 0;
    task_yield_noirq(); // so we don't starve other tasks
}

#endif
