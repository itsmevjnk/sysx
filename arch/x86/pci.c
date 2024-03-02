#ifdef FEAT_PCI

#include <drivers/pci.h>
#include <arch/x86cpu/asm.h>
#include <arch/x86/mptab.h>
#include <arch/x86cpu/apic.h>
#include <drivers/acpi.h>
#include <kernel/log.h>

#define PCI_CONFIG_ADDRESS                          0xCF8
#define PCI_CONFIG_DATA                             0xCFC

uint32_t pci_cfg_read_dword_aligned(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_cfg_address(bus, dev, func, offset));
    return inl(PCI_CONFIG_DATA);
}

void pci_cfg_write_dword_aligned(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    outl(PCI_CONFIG_ADDRESS, pci_cfg_address(bus, dev, func, offset));
    outl(PCI_CONFIG_DATA, val);
}

size_t pci_arch_get_irq_routing(uint8_t bus, uint8_t dev, uint8_t func, uint8_t pin, uint8_t* flags) {
    /* check if PCI device has been set up in advance */
    uint16_t irq = pci_cfg_read_word(bus, dev, func, PCI_CFG_H0_IRQ_LINE); // low byte = IRQ line, high byte = IRQ pin
    if(((pin == (uint8_t)-1 && (irq >> 8) && (irq >> 8) <= 4) || (irq >> 8) == pin) && (irq & 0xFF) <= 15) {
        /* valid IRQ line set by the BIOS */
        if(flags) *flags = PCI_IRQ_EDGE; // ISA: edge-triggered, active high
        if(apic_enabled) return ioapic_irq_gsi[irq & 0xFF]; // translate to APIC GSI
        else return (irq & 0xFF); // no translation if we're using legacy PIC
    }

    return (size_t)-1;
}

size_t pci_arch_route_irq(uint8_t bus, uint8_t dev, uint8_t func, uint8_t pin, uint8_t* flags) {
#ifdef FEAT_ACPI
    if(acpi_enabled) return (size_t)-1; // we only handle non-ACPI methods here
#endif

    size_t ret = (size_t)-1;
    
    /* routing via MP tables */
    if(ret == (size_t)-1 && apic_enabled && mp_cfg && mp_busid_cnt) {
        mp_cfg_entry_t* entry = &mp_cfg->first_entry;
        for(size_t n = 0; n < mp_cfg->base_cnt; n++, entry = (mp_cfg_entry_t*) ((uintptr_t) entry + ((entry->type) ? 8 : 20))) {
            if(entry->type == MP_ETYPE_IRQ_ASSIGN) {
                kassert(entry->data.irq_assign.bus_src < mp_busid_cnt);
                if(mp_busid[entry->data.irq_assign.bus_src] == MP_BUS_PCI) {
                    /* PCI entry */
                    (void) func; // discard func as it doesn't matter
                    if(bus == entry->data.irq_assign.bus_src && dev == (entry->data.irq_assign.irq_src >> 2) && pin == (entry->data.irq_assign.irq_src & 0b11)) {
                        for(size_t i = 0; i < ioapic_cnt; i++) {
                            if(ioapic_info[i].id == entry->data.irq_assign.id_dest) {
                                ret = ioapic_info[i].gsi_base + entry->data.irq_assign.intin_dest;
                                if(flags) *flags = ((entry->data.irq_assign.flags & (1 << 1)) ? PCI_IRQ_ACTIVE_LOW : 0) | ((entry->data.irq_assign.flags & (1 << 3)) ? 0 : PCI_IRQ_EDGE);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    /* TODO: routing via PCI BIOS */

    return ret;
}

#endif
