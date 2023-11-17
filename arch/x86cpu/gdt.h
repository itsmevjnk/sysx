#ifndef X86CPU_GDT_H
#define X86CPU_GDT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * tss_t
 *  TSS entry structure.
 */
typedef struct tss_entry {
  struct tss_entry* prev_tss;
  uint32_t esp0; // NOTE: only esp0 and ss0 are relevant - the rest are unused
  uint32_t ss0;
  uint32_t esp1;
  uint32_t ss1;
  uint32_t esp2;
  uint32_t ss2;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint32_t es;
  uint32_t cs;
  uint32_t ss;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
  uint32_t ldt;
  uint32_t trap;
  uint32_t iomap_base;
} __attribute__((packed)) tss_t;
extern tss_t tss_entry;

/*
 * void gdt_add_entry(uint8_t entry, uintptr_t base, uintptr_t limit, bool rw, bool dc,
 *                    bool exec, bool type, uint8_t privl, bool size, bool granularity)
 *  Adds an entry to the GDT with the specified properties.
 */
void gdt_add_entry(uint8_t entry, uintptr_t base, uintptr_t limit, bool rw, bool dc, bool exec, bool type, uint8_t privl, bool size, bool granularity);

/*
 * void gdt_init()
 *  Initializes and loads the GDT.
 */
void gdt_init();

#endif
