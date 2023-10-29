#ifndef EXEC_ELF64_TYPES_H
#define EXEC_ELF64_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <exec/elf32_types.h>

/* data types */
typedef uint16_t    Elf64_Half;
typedef uint64_t    Elf64_Off;
typedef uint64_t    Elf64_Addr;
typedef uint32_t    Elf64_Word;
typedef int32_t     Elf64_Sword;
typedef uint64_t    Elf64_Xword;
typedef int64_t     ELf64_Sxword;

/* ELF64-specific e_ident indices */
#define EI_OSABI        7
#define EI_ABIVERSION   8
#define EI_PAD_64       9

/* e_ident[EI_OSABI] values */
#define ELFOSABI_SYSV       0
#define ELFOSABI_HPUX       1
#define ELFOSABI_STANDALONE 255

/* ELF64-specific e_type values */
#define ET_LOOS         0xFE00
#define ET_HIOS         0xFEFF

/* file header */
typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    Elf64_Half      e_type;
    Elf64_Half      e_machine;
    Elf64_Word      e_version;
    Elf64_Addr      e_entry;
    Elf64_Off       e_phoff;
    Elf64_Off       e_shoff;
    Elf64_Word      e_flags;
    Elf64_Half      e_ehsize;
    Elf64_Half      e_phentsize;
    Elf64_Half      e_phnum;
    Elf64_Half      e_shentsize;
    Elf64_Half      e_shnum;
    Elf64_Half      e_shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

/* ELF64-specific special e_shnum values */
#define SHN_LOOS        0xFF20
#define SHN_HIOS        0xFF3F

/* ELF64-specific sh_type values */
#define SHT_LOOS        0x60000000
#define SHT_HIOS        0x6FFFFFFF

/* ELF64-specific sh_flags values */
#define SHF_MASKOS      0x0F000000

/* section header */
typedef struct {
    Elf64_Word      sh_name;
    Elf64_Word      sh_type;
    Elf64_Xword     sh_flags;
    Elf64_Addr      sh_addr;
    Elf64_Off       sh_offset;
    Elf64_Xword     sh_size;
    Elf64_Word      sh_link;
    Elf64_Word      sh_info;
    Elf64_Xword     sh_addralign;
    Elf64_Xword     sh_entsize;
} __attribute__((packed)) Elf64_Shdr;

/* ELF64-specific st_info upper 4 bit values */
#define STB_LOOS        10
#define STB_HIOS        12

/* ELF64-specific st_info lower 4 bit values */
#define STT_LOOS        10
#define STT_HIOS        12

/* symbol table entry */
typedef struct {
    Elf64_Word      st_name;
    unsigned char   st_info;
    unsigned char   st_other;
    Elf64_Half      st_shndx;
    Elf64_Addr      st_value;
    Elf64_Xword     st_size;
} __attribute__((packed)) Elf64_Sym;

#endif