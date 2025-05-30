#ifndef HELPERS_MUTEX_H
#define HELPERS_MUTEX_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef struct {
    atomic_bool locked; // TODO: more data?
} mutex_t;

/*
 * void mutex_acquire(mutex_t* m)
 *  Blocks the task until the specified mutex is free and then acquire it.
 */
void mutex_acquire(mutex_t* m);

/*
 * void mutex_release(mutex_t* m)
 *  Releases the mutex.
 */
void mutex_release(mutex_t* m);

/*
 * bool mutex_test(const mutex_t* m)
 *  Tests the mutex to check if it's locked.
 */
bool mutex_test(const mutex_t* m);

#endif