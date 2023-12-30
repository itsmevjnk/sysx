#ifndef ARCH_X86CPU_APIC_H
#define ARCH_X86CPU_APIC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern bool apic_enabled; // weak symbol - if this is NULL then it's safe to assume that APIC is not enabled
extern uint8_t ioapic_irq_gsi[16]; // legacy IRQ to GSI mapping
extern uintptr_t lapic_base;

/* IOAPIC information */
extern size_t ioapic_cnt; // number of detected IOAPICs
typedef struct {
    uint8_t id;
    uintptr_t base; // base address (mapped)
    uint8_t gsi_base; // starting GSI
    uint8_t inputs; // number of inputs
} ioapic_info_t;
extern ioapic_info_t* ioapic_info; // table of IOAPICs

/* CPU/LAPIC information */
extern size_t apic_cpu_cnt; // number of detected CPU cores
typedef struct {
    size_t cpu_id;
    uint8_t apic_id;
} apic_cpu_info_t;
extern apic_cpu_info_t* apic_cpu_info; // table of detected CPUs
extern size_t apic_bsp_idx; // bootstrap processor's index

#define IOAPIC_HANDLER_VECT_BASE        0x30 // IOAPIC interrupt handler vector base
extern uint8_t lapic_handler_vect_base; // LAPIC interrupt handler vector base (calculated in apic_init)

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

/*
 * void ioapic_set_trigger(uint8_t gsi, bool edge, bool active_low)
 *  Set the IOAPIC line trigger conditions for the specified GSI.
 */
void ioapic_set_trigger(uint8_t gsi, bool edge, bool active_low);

/* APIC timer */

enum apic_tmr_divisor {
    APIC_TIMER_DIV2 = 0,
    APIC_TIMER_DIV4,
    APIC_TIMER_DIV8,
    APIC_TIMER_DIV16,
    APIC_TIMER_DIV32 = (1 << 3),
    APIC_TIMER_DIV64,
    APIC_TIMER_DIV128,
    APIC_TIMER_DIV1
};
#ifndef APIC_TIMER_DIVISOR
#define APIC_TIMER_DIVISOR                  APIC_TIMER_DIV16
#endif

#define APIC_TIMER_VECT                     (lapic_handler_vect_base + 0x00)

/*
 * void apic_timer_calibrate()
 *  Calibrates the APIC timer using the current system timer source.
 *  This function sets the static apic_timer_delta variable, which
 *  contains the number of timer ticks to increase every time the
 *  timer generates an interrupt.
 */
void apic_timer_calibrate();

/*
 * void apic_timer_enable()
 *  Enables the APIC timer as the system timer source.
 *  NOTE: all other timer sources must be disabled!
 */
void apic_timer_enable();

/*
 * void apic_timer_disable()
 *  Disables the APIC timer as the system timer source.
 */
void apic_timer_disable();

#endif
