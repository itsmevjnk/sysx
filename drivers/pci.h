#ifndef DRIVERS_PCI_H
#define DRIVERS_PCI_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <drivers/pci_defs.h>
#include <hal/devtree.h>

#ifdef FEAT_PCI // -DFEAT_PCI must be in CFLAGS to enable PCI support

/*
 * #define pci_cfg_address(bus, dev, func, offset)
 *  Retrieves the PCI configuration address for the specified device.
 *  This follows the Configuration Space Access Mechanism #1.
 */
#define pci_cfg_address(bus, dev, func, offset)                 ((1 << 31) | ((bus) << 16) | ((dev) << 11) | ((func) << 8) | ((offset) & 0xFC))

/*
 * uint32_t pci_cfg_read_dword_aligned(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
 *  Reads a 32-bit value from the PCI configuration space of the
 *  specified device at the specified 4-byte aligned offset. 
 *  This is an architecture-specific function.
 */
uint32_t pci_cfg_read_dword_aligned(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/*
 * uint32_t pci_cfg_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
 *  Reads a 32-bit value from the PCI configuration space of the
 *  specified device at the specified offset. 
 *  This is a common-defined function.
 */
uint32_t pci_cfg_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/*
 * uint16_t pci_cfg_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
 *  Reads a 16-bit value from the PCI configuration space of the
 *  specified device at the specified offset. 
 *  This is a common-defined function.
 */
uint16_t pci_cfg_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/*
 * uint8_t pci_cfg_read_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
 *  Reads an 8-bit value from the PCI configuration space of the
 *  specified device at the specified offset. 
 *  This is a common-defined function.
 */
uint8_t pci_cfg_read_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

/*
 * void pci_cfg_write_dword_aligned(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val)
 *  Writes a 32-bit value from the PCI configuration space of the
 *  specified device at the specified 4-byte aligned offset. 
 *  This is an architecture-specific function.
 */
void pci_cfg_write_dword_aligned(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);

/*
 * void pci_cfg_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val)
 *  Writes a 32-bit value from the PCI configuration space of the
 *  specified device at the specified offset. 
 *  This is a common-defined function.
 */
void pci_cfg_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val);

/*
 * void pci_cfg_write_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val)
 *  Writes a 32-bit value from the PCI configuration space of the
 *  specified device at the specified offset. 
 *  This is a common-defined function.
 */
void pci_cfg_write_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val);

/*
 * void pci_cfg_write_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t val)
 *  Writes a 32-bit value from the PCI configuration space of the
 *  specified device at the specified offset. 
 *  This is a common-defined function.
 */
void pci_cfg_write_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t val);

/*
 * #define pci_read_hdrtype(bus, dev, func)
 *  Reads the Header Type field from the specified device's PCI
 *  configuration space.
 */
#define pci_read_hdrtype(bus, dev, func)                        pci_cfg_read_byte((bus), (dev), (func), PCI_CFG_HDR_TYPE)

/*
 * #define pci_read_vid(bus, dev, func)
 *  Reads the Vendor ID field from the specified device's PCI
 *  configuration space.
 */
#define pci_read_vid(bus, dev, func)                            pci_cfg_read_word((bus), (dev), (func), PCI_CFG_VID)

/*
 * #define pci_read_pid(bus, dev, func)
 *  Reads the Product ID field from the specified device's PCI
 *  configuration space.
 */
#define pci_read_pid(bus, dev, func)                            pci_cfg_read_word((bus), (dev), (func), PCI_CFG_PID)

/*
 * #define pci_read_class(bus, dev, func)
 *  Reads the class of the specified device from its PCI configuration
 *  space.
 */
#define pci_read_class(bus, dev, func)                          pci_cfg_read_byte((bus), (dev), (func), PCI_CFG_CLASS)

/*
 * #define pci_read_subclass(bus, dev, func)
 *  Reads the subclass of the specified device from its PCI configuration
 *  space.
 */
#define pci_read_subclass(bus, dev, func)                       pci_cfg_read_byte((bus), (dev), (func), PCI_CFG_SUBCLASS)

/*
 * #define pci_read_progif(bus, dev, func)
 *  Reads the Prog IF field of the specified device's PCI configuration
 *  space.
 */
#define pci_read_progif(bus, dev, func)                         pci_cfg_read_byte((bus), (dev), (func), PCI_CFG_PROG_IF)

extern devtree_t pci_devtree_root;
extern devtree_t pci_devtree_geo_root; // device tree root for PCI (geographically named)
extern devtree_t pci_devtree_id_root; // device tree root for PCI (named by VID-PID)

/* PCI device node structure */
typedef struct {
    devtree_t header;
    uint16_t vid;
    uint16_t pid;
    uint8_t class;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t bus;
    uint8_t dev;
    uint8_t func;
} pci_devtree_t;

/*
 * void pci_init()
 *  Detects devices on the PCI buses and add them to the device tree.
 */
void pci_init();

#endif

#endif
