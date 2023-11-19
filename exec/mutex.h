#ifndef EXEC_MUTEX_H
#define EXEC_MUTEX_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    size_t locked; // TODO: more data?
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

#endif