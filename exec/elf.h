#ifndef EXEC_ELF_H
#define EXEC_ELF_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>

#include <exec/elf_machines.h>
#include <exec/elf32_types.h>
#include <exec/elf64_types.h>

#include <fs/vfs.h>

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

/*
 * enum elf_check_result elf_check_header(void* buf)
 *  Checks the validity of an ELF file's header.
 *  Returns an elf_check_result value depending on the error/success status.
 */
enum elf_check_result elf_check_header(void* buf);

/*
 * enum elf_load_result elf_load(vfs_node_t* file, const char* entry_name, uintptr_t* entry_ptr)
 *  Loads the specified ELF file (as a VFS node), and optionally retrieve
 *  the file's entry point via entry_ptr (optionally by searching for the
 *  entry symbol stored at entry_name, otherwise retrieving from the ELF
 *  header).
 *  Returns an elf_load_result value depending on the error/success status.
 */
enum elf_load_result elf_load(vfs_node_t* file, const char* entry_name, uintptr_t* entry_ptr);

#endif