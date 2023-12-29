#ifndef ARCH_X86_I8253_H
#define ARCH_X86_I8253_H

#include <stddef.h>
#include <stdint.h>

/* PIT clock frequency */
#ifndef PIT_FREQ
#define PIT_FREQ                1193182 // 1.193182 MHz
#endif

/* desired system timer frequency */
#ifndef PIT_SYSTIMER_FREQ
#define PIT_SYSTIMER_FREQ       1000
#endif

#define PIT_DIVISOR             ((uint16_t)(PIT_FREQ / PIT_SYSTIMER_FREQ)) // the divisor value to be fed into the PIT

#define PIT_SYSTIMER_FREQ_REAL  ((float)PIT_FREQ / (float)PIT_DIVISOR) // the resulting (actual) system timer frequency

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