#ifndef ARCH_X86CPU_APIC_H
#define ARCH_X86CPU_APIC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern bool apic_enabled; // weak symbol - if this is NULL then it's safe to assume that APIC is not enabled
extern uint8_t ioapic_irq_gsi[16]; // legacy IRQ to GSI mapping
extern uintptr_t lapic_base;

/*
 * void ioapic_handle(uint8_t gsi, void (*handler)(uint8_t gsi, void* context))
 *  Assigns an interrupt handler to the specified GSI.
 */
void ioapic_handle(uint8_t gsi, void (*handler)(uint8_t gsi, void* context));

/*
 * void ioapic_legacy_handler(uint8_t gsi, void* context)
 *  Handler adapter for legacy PIC IRQs.
 */
void ioapic_legacy_handler(uint8_t gsi, void* context);

/*
 * bool ioapic_is_handled(uint8_t gsi)
 *  Checks if the specified GSI has an assigned interrupt handler.
 */
bool ioapic_is_handled(uint8_t gsi);

/*
 * void ioapic_mask(uint8_t gsi)
 *  Masks the specified GSI on the IOAPICs.
 */
void ioapic_mask(uint8_t gsi);

/*
 * void ioapic_unmask(uint8_t gsi)
 *  Unmasks the specified GSI on the IOAPICs.
 */
void ioapic_unmask(uint8_t gsi);

/*
 * bool ioapic_get_mask(uint8_t gsi)
 *  Checks if the specified GSI is masked.
 */
bool ioapic_get_mask(uint8_t gsi);

/*
 * void apic_eoi()
 *  Issues an End-of-Interrupt (EOI) to the local APIC.
 */
void apic_eoi();

/*
 * bool apic_init()
 *  Sets up the local and I/O APIC.
 */
bool apic_init();

#endif
