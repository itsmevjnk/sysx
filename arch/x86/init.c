#include <arch/x86cpu/gdt.h>
#include <arch/x86cpu/idt.h>
#include <kernel/log.h>

int ktgtinit() {
  kinfo("initializing GDT"); gdt_init();
  kinfo("initializing IDT"); idt_init();
  kinfo("initialization for target x86 completed successfully");
  return 0;
}
