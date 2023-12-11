#include <arch/x86cpu/int32.h>
#include <mm/vmm.h>

void int32_init() {
    vmm_pgmap(vmm_kernel, 0x7000, 0x7000, 0, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
}