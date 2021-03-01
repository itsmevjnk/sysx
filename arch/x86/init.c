#include <arch/x86cpu/gdt.h>
#include <arch/x86cpu/idt.h>
#include <kernel/log.h>
#include <arch/x86/multiboot.h>

int ktgtinit() {
  kdebug("Multiboot information structure address: 0x%08x", mb_info);
  if(mb_info->flags & (1 << 0)) kdebug("memory size: lo %uK, hi %uK", mb_info->mem_lower, mb_info->mem_upper);
  kinfo("initializing GDT"); gdt_init();
  kinfo("initializing IDT"); idt_init();
  kinfo("initialization for target x86 completed successfully");
  return 0;
}
