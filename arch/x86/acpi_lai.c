#ifdef FEAT_ACPI_LAI

#include <drivers/acpi.h>
#include <kernel/log.h>

#include <mm/addr.h>
#include <mm/vmm.h>

#include <stdlib.h>

#include <lai/host.h>
#include <lai/helpers/pc-bios.h>
#include <acpispec/tables.h>

static size_t acpi_version = 0;
static uintptr_t acpi_sdthdr_vaddr = 0; // temporary mapping location for SDT headers

/* SDT lookup table */
static acpi_header_t** acpi_sdttab = NULL;
static size_t acpi_sdttab_sz = 0; // number of SDT table entries

static void* acpi_map_sdthdr(uintptr_t paddr) {
    return (void*) vmm_map(vmm_kernel, paddr, acpi_sdthdr_vaddr, sizeof(acpi_header_t), VMM_FLAGS_PRESENT);
}

static void acpi_add_sdt(acpi_header_t* header, size_t idx) {
    /* map the entire SDT */
    acpi_sdttab[idx] = (acpi_header_t*) vmm_alloc_map(vmm_kernel, vmm_get_paddr(vmm_kernel, (uintptr_t) header), header->length, kernel_end, UINTPTR_MAX, false, VMM_FLAGS_PRESENT | VMM_FLAGS_CACHE); // no need for RW access (since we're not supposed to write into this)
    if(acpi_sdttab[idx] == NULL) {
        kerror("cannot find empty space to map SDT with signature %c%c%c%c (%u bytes)", header->signature[0], header->signature[1], header->signature[2], header->signature[3], header->length);
        return;
    }
    kdebug("mapped SDT with signature %c%c%c%c to 0x%x", acpi_sdttab[idx]->signature[0], acpi_sdttab[idx]->signature[1], acpi_sdttab[idx]->signature[2], acpi_sdttab[idx]->signature[3], acpi_sdttab[idx]);
}

void *laihost_scan(const char *sig, size_t index) {
    size_t hit = 0; // hit index
    for(size_t i = 0; i < acpi_sdttab_sz; i++) {
        if(acpi_sdttab[i] != NULL && !memcmp(acpi_sdttab[i]->signature, sig, 4)) {
            if(index == hit) {
                // kdebug("occurrence #%u of %c%c%c%c found at 0x%x", index + 1, sig[0], sig[1], sig[2], sig[3], acpi_sdttab[i]);
                return acpi_sdttab[i];
            }
            hit++;
        }
    }
    kwarn("cannot find occurrence #%u of SDT with signature %c%c%c%c", index + 1, sig[0], sig[1], sig[2], sig[3]);
    return NULL; // cannot find SDT table
}

bool acpi_arch_init() {
    struct lai_rsdp_info info;
    if(lai_bios_detect_rsdp(&info) != LAI_ERROR_NONE) {
        kerror("lai_bios_detect_rsdp() failed");
        return false;
    }
    acpi_version = info.acpi_version;
    uintptr_t rsdt_address = (acpi_version == 2) ? info.xsdt_address : info.rsdt_address; // RSDT/XSDT physical address
    kinfo("ACPI version %d, RSDP @ 0x%x, RSDT/XSDT @ 0x%x", info.acpi_version, info.rsdp_address, rsdt_address);
    lai_set_acpi_revision(acpi_version);

    /* allocate space for mapping SDT headers */
    acpi_sdthdr_vaddr = vmm_first_free(vmm_kernel, kernel_end, UINTPTR_MAX, 2 * vmm_pgsz(), false);
    if(!acpi_sdthdr_vaddr) {
        kerror("cannot find mapping location for SDT headers");
        return false;
    }
    // kdebug("SDT headers will be temporarily mapped to 0x%x", acpi_sdthdr_vaddr);
    vmm_pgmap(vmm_kernel, 0xDEADB000, acpi_sdthdr_vaddr, VMM_FLAGS_PRESENT | VMM_FLAGS_CACHE); // reserve space - we'll fill in the location later
    vmm_pgmap(vmm_kernel, 0xDEADB000, acpi_sdthdr_vaddr + vmm_pgsz(), VMM_FLAGS_PRESENT | VMM_FLAGS_CACHE);

    /* map RSDT/XSDT */
    acpi_header_t* rsdt_header = acpi_map_sdthdr(rsdt_address);
    kdebug("%c%c%c%c (%u bytes) mapped to 0x%x", rsdt_header->signature[0], rsdt_header->signature[1], rsdt_header->signature[2], rsdt_header->signature[3], rsdt_header->length, rsdt_header);
    size_t sdtptr_sz = rsdt_header->length - sizeof(acpi_header_t);
    acpi_sdttab_sz = sdtptr_sz / ((acpi_version == 2) ? 8 : 4) + 2; // XSDT uses 64-bit pointers, while RSDT uses 32-bit. we also add 2 more entries for 
    kdebug("found %u SDTs including RSDT/XSDT", acpi_sdttab_sz);
    acpi_sdttab = kcalloc(acpi_sdttab_sz, sizeof(acpi_header_t*));
    if(acpi_sdttab == NULL) {
        kerror("cannot allocate SDT lookup table (%u entries)", acpi_sdttab_sz);
        vmm_unmap(vmm_kernel, acpi_sdthdr_vaddr, 2 * vmm_pgsz());
        return false;
    }
    acpi_add_sdt(rsdt_header, 0); // add RSDT/XSDT to first entry
    
    /* map all the other SDT tables */
    void* ptr = (void*) ((uintptr_t) acpi_sdttab[0] + sizeof(acpi_header_t));
    kdebug("SDT pointer table starts at 0x%x", ptr);
    static acpi_fadt_t* acpi_fadt = NULL; // FADT
    for(size_t i = 2; i < acpi_sdttab_sz; i++, ptr = (void*) ((uintptr_t) ptr + ((acpi_version == 2) ? 8 : 4))) {
        acpi_header_t* header = acpi_map_sdthdr((acpi_version == 2) ? *((uint64_t*) ptr) : *((uint32_t*) ptr));
        acpi_add_sdt(header, i);
        if(acpi_fadt == NULL && !memcmp(header->signature, "FACP", 4)) acpi_fadt = (acpi_fadt_t*) acpi_sdttab[i];
    }

    /* add DSDT to lookup table */
    if(acpi_fadt == NULL) kwarn("FADT not found");
    else {
        acpi_header_t* header = acpi_map_sdthdr((acpi_version == 2) ? acpi_fadt->x_dsdt : acpi_fadt->dsdt);
        acpi_add_sdt(header, 1);
    }

    vmm_unmap(vmm_kernel, acpi_sdthdr_vaddr, 2 * vmm_pgsz());

    return true;
}

#endif