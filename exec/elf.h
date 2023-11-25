#ifndef EXEC_ELF_H
#define EXEC_ELF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include <exec/elf_machines.h>
#include <exec/elf_reloc.h>
#include <exec/elf32_types.h>
#include <exec/elf64_types.h>

#include <fs/vfs.h>

#define ELF_KMOD_INIT_FUNC          "kmod_init" // kernel module initialization function name

enum elf_check_result {
    OK_ELF32 = 0, // file is OK, and is ELF32
    OK_ELF64, // file is OK, and is ELF64

    ERR_NULLBUF = INT_MIN, // null input buffer
    ERR_MAGIC, // ELF magic bytes mismatch
    ERR_CLASS, // invalid ELF class (not 32 nor 64)
    ERR_ENDIAN, // endianness mismatch (e.g. attempting to load big-endian file on a little-endian system)
    ERR_VERSION, // invalid version
    ERR_TYPE, // invalid file type
    ERR_MACHINE, // target architecture mismatch
};

enum elf_load_result {
    LOAD_OK = 0,

    ERR_CHECK = INT_MIN, // file checking failed
    ERR_ALLOC, // cannot allocate critical memory
    ERR_UNSUPPORTED_TYPE, // unsupported file type
    ERR_INVALID_DATA, // invalid data encountered
    ERR_ENTRY, // file has been loaded, but the entry point cannot be found
};

/* program loading result entry */
typedef struct {
    size_t idx; // section/program header entry index
    uintptr_t vaddr; // starting virtual address
    size_t size; // section size
} elf_prgload_t;

/*
 * enum elf_check_result elf_check_header(void* buf)
 *  Checks the validity of an ELF file's header.
 *  Returns an elf_check_result value depending on the error/success status.
 */
enum elf_check_result elf_check_header(void* buf);

/*
 * enum elf_load_result elf_load(vfs_node_t* file, void* alloc_vmm, elf_prgload_t** load_result, size_t* load_result_len, const char* entry_name, uintptr_t* entry_ptr);
 *  Loads the specified ELF kernel module (as a VFS node), and optionally
 *  retrieve its entry point via entry_ptr (by searching for the function whose
 *  name is specified in ELF_KMOD_INIT_FUNC above).
 *  Returns an elf_load_result value depending on the error/success status.
 */
enum elf_load_result elf_load_kmod(vfs_node_t* file, elf_prgload_t** load_result, size_t* load_result_len, uintptr_t* entry_ptr);

/*
 * enum elf_load_result elf_load_ksym(vfs_node_t* file)
 *  Loads the specified ELF kernel symbols file.
 *  Returns an elf_load_result value depending on the error/success status.
 */
enum elf_load_result elf_load_ksym(vfs_node_t* file);

/*
 * enum elf_load_result elf_load_exec(vfs_node_t* file, bool user, void* alloc_vmm, elf_prgload_t** load_result, size_t* load_result_len, uintptr_t* entry_ptr)
 *  Loads the specified ELF executable binary file.
 *  Returns an elf_load_result value depending on the error/success status.
 */
enum elf_load_result elf_load_exec(vfs_node_t* file, bool user, void* alloc_vmm, elf_prgload_t** load_result, size_t* load_result_len, uintptr_t* entry_ptr);

#endif