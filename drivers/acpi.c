#include <drivers/acpi.h>
#include <kernel/log.h>
#include <kernel/cmdline.h>
#include <string.h>

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
    
    return true;
}

#endif