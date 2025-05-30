#include <arch/x86cpu/gdt.h>
#include <string.h>
#include <kernel/log.h>

#define BOOLINT(x)      ((x) ? 1 : 0) // idk if this is necessary

/* gdt_t
 *  GDT entry structure.
 */
typedef struct {
  size_t limit_l : 16; // limit 0:15
  size_t base_l : 24; // base 0:23
  // access byte
  size_t accessed : 1;
  size_t rw : 1;
  size_t dc : 1;
  size_t exec : 1;
  size_t type : 1;
  size_t privl : 2;
  size_t present : 1;
  size_t limit_h : 4; // limit 16:19
  size_t zero : 2;
  // flags
  size_t size : 1;
  size_t granularity : 1;
  size_t base_h : 8; // base 24:31
} __attribute__((packed)) gdt_t;
gdt_t gdt_entries[256];

/*
 * gdt_desc_t
 *  GDT descriptor structure, one that would be used with LGDT
 *  (not to be confused with LGBT, that's a whole different thing)
 */
typedef struct {
  uint16_t size;
  uint32_t offset;
} __attribute__((packed)) gdt_desc_t;
gdt_desc_t gdt_desc = {
  256 * sizeof(gdt_t) - 1, // 65535
  (uint32_t) &gdt_entries
};

tss_t tss_entry;

void gdt_add_entry(uint8_t entry, uintptr_t base, uintptr_t limit, bool rw, bool dc, bool exec, bool type, uint8_t privl, bool size, bool granularity) {
  gdt_entries[entry].base_l = base & 0x00ffffff;
  gdt_entries[entry].base_h = (base & 0xff000000) >> 24;
  gdt_entries[entry].limit_l = limit & 0x0000ffff;
  gdt_entries[entry].limit_h = (limit & 0x000f0000) >> 16;
  gdt_entries[entry].accessed = 0;
  gdt_entries[entry].rw = BOOLINT(rw);
  gdt_entries[entry].dc = BOOLINT(dc);
  gdt_entries[entry].exec = BOOLINT(exec);
  gdt_entries[entry].type = BOOLINT(type);
  gdt_entries[entry].privl = privl;
  gdt_entries[entry].present = 1;
  gdt_entries[entry].zero = 0;
  gdt_entries[entry].size = BOOLINT(size);
  gdt_entries[entry].granularity = BOOLINT(granularity);
  kdebug("entry %u: base 0x%08x, limit 0x%05x, rw=%u, dc=%u, exec=%u, type=%u, privl=%u, sz=%u, gran=%u", \
          entry, base, limit, BOOLINT(rw), BOOLINT(dc), BOOLINT(exec), BOOLINT(type), privl, BOOLINT(size), BOOLINT(granularity));
}

extern void gdt_load(gdt_desc_t* desc);

void gdt_init() {
  /* initialize the GDT */
  kinfo("initializing the GDT");
  gdt_add_entry(0, 0, 0, false, false, false, false, 0, false, false); // so that certain emulators are happy
  gdt_add_entry(1, 0, 0xfffff, true, false, true, true, 0, true, true); // ring 0 code segment (0x08), configured to only be executable in ring 0
  gdt_add_entry(2, 0, 0xfffff, true, false, false, true, 0, true, true); // ring 0 data segment (0x10)
  gdt_add_entry(3, 0, 0xfffff, true, false, true, true, 3, true, true); // ring 3 code segment (0x18)
  gdt_add_entry(4, 0, 0xfffff, true, false, false, true, 3, true, true); // ring 3 data segment (0x20)
  gdt_add_entry(5, 0, 0xfffff, true, false, true, true, 0, false, false); // ring 0 16-bit code segment (0x28)
  gdt_add_entry(6, 0, 0xfffff, true, false, false, true, 0, false, false); // ring 0 16-bit data segment (0x30)
  
  /* set up TSS */
  memset(&tss_entry, 0, sizeof(tss_t));
  tss_entry.ss0 = 0x10;
  __asm__ __volatile__("mov %%esp, %0" : "=r"(tss_entry.esp0)); // it does not really matter how far this is off from the actual stack bottom
  gdt_add_entry(7, (uintptr_t)&tss_entry, sizeof(tss_t), false, false, true, false, 0, false, false); // TSS (0x38)
  gdt_entries[7].accessed = 1; // set accessed bit too

  /* load the GDT descriptor and flush the cache :flushed: */
  kinfo("loading GDT descriptor and flushing CPU cache");
  gdt_load(&gdt_desc); // defined in desctabs.asm
}
