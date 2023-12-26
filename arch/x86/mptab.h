#ifndef ARCH_X86_MPTAB_H
#define ARCH_X86_MPTAB_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* MP floating pointer structure */
typedef struct {
    char signature[4]; // _MP_
    uint32_t cfg_table; // ptr to MP configuration table (optional)
    uint8_t length; // MP floating pointer structure length in 16-byte increments
    uint8_t spec_rev; // MP specification version
    uint8_t checksum;
    uint8_t default_cfg; // default configuration (if non-zero) - in which case don't parse MP configuration table
    uint32_t reserved;
} __attribute__((packed)) mp_fptr_t;

/* MP configuration table entry */
typedef struct {
    uint8_t type;
    union {
        /* processor entry */
        #define MP_ETYPE_CPU                            0
        struct {
            uint8_t apic_id;
            uint8_t lapic_ver;
            uint8_t flags;
            uint8_t stepping : 4;
            uint8_t model : 4;
            uint8_t family : 4;
            size_t sig_reserved : 20;
            uint32_t features;
            uint32_t reserved[2];
        } __attribute__((packed)) cpu;

        /* bus entry */
        #define MP_ETYPE_BUS                            1
        struct {
            uint8_t bus_id;
            char type_str[6];
        } __attribute__((packed)) bus;

        /* IOAPIC entry */
        #define MP_ETYPE_IOAPIC                         2
        struct {
            uint8_t apic_id;
            uint8_t ioapic_ver;
            uint8_t flags; // bit 0: EN
            uint32_t base;
        } __attribute__((packed)) ioapic;

        /* interrupt assignment entry */
        #define MP_ETYPE_IRQ_ASSIGN                     3
        struct {
            uint8_t type; // 0 = INT, 1 = NMI, 2 = SMI, 3 = ExtINT
            uint16_t flags; // MPS INTI flags - similar to MADT
            uint8_t bus_src;
            uint8_t irq_src;
            uint8_t id_dest; // destination IOAPIC ID
            uint8_t intin_dest; // destination INTIN# pin
        } __attribute__((packed)) irq_assign;

        /* local interrupt assignment entry */
        #define MP_ETYPE_LINT_ASSIGN                    4
        struct {
            uint8_t type;
            uint16_t flags;
            uint8_t bus_src;
            uint8_t irq_src;
            uint8_t id_dest; // destination LAPIC ID
            uint8_t lint_dest; // destination LINT# pin
        } __attribute__((packed)) lint_assign;
    } data;
} __attribute__((packed)) mp_cfg_entry_t;

/* MP configuration table header */
typedef struct {
    char signature[4]; // PCMP
    uint16_t base_len; // base configuration table length (incl. header)
    uint8_t spec_rev; // MP specification version
    uint8_t checksum;
    char oem_id[8];
    char product_id[12];
    uint32_t oem_table; // ignored(?)
    uint16_t oem_table_len;
    uint16_t base_cnt; // number of entries in the base table
    uint32_t lapic_base; // LAPIC base
    uint16_t ext_len; // extended table length
    uint8_t ext_checksum;
    uint8_t reserved;

    mp_cfg_entry_t first_entry;
} __attribute__((packed)) mp_cfg_t;

/*
 * mp_fptr_t* mp_find_fptr()
 *  Finds and maps the MP Floating Pointer Structure.
 */
mp_fptr_t* mp_find_fptr();

/*
 * mp_cfg_t* mp_map_cfgtab(mp_fptr_t* fptr)
 *  Maps the configuration table specified in the supplied MP Floating Pointer
 *  Structure.
 */
mp_cfg_t* mp_map_cfgtab(mp_fptr_t* fptr);

extern mp_fptr_t* mp_fptr;
extern mp_cfg_t* mp_cfg;

/* bus IDs */
#define MP_BUS_NONE                         0
#define MP_BUS_CBUS                         1 // Corollary CBus
#define MP_BUS_CBUS2                        2 // Corollary CBus II
#define MP_BUS_EISA                         3 // Extended ISA
#define MP_BUS_FUTURE                       4 // IEEE FutureBus
#define MP_BUS_INTERNAL                     5 // internal bus
#define MP_BUS_ISA                          6 // ISA
#define MP_BUS_MB1                          7 // Multibus I
#define MP_BUS_MB2                          8 // Multibus II
#define MP_BUS_MCA                          9 // MCA
#define MP_BUS_MPI                          10
#define MP_BUS_MPSA                         11
#define MP_BUS_NUBUS                        12 // Apple NuBus
#define MP_BUS_PCI                          13
#define MP_BUS_PCMCIA                       14
#define MP_BUS_TC                           15 // DEC TurboChannel
#define MP_BUS_VLB                          16 // VESA Local Bus
#define MP_BUS_VME                          17
#define MP_BUS_EXPRESS                      18 // Express System Bus

extern uint8_t* mp_busid;
extern int mp_busid_cnt;

/*
 * bool mp_init()
 *  Maps the MP tables if they exist and return their pointers in mp_fptr and
 *  mp_cfg.
 */
bool mp_init();

#endif
