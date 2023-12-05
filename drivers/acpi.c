#include <drivers/acpi.h>
#include <kernel/log.h>

#ifdef FEAT_ACPI

bool acpi_init() {
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
    
    return true;
}

#endif