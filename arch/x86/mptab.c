#include <arch/x86/mptab.h>
#include <kernel/log.h>
#include <mm/vmm.h>
#include <mm/addr.h>
#include <string.h>
#include <stdlib.h>
#include <arch/x86/multiboot.h>

static mp_fptr_t* mp_find_fptr_stub(uintptr_t paddr, size_t len, uintptr_t va_start) {
    mp_fptr_t* ret = NULL;
    uintptr_t base = vmm_alloc_map(vmm_kernel, paddr, len, va_start, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT);
    if(!base) {
        kerror("cannot map 0x%x (%u bytes) into virtual address space", paddr, len);
        return NULL;
    }
    for(size_t i = 0; i < len; i += 16) {
        if(!memcmp((void*) (base + i), "_MP_", 4)) {
            ret = (mp_fptr_t*) vmm_get_paddr(vmm_kernel, base + i);
            break;
        }
    }
    vmm_unmap(vmm_kernel, base, len);
    return ret;
}

mp_fptr_t* mp_find_fptr() {
    /* get EBDA base address */
    uint16_t volatile* bda_window = (uint16_t*) vmm_alloc_map(vmm_kernel, 0x40E, 2, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT);
    if(bda_window == NULL) {
        kerror("cannot map BDA");
        return NULL;
    }
    uintptr_t ebda_paddr = *bda_window << 4;
    kdebug("EBDA is located at 0x%x", ebda_paddr);
    vmm_unmap(vmm_kernel, (uintptr_t) bda_window, 2);
    
    mp_fptr_t* ret = NULL;

    if(ret == NULL) ret = mp_find_fptr_stub(ebda_paddr, 1024, (uintptr_t) bda_window & ~0xFFF); // find in the first kilobyte of the EBDA
    if(ret == NULL) ret = mp_find_fptr_stub(mb_info->mem_lower << 10, 1024, (uintptr_t) bda_window & ~0xFFF); // find in the last KiB of system base memory
    if(ret == NULL) ret = mp_find_fptr_stub(0xF0000, 0x10000, (uintptr_t) bda_window & ~0xFFF); // find in the BIOS ROM region

    if(ret != NULL) {
        /* whatever mp_find_fptr_stub returned was a physical address, so we'll need to map it in */
        kdebug("found MP floating pointer structure at 0x%x", (uintptr_t)ret);
        ret = (mp_fptr_t*) vmm_alloc_map(vmm_kernel, (uintptr_t) ret, sizeof(mp_fptr_t), (uintptr_t) bda_window & ~0xFFF, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT);
        if(ret == NULL) {
            kerror("cannot map MP floating pointer structure into virtual address space");
            return NULL;
        }
    }

    return ret;
}

mp_cfg_t* mp_map_cfgtab(mp_fptr_t* fptr) {
    if(fptr->default_cfg) kwarn("caller is supposed to use default configuration #%u", fptr->default_cfg);

    mp_cfg_t* ret = (mp_cfg_t*) vmm_alloc_map(vmm_kernel, fptr->cfg_table, sizeof(mp_cfg_t) - sizeof(mp_cfg_entry_t), kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT); // only map header first
    if(ret == NULL) {
        kerror("cannot map configuration table header");
        return NULL;
    }
    
    uintptr_t len = ret->base_len + ret->ext_len;
    kdebug("MP configuration table is located at 0x%x (%u bytes)", fptr->cfg_table, len);
    vmm_unmap(vmm_kernel, (uintptr_t) ret, sizeof(mp_cfg_t) - sizeof(mp_cfg_entry_t)); // unmap for remapping the entire table
    ret = (mp_cfg_t*) vmm_alloc_map(vmm_kernel, fptr->cfg_table, len, (uintptr_t) ret & ~0xFFF, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT); // map the entire table
    if(ret == NULL) {
        kerror("cannot map configuration table (%u bytes)", len);
        return NULL;
    }

    return ret;
}

mp_fptr_t* mp_fptr = NULL;
mp_cfg_t* mp_cfg = NULL;
uint8_t* mp_busid = NULL;
int mp_busid_cnt = 0;

bool mp_init() {
    mp_fptr = mp_find_fptr();
    if(mp_fptr == NULL) {
        kerror("cannot find or map MP floating pointer structure");
        return false;
    }

    if(mp_fptr->default_cfg) kinfo("system uses default configuration %u", mp_fptr->default_cfg);
    else {
        mp_cfg = mp_map_cfgtab(mp_fptr);
        if(mp_cfg == NULL) {
            kerror("cannot map MP configuration table");
            vmm_unmap(vmm_kernel, (uintptr_t) mp_fptr, sizeof(mp_fptr_t));
            return false;
        }

        mp_cfg_entry_t* entry = &mp_cfg->first_entry;
        mp_busid_cnt = -1;
        for(size_t n = 0; n < mp_cfg->base_cnt; n++, entry = (mp_cfg_entry_t*) ((uintptr_t) entry + ((entry->type) ? 8 : 20))) {
            if(entry->type == MP_ETYPE_BUS && entry->data.irq_assign.bus_src > mp_busid_cnt) mp_busid_cnt = entry->data.irq_assign.bus_src; 
        }
        mp_busid_cnt++;

        if(mp_busid_cnt) {
            mp_busid = kcalloc(mp_busid_cnt, sizeof(uint8_t));
            if(mp_busid == NULL) {
                kerror("cannot allocate bus ID table");
                mp_busid_cnt = 0;
                return false;
            }

            entry = &mp_cfg->first_entry;
            for(size_t n = 0; n < mp_cfg->base_cnt; n++, entry = (mp_cfg_entry_t*) ((uintptr_t) entry + ((entry->type) ? 8 : 20))) {
                if(entry->type == MP_ETYPE_BUS) {
                    if(!memcmp(entry->data.bus.type_str, "PCI", 3)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_PCI;
                        kdebug("found PCI bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "ISA", 3)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_ISA;
                        kdebug("found ISA bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "PCMCIA", 6)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_PCMCIA;
                        kdebug("found PCMCIA bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "EISA", 4)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_EISA;
                        kdebug("found EISA bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "INTERN", 6)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_INTERNAL;
                        kdebug("found internal bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "VL", 2)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_VLB;
                        kdebug("found VLB bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "MCA", 3)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_MCA;
                        kdebug("found MCA bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "CBUS", 4)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_CBUS;
                        kdebug("found CBus bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "CBUSII", 6)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_CBUS2;
                        kdebug("found CBus II bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "FUTURE", 6)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_FUTURE;
                        kdebug("found FutureBus bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "MBII", 4)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_MB2;
                        kdebug("found Multibus II bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "MBI", 3)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_MB1;
                        kdebug("found Multibus I bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "MPI", 3)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_MPI;
                        kdebug("found MPI bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "MPSA", 4)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_MPSA;
                        kdebug("found MPSA bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "NUBUS", 5)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_NUBUS;
                        kdebug("found NuBus bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "TC", 2)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_TC;
                        kdebug("found TurboConnect bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "VME", 3)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_VME;
                        kdebug("found VMEbus bus ID %u", entry->data.bus.bus_id);
                    }
                    else if(!memcmp(entry->data.bus.type_str, "XPRESS", 6)) {
                        mp_busid[entry->data.bus.bus_id] = MP_BUS_EXPRESS;
                        kdebug("found Express System Bus bus ID %u", entry->data.bus.bus_id);
                    }
                    else kwarn("unknown bus ID %u of type %.6s", entry->data.bus.bus_id, entry->data.bus.type_str);
                }
            }
        }
    }

    return true;
}
