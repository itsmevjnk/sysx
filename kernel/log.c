#include <kernel/log.h>

#ifdef DEBUG
void kassert_print(const char* cond, const char* file, const size_t line) {
    kfprintf(kstderr, "[DBG] %s:%d: assertion %s failed\n", file, line, cond);
    // proc_abort();
    while(1);
}
#endif