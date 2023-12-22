#include <arch/x86cpu/apic.h>
#include <kernel/log.h>
#include <mm/vmm.h>
#include <mm/addr.h>
#include <stdlib.h>
#include <helpers/mmio.h>
#include <arch/x86/i8259.h>
#include <hal/intr.h>

#ifdef FEAT_ACPI

typedef struct {
    uint8_t type;
    uint8_t length; // total entry length
    union {
        #define MADT_ENTRY_CPU_LAPIC                    0
        struct {
            uint8_t cpu_id;
            uint8_t apic_id;
            uint32_t flags; // bit 0 = processor enabled, bit 1 = online capable. we shall check if either of these bits are on, meaning that the CPU can be enabled.
        } __attribute__((packed)) cpu_lapic; // processor local APIC (type 0)

        #define MADT_ENTRY_IOAPIC                       1
        struct {
            uint8_t apic_id;
            uint8_t reserved;
            uint32_t ioapic_base;
            uint32_t gsi_base;
        } __attribute__((packed)) ioapic; // I/O APIC (type 1)

        #define MADT_ENTRY_IOAPIC_SRC_OVERRIDE          2
        struct {
            uint8_t bus_src;
            uint8_t irq_src;
            uint32_t gsi;
            uint16_t flags;
        } __attribute__((packed)) ioapic_src_override; // I/O APIC interrupt source override (type 2)

        #define MADT_ENTRY_IOAPIC_NMI                   3
        struct {
            // uint8_t nmi_src;
            // uint8_t reserved;
            uint16_t flags;
            uint32_t gsi;
        } __attribute__((packed)) ioapic_nmi; // I/O APIC NMI source (type 3)

        #define MADT_ENTRY_LAPIC_NMI                    4
        struct {
            uint8_t cpu_id; // 0xFF = all CPUs
            uint16_t flags;
            uint8_t lint; // LINT# (0/1)
        } __attribute__((packed)) lapic_nmi; // local APIC NMI (type 4)

        #define MADT_ENTRY_LAPIC_BASE_OVERRIDE          5
        struct {
            uint16_t reserved;
            uint64_t base;
        } __attribute__((packed)) lapic_base_override; // local APIC base override (type 5)

        #define MADT_ENTRY_CPU_LAPIC_X2                 9
        struct {
            uint16_t reserved;
            uint32_t x2apic_id;
            uint32_t flags;
            uint32_t cpu_id;
        } __attribute__((packed)) cpu_lapic_x2; // local X2APIC (type 9)
    } data;
} __attribute__((packed)) acpi_madt_entry_t;

typedef struct {
    struct {
        char signature[4]; // APIC
        uint32_t length;
        uint8_t revision;
        uint8_t checksum;
        char oem_id[6];
        char oem_tabid[8];
        uint32_t oem_revision;
        uint32_t creator_id;
        uint32_t creator_revision;
    } __attribute__ ((packed)) header;

    uint32_t lapic_base; // LAPIC base physical address
    uint32_t flags; // bit 0 = PIC installed (though we should mask PIC interrupts anyway)

    acpi_madt_entry_t first_entry;
} __attribute__((packed)) acpi_madt_t;

uintptr_t lapic_base = 0; // LAPIC base (mapped)

static size_t ioapic_cnt = 0; // number of detected IOAPICs
typedef struct {
    // uint8_t apic_id; // might not be necessary
    uintptr_t base; // base address (mapped)
    uint8_t gsi_base; // starting GSI
    uint8_t inputs; // number of inputs
} ioapic_info_t;
static ioapic_info_t* ioapic_info = NULL; // table of IOAPICs

size_t apic_cpu_cnt = 0; // number of detected CPU cores
typedef struct {
    size_t cpu_id;
    uint8_t apic_id;
} apic_cpu_info_t;
apic_cpu_info_t* apic_cpu_info = NULL; // table of detected CPUs
size_t apic_bsp_idx = (size_t)-1; // bootstrap processor's index

uint8_t ioapic_irq_gsi[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}; // legacy IRQ to GSI mapping - we assume IRQs are identity mapped, then we'll go along and fill in any overrides
// static uint8_t ioapic_gsi_irq[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}; // GSI to legacy IRQ mapping

/* IOAPIC registers */
// MMIO registers
#define IOREGSEL                        0x00
#define IOWIN                           0x10
// indexed registers, accessible via IOREGSEL and IOWIN
#define IOAPICID                        0x00
#define IOAPICVER                       0x01
#define IOAPICARB                       0x02
#define IOREDTBL_BASE                   0x10
#define IOREDTBL_L(x)                   (IOREDTBL_BASE + ((x) << 1)) // bits 0-31
#define IOREDTBL_H(x)                   (IOREDTBL_BASE + ((x) << 1) + 1) // bits 32-63

static inline uint32_t ioapic_reg_read(size_t idx, uint8_t reg_idx) {
    mmio_outd(ioapic_info[idx].base + IOREGSEL, reg_idx);
    return mmio_ind(ioapic_info[idx].base + IOWIN);
}

static inline void ioapic_reg_write(size_t idx, uint8_t reg_idx, uint32_t val) {
    mmio_outd(ioapic_info[idx].base + IOREGSEL, reg_idx);
    mmio_outd(ioapic_info[idx].base + IOWIN, val);
}

/* LAPIC registers */
#define LAPIC_REG_ID                    0x020
#define LAPIC_REG_VERSION               0x030
#define LAPIC_REG_TPR                   0x080
#define LAPIC_REG_APR                   0x090
#define LAPIC_REG_PPR                   0x0A0
#define LAPIC_REG_EOI                   0x0B0
#define LAPIC_REG_RRD                   0x0C0
#define LAPIC_REG_LDR                   0x0D0
#define LAPIC_REG_DFR                   0x0E0
#define LAPIC_REG_SIVR                  0x0F0
#define LAPIC_REG_ISR                   0x100 // In-Service Register: 0x100-170
#define LAPIC_REG_TMR                   0x180 // Trigger Mode Register: 0x180-0x1F0
#define LAPIC_REG_IRR                   0x200 // Interrupt Request Register: 0x200-270
#define LAPIC_REG_ESR                   0x280
#define LAPIC_REG_LVT_CMCI              0x2F0
#define LAPIC_REG_ICR                   0x300 // Interrupt Command Register: 0x310-310
#define LAPIC_REG_ICR_PDEST             0x310
#define LAPIC_REG_LVT_TIMER             0x320
#define LAPIC_REG_LVT_TSENSE            0x330
#define LAPIC_REG_LVT_PFMON             0x340
#define LAPIC_REG_LVT_LINT0             0x350
#define LAPIC_REG_LVT_LINT1             0x360
#define LAPIC_REG_LVT_ERROR             0x370
#define LAPIC_REG_TMR_INITCNT           0x380
#define LAPIC_REG_TMR_CURRCNT           0x390
#define LAPIC_REG_TMR_DIV               0x3E0

static inline uint32_t lapic_reg_read(uint16_t reg_idx) {
    return mmio_ind(lapic_base + reg_idx);
}

static inline void lapic_reg_write(uint16_t reg_idx, uint32_t val) {
    mmio_outd(lapic_base + reg_idx, val);
}

/* LVT bitmasks */
#define APIC_LVT_BM_VECT                0xFF
#define APIC_LVT_BM_DELV_MODE           (1 << 8) | (1 << 9) | (1 << 10) // 0 = fixed, 1 = lowest priority, 2 = SMI, 4 = NMI, 5 = INIT, 7 = external interrupt
#define APIC_LVT_BM_DEST_MODE           (1 << 11) // 0 = physical destination, 1 = logical destination
#define APIC_LVT_BM_DELV_STAT           (1 << 12) // whether the IRQ has been serviced
#define APIC_LVT_BM_TRIG_POL            (1 << 13) // 0 = active high (ISA), 1 = active low
#define APIC_LVT_BM_RIRR                (1 << 14)
#define APIC_LVT_BM_TRIG_MODE           (1 << 15) // 0 = edge triggered (ISA), 1 = level triggered
#define APIC_LVT_BM_MASK                (1 << 16) // 1 = IRQ disabled, 0 = IRQ enabled

/* spurious interrupt handler */
#define LAPIC_SPURIOUS_VECT             0xFF // spurious interrupt vector
void apic_spurious_handler(uint8_t vector, void* context) {
    (void) vector; (void) context;
    kdebug("spurious APIC IRQ encountered");
}

/* IOAPIC interrupt handlers */
static void (**ioapic_handlers)(uint8_t gsi, void* context) = NULL;
#define IOAPIC_HANDLER_VECT_BASE        0x30
void ioapic_handler(uint8_t vector, void* context) {
    vector -= IOAPIC_HANDLER_VECT_BASE; // vector now holds the GSI
    // kdebug("IOAPIC interrupt GSI %u encountered", vector);

    if(ioapic_handlers[vector] != NULL) ioapic_handlers[vector](vector, context);
    else kdebug("unhandled GSI %u", vector);

    apic_eoi(); // send APIC EOI
}

void ioapic_legacy_handler(uint8_t gsi, void* context) {
    for(size_t i = 0; i < 16; i++) {
        if(ioapic_irq_gsi[i] == gsi && pic_handlers[i] != NULL) {
            pic_handlers[i](i, context);
            return;
        }
    }
    kdebug("unhandled GSI %u routed through legacy IRQ adapter", gsi);
}

void ioapic_handle(uint8_t gsi, void (*handler)(uint8_t gsi, void* context)) {
    ioapic_handlers[gsi] = handler;
}

bool ioapic_is_handled(uint8_t gsi) {
    return (ioapic_handlers[gsi] != NULL);
}

void ioapic_mask(uint8_t gsi) {
    for(size_t i = 0; i < ioapic_cnt; i++) {
        if(gsi >= ioapic_info[i].gsi_base && gsi <= ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
            uint8_t reg = IOREDTBL_L(gsi - ioapic_info[i].gsi_base);
            ioapic_reg_write(i, reg, ioapic_reg_read(i, reg) | APIC_LVT_BM_MASK);
            return;
        }
    }
    kdebug("invalid GSI %u", gsi);
}

void ioapic_unmask(uint8_t gsi) {
    for(size_t i = 0; i < ioapic_cnt; i++) {
        if(gsi >= ioapic_info[i].gsi_base && gsi <= ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
            uint8_t reg = IOREDTBL_L(gsi - ioapic_info[i].gsi_base);
            ioapic_reg_write(i, reg, ioapic_reg_read(i, reg) & ~APIC_LVT_BM_MASK);
            return;
        }
    }
    kdebug("invalid GSI %u", gsi);
}

bool ioapic_get_mask(uint8_t gsi) {
    for(size_t i = 0; i < ioapic_cnt; i++) {
        if(gsi >= ioapic_info[i].gsi_base && gsi <= ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
            return (ioapic_reg_read(i, IOREDTBL_L(gsi - ioapic_info[i].gsi_base)) & APIC_LVT_BM_MASK);
        }
    }
    kdebug("invalid GSI %u", gsi);
    return true;
}

void apic_eoi() {
    mmio_outd(lapic_base + LAPIC_REG_EOI, 0);
}

bool apic_enabled = false;

bool apic_init() {
    /* look for the MADT */    
#ifdef FEAT_ACPI_LAI
    acpi_madt_t* madt = laihost_scan("APIC", 0); // look for MADT
#endif
    if(madt == NULL) {
        kerror("cannot find MADT");
        return false;
    }

    uintptr_t lapic_base_paddr = madt->lapic_base;
    kdebug("MADT-indicated local APIC base address: 0x%x", madt->lapic_base);

    /* go through all the entries first to do some counting */
    acpi_madt_entry_t* entry = &madt->first_entry;
    while((uintptr_t) entry < (uintptr_t) madt + madt->header.length) {
        switch(entry->type) {
            case MADT_ENTRY_CPU_LAPIC:
                kdebug("MADT entry offset 0x%x: CPU LAPIC: CPU ID %u, APIC ID %u, flags 0x%x", (uintptr_t)entry - (uintptr_t)madt, entry->data.cpu_lapic.cpu_id, entry->data.cpu_lapic.apic_id, entry->data.cpu_lapic.flags);
                apic_cpu_cnt++;
                break;            
            case MADT_ENTRY_IOAPIC:
                kdebug("MADT entry offset 0x%x: I/O APIC: APIC ID %u, base physaddr 0x%x, GSI base %u", (uintptr_t)entry - (uintptr_t)madt, entry->data.ioapic.apic_id, entry->data.ioapic.ioapic_base, entry->data.ioapic.gsi_base);
                ioapic_cnt++;
                break;            
            case MADT_ENTRY_IOAPIC_SRC_OVERRIDE:
                kdebug("MADT entry offset 0x%x: interrupt source override: bus %u, IRQ %u, GSI %u, flags 0x%04x", (uintptr_t)entry - (uintptr_t)madt, entry->data.ioapic_src_override.bus_src, entry->data.ioapic_src_override.irq_src, entry->data.ioapic_src_override.gsi, entry->data.ioapic_src_override.flags);
                // ioapic_irq_gsi[entry->data.ioapic_src_override.irq_src] = entry->data.ioapic_src_override.gsi;
                break;
            case MADT_ENTRY_IOAPIC_NMI:
                kdebug("MADT entry offset 0x%x: I/O APIC NMI source: GSI %u, flags 0x%04x", (uintptr_t)entry - (uintptr_t)madt, entry->data.ioapic_nmi.gsi, entry->data.ioapic_nmi.flags);
                break;
            case MADT_ENTRY_LAPIC_NMI:
                kdebug("MADT entry offset 0x%x: LAPIC NMI source: CPU ID %u, LINT#%u, flags 0x%04x", (uintptr_t)entry - (uintptr_t)madt, entry->data.lapic_nmi.cpu_id, entry->data.lapic_nmi.lint, entry->data.lapic_nmi.flags);
                break;
            case MADT_ENTRY_LAPIC_BASE_OVERRIDE:
                kdebug("MADT entry offset 0x%x: 64-bit LAPIC base override: base 0x%llx", (uintptr_t)entry - (uintptr_t)madt, entry->data.lapic_base_override.base);
                lapic_base_paddr = entry->data.lapic_base_override.base;
                break;
            case MADT_ENTRY_CPU_LAPIC_X2: // TODO: implement x2APIC support?
                kdebug("MADT entry offset 0x%x: CPU x2LAPIC: CPU ID %u, x2APIC ID %u, flags 0x%x", (uintptr_t)entry - (uintptr_t)madt, entry->data.cpu_lapic_x2.cpu_id, entry->data.cpu_lapic_x2.x2apic_id, entry->data.cpu_lapic_x2.flags);
                break;
            default:
                kwarn("unknown MADT entry at offset 0x%x: type %u, length %u", (uintptr_t)entry - (uintptr_t)madt, entry->type, entry->length);
                break;
        }
        entry = (acpi_madt_entry_t*) ((uintptr_t) entry + entry->length);
    }

    /* map local APIC */
    lapic_base = vmm_alloc_map(vmm_kernel, lapic_base_paddr, 0x400, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_GLOBAL | VMM_FLAGS_RW);
    if(!lapic_base) {
        kerror("cannot map LAPIC base");
        return false;
    }
    kinfo("mapped LAPIC at 0x%x to 0x%x", lapic_base_paddr, lapic_base);

    /* allocate IOAPIC and CPU info tables */
    ioapic_info = kcalloc(ioapic_cnt, sizeof(ioapic_info_t));
    if(ioapic_info == NULL) {
        kerror("cannot allocate memory for IOAPIC information table");
        vmm_unmap(vmm_kernel, lapic_base, 0x400);
        return false;
    }
    apic_cpu_info = kcalloc(apic_cpu_cnt, sizeof(apic_cpu_info_t));
    if(apic_cpu_info == NULL) {
        kerror("cannot allocate memory for CPU information table");
        kfree(ioapic_info);
        vmm_unmap(vmm_kernel, lapic_base, 0x400);
        return false;
    }

    /* get bootstrap processor's APIC ID */
    uint8_t bsp_apic_id = lapic_reg_read(LAPIC_REG_ID) >> 24;
    kdebug("bootstrap processor APIC ID: %u", bsp_apic_id);

    /* store CPU and IOAPIC information */
    entry = &madt->first_entry;
    size_t ioapic_i = 0, cpu_i = 0;
    size_t max_gsis = 0;
    while((uintptr_t) entry < (uintptr_t) madt + madt->header.length) {
        switch(entry->type) {
            case MADT_ENTRY_CPU_LAPIC:
                apic_cpu_info[cpu_i].apic_id = entry->data.cpu_lapic.apic_id;
                apic_cpu_info[cpu_i].cpu_id = entry->data.cpu_lapic.cpu_id;
                if(apic_cpu_info[cpu_i].apic_id == bsp_apic_id) {
                    apic_bsp_idx = cpu_i;
                    kdebug("bootstrap processor information index: %u", apic_bsp_idx);
                }
                cpu_i++;
                break;            
            case MADT_ENTRY_IOAPIC:
                // ioapic_info[ioapic_i].apic_id = entry->data.ioapic.apic_id;
                ioapic_info[ioapic_i].gsi_base = entry->data.ioapic.gsi_base;
                ioapic_info[ioapic_i].base = vmm_alloc_map(vmm_kernel, entry->data.ioapic.ioapic_base, 0x20, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_GLOBAL | VMM_FLAGS_RW); // we only have 2 32-bit registers in memory: IOREGSEL and IOWIN
                if(!ioapic_info[ioapic_i].base) {
                    kerror("cannot allocate memory for interfacing IOAPIC %u (GSI base %u)", entry->data.ioapic.apic_id, entry->data.ioapic.gsi_base);
                    for(size_t i = 0; i < ioapic_i; i++) vmm_unmap(vmm_kernel, ioapic_info[i].base, 0x20);
                    kfree(ioapic_info); kfree(apic_cpu_info);
                    vmm_unmap(vmm_kernel, lapic_base, 0x400);
                    return false;
                }
                // kdebug("IOAPICVER = 0x%x", ioapic_reg_read(ioapic_i, IOAPICVER));
                ioapic_info[ioapic_i].inputs = ((ioapic_reg_read(ioapic_i, IOAPICVER) >> 16) & 0xFF) + 1;
                if(ioapic_info[ioapic_i].gsi_base + ioapic_info[ioapic_i].inputs > max_gsis) max_gsis = ioapic_info[ioapic_i].gsi_base + ioapic_info[ioapic_i].inputs;
                kdebug("IOAPIC #%u: base vaddr 0x%x, GSI base %u, %u input(s)", ioapic_i + 1, ioapic_info[ioapic_i].base, ioapic_info[ioapic_i].gsi_base, ioapic_info[ioapic_i].inputs);
                ioapic_i++;
                break;
            default: break;
        }
        entry = (acpi_madt_entry_t*) ((uintptr_t) entry + entry->length);
    }

    /* allocate memory for GSI handler pointers */
    ioapic_handlers = kcalloc(max_gsis, sizeof(void*));
    if(ioapic_handlers == NULL) {
        kerror("cannot allocate memory for IOAPIC GSI handler pointers");
        for(size_t i = 0; i < ioapic_cnt; i++) vmm_unmap(vmm_kernel, ioapic_info[i].base, 0x20);
        kfree(ioapic_info); kfree(apic_cpu_info);
        vmm_unmap(vmm_kernel, lapic_base, 0x400);
        return false;
    }

    /* set up IOAPIC interrupt handling */
    size_t i, j, vect; // we'll need these variables later
    for(i = 0; i < ioapic_cnt; i++) {
        /* iterate through each IOAPIC to set up their IOREDTBLs */
        for(j = 0; j < ioapic_info[i].inputs; j++) {
            vect = IOAPIC_HANDLER_VECT_BASE + ioapic_info[i].gsi_base + j; // interrupt vector
            intr_handle(vect, ioapic_handler); // set up interrupt handler
            ioapic_reg_write(i, IOREDTBL_L(j), vect | APIC_LVT_BM_MASK); // set up vector to trigger and mask the interrupt line for the time being (we'll unmask them later)
            ioapic_reg_write(i, IOREDTBL_H(j), bsp_apic_id << 24); // set target processor to BSP
        }
    }

    /* set up LAPIC/IOAPIC NMIs and IOAPIC source overrides */
    entry = &madt->first_entry;
    while((uintptr_t) entry < (uintptr_t) madt + madt->header.length) {
        switch(entry->type) {        
            case MADT_ENTRY_IOAPIC_SRC_OVERRIDE:
                ioapic_irq_gsi[entry->data.ioapic_src_override.irq_src] = entry->data.ioapic_src_override.gsi;
                // ioapic_gsi_irq[entry->data.ioapic_src_override.gsi]     = entry->data.ioapic_src_override.irq_src;
                for(i = 0; i < ioapic_cnt; i++) {
                    if(entry->data.ioapic_src_override.gsi >= ioapic_info[i].gsi_base && entry->data.ioapic_src_override.gsi <= ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
                        /* set up interrupt trigger */
                        ioapic_reg_write(i, IOREDTBL_L(entry->data.ioapic_src_override.gsi - ioapic_info[i].gsi_base), (IOAPIC_HANDLER_VECT_BASE + entry->data.ioapic_src_override.gsi) | APIC_LVT_BM_MASK | ((entry->data.ioapic_src_override.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.ioapic_src_override.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                        break;
                    }
                }
                break;
            case MADT_ENTRY_IOAPIC_NMI:
                for(i = 0; i < ioapic_cnt; i++) {
                    if(entry->data.ioapic_nmi.gsi >= ioapic_info[i].gsi_base && entry->data.ioapic_nmi.gsi <= ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
                        /* set up interrupt trigger and wire interrupt to NMI (interrupt 2) */
                        ioapic_reg_write(i, IOREDTBL_L(entry->data.ioapic_nmi.gsi - ioapic_info[i].gsi_base), 2 | ((entry->data.ioapic_nmi.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.ioapic_nmi.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                        break;
                    }
                }
                break;
            case MADT_ENTRY_LAPIC_NMI:
                for(i = 0; i < apic_cpu_cnt; i++) {
                    if(entry->data.lapic_nmi.cpu_id == 0xFF || apic_cpu_info[i].cpu_id == entry->data.lapic_nmi.cpu_id) // applies for current CPU or all CPUs
                        mmio_outd(lapic_base + ((entry->data.lapic_nmi.lint) ? LAPIC_REG_LVT_LINT1 : LAPIC_REG_LVT_LINT0), 2 | ((entry->data.lapic_nmi.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.lapic_nmi.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                }
                break;
            default: break;
        }
        entry = (acpi_madt_entry_t*) ((uintptr_t) entry + entry->length);
    }

    /* transparently adapt legacy PIC IRQs to APIC GSIs */
    uint16_t pic_irq_mask = pic_get_mask();
    for(i = 0; i < 16; i++) {
        if(pic_handlers[i] != NULL) {
            /* there's a PIC handler assigned - adapt this to APIC */
            ioapic_handle(ioapic_irq_gsi[i], ioapic_legacy_handler);
            if(pic_irq_mask & (1 << i)) ioapic_mask(ioapic_irq_gsi[i]); else ioapic_unmask(ioapic_irq_gsi[i]); // transfer masking status over
        }
    }

    /* enable local APIC */
    asm volatile("wrmsr" : : "c"(0x1B), "d"(0), "a"((lapic_base_paddr & ~0xFFF) | (1 << 11))); // write APIC base and enable bit to IA32_APIC_BASE MSR
    intr_handle(LAPIC_SPURIOUS_VECT, apic_spurious_handler);
    lapic_reg_write(LAPIC_REG_SIVR, LAPIC_SPURIOUS_VECT | (1 << 8)); // software enable LAPIC and set spurious interrupt vector
    pic_mask_bm(0xFFFF); // mask all PIC interrupts as we will be using the APIC
    pic_eoi(15); // send EOI to all PIC interrupt lines (TODO: is this needed?)
    apic_eoi(); // also send EOI to LAPIC just in case

    /* re-enable ACPI in IOAPIC mode */
#ifdef FEAT_ACPI_LAI
    lai_disable_acpi();
    lai_enable_acpi(1);
#endif

    apic_enabled = true;
    return true;
}

#endif
