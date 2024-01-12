#include <helpers/mutex.h>
#include <exec/task.h>

__attribute__((weak)) void mutex_acquire(mutex_t* m) {
    while(m->locked) task_yield_noirq(); // mutex is being held - yield to the next task.
    task_yield_block();
    m->locked = 1;
    task_yield_unblock();
}

__attribute__((weak)) void mutex_release(mutex_t* m) {
    task_yield_block();
    m->locked = 0;
    task_yield_unblock();
    // task_yield_noirq(); // so we don't starve other tasks (TODO: reimplement so that we (a) don't end up with nested yields (as task_yield uses mutex_release too) and (b) only yield if there's another task waiting to acquire the mutex)
}

__attribute__((weak)) bool mutex_test(const mutex_t* m) {
    return (m->locked);
}
