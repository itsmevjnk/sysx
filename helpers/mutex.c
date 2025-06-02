#include <helpers/mutex.h>

__attribute__((weak)) void mutex_acquire(mutex_t* m) {
    while(atomic_flag_test_and_set_explicit(&m->locked, memory_order_acquire));
        // task_yield_noirq(); // if atomic_flag_test_and_set returns true (i.e. m->locked was originally true) then we yield
}

__attribute__((weak)) void mutex_release(mutex_t* m) {
    atomic_flag_clear_explicit(&m->locked, memory_order_release);
    // task_yield_noirq(); // so we don't starve other tasks (TODO: reimplement so that we (a) don't end up with nested yields (as task_yield uses mutex_release too) and (b) only yield if there's another task waiting to acquire the mutex)
}

__attribute__((weak)) bool mutex_test(const mutex_t* m) {
    return atomic_load_explicit(&m->locked, memory_order_consume);
}
