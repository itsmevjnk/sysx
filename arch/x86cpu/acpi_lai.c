#ifdef FEAT_ACPI_LAI

#include <lai/host.h>
#include <arch/x86cpu/asm.h>

void laihost_outb(uint16_t port, uint8_t val) {
    outb(port, val);
}

void laihost_outw(uint16_t port, uint16_t val) {
    outw(port, val);
}

void laihost_outd(uint16_t port, uint32_t val) {
    outl(port, val);
}

uint8_t laihost_inb(uint16_t port) {
    return inb(port);
}

uint16_t laihost_inw(uint16_t port) {
    return inw(port);
}

uint32_t laihost_ind(uint16_t port) {
    return inl(port);
}

#endif
