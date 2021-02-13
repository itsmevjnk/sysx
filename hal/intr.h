#ifndef HAL_INTR_H
#define HAL_INTR_H

#include <stddef.h>
#include <stdint.h>

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
 * void intr_handle(uint8_t vector, void (*handler)(uint8_t vector, void* context))
 *  Sets a function pointed by handler for handling interrupt vector.
 */
void intr_handle(uint8_t vector, void (*handler)(uint8_t vector, void* context));

#endif
