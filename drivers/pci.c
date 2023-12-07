#ifdef FEAT_PCI

#include <drivers/pci.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <stdio.h>

#define PCI_CONFIG_ADDRESS                          0xCF8
#define PCI_CONFIG_DATA                             0xCFC

uint32_t pci_cfg_read_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint8_t offset_off = offset & ((1 << 0) | (1 << 1)); offset -= offset_off;
    if(!offset_off) return pci_cfg_read_dword_aligned(bus, dev, func, offset); // offset is already aligned
    else { // offset is not aligned - read 2 DWORDs and stitch them together
        return (pci_cfg_read_dword_aligned(bus, dev, func, offset) >> (offset_off * 8)) | (pci_cfg_read_dword_aligned(bus, dev, func, offset + 4) << (32 - offset_off * 8));
    }
}

void pci_cfg_write_dword(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t val) {
    uint8_t offset_off = offset & ((1 << 0) | (1 << 1)); offset -= offset_off;
    if(!offset_off) pci_cfg_write_dword_aligned(bus, dev, func, offset, val); // offset is already aligned
    else { // offset is not aligned - read and stitch data together
        uint32_t mask[] = {0x00000000, 0x000000FF, 0x0000FFFF, 0x00FFFFFF};
        pci_cfg_write_dword_aligned(bus, dev, func, offset + 0, (pci_cfg_read_dword_aligned(bus, dev, func, offset + 0) &  mask[offset_off]) | (val << (     offset_off * 8))); // 1st half
        pci_cfg_write_dword_aligned(bus, dev, func, offset + 4, (pci_cfg_read_dword_aligned(bus, dev, func, offset + 4) & ~mask[offset_off]) | (val << (32 - offset_off * 8))); // 2nd half
    }
}

uint16_t pci_cfg_read_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint8_t offset_off = offset & ((1 << 0) | (1 << 1)); offset -= offset_off;
    uint16_t ret = (pci_cfg_read_dword_aligned(bus, dev, func, offset) >> (8 * offset_off));
    if(offset_off == 3) ret |= (pci_cfg_read_dword_aligned(bus, dev, func, offset + 4) & 0xFF) << 8; // read high nibble from the next DWORD
    return ret;
}

void pci_cfg_write_word(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t val) {
    uint8_t offset_off = offset & ((1 << 0) | (1 << 1)); offset -= offset_off;
    uint32_t mask[] = {0xFFFF0000, 0xFF0000FF, 0x0000FFFF, 0x00FFFFFF};
    pci_cfg_write_dword_aligned(bus, dev, func, offset, (pci_cfg_read_dword_aligned(bus, dev, func, offset) & mask[offset_off]) | (val << (offset_off * 8)));
    if(offset_off == 3) pci_cfg_write_dword_aligned(bus, dev, func, offset + 4, (pci_cfg_read_dword_aligned(bus, dev, func, offset + 4) & 0xFFFFFF00) | (val >> 8)); // write to next DWORD
}

uint8_t pci_cfg_read_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint8_t offset_off = offset & ((1 << 0) | (1 << 1)); offset -= offset_off;
    return (pci_cfg_read_dword_aligned(bus, dev, func, offset) >> (8 * offset_off));
}

void pci_cfg_write_byte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t val) {
    uint8_t offset_off = offset & ((1 << 0) | (1 << 1)); offset -= offset_off;
    uint32_t mask[] = {0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF, 0x00FFFFFF};
    pci_cfg_write_dword_aligned(bus, dev, func, offset, (pci_cfg_read_dword_aligned(bus, dev, func, offset) & mask[offset_off]) | (val << (offset_off * 8)));
}

devtree_t pci_devtree_root = {
    sizeof(devtree_t),
    "pci",
    DEVTREE_NODE_BUS,
    NULL, // to be filled in
    NULL,
    NULL,
    {1}
};

static void pci_scan_bus(uint8_t bus); // defined below

static bool pci_scan_func(uint8_t bus, uint8_t dev, uint8_t func) {
    uint32_t vp = pci_cfg_read_dword(bus, dev, func, PCI_CFG_VID); // read VID and PID at the same time
    uint16_t* vid_pid = (uint16_t*) &vp; // vid_pid[0] = VID, vid_pid[1] = PID
    if(vid_pid[0] == 0xFFFF) {
        // kerror("%02x:%02x.%02x doesn't exist", bus, dev, func);
        return false; // function doesn't exist
    }

    uint16_t cs = pci_cfg_read_word(bus, dev, func, PCI_CFG_SUBCLASS); // read subclass and class at the same time
    uint8_t* class_sub = (uint8_t*) &cs; // class_sub[0] = subclass, class_sub[1] = class
    uint8_t prog_if = pci_read_progif(bus, dev, func);

    kinfo("device %02x:%02x.%x: ID %04x:%04x, class %02x:%02x, prog IF %02x", bus, dev, func, vid_pid[0], vid_pid[1], class_sub[1], class_sub[0], prog_if);

    pci_devtree_t* node = kcalloc(1, sizeof(pci_devtree_t));
    if(node == NULL) kwarn("cannot allocate memory for PCI device node");
    else {
        node->header.size = sizeof(pci_devtree_t);
        ksprintf(node->header.name, "%02x:%02x.%x", bus, dev, func);
        node->header.type = DEVTREE_NODE_DEVICE;
        node->vid = vid_pid[0]; node->pid = vid_pid[1];
        node->class = class_sub[1]; node->subclass = class_sub[0]; node->prog_if = prog_if;
        node->bus = bus; node->dev = dev; node->func = func;
        devtree_add_child(&pci_devtree_root, (devtree_t*) node);
        kdebug("created devtree node at 0x%x for device %s", node, node->header.name);
    }

    if(class_sub[1] == PCI_CLASS_BRIDGE && (class_sub[0] == PCI_BRG_PCI_BRIDGE || class_sub[0] == PCI_BRG_PCI_BRIDGE_ALT)) {
        /* scan devices behind the bridge - TODO: set up secondary buses */
        uint8_t bus_sec = pci_cfg_read_byte(bus, dev, func, PCI_CFG_H1_SEC_BUS);
        kdebug("device %02x:%02x.%x (%04x:%04x) is a PCI-to-PCI bridge - scanning secondary bus %02x", bus, dev, func, vid_pid[0], vid_pid[1], bus_sec);
        pci_scan_bus(bus_sec);
    }

    return true;
}

static void pci_scan_dev(uint8_t bus, uint8_t dev) {
    if(!pci_scan_func(bus, dev, 0)) return; // device doesn't exist
    if(pci_read_hdrtype(bus, dev, 0) & PCI_HDRTYPE_MF_MASK) {
        /* multifunction device - check remaining functions */
        // kinfo("device %02x:%02x is multifunction", bus, dev);
        for(uint8_t func = 1; func < 8; func++) {
            pci_scan_func(bus, dev, func);
        }
    }
}

static void pci_scan_bus(uint8_t bus) {
    for(uint8_t dev = 0; dev < 32; dev++) pci_scan_dev(bus, dev);
}

void pci_init() {
    kinfo("installing PCI device tree root node");
    devtree_add_child(&devtree_root, &pci_devtree_root);

    /* scan PCI buses */
    if(!(pci_read_hdrtype(0, 0, 0) & PCI_HDRTYPE_MF_MASK)) {
        kinfo("single PCI host controller found");
        pci_scan_bus(0); // single PCI host controller
    }
    else {
        /* multiple PCI host controllers */
        kinfo("multiple PCI host controllers found");
        for(uint8_t func = 0; func < 8; func++) {
            if(pci_read_vid(0, 0, func) != 0xFFFF) break; // TODO: why?
            pci_scan_bus(func);
        }
    }
}

#endif
