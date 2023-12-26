#include <drivers/acpi.h>
#include <kernel/log.h>
#include <kernel/cmdline.h>
#include <drivers/pci.h>
#include <string.h>

bool acpi_enabled = false;

#ifdef FEAT_ACPI

bool acpi_init() {
    /* check if ACPI is disabled according to cmdline */
    const char* acpi_state = cmdline_find_kvp("acpi");
    if(acpi_state != NULL && !strcmp(acpi_state, "off")) {
        kinfo("ACPI support is disabled via cmdline");
        return false;
    }
    
    if(!acpi_arch_init()) {
        kerror("acpi_arch_init() failed");
        return false;
    }

    if(!acpi_impl_init()) {
        kerror("acpi_impl_init() failed");
        return false;
    }

    if(!acpi_enable()) {
        kerror("acpi_enable() failed");
        return false;
    }
    
    acpi_enabled = true;
    return true;
}

size_t acpi_pci_route_irq(uint8_t bus, uint8_t dev, uint8_t func, uint8_t pin, uint8_t* flags) {
#ifdef FEAT_ACPI_LAI
    acpi_resource_t rsrc;
    if(lai_pci_route_pin(&rsrc, 0, bus, dev, func, pin + 1)) return (size_t)-1; // add 1 back so that LAI can subtract again
    if(flags != NULL) *flags = ((rsrc.irq_flags & ACPI_SMALL_IRQ_EDGE_TRIGGERED) ? PCI_IRQ_EDGE : 0) | ((rsrc.irq_flags & ACPI_SMALL_IRQ_ACTIVE_LOW) ? PCI_IRQ_ACTIVE_LOW : 0);
    return rsrc.base;
#endif
}

#endif