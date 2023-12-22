#ifndef HELPERS_MMIO_H
#define HELPERS_MMIO_H

#include <stddef.h>
#include <stdint.h>

/*
 * static inline void mmio_outb(uintptr_t ptr, uint8_t val)
 *  Performs an 8-bit MMIO write to the specified pointer.
 */
static inline void mmio_outb(uintptr_t ptr, uint8_t val) {
    *((uint8_t volatile*)ptr) = val;
}

/*
 * static inline void mmio_outw(uintptr_t ptr, uint16_t val)
 *  Performs a 16-bit MMIO write to the specified pointer.
 */
static inline void mmio_outw(uintptr_t ptr, uint16_t val) {
    *((uint16_t volatile*)ptr) = val;
}

/*
 * static inline void mmio_outd(uintptr_t ptr, uint32_t val)
 *  Performs a 32-bit MMIO write to the specified pointer.
 */
static inline void mmio_outd(uintptr_t ptr, uint32_t val) {
    *((uint32_t volatile*)ptr) = val;
}

/*
 * static inline void mmio_outq(uintptr_t ptr, uint64_t val)
 *  Performs a 64-bit MMIO write to the specified pointer.
 */
static inline void mmio_outq(uintptr_t ptr, uint64_t val) {
    *((uint64_t volatile*)ptr) = val;
}

/*
 * static inline uint8_t mmio_inb(uintptr_t ptr)
 *  Performs an 8-bit MMIO read from the specified pointer.
 */
static inline uint8_t mmio_inb(uintptr_t ptr) {
    return *((uint8_t volatile*)ptr);
}

/*
 * static inline uint16_t mmio_inw(uintptr_t ptr)
 *  Performs a 16-bit MMIO read from the specified pointer.
 */
static inline uint16_t mmio_inw(uintptr_t ptr) {
    return *((uint16_t volatile*)ptr);
}

/*
 * static inline uint32_t mmio_ind(uintptr_t ptr)
 *  Performs a 32-bit MMIO read from the specified pointer.
 */
static inline uint32_t mmio_ind(uintptr_t ptr) {
    return *((uint32_t volatile*)ptr);
}

/*
 * static inline uint64_t mmio_inq(uintptr_t ptr)
 *  Performs a 64-bit MMIO read from the specified pointer.
 */
static inline uint64_t mmio_inq(uintptr_t ptr) {
    return *((uint64_t volatile*)ptr);
}

#endif
