#ifndef HAL_INTR_H
#define HAL_INTR_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * void intr_enable()
 *  Enables interrupts.
 */
void intr_enable();

/*
 * void intr_disable()
 *  Disables interrupts.
 */
void intr_disable();

/*
 * bool intr_test()
 *  Checks whether interrupts is enabled.
 */
bool intr_test();

/*
 * void intr_handle(uint8_t vector, void (*handler)(uint8_t vector, void* context))
 *  Sets a function pointed by handler for handling interrupt vector.
 */
void intr_handle(uint8_t vector, void (*handler)(uint8_t vector, void* context));

/*
 * bool intr_is_handled(uint8_t vector)
 *  Checks if there's an interrupt handler assigned to the specified vector.
 */
bool intr_is_handled(uint8_t vector);

/* interrupt handler entry - for use by higher-level interrupt dispatchers (i.e. interrupt controller drivers) */
typedef struct {
    size_t irq;
    void (*handler)(size_t irq, void* context);
} intr_handler_t;

#endif
