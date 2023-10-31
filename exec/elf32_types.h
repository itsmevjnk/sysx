#ifndef EXEC_ELF32_TYPES_H
#define EXEC_ELF32_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* data types */
typedef uint16_t    Elf32_Half;
typedef uint32_t    Elf32_Off;
typedef uint32_t    Elf32_Addr;
typedef uint32_t    Elf32_Word;
typedef int32_t     Elf32_Sword;

/* e_ident indices */
#define EI_MAG0         0 // e_ident[EI_MAG0] = ELFMAG0 (0x7F)
#define EI_MAG1         1 // e_ident[EI_MAG1] = ELFMAG1 ('E')
#define EI_MAG2         2 // e_ident[EI_MAG2] = ELFMAG2 ('L')
#define EI_MAG3         3 // e_ident[EI_MAG3] = ELFMAG3 ('F')
#define EI_CLASS        4 // e_ident[EI_CLASS] = ELFCLASS32 (1) or ELFCLASS64 (2) for ELF32 or ELF64 respectively
#define EI_DATA         5 // e_ident[EI_DATA] = ELFDATA2LSB (1) or ELFDATA2MSB (2) for little or big endian respectively
#define EI_VERSION      6 // e_ident[EI_VERSION] = EV_CURRENT (1)
#define EI_PAD          7
#define EI_NIDENT       16 // number of e_ident bytes

/* e_version and e_ident[EI_VERSION] values */
#define EV_NONE         0 // invalid version
#define EV_CURRENT      1

/* e_ident[EI_MAG0] to e_ident[EI_MAG3] values */
#define ELFMAG0         0x7F
#define ELFMAG1         'E'
#define ELFMAG2         'L'
#define ELFMAG3         'F'

/* e_ident[EI_CLASS] values */
#define ELFCLASSNONE    0 // invalid class
#define ELFCLASS32      1 // ELF32
#define ELFCLASS64      2 // ELF64

/* e_ident[EI_DATA] values */
#define ELFDATANONE     0 // invalid encoding
#define ELFDATA2LSB     1 // little-endian
#define ELFDATA2MSB     2 // big-endian

/* e_type values */
#define ET_NONE         0
#define ET_REL          1
#define ET_EXEC         2
#define ET_DYN          3
#define ET_CORE         4
#define ET_LOPROC       0xFF00
#define ET_HIPROC       0xFFFF

/* file header */
typedef struct {
    unsigned char   e_ident[EI_NIDENT];
    Elf32_Half      e_type;
    Elf32_Half      e_machine;
    Elf32_Word      e_version;
    Elf32_Addr      e_entry;
    Elf32_Off       e_phoff;
    Elf32_Off       e_shoff;
    Elf32_Word      e_flags;
    Elf32_Half      e_ehsize;
    Elf32_Half      e_phentsize;
    Elf32_Half      e_phnum;
    Elf32_Half      e_shentsize;
    Elf32_Half      e_shnum;
    Elf32_Half      e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

/* special e_shnum values */
#define SHN_UNDEF       0
#define SHN_LORESERVE   0xFF00
#define SHN_LOPROC      0xFF00
#define SHN_HIPROC      0xFF1F
#define SHN_ABS         0xFFF1
#define SHN_COMMON      0xFFF2
#define SHN_HIRESERVE   0xFFFF

/* sh_type values */
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_RELA        4
#define SHT_HASH        5
#define SHT_DYNAMIC     6
#define SHT_NOTE        7
#define SHT_NOBITS      8
#define SHT_REL         9
#define SHT_SHLIB       10
#define SHT_DYNSYM      11
#define SHT_LOPROC      0x70000000
#define SHT_HIPROC      0x7FFFFFFF
#define SHT_LOUSER      0x80000000
#define SHT_HIUSER      0xFFFFFFFF

/* sh_flags values */
#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4
#define SHF_MASKPROC    0xF0000000

/* section header */
typedef struct {
    Elf32_Word      sh_name;
    Elf32_Word      sh_type;
    Elf32_Word      sh_flags;
    Elf32_Addr      sh_addr;
    Elf32_Off       sh_offset;
    Elf32_Word      sh_size;
    Elf32_Word      sh_link;
    Elf32_Word      sh_info;
    Elf32_Word      sh_addralign;
    Elf32_Word      sh_entsize;
} __attribute__((packed)) Elf32_Shdr;

/* st_info upper 4 bit values */
#define ELF_ST_BIND(i)          ((i) >> 4) // common for both ELF32 and ELF64
#define STB_LOCAL       0
#define STB_GLOBAL      1
#define STB_WEAK        2
#define STB_LOPROC      13
#define STB_HIPROC      15

/* st_info lower 4 bit values */
#define ELF_ST_TYPE(i)          ((i) & 0xF) // common for both ELF32 and ELF64
#define STT_NOTYPE      0
#define STT_OBJECT      1
#define STT_FUNC        2
#define STT_SECTION     3
#define STT_FILE        4
#define STT_LOPROC      13
#define STT_HIPROC      15 

/* st_other values */
#define STV_DEFAULT     0
#define STV_INTERNAL    1
#define STV_HIDDEN      2
#define STV_PROTECTED   3

/* symbol table entry */
typedef struct {
    Elf32_Word      st_name;
    Elf32_Addr      st_value;
    Elf32_Word      st_size;
    unsigned char   st_info;
    unsigned char   st_other;
    Elf32_Half      st_shndx;
} __attribute__((packed)) Elf32_Sym;

/* r_info breakdown */
#define ELF32_R_SYM(i)          ((i) >> 8)
#define ELF32_R_TYPE(i)         ((unsigned char)(i))

/* relocation entries */
typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
} __attribute__((packed)) Elf32_Rel;

typedef struct {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
    Elf32_Sword r_addend;
} __attribute__((packed)) Elf32_Rela;

#endif