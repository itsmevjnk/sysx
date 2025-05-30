#include <arch/x86/i8259.h>
#include <arch/x86cpu/idt.h>
#include <kernel/log.h>
#include <arch/x86cpu/apic.h>
#include <stdlib.h>
#include <string.h>

/* PIC initialization commands - see https://wiki.osdev.org/8259_PIC */
#define ICW1_ICW4           0x01 // ICW4 will be present
#define ICW1_SINGLE         0x02 // single (cascade) mode
#define ICW1_INTERVAL4      0x04 // call address interval 4 (8)
#define ICW1_LEVEL          0x08 // edge triggered mode
#define ICW1_INIT           0x10 // initialization

#define ICW4_8086           0x01 // 8086/88 mode
#define ICW4_AUTO           0x02 // auto EOI
#define ICW4_BUF_SLAVE      0x08 // buffered mode (slave)
#define ICW4_BUF_MASTER     0x0C // buffered mode (master)
#define ICW4_SFNM           0x10 // special fully nested (not)

void pic_eoi(uint8_t irq) {
    if(apic_enabled) {
        apic_eoi();
        return;
    }
    if(irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}

void pic_mask_bm(uint16_t bitmask) {
    if(apic_enabled) {
        for(size_t i = 0; i < 16; i++) {
            if((bitmask & (1 << i)) && !ioapic_get_mask(ioapic_irq_gsi[i])) ioapic_mask(ioapic_irq_gsi[i]);
        }
        return;
    }
    bitmask &= ~(1 << 2); // disallow masking of IRQ2, as it will kill the slave PIC
    if(((uint8_t) bitmask))
        outb(PIC1_DATA, inb(PIC1_DATA) | ((uint8_t) bitmask));
    if((uint8_t) (bitmask >> 8))
        outb(PIC2_DATA, inb(PIC2_DATA) | ((uint8_t) (bitmask >> 8)));
}

void pic_unmask_bm(uint16_t bitmask) {
    if(apic_enabled) {
        for(size_t i = 0; i < 16; i++) {
            if((bitmask & (1 << i)) && ioapic_get_mask(ioapic_irq_gsi[i])) ioapic_unmask(ioapic_irq_gsi[i]);
        }
        return;
    }
    bitmask = ~bitmask;
    if((uint8_t) bitmask != 0xFF)
        outb(PIC1_DATA, inb(PIC1_DATA) & ((uint8_t) bitmask));
    if((uint8_t) (bitmask >> 8) != 0xFF)
        outb(PIC2_DATA, inb(PIC2_DATA) & ((uint8_t) (bitmask >> 8)));
}

uint16_t pic_get_mask() {
    if(apic_enabled) {
        uint16_t ret = 0;
        for(size_t i = 0; i < 16; i++) {
            if(ioapic_get_mask(ioapic_irq_gsi[i])) ret |= (1 << i);
        }
        return ret;
    }
    return ((inb(PIC2_DATA) << 8) | inb(PIC1_DATA));
}

void pic_mask(uint8_t irq) {
    if(apic_enabled) {
        ioapic_mask(ioapic_irq_gsi[irq]);
        return;
    }
    pic_mask_bm(1 << irq);
}

void pic_unmask(uint8_t irq) {
    if(apic_enabled) {
        ioapic_unmask(ioapic_irq_gsi[irq]);
        return;
    }
    pic_unmask_bm(1 << irq);
}

uint16_t pic_read_irr() {
    outb(PIC1_CMD, 0x0A); outb(PIC2_CMD, 0x0A);
    return (inb(PIC2_CMD) << 8) | inb(PIC1_CMD);
}

uint16_t pic_read_isr() {
    outb(PIC1_CMD, 0x0B); outb(PIC2_CMD, 0x0B);
    return (inb(PIC2_CMD) << 8) | inb(PIC1_CMD);
}

intr_handler_t* pic_handlers = NULL;
size_t pic_handlers_cnt = 0;
#define PIC_HANDLERS_ALLOCSZ            4

void pic_handler_stub(uint8_t vector, void* context) {
    vector -= PIC_VECT_BASE; // get IRQ number
    if(vector == 7 || vector == 15) {
        /* lowest priority IRQ - check if it's a spurious one */
        uint16_t isr = pic_read_isr();
        if(!(isr & (1 << vector))) {
            /* spurious IRQ */
            if(vector == 15) pic_eoi_inline(2); // we still need to acknowledge the cascade IRQ
            kdebug("spurious IRQ %u", vector);
            return; // don't proceed further
        }
    }

    size_t n = 0;
    for(size_t i = 0; i < pic_handlers_cnt; i++) {
        if(pic_handlers[i].handler && pic_handlers[i].irq == vector) {
            pic_handlers[i].handler(vector, context);
            n++;
        }
    }
    if(!n) kdebug("unhandled IRQ %u", vector);
    pic_eoi_inline(vector); // acknowledge interrupt
}

size_t pic_handle(uint8_t irq, void (*handler)(size_t irq, void* context)) {
    size_t id = (size_t)-1;
    for(size_t i = 0; i < pic_handlers_cnt; i++) {
        if(!pic_handlers[i].handler) {
            id = i;
            break;
        }
    }
    if(id == (size_t)-1) {
        void* new_handlers = krealloc(pic_handlers, (pic_handlers_cnt + PIC_HANDLERS_ALLOCSZ) * sizeof(intr_handler_t));
        if(!new_handlers) {
            kerror("cannot allocate more memory for handlers list");
            return id;
        }
#if PIC_HANDLERS_ALLOCSZ > 1
        memset(&pic_handlers[pic_handlers_cnt + 1], 0, (PIC_HANDLERS_ALLOCSZ - 1) * sizeof(intr_handler_t));
#endif
        
        id = pic_handlers_cnt; pic_handlers_cnt++;
    }
    pic_handlers[id].irq = irq;
    pic_handlers[id].handler = handler;
    if(apic_enabled) ioapic_handle(ioapic_irq_gsi[irq], ioapic_legacy_handler);
    return id;
}

void pic_unhandle(size_t id) {
    if(id < pic_handlers_cnt) {
        pic_handlers[id].handler = NULL;
    }
}

bool pic_is_handled(uint8_t irq) {
    for(size_t i = 0; i < pic_handlers_cnt; i++) {
        if(pic_handlers[i].handler && pic_handlers[i].irq == irq) return true;
    }
    return false;
}

void pic_init() {
    __asm__ __volatile__("cli"); // disable interrupts

    /* initialize the PIC */
    outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); io_wait(); // give the PIC some time to do its thing
    outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4); io_wait();
    outb(PIC1_DATA, PIC_VECT_BASE); io_wait();
    outb(PIC2_DATA, PIC_VECT_BASE + 8); io_wait();
    outb(PIC1_DATA, (1 << 2)); io_wait(); // slave PIC on master PIC's IRQ2
    outb(PIC2_DATA, (1 << 1)); io_wait(); // cascade
    outb(PIC1_DATA, ICW4_8086); io_wait();
    outb(PIC2_DATA, ICW4_8086); io_wait();
    outb(PIC1_DATA, ~(1 << 2)); // mask all master PIC IRQs except IRQ2 (for cascade)
    outb(PIC2_DATA, ~0); // mask all slave PIC IRQs

    /* set up interrupt handling */
    pic_handlers = kcalloc(16, sizeof(intr_handler_t));
    if(!pic_handlers) kerror("cannot allocate handlers list");
    else {
        pic_handlers_cnt = 16;
        for(size_t i = 0; i < 16; i++) intr_handle(PIC_VECT_BASE + i, pic_handler_stub);
    }

    pic_eoi_inline(15); // EOI on both PICs in case some interrupt managed to fire

    __asm__ __volatile__("sti"); // re-enable interrupts

}