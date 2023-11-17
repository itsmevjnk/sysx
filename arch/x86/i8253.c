#include <arch/x86/i8253.h>
#include <arch/x86/i8259.h>
#include <arch/x86cpu/asm.h>
#include <hal/timer.h>

/* Intel 8253/8254 PIT IO ports */
#define PIT_BASE                0x40
#define PIT_CH0_DATA            PIT_BASE
#define PIT_CH1_DATA            (PIT_BASE + 1)
#define PIT_CH2_DATA            (PIT_BASE + 2)
#define PIT_CMD                 (PIT_BASE + 3)

/* PIT read/write modes */
#define PIT_RW_LSB              1
#define PIT_RW_MSB              2
#define PIT_RW_LSB_MSB          3

#define pit_command(ch, rw, mode, bcd)      outb(PIT_CMD, ((ch) << 6) | ((rw) << 4) | ((mode) << 1) | (bcd))

#define PIT_IRQ                 0 // PIT IRQ line

/* PIT clock frequency */
#ifndef PIT_FREQ
#define PIT_FREQ                1193182 // 1.193182 MHz
#endif

/* desired system timer frequency */
#ifndef PIT_SYSTIMER_FREQ
#define PIT_SYSTIMER_FREQ       1000
#endif

#define PIT_DIVISOR             ((uint16_t)(PIT_FREQ / PIT_SYSTIMER_FREQ))

void pit_isr(uint8_t irq, void* context) {
    (void) irq;
    timer_handler(1000000UL / PIT_SYSTIMER_FREQ, context);
}

void pit_systimer_init() {
    pit_command(0, PIT_RW_LSB_MSB, 3, 0); // mode 3 - square wave generator
    outb(PIT_CH0_DATA, (uint8_t)PIT_DIVISOR); // LSB
    outb(PIT_CH0_DATA, (uint8_t)(PIT_DIVISOR >> 8)); // MSB
    pic_handle(PIT_IRQ, &pit_isr); // register handler
    pic_unmask(PIT_IRQ);
}

void pit_systimer_stop() {
    pic_mask(PIT_IRQ);
}

