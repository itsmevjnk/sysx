#ifndef ARCH_X86_I8253_H
#define ARCH_X86_I8253_H

#include <stddef.h>
#include <stdint.h>

/*
 * void pit_systimer_init()
 *  Initializes the PIT's channel 0 to work as the system timer
 *  source (incrementing every microsecond).
 */
void pit_systimer_init();

/*
 * void pit_systimer_stop()
 *  Disables the PIT's channel 0, thus ending its role as the
 *  system timer source.
 */
void pit_systimer_stop();

#endif