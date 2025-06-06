#ifndef X86CPU_IDT_H
#define X86CPU_IDT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* gate types */
#define IDT_386_TASK32    0b0101
#define IDT_286_INTR16    0b0110
#define IDT_286_TRAP16    0b0111
#define IDT_386_INTR32    0b1110
#define IDT_386_TRAP32    0b1111

/*
 * idt_context_t
 *  Registers structure produced by IDT handler stub and given
 *  to interrupt handlers.
 */
typedef struct {
  /* 0 */ uint32_t gs, fs, es, ds;
  /* 16 */ uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
  /* 48 */ uint32_t vector, exc_code; // exc_code is for exceptions
  /* 56 */ uint32_t eip, cs, eflags;
  /* 68 */ uint32_t esp_usr, ss_usr; // only available if entering from ring 3 (as evident by first 2 bits of CS being 0b11)
} idt_context_t;

/*
 * void idt_add_gate(uint8_t vector, uint16_t selector, uintptr_t offset, uint8_t type, uint8_t dpl)
 *  Adds a gate with the specified properties.
 */
void idt_add_gate(uint8_t vector, uint16_t selector, uintptr_t offset, uint8_t type, uint8_t dpl);

/*
 * void idt_init()
 *  Initializes, then loads the IDT.
 */
void idt_init();

#endif
