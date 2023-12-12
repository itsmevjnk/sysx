ENTRY(_start)

SECTIONS
{
  /* lower half portion - only for initialization */
  . = 0x00100000;
  __pre_start = .;

  __ptext_start = .;
  .pre.text BLOCK(4K) : ALIGN(4K)
  {
    *(.multiboot)
    *(.pre.text)
  }
  __ptext_end = .;

  __pdata_start = .;
  .pre.data BLOCK(4K) : ALIGN(4K)
  {
    *(.pre.data)
  }
  __pdata_end = .;

  __pre_end = .;

  /* recursive mapping region */
  . = 0xC0000000 - 0x400000;
  __rmap_start = .;
  . += 0x400000;
  __rmap_end = .;

  /* higher half portion - for everything else ;) */
  . = 0xC0100000 + (__pre_end - __pre_start);
  __kernel_start = .;

  __text_start = .;
  .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000)
  {
    *(.text)
  }
  __text_end = .;

  __rodata_start = .;
  .rodata ALIGN(4K) : AT(ADDR(.rodata) - 0xC0000000)
  {
    *(.rodata)
  }
  __rodata_end = .;

  __data_start = .;
  .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000)
  {
    *(.data)
  }
  __data_end = .;

  __bss_start = .;
  .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000)
  {
    *(COMMON)
    *(.bss)
  }
  __bss_end = .;

  __kernel_end = .;

  /* other kernel memory spaces, which will be allocated during runtime */

  /* kernel heap - REQUIRED FOR ALL PLATFORMS */
  . = 0xF0000000;
  __kheap_start = .;
  . += 16777216;
  __kheap_end = .;
}
