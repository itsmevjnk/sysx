#ifndef HAL_TIMER_H
#define HAL_TIMER_H

#include <stddef.h>
#include <stdint.h>

#if defined(TIMER_SIZE_64)
typedef uint64_t    timer_tick_t;
#elif defined(TIMER_SIZE_32)
typedef uint32_t    timer_tick_t;
#else
typedef size_t      timer_tick_t;
#endif

extern volatile timer_tick_t timer_tick; // microsecond resolution

/*
 * void timer_handler(size_t delta, void* context)
 *  Increments timer_tick by the specified delta microseconds, and
 *  performs any other timer-related tasks.
 *  This is to be called from the timer interrupt handler.
 */
void timer_handler(size_t delta, void* context);

#endif