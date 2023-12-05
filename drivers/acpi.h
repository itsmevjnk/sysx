#ifndef DRIVERS_ACPI_H
#define DRIVERS_ACPI_H

#if defined(FEAT_ACPI_LAI)

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FEAT_ACPI

#ifdef FEAT_ACPI_LAI
#include <lai/core.h>
#include <lai/helpers/pm.h>
#include <lai/helpers/sci.h>
#include <lai/drivers/ec.h>
#include <lai/drivers/timer.h>
#endif

/*
 * bool acpi_arch_init()
 *  Initializes any architecture-specific features required for ACPI.
 *  Returns true on success, or false on failure.
 *  This is an architecture-specific function.
 */
bool acpi_arch_init();

/*
 * bool acpi_impl_init()
 *  Initializes the used ACPI implementation.
 *  Returns true on success, or false on failure.
 *  This is an implementation-specific function.
 */
bool acpi_impl_init();

/*
 * bool acpi_init()
 *  Initializes any features required for ACPI and enables ACPI mode.
 *  Returns true on success, or false on failure.
 */
bool acpi_init();

/*
 * bool acpi_enable()
 *  Enables ACPI mode.
 *  Returns true on success, or false on failure.
 *  This is an implementation-specific function.
 */
bool acpi_enable();

/*
 * bool acpi_disable()
 *  Disables ACPI mode.
 *  Returns true on success, or false on failure.
 *  This is an implementation-specific function.
 */
bool acpi_disable();

/*
 * bool acpi_enter_sleep(uint8_t state)
 *  Enters an ACPI sleeping state (S0-S5).
 *  Returns true on success, or false on failure.
 *  This is an implementation-specific function.
 */
bool acpi_enter_sleep(uint8_t state);

/*
 * bool acpi_reboot()
 *  Reboots the system via ACPI.
 *  Returns true on success, or false on failure.
 *  This is an implementation-specific function.
 */
bool acpi_reboot();

#endif

#endif