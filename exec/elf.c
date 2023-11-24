#include <exec/elf.h>
#include <exec/syms.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <string.h>

#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/addr.h>

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

#ifndef ELF_LOAD_ADDR_START
#define ELF_LOAD_ADDR_START                     kernel_end
#endif

#ifndef ELF_LOAD_ADDR_END
#define ELF_LOAD_ADDR_END                       UINTPTR_MAX
#endif

/* decomposed ELF loading stages */

enum elf_load_result elf_read_header(vfs_node_t* file, void** bufptr, bool* is_elf64) {
    /* read ELF header */
    *bufptr = kmalloc(sizeof(Elf64_Ehdr)); // ELF64 header
    if(*bufptr == NULL) {
        kerror("cannot allocate memory for header");
        return ERR_ALLOC;
    }
    vfs_read(file, 0, sizeof(Elf64_Ehdr), (uint8_t*) *bufptr);

    /* check header */
    enum elf_check_result chk_result = elf_check_header(*bufptr);
    switch(chk_result) {
        case OK_ELF32: *is_elf64 = false; break;
        case OK_ELF64: *is_elf64 = true; break;
        default:
            kerror("elf_check_header() failed with error code %d", chk_result);
            kfree(*bufptr); return ERR_CHECK;
    }

    return LOAD_OK;
}

enum elf_load_result elf_read_shdr(vfs_node_t* file, void* hdr, bool is_elf64, void** shdr_bufptr, char** shstrtab_bufptr, size_t* e_shentsize, size_t* e_shnum) {
    Elf64_Ehdr* hdr_64 = hdr; Elf32_Ehdr* hdr_32 = hdr;
#ifndef ELF_FORCE_CLASS
    if(is_elf64) {
#endif
#if !defined(ELF_FORCE_CLASS) || ELF_FORCE_CLASS == ELFCLASS64
        *e_shentsize = ((Elf64_Ehdr*)hdr)->e_shentsize;
        *e_shnum = ((Elf64_Ehdr*)hdr)->e_shnum;
#endif
#ifndef ELF_FORCE_CLASS
    } else {
#endif
#if !defined(ELF_FORCE_CLASS) || ELF_FORCE_CLASS == ELFCLASS32
        *e_shentsize = ((Elf32_Ehdr*)hdr)->e_shentsize;
        *e_shnum = ((Elf32_Ehdr*)hdr)->e_shnum;
#endif
#ifndef ELF_FORCE_CLASS
    }
#endif
    *shdr_bufptr = kmalloc(*e_shentsize * *e_shnum);
    if(*shdr_bufptr == NULL) {
        kerror("cannot allocate memory for section headers");
        return ERR_ALLOC;
    }
    vfs_read(   file,
#if ELF_FORCE_CLASS == ELFCLASS64
                ((Elf64_Ehdr*)hdr)->e_shoff,
#elif ELF_FORCE_CLASS == ELFCLASS32
                ((Elf32_Ehdr*)hdr)->e_shoff,
#else
                (is_elf64) ? ((Elf64_Ehdr*)hdr)->e_shoff : ((Elf32_Ehdr*)hdr)->e_shoff,
#endif
                *e_shentsize * *e_shnum,
                (uint8_t*) *shdr_bufptr);

    /* load section header string table */
    void* shstrtab = (Elf64_Shdr*) ((uintptr_t) *shdr_bufptr + (
#if ELF_FORCE_CLASS == ELFCLASS64
                                    ((Elf64_Ehdr*)hdr)->e_shstrndx
#elif ELF_FORCE_CLASS == ELFCLASS32
                                    ((Elf32_Ehdr*)hdr)->e_shstrndx
#else
                                    (is_elf64) ? ((Elf64_Ehdr*)hdr)->e_shstrndx : ((Elf32_Ehdr*)hdr)->e_shstrndx
#endif
                                    ) * *e_shentsize);
#if ELF_FORCE_CLASS == ELFCLASS64
    if(((Elf64_Shdr*)shstrtab)->sh_type != SHT_STRTAB) {
#elif ELF_FORCE_CLASS == ELFCLASS32
    if(((Elf32_Shdr*)shstrtab)->sh_type != SHT_STRTAB) {
#else
    if((is_elf64 && ((Elf64_Shdr*)shstrtab)->sh_type != SHT_STRTAB) || (!is_elf64 && ((Elf32_Shdr*)shstrtab)->sh_type != SHT_STRTAB)) {
#endif
        kerror("section header string table indicated in e_shstrndx is not of type SHT_STRTAB");
        kfree(*shdr_bufptr); return ERR_INVALID_DATA;
    }
    *shstrtab_bufptr = kmalloc(
#if ELF_FORCE_CLASS == ELFCLASS64
        ((Elf64_Shdr*)shstrtab)->sh_size
#elif ELF_FORCE_CLASS == ELFCLASS32
        ((Elf32_Shdr*)shstrtab)->sh_size
#else
        (is_elf64) ? ((Elf64_Shdr*)shstrtab)->sh_size : ((Elf32_Shdr*)shstrtab)->sh_size
#endif
    );
    if(*shstrtab_bufptr == NULL) {
        kerror("cannot allocate memory for section header string table");
        kfree(*shdr_bufptr); return ERR_ALLOC;
    }
    vfs_read(   file,
#if ELF_FORCE_CLASS == ELFCLASS64
                ((Elf64_Shdr*)shstrtab)->sh_offset,
                ((Elf64_Shdr*)shstrtab)->sh_size,
#elif ELF_FORCE_CLASS == ELFCLASS32
                ((Elf32_Shdr*)shstrtab)->sh_offset,
                ((Elf32_Shdr*)shstrtab)->sh_size,
#else
                (is_elf64) ? ((Elf64_Shdr*)shstrtab)->sh_offset : ((Elf32_Shdr*)shstrtab)->sh_offset,
                (is_elf64) ? ((Elf64_Shdr*)shstrtab)->sh_size : ((Elf32_Shdr*)shstrtab)->sh_size,
#endif
                (uint8_t*) *shstrtab_bufptr);
    
    return LOAD_OK;
}

enum elf_load_result elf_load_rel(vfs_node_t* file, void* shdr, bool is_elf64, size_t e_shentsize, size_t e_shnum, void* alloc_vmm, elf_prgload_t** prgload_result, size_t* prgload_result_len) {
    *prgload_result = NULL; *prgload_result_len = 0;

    /* relocatable file - handle file loading based on ALLOC flag of sections */
    void* shdr_ent = shdr;
    for(size_t i = 0; i < e_shnum; i++, shdr_ent = (void*) ((uintptr_t) shdr_ent + e_shentsize)) {
        /* retrieve section header information */
#if ELF_FORCE_CLASS == ELFCLASS64
        size_t sh_name = ((Elf64_Shdr*) shdr_ent)->sh_name;
        size_t sh_type = ((Elf64_Shdr*) shdr_ent)->sh_type;
        size_t sh_flags = ((Elf64_Shdr*) shdr_ent)->sh_flags;
        uintptr_t sh_off = ((Elf64_Shdr*) shdr_ent)->sh_offset;
        size_t sh_size = ((Elf64_Shdr*) shdr_ent)->sh_size;
#elif ELF_FORCE_CLASS == ELFCLASS32
        size_t sh_name = ((Elf32_Shdr*) shdr_ent)->sh_name;
        size_t sh_type = ((Elf32_Shdr*) shdr_ent)->sh_type;
        size_t sh_flags = ((Elf32_Shdr*) shdr_ent)->sh_flags;
        uintptr_t sh_off = ((Elf32_Shdr*) shdr_ent)->sh_offset;
        size_t sh_size = ((Elf32_Shdr*) shdr_ent)->sh_size;
#else
        size_t sh_name = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_name : ((Elf32_Shdr*) shdr_ent)->sh_name;
        size_t sh_type = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_type : ((Elf32_Shdr*) shdr_ent)->sh_type;
        size_t sh_flags = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_flags : ((Elf32_Shdr*) shdr_ent)->sh_flags;
        uintptr_t sh_off = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_offset : ((Elf32_Shdr*) shdr_ent)->sh_offset;
        size_t sh_size = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_size : ((Elf32_Shdr*) shdr_ent)->sh_size;
#endif
        // kdebug("section header entry %u: %s, type %u, flags 0x%x, offset 0x%x, size %u", i, &shstrtab_data[sh_name], sh_type, sh_flags, sh_off, sh_size);
        
        if((sh_flags & SHF_ALLOC) && sh_size > 0 && (sh_type == SHT_PROGBITS || sh_type == SHT_NOBITS)) {
            /* memory needs to be allocated for this section */
            uintptr_t vaddr = vmm_first_free(alloc_vmm, ELF_LOAD_ADDR_START, ELF_LOAD_ADDR_END, sh_size, false);
            if(vaddr == 0) {
                kerror("cannot find virtual memory space for loading section");
                return ERR_ALLOC;
            }
            for(size_t j = 0; j < (sh_size + pmm_framesz() - 1) / pmm_framesz(); j++) {
                size_t frame = pmm_alloc_free(1); // no need for contiguous memory
                if(frame == (size_t)-1) {
                    kerror("cannot allocate memory for loading section");
                    for(size_t k = 0; k < j; k++) pmm_free(vmm_get_paddr(alloc_vmm, vaddr) / pmm_framesz());
                    vmm_unmap(vmm_current, vaddr, j * pmm_framesz());
                    return ERR_ALLOC;
                }
                // pmm_alloc(frame);
                vmm_pgmap(vmm_current, frame * pmm_framesz(), vaddr + j * pmm_framesz(), VMM_FLAGS_PRESENT | VMM_FLAGS_RW | VMM_FLAGS_CACHE | VMM_FLAGS_GLOBAL);
            }
            if(sh_type != SHT_NOBITS) vfs_read(file, sh_off, sh_size, (uint8_t*) vaddr); // copy data from file
            (*prgload_result_len)++;
            elf_prgload_t* prgload_result_old = *prgload_result;
            *prgload_result = krealloc(*prgload_result, *prgload_result_len * sizeof(elf_prgload_t));
            if(*prgload_result == NULL) {
                kerror("cannot allocate memory for program loading result");
                for(size_t j = 0; j < (sh_size + pmm_framesz() - 1) / pmm_framesz(); j++) pmm_free(vmm_get_paddr(alloc_vmm, vaddr + j * pmm_framesz()) / pmm_framesz());
                vmm_unmap(vmm_current, vaddr, ((sh_size + pmm_framesz() - 1) / pmm_framesz()) * pmm_framesz());
                kfree(prgload_result_old); return ERR_ALLOC;
            }
            (*prgload_result)[*prgload_result_len - 1].idx = i;
            (*prgload_result)[*prgload_result_len - 1].vaddr = vaddr;
            (*prgload_result)[*prgload_result_len - 1].size = sh_size;
            kdebug("section %u loaded to address 0x%x, size %u", i, vaddr, sh_size);
        }
    }

    return LOAD_OK;
}

enum elf_load_result elf_do_reloc(vfs_node_t* file, void* hdr, void* shdr, bool is_elf64, size_t e_shentsize, size_t e_shnum, elf_prgload_t* prgload_result, size_t prgload_result_len) {
    void* shdr_ent = shdr;
    void* symtab_shdr = NULL; void* symtab = NULL; size_t symtab_idx = 0, symtab_sh_entsize;
    char* strtab_data = NULL; size_t strtab_idx = 0;

    /* this function assumes that if we have loaded sections */
    for(size_t i = 0; i < e_shnum; i++, shdr_ent = (void*) ((uintptr_t) shdr_ent + e_shentsize)) {
        /* section header parameters */
#if ELF_FORCE_CLASS == ELFCLASS64
        size_t sh_type = ((Elf64_Shdr*) shdr_ent)->sh_type;
        uintptr_t sh_off = ((Elf64_Shdr*) shdr_ent)->sh_offset;
        size_t sh_size = ((Elf64_Shdr*) shdr_ent)->sh_size;
        size_t sh_entsize = ((Elf64_Shdr*) shdr_ent)->sh_entsize;
        size_t sh_link = ((Elf64_Shdr*) shdr_ent)->sh_link;
        size_t sh_info = ((Elf64_Shdr*) shdr_ent)->sh_info;
#elif ELF_FORCE_CLASS == ELFCLASS32
        size_t sh_type = ((Elf32_Shdr*) shdr_ent)->sh_type;
        uintptr_t sh_off = ((Elf32_Shdr*) shdr_ent)->sh_offset;
        size_t sh_size = ((Elf32_Shdr*) shdr_ent)->sh_size;
        size_t sh_entsize = ((Elf32_Shdr*) shdr_ent)->sh_entsize;
        size_t sh_link = ((Elf32_Shdr*) shdr_ent)->sh_link;
        size_t sh_info = ((Elf32_Shdr*) shdr_ent)->sh_info;
#else
        size_t sh_type = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_type : ((Elf32_Shdr*) shdr_ent)->sh_type;
        uintptr_t sh_off = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_offset : ((Elf32_Shdr*) shdr_ent)->sh_offset;
        size_t sh_size = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_size : ((Elf32_Shdr*) shdr_ent)->sh_size;
        size_t sh_entsize = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_entsize : ((Elf32_Shdr*) shdr_ent)->sh_entsize;
        size_t sh_link = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_link : ((Elf32_Shdr*) shdr_ent)->sh_link;
        size_t sh_info = (is_elf64) ? ((Elf64_Shdr*) shdr_ent)->sh_info : ((Elf32_Shdr*) shdr_ent)->sh_info;
#endif
        if(sh_type == SHT_REL || sh_type == SHT_RELA) {
            /* relocation section has been found */

            /* discover target section */
            elf_prgload_t* target = NULL; // target section
            for(size_t j = 0; j < prgload_result_len; j++) {
                if(prgload_result[j].idx == sh_info) {
                    target = &prgload_result[j];
                    break;
                }
            }
            if(target == NULL) {
                // kdebug("relocation section %u applies to section %u which is not loaded, skipping", i, sh_info);
                continue;
            }

            /* discover refereneced symbol table */
            if(sh_link != symtab_idx) {
                /* (re)load symbol table (part 1) */
                kfree(symtab);
                symtab_shdr = (void*) ((uintptr_t) shdr + e_shentsize * sh_link); // get symbol table referenced by the section
#if ELF_FORCE_CLASS == ELFCLASS64
                if(((Elf64_Shdr*)symtab_shdr)->sh_type != SHT_SYMTAB) {
#elif ELF_FORCE_CLASS == ELFCLASS32
                if(((Elf32_Shdr*)symtab_shdr)->sh_type != SHT_SYMTAB) {
#else
                if((is_elf64 && ((Elf64_Shdr*)symtab_shdr)->sh_type != SHT_SYMTAB) || (!is_elf64 && ((Elf32_Shdr*)symtab_shdr)->sh_type != SHT_SYMTAB)) {
#endif
                    kerror("relocation section %u refers to non-symbol table section %u", i, sh_link);
                    return ERR_INVALID_DATA;
                }
            }
#if ELF_FORCE_CLASS == ELFCLASS64
            size_t symtab_sh_size = ((Elf64_Shdr*)symtab_shdr)->sh_size;
            symtab_sh_entsize = ((Elf64_Shdr*)symtab_shdr)->sh_entsize;
#elif ELF_FORCE_CLASS == ELFCLASS32
            size_t symtab_sh_size = ((Elf32_Shdr*)symtab_shdr)->sh_size;
            symtab_sh_entsize = ((Elf32_Shdr*)symtab_shdr)->sh_entsize;
#else
            size_t symtab_sh_size = (is_elf64) ? ((Elf64_Shdr*)symtab_shdr)->sh_size : ((Elf32_Shdr*)symtab_shdr)->sh_size;
            symtab_sh_entsize = (is_elf64) ? ((Elf64_Shdr*)symtab_shdr)->sh_entsize : ((Elf32_Shdr*)symtab_shdr)->sh_entsize;
#endif
            if(sh_link != symtab_idx) {
                /* (re)load symbol table (part 2) */
                symtab = kmalloc(symtab_sh_size);
                if(symtab == NULL) {
                    kerror("cannot allocate memory for symbol table");
                    return ERR_ALLOC;
                }
                vfs_read(   file,
#if ELF_FORCE_CLASS == ELFCLASS64
                            ((Elf64_Shdr*)symtab_shdr)->sh_offset,
#elif ELF_FORCE_CLASS == ELFCLASS32
                            ((Elf32_Shdr*)symtab_shdr)->sh_offset,
#else
                            (is_elf64) ? ((Elf64_Shdr*)symtab_shdr)->sh_offset : ((Elf32_Shdr*)symtab_shdr)->sh_offset,
#endif
                            symtab_sh_size,
                            (uint8_t*) symtab);
                symtab_idx = sh_link;
            }
            size_t symtab_sh_link = (is_elf64) ? ((Elf64_Shdr*)symtab_shdr)->sh_link : ((Elf32_Shdr*)symtab_shdr)->sh_link;
            if(symtab_sh_link != strtab_idx) {
                /* load string table */
                kfree(strtab_data);
                void* strtab_shdr = (void*) ((uintptr_t) shdr + e_shentsize * symtab_sh_link); // get string table section header
#if ELF_FORCE_CLASS == ELFCLASS64
                if(((Elf64_Shdr*)strtab_shdr)->sh_type != SHT_STRTAB) {
#elif ELF_FORCE_CLASS == ELFCLASS32
                if(((Elf32_Shdr*)strtab_shdr)->sh_type != SHT_STRTAB) {
#else
                if((is_elf64 && ((Elf64_Shdr*)strtab_shdr)->sh_type != SHT_STRTAB) || (!is_elf64 && ((Elf32_Shdr*)strtab_shdr)->sh_type != SHT_STRTAB)) {
#endif
                    kerror("symbol table at entry %u points to an invalid table at entry %u as its string table", sh_link, symtab_sh_link);
                    kfree(symtab); return ERR_INVALID_DATA;
                }
#if ELF_FORCE_CLASS == ELFCLASS64
                size_t strtab_sh_size = ((Elf64_Shdr*)strtab_shdr)->sh_size;
#elif ELF_FORCE_CLASS == ELFCLASS32
                size_t strtab_sh_size = ((Elf32_Shdr*)strtab_shdr)->sh_size;
#else
                size_t strtab_sh_size = (is_elf64) ? ((Elf64_Shdr*)strtab_shdr)->sh_size : ((Elf32_Shdr*)strtab_shdr)->sh_size;
#endif
                strtab_data = kmalloc(strtab_sh_size);
                if(strtab_data == NULL) {
                    kerror("cannot allocate memory for string table");
                    return ERR_INVALID_DATA;
                }
                vfs_read(   file,
#if ELF_FORCE_CLASS == ELFCLASS64
                            ((Elf64_Shdr*)strtab_shdr)->sh_offset,
#elif ELF_FORCE_CLASS == ELFCLASS32
                            ((Elf32_Shdr*)strtab_shdr)->sh_offset,
#else
                            (is_elf64) ? ((Elf64_Shdr*)strtab_shdr)->sh_offset : ((Elf32_Shdr*)strtab_shdr)->sh_offset,
#endif
                            strtab_sh_size,
                            (uint8_t*) strtab_data);
                strtab_idx = symtab_sh_link;
            }

            /* load relocation section */
            void* rel = kmalloc(sh_size);
            if(rel == NULL) {
                kerror("cannot allocate memory for relocation section");
                return ERR_ALLOC;
            }
            vfs_read(file, sh_off, sh_size, (uint8_t*) rel);

            void* rel_ent = rel;
            for(size_t j = 0; j < sh_size / sh_entsize; j++, rel_ent = (void*) ((uintptr_t) rel_ent + sh_entsize)) {
#if ELF_FORCE_CLASS == ELFCLASS64
                uintptr_t r_offset = ((Elf64_Rel*)rel_ent)->r_offset;
                size_t r_sym = ELF64_R_SYM(((Elf64_Rel*)rel_ent)->r_info);
                size_t r_type = ELF64_R_TYPE(((Elf64_Rel*)rel_ent)->r_info);
                intptr_t r_addend = (sh_type == SHT_RELA) ? ((Elf64_Rela*)rel_ent)->r_addend : 0;
#elif ELF_FORCE_CLASS == ELFCLASS32
                uintptr_t r_offset = ((Elf32_Rel*)rel_ent)->r_offset;
                size_t r_sym = ELF32_R_SYM(((Elf32_Rel*)rel_ent)->r_info);
                size_t r_type = ELF32_R_TYPE(((Elf32_Rel*)rel_ent)->r_info);
                intptr_t r_addend = (sh_type == SHT_RELA) ? ((Elf32_Rela*)rel_ent)->r_addend : 0;
#else
                uintptr_t r_offset = (is_elf64) ? ((Elf64_Rel*)rel_ent)->r_offset : ((Elf32_Rel*)rel_ent)->r_offset;
                size_t r_sym = (is_elf64) ? ELF64_R_SYM(((Elf64_Rel*)rel_ent)->r_info) : ELF32_R_SYM(((Elf32_Rel*)rel_ent)->r_info);
                size_t r_type = (is_elf64) ? ELF64_R_TYPE(((Elf64_Rel*)rel_ent)->r_info) : ELF32_R_TYPE(((Elf32_Rel*)rel_ent)->r_info);
                intptr_t r_addend = (sh_type == SHT_RELA) ? ((is_elf64) ? ((Elf64_Rela*)rel_ent)->r_addend : ((Elf32_Rela*)rel_ent)->r_addend) : 0;
#endif
                
                /* resolve symbol */
                void* sym = (void*) ((uintptr_t) symtab + symtab_sh_entsize * r_sym);
#if ELF_FORCE_CLASS == ELFCLASS64
                size_t st_name = ((Elf64_Sym*)sym)->st_name;
                uintptr_t st_value = ((Elf64_Sym*)sym)->st_value;
                size_t st_shndx = ((Elf64_Sym*)sym)->st_shndx;
#elif ELF_FORCE_CLASS == ELFCLASS32
                size_t st_name = ((Elf32_Sym*)sym)->st_name;
                uintptr_t st_value = ((Elf32_Sym*)sym)->st_value;
                size_t st_shndx = ((Elf32_Sym*)sym)->st_shndx;
#else
                size_t st_name = (is_elf64) ? ((Elf64_Sym*)sym)->st_name : ((Elf32_Sym*)sym)->st_name;
                uintptr_t st_value = (is_elf64) ? ((Elf64_Sym*)sym)->st_value : ((Elf32_Sym*)sym)->st_value;
                size_t st_shndx = (is_elf64) ? ((Elf64_Sym*)sym)->st_shndx : ((Elf32_Sym*)sym)->st_shndx;
#endif
                if(st_shndx != SHN_ABS) {
                    /* leave absolute values as is */
                    if(st_shndx == SHN_UNDEF) {
                        /* symbol is not defined here, so try to resolve it from the kernel symbols pool instead */
                        sym_entry_t* ent = sym_resolve(kernel_syms, &strtab_data[st_name]);
                        if(ent == NULL) {
                            kdebug("symbol %s (value 0x%x) cannot be resolved", &strtab_data[st_name], st_value);
                        } else st_value = ent->addr;
                    } else {
                        /* symbol is in one of the file's functional sections, which might have been loaded */
                        bool loaded = false;
                        for(size_t k = 0; k < prgload_result_len; k++) {
                            if(prgload_result[k].idx == st_shndx) {
                                st_value += prgload_result[k].vaddr;
                                loaded = true;
                            }
                        }
                        if(!loaded) kdebug("symbol %s (value 0x%x) is not loaded, skipping address resolution", &strtab_data[st_name], st_value);
                    }
                }

                void* ref = (uint64_t*) (target->vaddr + r_offset); // will be casted to field types depending on relocation type
#if ELF_FORCE_CLASS == ELFCLASS64
                switch(((Elf64_Ehdr*)hdr)->e_machine) {
#elif ELF_FORCE_CLASS == ELFCLASS32
                switch(((Elf32_Ehdr*)hdr)->e_machine) {
#else
                switch((is_elf64) ? ((Elf64_Ehdr*)hdr)->e_machine : ((Elf32_Ehdr*)hdr)->e_machine) {
#endif
#if defined(__i386__) || defined(__x86_64__)
                    case EM_386: // x86
                        switch(r_type) {
                            case R_386_NONE: // no relocation
                                break;
                            case R_386_32: // word32 S + A
                                *((uint32_t*)ref) = (uint32_t) (st_value + *((uint32_t*)ref) + r_addend);
                                break;
                            case R_386_PC32: // word32 S + A - P
                                *((uint32_t*)ref) = (uint32_t) (st_value + *((uint32_t*)ref) - (uint32_t)ref + r_addend);
                                break;
                            case R_386_COPY: case R_386_JMP_SLOT: // word32 S
                                *((uint32_t*)ref) = (uint32_t) (st_value + r_addend);
                                break;
                            case R_386_16: // word16 S + A
                                *((uint16_t*)ref) = (uint16_t) (st_value + *((uint16_t*)ref) + r_addend);
                                break;
                            case R_386_PC16: // word16 S + A - P
                                *((uint16_t*)ref) = (uint16_t) (st_value + *((uint16_t*)ref) - (uint32_t)ref + r_addend);
                                break;
                            case R_386_8: // word8 S + A
                                *((uint8_t*)ref) = (uint8_t) (st_value + *((uint8_t*)ref) + r_addend);
                                break;
                            case R_386_PC8: // word8 S + A - P
                                *((uint8_t*)ref) = (uint8_t) (st_value + *((uint8_t*)ref) - (uint32_t)ref + r_addend);
                                break;
                            default:
                                kdebug("unsupported x86 relocation type %u", r_type);
                                break;
                        }
                        break;
#endif
#if defined(__x86_64__)
                    case EM_AMD64: // x86-64
                        switch(r_type) {
                            case R_AMD64_NONE: // no relocation
                                break;
                            case R_AMD64_64: // word64 S + A
                                *((uint64_t*)ref) = (uint64_t) (st_value + *((uint64_t*)ref) + r_addend);
                                break;
                            case R_AMD64_PC64: // word64 S + A - P
                                *((uint64_t*)ref) = (uint64_t) (st_value + *((uint64_t*)ref) - (uint64_t)ref + r_addend);
                                break;
                            case R_AMD64_32: // word32 S + A
                                *((uint32_t*)ref) = (uint32_t) (st_value + *((uint32_t*)ref) + r_addend);
                                break;
                            case R_AMD64_PC32: // word32 S + A - P
                                *((uint32_t*)ref) = (uint32_t) (st_value + *((uint32_t*)ref) - (uint32_t)ref + r_addend);
                                break;
                            case R_AMD64_COPY: case R_AMD64_JMP_SLOT: // word32 S
                                *((uint32_t*)ref) = (uint32_t) (st_value + r_addend);
                                break;
                            case R_AMD64_16: // word16 S + A
                                *((uint16_t*)ref) = (uint16_t) (st_value + *((uint16_t*)ref) + r_addend);
                                break;
                            case R_AMD64_PC16: // word16 S + A - P
                                *((uint16_t*)ref) = (uint16_t) (st_value + *((uint16_t*)ref) - (uint32_t)ref + r_addend);
                                break;
                            case R_AMD64_8: // word8 S + A
                                *((uint8_t*)ref) = (uint8_t) (st_value + *((uint8_t*)ref) + r_addend);
                                break;
                            case R_AMD64_PC8: // word8 S + A - P
                                *((uint8_t*)ref) = (uint8_t) (st_value + *((uint8_t*)ref) - (uint32_t)ref + r_addend);
                                break;
                            default:
                                kdebug("unsupported AMD64 relocation type %u", r_type);
                                break;
                        }
                        break;
#endif
                    default:
                        kdebug("relocation is not supported for machine 0x%x", (is_elf64) ? ((Elf64_Ehdr*)hdr)->e_machine : ((Elf32_Ehdr*)hdr)->e_machine);
                        break;
                }
            }

            kfree(rel);
        }
    }
    kfree(symtab); // free up symbol table
    kfree(strtab_data);

    return LOAD_OK;
}

enum elf_load_result elf_load_syms(vfs_node_t* file, void* shdr, bool is_elf64, bool is_rel, size_t e_shentsize, size_t e_shnum, elf_prgload_t* prgload_result, size_t prgload_result_len, sym_table_t* syms, char* entry_name, uintptr_t* entry_ptr) {
    if(entry_ptr != NULL) *entry_ptr = 0; // return null pointer if entry point cannot be found

    void* shdr_ent = shdr;
    for(size_t i = 0; i < e_shnum; i++, shdr_ent = (void*) ((uintptr_t) shdr_ent + e_shentsize)) {
#if ELF_FORCE_CLASS == ELFCLASS64
        size_t sh_link = ((Elf64_Shdr*)shdr_ent)->sh_link;
#elif ELF_FORCE_CLASS == ELFCLASS32
        size_t sh_link = ((Elf32_Shdr*)shdr_ent)->sh_link;
#else
        size_t sh_link = (is_elf64) ? ((Elf64_Shdr*)shdr_ent)->sh_link : ((Elf32_Shdr*)shdr_ent)->sh_link;
#endif
        void* strtab_shdr = (void*) ((uintptr_t) shdr + e_shentsize * sh_link); // get its matching string table
#if ELF_FORCE_CLASS == ELFCLASS64
        if(((Elf64_Shdr*)shdr_ent)->sh_type == SHT_SYMTAB) {
#elif ELF_FORCE_CLASS == ELFCLASS32
        if(((Elf32_Shdr*)shdr_ent)->sh_type == SHT_SYMTAB) {
#else
        if((is_elf64 && ((Elf64_Shdr*)shdr_ent)->sh_type == SHT_SYMTAB) || (!is_elf64 && ((Elf32_Shdr*)shdr_ent)->sh_type == SHT_SYMTAB)) {
#endif
            /* we've found the symbol table */
            // kdebug("symbol table at entry %u", i);
#if ELF_FORCE_CLASS == ELFCLASS64
            size_t strtab_type = ((Elf64_Shdr*)strtab_shdr)->sh_type;
            size_t sh_size = ((Elf64_Shdr*)strtab_shdr)->sh_size;
#elif ELF_FORCE_CLASS == ELFCLASS32
            size_t strtab_type = ((Elf32_Shdr*)strtab_shdr)->sh_type;
            size_t sh_size = ((Elf32_Shdr*)strtab_shdr)->sh_size;
#else
            size_t strtab_type = (is_elf64) ? ((Elf64_Shdr*)strtab_shdr)->sh_type : ((Elf32_Shdr*)strtab_shdr)->sh_type;
            size_t sh_size = (is_elf64) ? ((Elf64_Shdr*)strtab_shdr)->sh_size : ((Elf32_Shdr*)strtab_shdr)->sh_size;
#endif
            if(strtab_type != SHT_STRTAB) {
                kerror("symbol table at entry %u points to an invalid table at entry %u as its string table (type 0x%x)", i, sh_link, strtab_type);
                return ERR_INVALID_DATA;
            }
            char* strtab_data = kmalloc(sh_size);
            if(strtab_data == NULL) {
                kerror("cannot allocate memory for string table");
                return ERR_ALLOC;
            }
            vfs_read(   file,
#if ELF_FORCE_CLASS == ELFCLASS64
                        ((Elf64_Shdr*)strtab_shdr)->sh_offset,
#elif ELF_FORCE_CLASS == ELFCLASS32
                        ((Elf32_Shdr*)strtab_shdr)->sh_offset,
#else
                        (is_elf64) ? ((Elf64_Shdr*)strtab_shdr)->sh_offset : ((Elf32_Shdr*)strtab_shdr)->sh_offset,
#endif
                        sh_size,
                        (uint8_t*) strtab_data);
#if ELF_FORCE_CLASS == ELFCLASS64
            sh_size = ((Elf64_Shdr*)shdr_ent)->sh_size;
            size_t sh_entsize = ((Elf64_Shdr*)shdr_ent)->sh_entsize;
#elif ELF_FORCE_CLASS == ELFCLASS32
            sh_size = ((Elf32_Shdr*)shdr_ent)->sh_size;
            size_t sh_entsize = ((Elf32_Shdr*)shdr_ent)->sh_entsize;
#else
            sh_size = (is_elf64) ? ((Elf64_Shdr*)shdr_ent)->sh_size : ((Elf32_Shdr*)shdr_ent)->sh_size;
            size_t sh_entsize = (is_elf64) ? ((Elf64_Shdr*)shdr_ent)->sh_entsize : ((Elf32_Shdr*)shdr_ent)->sh_entsize;
#endif
            void* symtab = kmalloc(sh_size);
            if(symtab == NULL) {
                kerror("cannot allocate memory for symbol table");
                kfree(strtab_data); return ERR_ALLOC;
            }
            vfs_read(   file,
#if ELF_FORCE_CLASS == ELFCLASS64
                        ((Elf64_Shdr*)shdr_ent)->sh_offset,
#elif ELF_FORCE_CLASS == ELFCLASS32
                        ((Elf32_Shdr*)shdr_ent)->sh_offset,
#else
                        (is_elf64) ? ((Elf64_Shdr*)shdr_ent)->sh_offset : ((Elf32_Shdr*)shdr_ent)->sh_offset,
#endif
                        sh_size,
                        (uint8_t*) symtab);
            for(size_t j = 0; j < sh_size / sh_entsize; j++) {
                void* sym = (void*) ((uintptr_t) symtab + j * sh_entsize);
#if ELF_FORCE_CLASS == ELFCLASS64
                size_t st_name = ((Elf64_Sym*)sym)->st_name;
                size_t st_other = ((Elf64_Sym*)sym)->st_other;
                size_t st_shndx = ((Elf64_Sym*)sym)->st_shndx;
                size_t st_info = ((Elf64_Sym*)sym)->st_info;
                uintptr_t st_value = ((Elf64_Sym*)sym)->st_value;
#elif ELF_FORCE_CLASS == ELFCLASS32
                size_t st_name = ((Elf32_Sym*)sym)->st_name;
                size_t st_other = ((Elf32_Sym*)sym)->st_other;
                size_t st_shndx = ((Elf32_Sym*)sym)->st_shndx;
                size_t st_info = ((Elf32_Sym*)sym)->st_info;
                uintptr_t st_value = ((Elf32_Sym*)sym)->st_value;
#else
                size_t st_name = (is_elf64) ? ((Elf64_Sym*)sym)->st_name : ((Elf32_Sym*)sym)->st_name;
                size_t st_other = (is_elf64) ? ((Elf64_Sym*)sym)->st_other : ((Elf32_Sym*)sym)->st_other;
                size_t st_shndx = (is_elf64) ? ((Elf64_Sym*)sym)->st_shndx : ((Elf32_Sym*)sym)->st_shndx;
                size_t st_info = (is_elf64) ? ((Elf64_Sym*)sym)->st_info : ((Elf32_Sym*)sym)->st_info;
                uintptr_t st_value = (is_elf64) ? ((Elf64_Sym*)sym)->st_value : ((Elf32_Sym*)sym)->st_value;
#endif
                // kdebug("0x%x %s: other %u shndx %u info 0x%x value 0x%x", sym, &strtab_data[st_name], st_other, st_shndx, st_info, st_value);
                if(st_other != STV_HIDDEN && st_shndx != SHN_UNDEF && st_shndx != SHN_ABS && ELF_ST_TYPE(st_info) != STT_FILE && ELF_ST_TYPE(st_info) != STT_SECTION) {
                    if(prgload_result != NULL && is_rel) {
                        /* symbol value is an offset */
                        bool resolved = false;
                        for(size_t k = 0; k < prgload_result_len; k++) {
                            if(prgload_result[k].idx == st_shndx) {
                                st_value += prgload_result[k].vaddr;
                                resolved = true;
                                break;
                            }
                        }
                        if(!resolved) kdebug("cannot resolve symbol entry %u (%s) of symbol table at entry %u", j, &strtab_data[st_name], i);
                    }

                    // kdebug("symbol %s (0x%x)", &strtab_data[st_name], st_value);
                    
                    if(entry_name != NULL && entry_ptr != NULL && *entry_ptr == 0 && !strcmp(&strtab_data[st_name], entry_name)) {
                        kdebug("found entry point %s at 0x%x", entry_name, st_value);
                        *entry_ptr = st_value;
                    } else if(ELF_ST_BIND(st_info) == STB_GLOBAL && syms != NULL) {
#ifdef ELF_DEBUG_ADDSYM
                        kdebug("adding symbol %s (0x%x)", &strtab_data[st_name], st_value);
#endif
                        sym_add_entry(syms, &strtab_data[st_name], st_value);
                    }
                }
            }
            kfree(symtab);
            kfree(strtab_data);
        }
    }

    return LOAD_OK;
}

enum elf_load_result elf_load_ksym(vfs_node_t* file) {
    /* load and check file header */
    void* hdr; bool is_elf64;
    enum elf_load_result result = elf_read_header(file, &hdr, &is_elf64);
    if(result != LOAD_OK) return result; // failure

    /* check file type */
#if ELF_FORCE_CLASS == ELFCLASS64
    if(((Elf64_Ehdr*)hdr)->e_type != ET_EXEC) {
#elif ELF_FORCE_CLASS == ELFCLASS32
    if(((Elf32_Ehdr*)hdr)->e_type != ET_EXEC) {
#else
    if((is_elf64 && ((Elf64_Ehdr*)hdr)->e_type != ET_REL) || (!is_elf64 && ((Elf32_Ehdr*)hdr)->e_type != ET_REL)) {
#endif
        kerror("not a kernel symbols file (not ET_EXEC)");
        kfree(hdr); return ERR_UNSUPPORTED_TYPE;
    }

    /* load section headers and section header string table */
    size_t e_shentsize, e_shnum; // size of each section header entry and number of section headers
    void* shdr; char* shstrtab_data;
    result = elf_read_shdr(file, hdr, is_elf64, &shdr, &shstrtab_data, &e_shentsize, &e_shnum);
    if(result != LOAD_OK) {
        kfree(hdr);
        return result;
    }

    /* save symbols */
    result = elf_load_syms(file, shdr, is_elf64, true, e_shentsize, e_shnum, NULL, 0, kernel_syms, NULL, NULL);
    kfree(shdr); kfree(hdr); kfree(shstrtab_data);
    return result;
}

enum elf_load_result elf_load_kmod(vfs_node_t* file, elf_prgload_t** load_result, size_t* load_result_len, uintptr_t* entry_ptr) {
    /* load and check file header */
    void* hdr; bool is_elf64;
    enum elf_load_result result = elf_read_header(file, &hdr, &is_elf64);
    if(result != LOAD_OK) return result; // failure

    /* check file type */
#if ELF_FORCE_CLASS == ELFCLASS64
    if(((Elf64_Ehdr*)hdr)->e_type != ET_REL) {
#elif ELF_FORCE_CLASS == ELFCLASS32
    if(((Elf32_Ehdr*)hdr)->e_type != ET_REL) {
#else
    if((is_elf64 && ((Elf64_Ehdr*)hdr)->e_type != ET_REL) || (!is_elf64 && ((Elf32_Ehdr*)hdr)->e_type != ET_REL)) {
#endif
        kerror("not a kernel module (not ET_REL)");
        kfree(hdr); return ERR_UNSUPPORTED_TYPE;
    }

    /* load section headers and section header string table */
    size_t e_shentsize, e_shnum; // size of each section header entry and number of section headers
    void* shdr; char* shstrtab_data;
    result = elf_read_shdr(file, hdr, is_elf64, &shdr, &shstrtab_data, &e_shentsize, &e_shnum);
    if(result != LOAD_OK) {
        kfree(hdr);
        return result;
    }

    /* load file and relocate symbols */
    elf_prgload_t* prgload_result = NULL; size_t prgload_result_len = 0;
    result = elf_load_rel(file, shdr, is_elf64, e_shentsize, e_shnum, vmm_kernel, &prgload_result, &prgload_result_len);
    if(result != LOAD_OK) {
        kfree(shdr); kfree(hdr); kfree(shstrtab_data);
        return result;
    }
    result = elf_do_reloc(file, hdr, shdr, is_elf64, e_shentsize, e_shnum, prgload_result, prgload_result_len);
    if(result != LOAD_OK) {
        kfree(prgload_result); kfree(shdr); kfree(hdr); kfree(shstrtab_data);
        return result;
    }
    
    /* save symbols */
    result = elf_load_syms(file, shdr, is_elf64, true, e_shentsize, e_shnum, prgload_result, prgload_result_len, kernel_syms, ELF_KMOD_INIT_FUNC, entry_ptr);
    if(result != LOAD_OK) {
        kfree(prgload_result); kfree(shdr); kfree(hdr); kfree(shstrtab_data);
        return result;
    }
    if(entry_ptr != NULL && *entry_ptr == 0) kwarn("cannot find entry point");

    if(load_result != NULL) *load_result = prgload_result;
    else kfree(prgload_result); // no need to keep prgload_result if it's not requested
    if(load_result_len != NULL) *load_result_len = prgload_result_len;

    kinfo("kernel module loading is complete");
    kfree(shstrtab_data);
    kfree(shdr);
    kfree(hdr);
    return LOAD_OK;
}