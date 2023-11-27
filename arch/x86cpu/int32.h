#ifndef ARCH_X86CPU_INT32_H
#define ARCH_X86CPU_INT32_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t gs, fs, es, ds;
    uint16_t di, si, bp, sp, bx, dx, cx, ax;
    uint16_t eflags;
} __attribute__((packed)) int32_regs_t;

/*
 * void int32(uint8_t vector, int32_regs_t* regs)
 *  Calls the specified 16-bit (real mode) interrupt vector.
 *  This function is based on Napalm's protected mode BIOS
 *  call functionality code:
 *  http://www.rohitab.com/discuss/topic/35103-switch-between-real-mode-and-protected-mode/
 */
void int32(uint8_t vector, int32_regs_t* regs);

/*
 * void int32_init()
 *  Maps the memory range required for int32() to function.
 */
void int32_init();

#endif
