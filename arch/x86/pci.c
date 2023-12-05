#ifdef FEAT_PCI

#include <drivers/pci.h>
#include <arch/x86cpu/asm.h>

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

#endif
