#include <kernel/cmdline.h>
#include <kernel/log.h>
#include <stdlib.h>
#include <string.h>

size_t kernel_argc = 0;
char** kernel_argv = NULL;
static char* kernel_cmdline = NULL;

void cmdline_init(const char* str) {
    size_t len = strlen(str);
    if(len == 0) return; // no cmdline to work on here

    kernel_cmdline = kmalloc(len + 1);
    kassert(kernel_cmdline != NULL);
    memcpy(kernel_cmdline, str, len + 1);

    /* tokenize */
    size_t start = 0;
    for(size_t end = 1; end <= len && start < len; end++) {
        if(kernel_cmdline[end] == ' ' || kernel_cmdline[end] == '\0') {
            size_t start_orig = start; start = end + 1;
            if(end - start_orig <= 1) continue;
            kernel_cmdline[end] = '\0';
            kernel_argv = krealloc(kernel_argv, ++kernel_argc * sizeof(char*));
            kernel_argv[kernel_argc - 1] = &kernel_cmdline[start_orig];
        }
    }
}

bool cmdline_find_flag(const char* key) {
    for(size_t i = 0; i < kernel_argc; i++) {
        if(!strcmp(kernel_argv[i], key)) return true;
    }
    return false;
}

const char* cmdline_find_kvp(const char* key) {
    size_t len = strlen(key);
    for(size_t i = 0; i < kernel_argc; i++) {
        if(!strncmp(kernel_argv[i], key, len) && kernel_argv[i][len] == '=') return &kernel_argv[i][len + 1];
    }
    return NULL;
}
