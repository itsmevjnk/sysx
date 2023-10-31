#ifndef EXEC_ELF_RELOC_H
#define EXEC_ELF_RELOC_H

/* x86 relocation types */
#define R_386_NONE                  0
#define R_386_32                    1 // S + A
#define R_386_PC32                  2 // S + A - P
#define R_386_GOT32                 3 // G + A
#define R_386_PLT32                 4 // L + A - P
#define R_386_COPY                  5
#define R_386_GLOB_DAT              6 // S
#define R_386_JMP_SLOT              7 // S
#define R_386_RELATIVE              8 // B + A
#define R_386_GOTOFF                9 // S + A - GOT
#define R_386_GOTPC                 10 // GOT + A - P
#define R_386_32PLT                 11 // L + A
#define R_386_16                    20 // S + A
#define R_386_PC16                  21 // S + A - P
#define R_386_8                     22 // S + A
#define R_386_PC8                   23 // S + A - P
#define R_386_SIZE32                38 // Z + A

/* AMD64 relocation types */
#define R_AMD64_NONE                0
#define R_AMD64_64                  1 // S + A
#define R_AMD64_PC32                2 // S + A - P
#define R_AMD64_GOT32               3 // G + A
#define R_AMD64_PLT32               4 // L + A - P
#define R_AMD64_COPY                5
#define R_AMD64_GLOB_DAT            6 // S
#define R_AMD64_JMP_SLOT            7 // S
#define R_AMD64_RELATIVE            8 // B + A
#define R_AMD64_GOTPCREL            9 // G + GOT + A - P
#define R_AMD64_32                  10 // S + A
#define R_AMD64_32S                 11 // S + A
#define R_AMD64_16                  12 // S + A
#define R_AMD64_PC16                13 // S + A - P
#define R_AMD64_8                   14 // S + A
#define R_AMD64_PC8                 15 // S + A - P
#define R_AMD64_PC64                24 // S + A - P
#define R_AMD64_GOTOFF64            25 // S + A - GOT
#define R_AMD64_GOTPC32             26 // GOT + A - P
#define R_AMD64_SIZE32              32 // Z + A
#define R_AMD64_SIZE64              33 // Z + A

#endif