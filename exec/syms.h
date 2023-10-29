#ifndef EXEC_SYMS_H
#define EXEC_SYMS_H

#include <stddef.h>
#include <stdint.h>

#define SYM_NAME_MAXLEN             47 // maximum length of a symbol's name
#define SYM_ALLOC_MIN               4 // minimum number of entries to allocate at once
/* symbol entry structure */
typedef struct 
{
    char name[SYM_NAME_MAXLEN + 1];
    uintptr_t addr;
} sym_entry_t;

/* symbol table structure */
typedef struct 
{
    sym_entry_t* syms;
    size_t count; // number of symbols stored
    size_t max_count; // number of symbol entries allocated
} sym_table_t;

/* symbol address resolution result structure */
struct sym_addr {
    sym_entry_t* sym; // the symbol's entry
    uintptr_t delta; // how many bytes the address is away from the symbol's starting address
};

extern sym_table_t* kernel_syms; // symbols from the kernel and kernel modules

/*
 * sym_table_t* sym_new_table(size_t initial_count)
 *  Creates a new symbol table structure with an initial number of entries
 *  allocated.
 *  Returns the table if it is allocated successfully, or NULL otherwise.
 */
sym_table_t* sym_new_table(size_t initial_count);

/*
 * sym_entry_t* sym_add_entry(sym_table_t* table, const char* name, uintptr_t addr)
 *  Adds a new entry to the specified table.
 *  Returns the entry's pointer on success, or NULL on failure.
 */
sym_entry_t* sym_add_entry(sym_table_t* table, const char* name, uintptr_t addr);

/*
 * sym_entry_t* sym_resolve(sym_table_t* table, const char* name)
 *  Resolves the address of a symbol in the specified symbol table, given
 *  the symbol's name.
 *  Returns the symbol's entry on success, or NULL if the symbol cannot
 *  be found.
 */
sym_entry_t* sym_resolve(sym_table_t* table, const char* name);

/*
 * struct sym_addr* sym_addr2sym(sym_table_t* table, uintptr_t addr)
 *  Finds the closest symbol to the specified address, and returns
 *  the symbol (as well as its distance from the addr).
 */
struct sym_addr* sym_addr2sym(sym_table_t* table, uintptr_t addr);

/*
 * void sym_merge(sym_table_t* dest, sym_table_t* src)
 *  Merges the symbol table located in src into the table located in dest.
 */
void sym_merge(sym_table_t* dest, sym_table_t* src);

/*
 * void sym_free_table(sym_table_t* table)
 *  Deallocates the specified table from memory.
 */
void sym_free_table(sym_table_t* table);

#endif