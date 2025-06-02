#include <lai/host.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <mm/vmm.h>
#include <mm/addr.h>
#include <drivers/pci.h>
#include <hal/timer.h>

void laihost_log(int level, const char* msg) {
    switch(level) {
        case LAI_DEBUG_LOG:
            kdebug("acpi/lai: %s", msg);
            break;
        case LAI_WARN_LOG:
            kwarn("acpi/lai: %s", msg);
            break;
        default:
            kinfo("acpi/lai: log level %u: %s", level, msg);
            break;
    }
}

__attribute__((noreturn)) void laihost_panic(const char* msg) {
    kerror("acpi/lai: fatal error: %s", msg);
    while(1); // TODO: abort
}

void* laihost_malloc(size_t size) {
    return kmalloc(size);
}

void* laihost_realloc(void* oldptr, size_t newsize, size_t oldsize) {
    (void) oldsize;
    return krealloc(oldptr, newsize);
}

void laihost_free(void* ptr, size_t size) {
    (void) size;
    kfree(ptr);
}

void* laihost_map(size_t address, size_t count) {
    uintptr_t ret = vmm_alloc_map(vmm_kernel, address, count, kernel_end, UINTPTR_MAX, 0, 0, false, VMM_FLAGS_PRESENT | VMM_FLAGS_RW);
    if(!ret) {
        kerror("cannot allocate virtual address space of size %u", count);
        return NULL;
    }
    return (void*) ret;
}

void laihost_unmap(void* pointer, size_t count) {
    vmm_unmap(vmm_kernel, (uintptr_t) pointer, count);
}

#ifdef FEAT_PCI

void laihost_pci_writeb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint8_t val) {
    if(seg) kwarn("attempting to access non-zero PCI segment %u (device %02x:%02x.%x)", seg, bus, slot, fun); // TODO: implement PCI segments
    else pci_cfg_write_byte(bus, slot, fun, offset, val);
}

void laihost_pci_writew(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint16_t val) {
    if(seg) kwarn("attempting to access non-zero PCI segment %u (device %02x:%02x.%x)", seg, bus, slot, fun); // TODO: implement PCI segments
    else pci_cfg_write_word(bus, slot, fun, offset, val);
}

void laihost_pci_writed(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset, uint32_t val) {
    if(seg) kwarn("attempting to access non-zero PCI segment %u (device %02x:%02x.%x)", seg, bus, slot, fun); // TODO: implement PCI segments
    else pci_cfg_write_dword(bus, slot, fun, offset, val);
}

uint8_t laihost_pci_readb(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    if(seg) {
        kwarn("attempting to access non-zero PCI segment %u (device %02x:%02x.%x)", seg, bus, slot, fun); // TODO: implement PCI segments
        return 0xFF;
    } else return pci_cfg_read_byte(bus, slot, fun, offset);
}

uint16_t laihost_pci_readw(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    if(seg) {
        kwarn("attempting to access non-zero PCI segment %u (device %02x:%02x.%x)", seg, bus, slot, fun); // TODO: implement PCI segments
        return 0xFFFF;
    } else return pci_cfg_read_word(bus, slot, fun, offset);
}

uint32_t laihost_pci_readd(uint16_t seg, uint8_t bus, uint8_t slot, uint8_t fun, uint16_t offset) {
    if(seg) {
        kwarn("attempting to access non-zero PCI segment %u (device %02x:%02x.%x)", seg, bus, slot, fun); // TODO: implement PCI segments
        return 0xFFFFFFFF;
    } else return pci_cfg_read_dword(bus, slot, fun, offset);
}

#endif

void laihost_sleep(uint64_t ms) {
    timer_delay_ms(ms);
}

uint64_t laihost_timer(void) {
    return (timer_tick * 10); // timer tick is in microseconds
}

// optional TODO: implement laihost_handle_amldebug() and laihost_handle_global_notify()

// int laihost_sync_wait(struct lai_sync_state* sync, unsigned int val, int64_t timeout) {
//     timeout = (timeout + 9) / 10; // TODO: is the timeout in 100ns increments (like laihost_timer())?
//     timer_tick_t t_start = timer_tick;
//     do {
//         if(sync->val != val) return 0; // we're good to go
//         task_yield_noirq(); // otherwise, yield to another task
//     } while(timer_tick - t_start < timeout);
//     return (timer_tick - t_start); // timeout exhausted
// }

// void laihost_sync_wake(struct lai_sync_state* sync) {
//     (void) sync; // TODO: proper futex implementation
//     task_yield_noirq();
// }
