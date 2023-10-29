#include <exec/elf.h>
#include <exec/syms.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <string.h>

/* most architectures can only load either ELF32 or ELF64 files */
#if defined(__i386__)
#define ELF_FORCE_CLASS                 ELFCLASS32
#elif defined(__x86_64__)
#define ELF_FORCE_CLASS                 ELFCLASS64 // TODO: ELF32 support for loading 32-bit code?
#elif defined(__arm__)
#define ELF_FORCE_CLASS                 ELFCLASS32
#elif defined(__aarch64__)
#define ELF_FORCE_CLASS                 ELFCLASS64
#endif

enum elf_check_result elf_check_header(void* buf) {
    if(buf == NULL) {
        kerror("null pointer passed for ELF header checking");
        return ERR_NULLBUF;
    }
    
    Elf32_Ehdr* hdr = buf; // assume ELF32 header as of now

    /* check magic bytes */
    if(hdr->e_ident[EI_MAG0] != ELFMAG0) {
        kerror("invalid magic byte 0 (expected 0x%02x, got 0x%02x)", ELFMAG0, hdr->e_ident[EI_MAG0]);
        return ERR_MAGIC;
    }
    if(hdr->e_ident[EI_MAG1] != ELFMAG1) {
        kerror("invalid magic byte 1 (expected 0x%02x, got 0x%02x)", ELFMAG1, hdr->e_ident[EI_MAG1]);
        return ERR_MAGIC;
    }
    if(hdr->e_ident[EI_MAG2] != ELFMAG2) {
        kerror("invalid magic byte 2 (expected 0x%02x, got 0x%02x)", ELFMAG2, hdr->e_ident[EI_MAG2]);
        return ERR_MAGIC;
    }
    if(hdr->e_ident[EI_MAG3] != ELFMAG3) {
        kerror("invalid magic byte 3 (expected 0x%02x, got 0x%02x)", ELFMAG3, hdr->e_ident[EI_MAG3]);
        return ERR_MAGIC;
    }

    /* check ELF class (32/64) */
    if(hdr->e_ident[EI_CLASS] != ELFCLASS32 && hdr->e_ident[EI_CLASS] != ELFCLASS64) {
        kerror("invalid ELF class %u", hdr->e_ident[EI_CLASS]);
        return ERR_CLASS;
    }
#ifdef ELF_FORCE_CLASS
    if(hdr->e_ident[EI_CLASS] != ELF_FORCE_CLASS) {
        kerror("unsupported ELF class %u (platform only supports class %u)", hdr->e_ident[EI_CLASS], ELF_FORCE_CLASS);
        return ERR_CLASS;
    }
#endif

    /* check endianness */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
#else
    if(hdr->e_ident[EI_DATA] != ELFDATA2MSB) {
#endif
        kerror("incorrect ELF endianness %u", hdr->e_ident[EI_DATA]);
        return ERR_ENDIAN;
    }

    /* check version */
    if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        kerror("incorrect ELF version %u encoundered in e_ident[EI_VERSION]", hdr->e_ident[EI_VERSION]);
        return ERR_VERSION;
    }

    /* check other fields outside e_ident - we can keep using the ELF32 header as e_type, e_machine and e_version locations are identical */
    bool is_elf64 = (hdr->e_ident[EI_CLASS] == ELFCLASS64); // this may come in handy later
    kdebug("ELF file is %u-bit", (is_elf64) ? 64 : 32);

    if(hdr->e_type != ET_NONE && hdr->e_type != ET_REL && hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN && hdr->e_type != ET_CORE && !(hdr->e_type >= ET_LOPROC && hdr->e_type <= ET_HIPROC) && !(!is_elf64 || (hdr->e_type >= ET_LOOS && hdr->e_type <= ET_HIOS))) {
        kerror("invalid ELF type %u", hdr->e_type);
        return ERR_TYPE;
    }

    if(hdr->e_version != EV_CURRENT) {
        kerror("incorrect ELF version %u encountered in e_version", hdr->e_version);
        return ERR_VERSION;
    }

    /* check if target architecture is supported */
    if(hdr->e_machine != EM_NONE) {
        /* only check if the architecture is not generic */
#if defined(__i386__)
        if(hdr->e_machine != EM_386) {
#elif defined(__x86_64__)
        if(hdr->e_machine != EM_AMD64) {
#elif defined(__arm__)
        if(hdr->e_machine != EM_ARM) {
#elif defined(__aarch64__)
        if(hdr->e_machine != EM_ARM64) {
#else
        if(0) { // no checking
#endif
            kerror("unsupported machine 0x%04x", hdr->e_machine);
            return ERR_MACHINE;
        }

    } else kdebug("ELF file targets generic architecture, skipping machine check");

    return (is_elf64) ? OK_ELF64 : OK_ELF32;
}

static inline uint64_t elf_read_secthdr(vfs_node_t* file, bool is_elf64, size_t shoff, size_t start, size_t count, void* buf) {
    size_t sect_len = (is_elf64) ? sizeof(Elf64_Shdr) : sizeof(Elf32_Shdr);
    return vfs_read(file, shoff + start * sect_len, count * sect_len, (uint8_t*) buf);
}

static inline uint64_t elf_read_section(vfs_node_t* file, bool is_elf64, void* secthdr, uint8_t* buf) {
    if(is_elf64) {
        Elf64_Shdr* hdr = secthdr;
        return vfs_read(file, hdr->sh_offset, hdr->sh_size, buf);
    } else {
        Elf32_Shdr* hdr = secthdr;
        return vfs_read(file, hdr->sh_offset, hdr->sh_size, buf);
    }
}

static void* elf_section(void* ptr, size_t entsize, size_t i) {
    return (void*) ((uintptr_t) ptr + entsize * i);
}

enum elf_load_result elf_load(vfs_node_t* file, const char* entry_name, uintptr_t* entry_ptr) {
    if(entry_ptr != NULL) *entry_ptr = 0; // return null pointer if entry point cannot be found

    /* read ELF header */
    Elf64_Ehdr* hdr_64 = kmalloc(sizeof(Elf64_Ehdr)); // ELF64 header
    if(hdr_64 == NULL) {
        kerror("cannot allocate memory for header");
        return ERR_ALLOC;
    }
    Elf32_Ehdr* hdr_32 = (Elf32_Ehdr*) hdr_64; // ELF32 header
    vfs_read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*) hdr_64);

    /* check header */
    enum elf_check_result chk_result = elf_check_header(hdr_64);
    bool is_elf64 = false;
    switch(chk_result) {
        case OK_ELF32: is_elf64 = false; break;
        case OK_ELF64: is_elf64 = true; break;
        default:
            kerror("elf_check_header() failed with error code %d", chk_result);
            kfree(hdr_64); return ERR_CHECK;
    }

    /* load section headers */
    size_t shentsize, shnum; // size of each section header entry and number of section headers
    if(is_elf64) {
        shentsize = hdr_64->e_shentsize;
        shnum = hdr_64->e_shnum;
    } else {
        shentsize = hdr_32->e_shentsize;
        shnum = hdr_32->e_shnum;
    }
    Elf64_Shdr* shdr_64 = kmalloc(shentsize * shnum);
    if(shdr_64 == NULL) {
        kerror("cannot allocate memory for section headers");
        kfree(hdr_64); return ERR_ALLOC;
    }
    Elf32_Shdr* shdr_32 = (Elf32_Shdr*) shdr_64;
    elf_read_secthdr(file, is_elf64, (is_elf64) ? hdr_64->e_shoff : hdr_32->e_shoff, 0, shnum, shdr_64);

    /* load section header string table */
    Elf64_Shdr* shstrtab_64 = elf_section(shdr_64, shentsize, (is_elf64) ? hdr_64->e_shstrndx : hdr_32->e_shstrndx);
    Elf32_Shdr* shstrtab_32 = (Elf32_Shdr*) shstrtab_64;
    if((is_elf64 && shstrtab_64->sh_type != SHT_STRTAB) || (!is_elf64 && shstrtab_32->sh_type != SHT_STRTAB)) {
        kerror("section header string table indicated in e_shstrndx is not of type SHT_STRTAB");
        kfree(shdr_64); kfree(hdr_64); return ERR_INVALID_DATA;
    }
    char* shstrtab_data = kmalloc((is_elf64) ? shstrtab_64->sh_size : shstrtab_32->sh_size);
    if(shstrtab_data == NULL) {
        kerror("cannot allocate memory for section header string table");
        kfree(shdr_64); kfree(hdr_64); return ERR_ALLOC;
    }
    elf_read_section(file, is_elf64, shstrtab_64, (uint8_t*) shstrtab_data);

    /* handle loading logic depending on file type */
    kinfo("file loading is not supported yet");
    // if(hdr_64->e_type == ET_REL) { // e_type offset in ELF32 and ELF64 headers are identical
    //     /* relocatable file - handle file loading based on ALLOC flag of sections */
    //     void* shdr_ent = shdr_64;
    //     for(size_t i = 0; i < shnum; i++) {
    //         size_t sh_name, sh_type, sh_flags; // section header name, type and flags
    //         uintptr_t sh_off;
    //         if(is_elf64) {
    //             Elf64_Shdr* shdr = shdr_ent;
    //             sh_name = shdr->sh_name;
    //             sh_type = shdr->sh_type;
    //             sh_flags = shdr->sh_flags;
    //             sh_off = shdr->sh_offset;
    //         } else {
    //             Elf32_Shdr* shdr = shdr_ent;
    //             sh_name = shdr->sh_name;
    //             sh_type = shdr->sh_type;
    //             sh_flags = shdr->sh_flags;
    //             sh_off = shdr->sh_offset;
    //         }
    //         kdebug("section header entry %u: %s, type %u, flags 0x%x, offset 0x%x", i, &shstrtab_data[sh_name], sh_type, sh_flags, sh_off);
    //         shdr_ent = (void*) ((uintptr_t) shdr_ent + shentsize); // next entry
    //     }
    // } else if(hdr_64->e_type == ET_EXEC) {
    //     /* executable file - handle file loading based on program header */
    // } else {
    //     kerror("unsupported file type %u", hdr_32->e_type);
    //     kfree(shdr_64); kfree(hdr_64); kfree(shstrtab_data); return ERR_UNSUPPORTED_TYPE;
    // }

    /* load symbols */
    void* shdr_ent = shdr_64;
    for(size_t i = 0; i < shnum; i++, shdr_ent = (void*) ((uintptr_t) shdr_ent + shentsize)) {
        if(is_elf64) {
            Elf64_Shdr* shdr = shdr_ent;
            if(shdr->sh_type == SHT_SYMTAB) {
                /* we've found the symbol table */
                // kdebug("symbol table at entry %u: offset 0x%llx, size %u, entsize %u", i, shdr->sh_offset, shdr->sh_size, shdr->sh_entsize);
                Elf64_Shdr* strtab_shdr = elf_section(shdr_64, shentsize, shdr->sh_link); // get its matching string table
                if(strtab_shdr->sh_type != SHT_STRTAB) {
                    kerror("symbol table at entry %u points to an invalid table at entry %u as its string table (type 0x%x)", i, shdr->sh_link, strtab_shdr->sh_type);
                    kfree(shdr_64); kfree(hdr_64); kfree(shstrtab_data); return ERR_INVALID_DATA;
                }
                char* strtab_data = kmalloc(strtab_shdr->sh_size);
                if(strtab_data == NULL) {
                    kerror("cannot allocate memory for string table");
                    kfree(shdr_64); kfree(hdr_64); kfree(shstrtab_data); return ERR_ALLOC;
                }
                elf_read_section(file, is_elf64, strtab_shdr, strtab_data); // read string table
                Elf64_Sym* symtab = kmalloc(shdr->sh_size);
                if(symtab == NULL) {
                    kerror("cannot allocate memory for symbol table");
                    kfree(strtab_data); kfree(shdr_64); kfree(hdr_64); kfree(shstrtab_data); return ERR_ALLOC;
                }
                for(size_t j = 0; j < shdr->sh_size / shdr->sh_entsize; j++) {
                    Elf64_Sym* sym = (Elf64_Sym*) ((uintptr_t) symtab + j * shdr->sh_entsize);
                    if(sym->st_other != STV_HIDDEN && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS && ELF_ST_BIND(sym->st_info) == STB_GLOBAL) {
#ifdef ELF_DEBUG_ADDSYM
                        kdebug("adding symbol %s (0x%llx)", &strtab_data[sym->st_name], sym->st_value);
#endif
                        sym_add_entry(kernel_syms, &strtab_data[sym->st_name], sym->st_value);
                    }
                    if(entry_name != NULL && entry_ptr != NULL && *entry_ptr == 0 && !strcmp(&strtab_data[sym->st_name], entry_name)) {
                        kdebug("found entry point %s at 0x%llx", entry_name, sym->st_value);
                        *entry_ptr = sym->st_value;
                    }
                }
                kfree(symtab);
                kfree(strtab_data);
            }
        } else {
            Elf32_Shdr* shdr = shdr_ent;
            if(shdr->sh_type == SHT_SYMTAB) {
                /* we've found the symbol table */
                // kdebug("symbol table at entry %u: offset 0x%x, size %u, entsize %u", i, shdr->sh_offset, shdr->sh_size, shdr->sh_entsize);
                Elf32_Shdr* strtab_shdr = elf_section(shdr_32, shentsize, shdr->sh_link); // get its matching string table
                if(strtab_shdr->sh_type != SHT_STRTAB) {
                    kerror("symbol table at entry %u points to an invalid table at entry %u as its string table (type 0x%x)", i, shdr->sh_link, strtab_shdr->sh_type);
                    kfree(shdr_64); kfree(hdr_64); kfree(shstrtab_data); return ERR_INVALID_DATA;
                }
                char* strtab_data = kmalloc(strtab_shdr->sh_size);
                if(strtab_data == NULL) {
                    kerror("cannot allocate memory for string table");
                    kfree(shdr_64); kfree(hdr_64); kfree(shstrtab_data); return ERR_ALLOC;
                }
                elf_read_section(file, is_elf64, strtab_shdr, strtab_data); // read string table
                Elf32_Sym* symtab = kmalloc(shdr->sh_size);
                if(symtab == NULL) {
                    kerror("cannot allocate memory for symbol table");
                    kfree(strtab_data); kfree(shdr_64); kfree(hdr_64); kfree(shstrtab_data); return ERR_ALLOC;
                }
                elf_read_section(file, is_elf64, shdr, symtab);
                for(size_t j = 0; j < shdr->sh_size / shdr->sh_entsize; j++) {
                    Elf32_Sym* sym = (Elf32_Sym*) ((uintptr_t) symtab + j * shdr->sh_entsize);
                    // kdebug("symbol entry %u (0x%x): %s, shndx %u, bind %u, visibility %u", j, sym, &strtab_data[sym->st_name], sym->st_shndx, ELF_ST_BIND(sym->st_info), sym->st_other);
                    if(sym->st_other != STV_HIDDEN && sym->st_shndx != SHN_UNDEF && sym->st_shndx != SHN_ABS && ELF_ST_BIND(sym->st_info) == STB_GLOBAL) {
#ifdef ELF_DEBUG_ADDSYM
                        kdebug("adding symbol %s (0x%x)", &strtab_data[sym->st_name], sym->st_value);
#endif
                        sym_add_entry(kernel_syms, &strtab_data[sym->st_name], sym->st_value);
                    }
                    if(entry_name != NULL && entry_ptr != NULL && *entry_ptr == 0 && !strcmp(&strtab_data[sym->st_name], entry_name)) {
                        kdebug("found entry point %s at 0x%x", entry_name, sym->st_value);
                        *entry_ptr = sym->st_value;
                    }
                }
                kfree(symtab);
                kfree(strtab_data);
            }
        }        
    }

    if(entry_ptr != NULL && *entry_ptr == 0) {
        if(entry_name != NULL) kdebug("cannot find entry point, taking value from ELF header");
        *entry_ptr = (is_elf64) ? hdr_64->e_entry : hdr_32->e_entry;
    }

    kinfo("file loading is complete");
    kfree(shstrtab_data);
    kfree(shdr_64);
    kfree(hdr_64);
    return LOAD_OK;
}