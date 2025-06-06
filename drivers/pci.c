#ifdef FEAT_PCI

#include <drivers/pci.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <drivers/acpi.h>

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
    &pci_devtree_geo_root,
    NULL,
    {1}
};

devtree_t pci_devtree_geo_root = {
    sizeof(devtree_t),
    "by-geo",
    DEVTREE_NODE_BUS,
    &pci_devtree_root,
    NULL,
    &pci_devtree_id_root,
    {1}
};

devtree_t pci_devtree_id_root = {
    sizeof(devtree_t),
    "by-id",
    DEVTREE_NODE_BUS,
    &pci_devtree_root,
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

    kinfo("device %02x:%02x.%x: ID %04x:%04x, class %02x:%02x", bus, dev, func, vid_pid[0], vid_pid[1], class_sub[1], class_sub[0]);

    pci_devtree_t* node_geo = kcalloc(1, sizeof(pci_devtree_t));
    if(node_geo == NULL) kwarn("cannot allocate memory for PCI device node");
    else {
        node_geo->header.size = sizeof(pci_devtree_t);
        ksprintf(node_geo->header.name, "%02x:%02x.%x", bus, dev, func);
        node_geo->header.type = DEVTREE_NODE_DEVICE;
        node_geo->vid = vid_pid[0]; node_geo->pid = vid_pid[1];
        node_geo->class = class_sub[1]; node_geo->subclass = class_sub[0];
        node_geo->bus = bus; node_geo->dev = dev; node_geo->func = func;
        devtree_add_child(&pci_devtree_geo_root, (devtree_t*) node_geo);
        kdebug("created geo devtree node at 0x%x for device %s", node_geo, node_geo->header.name);
    }
    pci_devtree_t* node_id = kcalloc(1, sizeof(pci_devtree_t));
    if(node_id == NULL) kwarn("cannot allocate memory for PCI device node");
    else {
        node_id->header.size = sizeof(pci_devtree_t);
        ksprintf(node_id->header.name, "%04x:%04x", vid_pid[0], vid_pid[1]);
        node_id->header.type = DEVTREE_NODE_DEVICE;
        node_id->vid = vid_pid[0]; node_id->pid = vid_pid[1];
        node_id->class = class_sub[1]; node_id->subclass = class_sub[0];
        node_id->bus = bus; node_id->dev = dev; node_id->func = func;
        devtree_add_child(&pci_devtree_id_root, (devtree_t*) node_id);
        kdebug("created ID devtree node at 0x%x for device %s", node_id, node_id->header.name);
    }

    if(class_sub[1] == PCI_CLASS_BRIDGE && (class_sub[0] == PCI_BRG_PCI_BRIDGE || class_sub[0] == PCI_BRG_PCI_BRIDGE_ALT)) {
        /* scan devices behind the bridge - TODO: set up secondary buses */
        uint8_t bus_sec = pci_cfg_read_byte(bus, dev, func, PCI_CFG_H1_SEC_BUS);
        kdebug("device %02x:%02x.%x (%04x:%04x) is a PCI-to-PCI bridge - scanning secondary bus %02x", bus, dev, func, vid_pid[0], vid_pid[1], bus_sec);
        pci_scan_bus(bus_sec);
    } else {
        /* print pre-routing information for debugging */
        size_t irq = pci_arch_get_irq_routing(bus, dev, func, (uint8_t)-1, NULL);
        if(irq != (size_t)-1) kdebug("device %02x:%02x.%x has its interrupt pin pre-routed to IRQ %u", bus, dev, func, irq);
        else kdebug("device %02x:%02x.%x does not have its interrupt pin pre-routed", bus, dev, func);
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

size_t pci_route_irq(uint8_t bus, uint8_t dev, uint8_t func, uint8_t pin, uint8_t* flags) {
    size_t ret = pci_arch_get_irq_routing(bus, dev, func, pin, flags); // do not route if it's already done by the BIOS

#ifdef FEAT_ACPI
    if(ret == (size_t)-1 && acpi_enabled) ret = acpi_pci_route_irq(bus, dev, func, pin, flags); // try using ACPI first
#endif

    if(ret == (size_t)-1) ret = pci_arch_route_irq(bus, dev, func, pin, flags);

    return ret;
}

#endif
