#include <arch/x86/mptab.h>
#include <kernel/log.h>
#include <mm/vmm.h>
#include <mm/addr.h>
#include <string.h>
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
    }

    return true;
}
