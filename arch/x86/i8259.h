#ifndef ARCH_X86_I8259_H
#define ARCH_X86_I8259_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <arch/x86cpu/asm.h>

/* Intel 8259 PIC base interrupt vector */
#ifndef PIC_VECT_BASE
#define PIC_VECT_BASE           0x20
#endif

/* Intel 8259 PIC IO addresses */
#define PIC1_BASE           0x20
#define PIC2_BASE           0xA0

#define PIC1_CMD            PIC1_BASE
#define PIC1_DATA           (PIC1_BASE + 1)

#define PIC2_CMD            PIC2_BASE
#define PIC2_DATA           (PIC2_BASE + 1)

/*
 * inline static void pic_eoi_inline(uint8_t irq)
 *  Acknowledges an IRQ.
 *  This is a short routine of two I/O byte outputs; therefore, it
 *  is defined as an inline function. Alternately, the proper
 *  function pic_eoi(uint8_t) can be used.
 */
inline static void pic_eoi_inline(uint8_t irq) {
    if(irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

/*
 * void pic_eoi(uint8_t irq)
 *  Acknowledges an IRQ.
 */
void pic_eoi(uint8_t irq);

/*
 * void pic_init()
 *  Initializes the PICs to send interrupts at the vector specified
 *  by PIC_VECT_BASE, and masks all of their available IRQ lines.
 */
void pic_init();

/*
 * void pic_mask_bm(uint16_t bitmask)
 *  Masks one or multiple IRQs following a specified bitmask.
 */
void pic_mask_bm(uint16_t bitmask);

/*
 * void pic_unmask_bm(uint16_t bitmask)
 *  Unmasks one or multiple IRQs following a specified bitmask.
 */
void pic_unmask_bm(uint16_t bitmask);

/*
 * void pic_mask(uint8_t irq)
 *  Masks the specified IRQ line.
 */
void pic_mask(uint8_t irq);

/*
 * void pic_mask(uint8_t irq)
 *  Unmasks the specified IRQ line.
 */
void pic_unmask(uint8_t irq);

/*
 * uint16_t pic_read_irr()
 *  Reads the PICs' Interrupt Request Register (IRR), which indicates
 *  which interrupt(s) have been raised.
 */
uint16_t pic_read_irr();

/*
 * uint16_t pic_read_isr()
 *  Reads the PICs' In-Service Register (ISR), which indicates which
 *  interrupt(s) are currently being serviced (except spurious IRQs).
 */
uint16_t pic_read_isr();

/*
 * void pic_handle(uint8_t irq, void (*handler)(uint8_t irq, void* context))
 *  Registers an IRQ handler. The handler function does not have to
 *  acknowledge the IRQ, as this is handled by the handler stub.
 */
void pic_handle(uint8_t irq, void (*handler)(uint8_t irq, void* context));

/*
 * bool pic_is_handled(uint8_t irq)
 *  Checks whether the specified IRQ handler is handled by a handler
 *  function.
 */
bool pic_is_handled(uint8_t irq);

#endif