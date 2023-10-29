#include <exec/syms.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <string.h>

sym_table_t* kernel_syms = NULL;

sym_table_t* sym_new_table(size_t initial_count) {
    if(initial_count < SYM_ALLOC_MIN) initial_count = SYM_ALLOC_MIN;
    sym_table_t* table = kcalloc(1, sizeof(sym_table_t));
    
    if(table != NULL) {
        table->syms = kcalloc(initial_count, sizeof(sym_entry_t));
        if(table->syms != NULL) table->max_count = initial_count;
        else kerror("cannot allocate memory for %u symbol entry/entries", initial_count);
    } else kerror("cannot allocate memory for symbol table");

    return table;
}

sym_entry_t* sym_add_entry(sym_table_t* table, const char* name, uintptr_t addr) {
    sym_entry_t* entry = sym_resolve(table, name); // check if the symbol already exists
    if(entry == NULL) {
        /* entry does not exist yet - let's allocate more entry */
        if(table->count == table->max_count) {
            /* allocate more entries */
            sym_entry_t* new_syms = krealloc(table->syms, (table->max_count + SYM_ALLOC_MIN) * sizeof(sym_entry_t));
            if(new_syms == NULL) {
                kerror("cannot allocate more memory to store %u extra symbol entries", SYM_ALLOC_MIN);
                return NULL;
            }
            table->syms = new_syms;
            table->max_count += SYM_ALLOC_MIN;
        }
        entry = &table->syms[table->count];
        table->count++;
    } else kdebug("symbol %s already exists (addr 0x%x)", entry->name, entry->addr);
    strncpy(entry->name, name, SYM_NAME_MAXLEN); entry->name[SYM_NAME_MAXLEN] = 0;
    entry->addr = addr;
    return entry;
}

sym_entry_t* sym_resolve(sym_table_t* table, const char* name) {
    size_t name_len = strlen(name);
    if(name_len > SYM_NAME_MAXLEN) name_len = SYM_NAME_MAXLEN;
    for(size_t i = 0; i < table->count; i++) {
        if(!memcmp(table->syms[i].name, name, name_len + 1)) return &table->syms[i];
    }
    return NULL; // cannot find symbol
}

struct sym_addr* sym_addr2sym(sym_table_t* table, uintptr_t addr) {
    struct sym_addr* result = kcalloc(1, sizeof(struct sym_addr));
    if(result != NULL) {
        for(size_t i = 0; i < table->count; i++) {
            if(table->syms[i].addr > addr) break; // ignore symbols that cannot contain this address
            if(result->sym == NULL) {
                result->sym = &table->syms[i]; // initialize result
                result->delta = addr - result->sym->addr;
            }
            if(addr - table->syms[i].addr < result->delta) {
                result->sym = &table->syms[i];
                result->delta = addr - result->sym->addr;
            }
        }
        if(result->sym == NULL) {
            kfree(result);
            return NULL; // cannot find anything
        }
    } else kerror("cannot allocate memory for result struct");
    return result;
}

void sym_merge(sym_table_t* dest, sym_table_t* src) {
    for(size_t i = 0; i < src->count; i++) sym_add_entry(dest, src->syms[i].name, src->syms[i].addr);
}

void sym_free_table(sym_table_t* table) {
    kfree(table->syms); // will also work if syms == NULL
    kfree(table);
}