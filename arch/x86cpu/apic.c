#include <arch/x86cpu/apic.h>
#include <kernel/log.h>
#include <mm/vmm.h>
#include <mm/addr.h>
#include <stdlib.h>
#include <helpers/mmio.h>
#include <arch/x86/i8259.h>
#include <hal/intr.h>
#include <drivers/acpi.h>
#include <arch/x86/mptab.h>
#include <kernel/cmdline.h>
#include <hal/timer.h>

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
            uint8_t ioapic_id;
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

size_t ioapic_cnt = 0; // number of detected IOAPICs
ioapic_info_t* ioapic_info = NULL; // table of IOAPICs

size_t apic_cpu_cnt = 0; // number of detected CPU cores
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
#define APIC_LVT_BM_DELV_MODE           (1 << 8) | (1 << 9) | (1 << 10) // 0 = fixed, 1 = lowest priority, 2 = SMI, 4 = NMI, 5 = INIT, 7 = external interrupt - does not apply to timer LVT
#define APIC_LVT_BM_DEST_MODE           (1 << 11) // 0 = physical destination, 1 = logical destination - does not apply to timer LVT
#define APIC_LVT_BM_DELV_STAT           (1 << 12) // whether the IRQ has been serviced
#define APIC_LVT_BM_TRIG_POL            (1 << 13) // 0 = active high (ISA), 1 = active low - does not apply to timer LVT
#define APIC_LVT_BM_RIRR                (1 << 14) // does not apply to timer LVT
#define APIC_LVT_BM_TRIG_MODE           (1 << 15) // 0 = edge triggered (ISA), 1 = level triggered - does not apply to timer LVT
#define APIC_LVT_BM_MASK                (1 << 16) // 1 = IRQ disabled, 0 = IRQ enabled
#define APIC_LVT_BM_TMR_MODE            (1 << 17) // 0 = one-shot, 1 = periodic - only applies to timer LVT

/* spurious interrupt handler */
#define LAPIC_SPURIOUS_VECT             0xFF // spurious interrupt vector
void apic_spurious_handler(uint8_t vector, void* context) {
    (void) vector; (void) context;
    kdebug("spurious APIC IRQ encountered");
}

/* IOAPIC interrupt handlers */
static void (**ioapic_handlers)(uint8_t gsi, void* context) = NULL;
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
uint8_t lapic_handler_vect_base = IOAPIC_HANDLER_VECT_BASE;

bool apic_init() {
    /* APIC descriptor structure(s) */
    acpi_madt_t* madt = NULL; // MADT (ACPI)

    uintptr_t lapic_base_paddr = 0xFEE00000; // default configuration

    /* look for the MADT */
#ifdef FEAT_ACPI
    if(acpi_enabled) {
#ifdef FEAT_ACPI_LAI
        madt = laihost_scan("APIC", 0); // look for MADT
#endif
        if(madt == NULL) kerror("MADT not found");
        else lapic_base_paddr = madt->lapic_base;
    }
#endif

    if(madt == NULL && mp_fptr == NULL) {
        kerror("no methods to gather information on LAPIC and IOAPIC");
        return false;
    }

    /* go through all the entries first to do some counting */
    if(madt != NULL) {
        /* using MADT */
        acpi_madt_entry_t* entry = &madt->first_entry;
        while((uintptr_t) entry < (uintptr_t) madt + madt->header.length) {
            switch(entry->type) {
                case MADT_ENTRY_CPU_LAPIC:
                    kdebug("MADT entry offset 0x%x: CPU: CPU ID %u, APIC ID %u, flags 0x%x", (uintptr_t)entry - (uintptr_t)madt, entry->data.cpu_lapic.cpu_id, entry->data.cpu_lapic.apic_id, entry->data.cpu_lapic.flags);
                    if(entry->data.cpu_lapic.flags & ((1 << 0) | (1 << 1))) apic_cpu_cnt++; // only count usable ones
                    break;            
                case MADT_ENTRY_IOAPIC:
                    kdebug("MADT entry offset 0x%x: I/O APIC: ID %u, base physaddr 0x%x, GSI base %u", (uintptr_t)entry - (uintptr_t)madt, entry->data.ioapic.ioapic_id, entry->data.ioapic.ioapic_base, entry->data.ioapic.gsi_base);
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
    } else if(mp_cfg != NULL) {
        /* using specified MP configuration table */
        mp_cfg_entry_t* entry = &mp_cfg->first_entry;
        for(size_t n = 0; n < mp_cfg->base_cnt; n++, entry = (mp_cfg_entry_t*) ((uintptr_t) entry + ((entry->type) ? 8 : 20))) {
            switch(entry->type) {
                case MP_ETYPE_CPU:
                    kdebug("MP configuration entry %u: CPU: LAPIC ID %u, LAPIC version %u, family %u, model %u, stepping %u, CPU flags 0x%x, feature flags 0x%x", n, entry->data.cpu.apic_id, entry->data.cpu.lapic_ver, entry->data.cpu.family, entry->data.cpu.model, entry->data.cpu.stepping, entry->data.cpu.flags, entry->data.cpu.features);
                    if(entry->data.cpu.flags & (1 << 0)) apic_cpu_cnt++;
                    break;
                case MP_ETYPE_BUS:
                    kdebug("MP configuration entry %u: bus: ID %u, name %.6s", n, entry->data.bus.bus_id, entry->data.bus.type_str);
                    break;
                case MP_ETYPE_IOAPIC:
                    kdebug("MP configuration entry %u: I/O APIC: ID %u, IOAPIC version %u, base physaddr 0x%x, flags 0x%x", n, entry->data.ioapic.apic_id, entry->data.ioapic.ioapic_ver, entry->data.ioapic.base, entry->data.ioapic.flags);
                    ioapic_cnt++;
                    break;
                case MP_ETYPE_IRQ_ASSIGN:
                    kdebug("MP configuration entry %u: I/O interrupt: type %u, flags 0x%x, bus %u, IRQ %u, dest. IOAPIC %u, dest. INTIN#%u", n, entry->data.irq_assign.type, entry->data.irq_assign.flags, entry->data.irq_assign.bus_src, entry->data.irq_assign.irq_src, entry->data.irq_assign.id_dest, entry->data.irq_assign.intin_dest);
                    break;
                case MP_ETYPE_LINT_ASSIGN:
                    kdebug("MP configuration entry %u: local interrupt: type %u, flags 0x%x, bus %u, IRQ %u, dest. LAPIC %u, dest. LINT#%u", n, entry->data.lint_assign.type, entry->data.lint_assign.flags, entry->data.lint_assign.bus_src, entry->data.lint_assign.irq_src, entry->data.lint_assign.id_dest, entry->data.lint_assign.lint_dest);
                    break;
                default:
                    kwarn("unknown MP configuration entry %u: type %u", n, entry->type);
                    n = mp_cfg->base_cnt; // exit immediately
                    break;
            }
        }
    } else {
        /* using default configuration */
        apic_cpu_cnt = 2;
        ioapic_cnt = 1;
    }

    /* map local APIC */
    kdebug("local APIC base physical address: 0x%x", lapic_base_paddr);
    lapic_base = vmm_alloc_map(vmm_kernel, lapic_base_paddr, 0x400, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_GLOBAL | VMM_FLAGS_RW);
    if(!lapic_base) {
        kerror("cannot map LAPIC base");
        return false;
    }
    kinfo("mapped LAPIC at 0x%x to 0x%x", lapic_base_paddr, lapic_base);

    /* allocate IOAPIC and CPU info tables */
    ioapic_info = kcalloc(ioapic_cnt, sizeof(ioapic_info_t));
    if(ioapic_cnt && ioapic_info == NULL) {
        kerror("cannot allocate memory for IOAPIC information table");
        vmm_unmap(vmm_kernel, lapic_base, 0x400);
        return false;
    }
    apic_cpu_info = kcalloc(apic_cpu_cnt, sizeof(apic_cpu_info_t));
    if(apic_cpu_cnt && apic_cpu_info == NULL) {
        kerror("cannot allocate memory for CPU information table");
        kfree(ioapic_info);
        vmm_unmap(vmm_kernel, lapic_base, 0x400);
        return false;
    }

    /* get bootstrap processor's APIC ID */
    uint8_t bsp_apic_id = lapic_reg_read(LAPIC_REG_ID) >> 24;
    kdebug("bootstrap processor APIC ID: %u", bsp_apic_id);

    /* store CPU and IOAPIC information */
    size_t max_gsis = 0;
    size_t ioapic_i = 0, cpu_i = 0;
    if(madt != NULL) {
        acpi_madt_entry_t* entry = &madt->first_entry;
        while((uintptr_t) entry < (uintptr_t) madt + madt->header.length) {
            switch(entry->type) {
                case MADT_ENTRY_CPU_LAPIC:
                    if(!(entry->data.cpu_lapic.flags & ((1 << 0) | (1 << 1)))) break; // skip this CPU
                    apic_cpu_info[cpu_i].apic_id = entry->data.cpu_lapic.apic_id;
                    apic_cpu_info[cpu_i].cpu_id = entry->data.cpu_lapic.cpu_id;
                    if(apic_cpu_info[cpu_i].apic_id == bsp_apic_id) {
                        apic_bsp_idx = cpu_i;
                        kdebug("bootstrap processor information index: %u", apic_bsp_idx);
                    }
                    cpu_i++;
                    break;            
                case MADT_ENTRY_IOAPIC:
                    ioapic_info[ioapic_i].id = entry->data.ioapic.ioapic_id;
                    ioapic_info[ioapic_i].gsi_base = entry->data.ioapic.gsi_base;
                    ioapic_info[ioapic_i].base = vmm_alloc_map(vmm_kernel, entry->data.ioapic.ioapic_base, 0x20, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_GLOBAL | VMM_FLAGS_RW); // we only have 2 32-bit registers in memory: IOREGSEL and IOWIN
                    if(!ioapic_info[ioapic_i].base) {
                        kerror("cannot allocate memory for interfacing IOAPIC %u (GSI base %u)", entry->data.ioapic.ioapic_id, entry->data.ioapic.gsi_base);
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
    } else if(mp_cfg != NULL) {
        mp_cfg_entry_t* entry = &mp_cfg->first_entry;
        for(size_t n = 0; n < mp_cfg->base_cnt; n++, entry = (mp_cfg_entry_t*) ((uintptr_t) entry + ((entry->type) ? 8 : 20))) {
            switch(entry->type) {
                case MP_ETYPE_CPU:
                    if(!(entry->data.cpu.flags & (1 << 0))) break; // skip this CPU
                    apic_cpu_info[cpu_i].apic_id = entry->data.cpu.apic_id;
                    apic_cpu_info[cpu_i].cpu_id = cpu_i; // TODO
                    if(apic_cpu_info[cpu_i].apic_id == bsp_apic_id) {
                        apic_bsp_idx = cpu_i;
                        kdebug("bootstrap processor information index: %u", apic_bsp_idx);
                    }
                    cpu_i++;
                    break;
                case MP_ETYPE_IOAPIC:
                    ioapic_info[ioapic_i].id = entry->data.ioapic.apic_id;
                    ioapic_info[ioapic_i].base = vmm_alloc_map(vmm_kernel, entry->data.ioapic.base, 0x20, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_GLOBAL | VMM_FLAGS_RW); // we only have 2 32-bit registers in memory: IOREGSEL and IOWIN
                    if(!ioapic_info[ioapic_i].base) {
                        kerror("cannot allocate memory for interfacing IOAPIC %u", entry->data.ioapic.apic_id);
                        for(size_t i = 0; i < ioapic_i; i++) vmm_unmap(vmm_kernel, ioapic_info[i].base, 0x20);
                        kfree(ioapic_info); kfree(apic_cpu_info);
                        vmm_unmap(vmm_kernel, lapic_base, 0x400);
                        return false;
                    }
                    // kdebug("IOAPICVER = 0x%x", ioapic_reg_read(ioapic_i, IOAPICVER));
                    ioapic_info[ioapic_i].inputs = ((ioapic_reg_read(ioapic_i, IOAPICVER) >> 16) & 0xFF) + 1;
                    if(ioapic_info[ioapic_i].gsi_base + ioapic_info[ioapic_i].inputs > max_gsis) max_gsis = ioapic_info[ioapic_i].gsi_base + ioapic_info[ioapic_i].inputs;
                    kdebug("IOAPIC #%u: base vaddr 0x%x, %u input(s)", ioapic_i + 1, ioapic_info[ioapic_i].base, ioapic_info[ioapic_i].inputs);
                    ioapic_i++;
                    break;
                default: break;
            }
        }

        /* figure out the GSI base if there are more than 1 IOAPICs */
        for(size_t i = 1; i < ioapic_cnt; i++) ioapic_info[i].gsi_base = ioapic_info[i - 1].gsi_base + ioapic_info[i - 1].inputs; // TODO: this assumes IOAPICs are sequentially presented
    } else {
        apic_cpu_info[0].apic_id = 0; apic_cpu_info[0].cpu_id = 0;
        apic_cpu_info[1].apic_id = 1; apic_cpu_info[1].cpu_id = 1;
        ioapic_info->id = 0;
        ioapic_info->base = vmm_alloc_map(vmm_kernel, 0xFEC00000, 0x20, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_GLOBAL | VMM_FLAGS_RW); // we only have 2 32-bit registers in memory: IOREGSEL and IOWIN
        if(!ioapic_info[ioapic_i].base) {
            kerror("cannot allocate memory for interfacing IOAPIC");
            kfree(ioapic_info); kfree(apic_cpu_info);
            vmm_unmap(vmm_kernel, lapic_base, 0x400);
            return false;
        }
        ioapic_info->inputs = ((ioapic_reg_read(ioapic_i, IOAPICVER) >> 16) & 0xFF) + 1; max_gsis = ioapic_info->inputs;
        kdebug("IOAPIC: base vaddr 0x%x, %u input(s)", ioapic_info->base, ioapic_info->inputs);
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
    lapic_handler_vect_base = IOAPIC_HANDLER_VECT_BASE + max_gsis;

    /* set up LAPIC interrupt handling - stabilize all LVTs that might have been uninitialized */
    mmio_outd(lapic_base + LAPIC_REG_LVT_CMCI, 0xFF | APIC_LVT_BM_MASK);
    mmio_outd(lapic_base + LAPIC_REG_LVT_TIMER, 0xFF | APIC_LVT_BM_MASK);
    mmio_outd(lapic_base + LAPIC_REG_LVT_TSENSE, 0xFF | APIC_LVT_BM_MASK);
    mmio_outd(lapic_base + LAPIC_REG_LVT_PFMON, 0xFF | APIC_LVT_BM_MASK);
    mmio_outd(lapic_base + LAPIC_REG_LVT_LINT0, 0xFF | APIC_LVT_BM_MASK);
    mmio_outd(lapic_base + LAPIC_REG_LVT_LINT1, 0xFF | APIC_LVT_BM_MASK);
    mmio_outd(lapic_base + LAPIC_REG_LVT_ERROR, 0xFF | APIC_LVT_BM_MASK);

    /* set up IOAPIC interrupt handling */
    for(size_t i = 0; i < ioapic_cnt; i++) {
        /* iterate through each IOAPIC to set up their IOREDTBLs */
        for(size_t j = 0; j < ioapic_info[i].inputs; j++) {
            uint8_t vect = IOAPIC_HANDLER_VECT_BASE + ioapic_info[i].gsi_base + j; // interrupt vector
            intr_handle(vect, ioapic_handler); // set up interrupt handler
            ioapic_reg_write(i, IOREDTBL_L(j), vect | APIC_LVT_BM_MASK); // set up vector to trigger and mask the interrupt line for the time being (we'll unmask them later)
            ioapic_reg_write(i, IOREDTBL_H(j), bsp_apic_id << 24); // set target processor to BSP
        }
    }

    /* set up LAPIC/IOAPIC NMIs and IOAPIC source overrides */
    if(madt != NULL) {
        acpi_madt_entry_t* entry = &madt->first_entry;
        size_t i;
        while((uintptr_t) entry < (uintptr_t) madt + madt->header.length) {
            switch(entry->type) {        
                case MADT_ENTRY_IOAPIC_SRC_OVERRIDE:
                    ioapic_irq_gsi[entry->data.ioapic_src_override.irq_src] = entry->data.ioapic_src_override.gsi;
                    // ioapic_gsi_irq[entry->data.ioapic_src_override.gsi]     = entry->data.ioapic_src_override.irq_src;
                    for(i = 0; i < ioapic_cnt; i++) {
                        if(entry->data.ioapic_src_override.gsi >= ioapic_info[i].gsi_base && entry->data.ioapic_src_override.gsi < ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
                            /* set up interrupt trigger */
                            ioapic_reg_write(i, IOREDTBL_L(entry->data.ioapic_src_override.gsi - ioapic_info[i].gsi_base), (IOAPIC_HANDLER_VECT_BASE + entry->data.ioapic_src_override.gsi) | APIC_LVT_BM_MASK | ((entry->data.ioapic_src_override.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.ioapic_src_override.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                            break;
                        }
                    }
                    break;
                case MADT_ENTRY_IOAPIC_NMI:
                    for(i = 0; i < ioapic_cnt; i++) {
                        if(entry->data.ioapic_nmi.gsi >= ioapic_info[i].gsi_base && entry->data.ioapic_nmi.gsi < ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
                            /* set up interrupt trigger and wire interrupt to NMI (interrupt 2) */
                            ioapic_reg_write(i, IOREDTBL_L(entry->data.ioapic_nmi.gsi - ioapic_info[i].gsi_base), 2 | (1 << 10) | ((entry->data.ioapic_nmi.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.ioapic_nmi.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                            break;
                        }
                    }
                    break;
                case MADT_ENTRY_LAPIC_NMI:
                    for(i = 0; i < apic_cpu_cnt; i++) {
                        if(entry->data.lapic_nmi.cpu_id == 0xFF || apic_cpu_info[i].cpu_id == entry->data.lapic_nmi.cpu_id) // applies for current CPU or all CPUs
                            mmio_outd(lapic_base + ((entry->data.lapic_nmi.lint) ? LAPIC_REG_LVT_LINT1 : LAPIC_REG_LVT_LINT0), 2 | (1 << 10) | ((entry->data.lapic_nmi.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.lapic_nmi.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                    }
                    break;
                default: break;
            }
            entry = (acpi_madt_entry_t*) ((uintptr_t) entry + entry->length);
        }
    } else if(mp_cfg != NULL) {
        mp_cfg_entry_t* entry = &mp_cfg->first_entry;
        size_t i;
        for(size_t n = 0; n < mp_cfg->base_cnt; n++, entry = (mp_cfg_entry_t*) ((uintptr_t) entry + ((entry->type) ? 8 : 20))) {
            switch(entry->type) {
                case MP_ETYPE_IRQ_ASSIGN:
                    if(mp_busid_cnt <= entry->data.irq_assign.bus_src || mp_busid[entry->data.irq_assign.bus_src] == MP_BUS_NONE || mp_busid[entry->data.irq_assign.bus_src] == MP_BUS_ISA || mp_busid[entry->data.irq_assign.bus_src] == MP_BUS_EISA) // bus is ISA or EISA (or we aren't sure what bus this is)
                        ioapic_irq_gsi[entry->data.irq_assign.irq_src] = ioapic_info[entry->data.irq_assign.id_dest].gsi_base + entry->data.irq_assign.intin_dest;
                    if(entry->data.irq_assign.type == 1) // NMI
                        ioapic_reg_write(entry->data.irq_assign.id_dest, IOREDTBL_L(entry->data.irq_assign.intin_dest), 2 | (1 << 10) | ((entry->data.irq_assign.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.irq_assign.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                    else if(entry->data.irq_assign.type == 3) // external interrupt (8259)
                        ioapic_reg_write(entry->data.irq_assign.id_dest, IOREDTBL_L(entry->data.irq_assign.intin_dest), APIC_LVT_BM_VECT | APIC_LVT_BM_DELV_MODE | ((entry->data.irq_assign.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.irq_assign.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                    else // any other interrupt
                        ioapic_reg_write(entry->data.irq_assign.id_dest, IOREDTBL_L(entry->data.irq_assign.intin_dest), (IOAPIC_HANDLER_VECT_BASE + ioapic_irq_gsi[entry->data.irq_assign.irq_src]) | APIC_LVT_BM_MASK | ((entry->data.irq_assign.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.irq_assign.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                    break;
                case MP_ETYPE_LINT_ASSIGN:
                    for(i = 0; i < apic_cpu_cnt; i++) {
                        if(entry->data.lint_assign.id_dest == 0xFF || apic_cpu_info[i].cpu_id == entry->data.lint_assign.id_dest) { // applies for current CPU or all CPUs
                            if(entry->data.lint_assign.type == 1) // NMI
                                mmio_outd(lapic_base + ((entry->data.lint_assign.lint_dest) ? LAPIC_REG_LVT_LINT1 : LAPIC_REG_LVT_LINT0), 2 | (1 << 10) | ((entry->data.lint_assign.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.lint_assign.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                            else if(entry->data.lint_assign.type == 3) // external interrupt (8259)
                                mmio_outd(lapic_base + ((entry->data.lint_assign.lint_dest) ? LAPIC_REG_LVT_LINT1 : LAPIC_REG_LVT_LINT0), APIC_LVT_BM_VECT | APIC_LVT_BM_DELV_MODE | ((entry->data.lint_assign.flags & (1 << 1)) ? APIC_LVT_BM_TRIG_POL : 0) | ((entry->data.lint_assign.flags & (1 << 3)) ? APIC_LVT_BM_TRIG_MODE : 0));
                            // TODO: handle other cases
                        }
                    }
                    break;
                default: break;
            }
        }
    } else {
        bool invert = (mp_fptr->default_cfg == 4 || mp_fptr->default_cfg == 7); // inverters before INTIN1-15

        /* program IOAPIC */
        ioapic_reg_write(0, IOREDTBL_L(0), APIC_LVT_BM_VECT | APIC_LVT_BM_DELV_MODE); // INTIN0 -> 8259A INTR
#define ioapic_defcfg_setup(intin, irql) ioapic_reg_write(0, IOREDTBL_L((intin)), (IOAPIC_HANDLER_VECT_BASE + (irql)) | APIC_LVT_BM_MASK | ((invert) ? APIC_LVT_BM_TRIG_POL : 0)) // macro for setting up each INTIN -> IRQ mapping
        ioapic_defcfg_setup(1, 1);
        ioapic_defcfg_setup(2, 0); // NOTE: config 2 leaves INTIN2 unconnected
        for(size_t i = 3; i < 16; i++) ioapic_defcfg_setup(i, i); // NOTE: config 2 leaves INTIN13 unconnected

        /* program LAPIC */
        mmio_outd(lapic_base + LAPIC_REG_LVT_LINT0, APIC_LVT_BM_VECT | APIC_LVT_BM_DELV_MODE); // LINT0 -> 8259A INTR
        mmio_outd(lapic_base + LAPIC_REG_LVT_LINT1, 2 | (1 << 10)); // LINT1 -> NMI
    }

    /* transparently adapt legacy PIC IRQs to APIC GSIs */
    uint16_t pic_irq_mask = pic_get_mask();
    for(size_t i = 0; i < 16; i++) {
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

#ifdef FEAT_ACPI
    if(acpi_enabled) {
        /* re-enable ACPI in IOAPIC mode */
#ifdef FEAT_ACPI_LAI
        lai_disable_acpi();
        lai_enable_acpi(1);
#endif
    }
#endif

    apic_enabled = true;
    return true;
}

void ioapic_set_trigger(uint8_t gsi, bool edge, bool active_low) {
    for(size_t i = 0; i < ioapic_cnt; i++) {
        if(gsi >= ioapic_info[i].gsi_base && gsi < ioapic_info[i].gsi_base + ioapic_info[i].inputs) {
            /* set up interrupt trigger and wire interrupt to NMI (interrupt 2) */
            uint8_t reg = IOREDTBL_L(gsi - ioapic_info[i].gsi_base);
            ioapic_reg_write(i, reg, (ioapic_reg_read(i, reg) & ~(APIC_LVT_BM_TRIG_MODE | APIC_LVT_BM_TRIG_POL)) | ((edge) ? 0 : APIC_LVT_BM_TRIG_MODE) | ((active_low) ? APIC_LVT_BM_TRIG_POL : 0));
            break;
        }
    }
}

static uint32_t apic_timer_initcnt = ~0;
static size_t apic_timer_delta = 0;

#define APIC_TIMER_CALIBRATE_DURATION               10000UL // minimum duration to wait between sampling timer counts

/* accuracy optimization */
#define APIC_TIMER_RATE_ACCOPT // set this flag to optimize accuracy by firing at lower frequency such that rounding errors are minimized
#define APIC_TIMER_RATE_ACCOPT_DMIN                 75 // minimum delta (uS)
#define APIC_TIMER_RATE_ACCOPT_DMAX                 150 // maximum delta (uS)

static void apic_timer_handler(uint8_t vector, void* context) {
    (void) vector;
    timer_handler(apic_timer_delta, context);
    apic_eoi();
}

void apic_timer_calibrate() {
    if(!apic_enabled) {
        kerror("APIC has not been initialized yet");
        return;
    }

    uint32_t lvt_orig = mmio_ind(lapic_base + LAPIC_REG_LVT_TIMER); // original LVT
    mmio_outd(lapic_base + LAPIC_REG_TMR_DIV, APIC_TIMER_DIVISOR); // set divisor
    mmio_outd(lapic_base + LAPIC_REG_LVT_TIMER, 0xFF); // enable APIC timer in one-shot mode

    timer_tick_t t_start = timer_tick;
    while(timer_tick == t_start); // wait until we get to the start of a new tick

    mmio_outd(lapic_base + LAPIC_REG_TMR_INITCNT, ~0); // start timer by setting the initial count to 0xFFFFFFFF
    t_start = timer_tick; // starting timestamp
    timer_delay_us(APIC_TIMER_CALIBRATE_DURATION);
    mmio_outd(lapic_base + LAPIC_REG_LVT_TIMER, 0xFF | APIC_LVT_BM_MASK); // disable APIC timer
    timer_tick_t t_stop = timer_tick; // stopping timestamp
    uint32_t cnt = ~0 - mmio_ind(lapic_base + LAPIC_REG_TMR_CURRCNT); // get number of APIC ticks that have elapsed

    double rate = (double)cnt / (double)(t_stop - t_start); // APIC firing rate
    kdebug("APIC timer calibration (DCR=%u): %u ticks in %u uS -> %.8f APIC ticks per uS", APIC_TIMER_DIVISOR, cnt, (size_t)(t_stop - t_start), rate);
    
#ifdef APIC_TIMER_RATE_ACCOPT
    double error_min = 1;
    apic_timer_delta = APIC_TIMER_RATE_ACCOPT_DMIN;
    for(size_t i = APIC_TIMER_RATE_ACCOPT_DMIN; i <= APIC_TIMER_RATE_ACCOPT_DMAX; i++) {
        double rate_tmp = rate * i;
        double error = rate_tmp - (uint32_t)rate_tmp; if(error >= 0.5) error = 1.0 - error; // round up/down error
        if(error_min > error) {
            apic_timer_delta = i;
            apic_timer_initcnt = (uint32_t)rate_tmp;
            if(rate_tmp - (double)apic_timer_initcnt >= 0.5) apic_timer_initcnt++;
            error_min = error;
            if(i == APIC_TIMER_RATE_ACCOPT_DMIN) kdebug("APIC timer rate accuracy optimization: initial error (at %u uS): %.8f ticks per uS", APIC_TIMER_RATE_ACCOPT_DMIN, error);
        }
    }
    kdebug("APIC timer rate accuracy optimization: minimum error: %.8f ticks per uS", error_min);
#else
    /* use calculated rate rounded up/down for 1uS ticks */
    apic_timer_initcnt = (uint32_t)rate;
    if(rate - (double)apic_timer_initcnt >= 0.5) apic_timer_initcnt++;
    apic_timer_delta = 1;
#endif
    kdebug("APIC timer calibration: initial count 0x%08X, delta %u uS", apic_timer_initcnt, apic_timer_delta);

    mmio_outd(lapic_base + LAPIC_REG_LVT_TIMER, lvt_orig); // restore APIC timer LVT
}

void apic_timer_enable() {
    if(!apic_enabled) {
        kerror("APIC has not been initialized yet");
        return;
    }

    if(!apic_timer_delta) {
        kerror("APIC timer has not been calibrated yet");
        return;
    }

    kdebug("assigning APIC timer to interrupt vector 0x%02X", APIC_TIMER_VECT);

    intr_handle(APIC_TIMER_VECT, apic_timer_handler);
    mmio_outd(lapic_base + LAPIC_REG_TMR_DIV, APIC_TIMER_DIVISOR);
    mmio_outd(lapic_base + LAPIC_REG_LVT_TIMER, APIC_TIMER_VECT | APIC_LVT_BM_TMR_MODE); // periodic mode
    mmio_outd(lapic_base + LAPIC_REG_TMR_INITCNT, apic_timer_initcnt);
}

void apic_timer_disable() {
    if(!apic_enabled) {
        kerror("APIC has not been initialized yet");
        return;
    }

    mmio_outd(lapic_base + LAPIC_REG_LVT_TIMER, 0xFF | APIC_LVT_BM_MASK);
}
