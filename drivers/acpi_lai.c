#include <drivers/acpi.h>
#include <kernel/log.h>

bool acpi_impl_init() {
    kinfo("creating namespace");
    lai_create_namespace();
    return true;
}

bool acpi_enable() {
    return (!lai_enable_acpi(0)); // TODO: use APIC
}

bool acpi_disable() {
    return (!lai_disable_acpi());
}

bool acpi_enter_sleep(uint8_t state) {
    return (lai_enter_sleep(state) == LAI_ERROR_NONE);
}

bool acpi_reboot() {
    return (lai_acpi_reset() == LAI_ERROR_NONE);
}
