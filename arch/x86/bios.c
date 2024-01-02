#include <arch/x86/bios.h>
#include <mm/vmm.h>
#include <kernel/log.h>
#include <string.h>
#include <mm/addr.h>

uintptr_t bios_ebda_paddr = 0;

void* bios_find_tab(const char* sig, size_t sig_len, uintptr_t range_paddr, size_t range_len, size_t boundary, uintptr_t va_start) {
    void* ret = NULL;
    uintptr_t base = vmm_alloc_map(vmm_kernel, range_paddr, range_len, va_start, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT);
    if(!base) {
        kerror("cannot map 0x%x (%u bytes) into virtual address space", range_paddr, range_len);
        return NULL;
    }
    for(size_t i = 0; i < range_len; i += boundary) {
        if(!memcmp((void*) (base + i), sig, sig_len)) {
            ret = (void*) (range_paddr + i);
            break;
        }
    }
    vmm_unmap(vmm_kernel, base, range_len);
    return ret;
}

uintptr_t bios_get_ebda_paddr() {
    uint16_t volatile* bda_window = (uint16_t*) vmm_alloc_map(vmm_kernel, 0x40E, 2, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT);
    if(bda_window == NULL) {
        kerror("cannot map BDA");
        return 0;
    }
    bios_ebda_paddr = *bda_window << 4;
    // kdebug("EBDA is located at 0x%x", ebda_paddr);
    vmm_unmap(vmm_kernel, (uintptr_t) bda_window, 2);
    return bios_ebda_paddr;
}
