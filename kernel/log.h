#ifndef _KERNEL_LOG_H_
#define _KERNEL_LOG_H_

#include <stdio.h>
/* kernel log macros - should be self explanatory */

#define kinfo(fmt, ...)     do { kprintf("[INF] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define kwarn(fmt, ...)     do { kprintf("[WRN] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define kerror(fmt, ...)    do { kprintf("[ERR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)

#ifndef DEBUG
#define kdebug(fmt, ...)    do {} while(0)
#else
#define kdebug(fmt, ...)    do { kfprintf(kstderr, "[DBG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#endif

#ifndef DEBUG
#define kassert(cond)       do {} while(0)
#else
void kassert_print(const char* cond, const char* file, const size_t line);
#define kassert(cond)       (void)((cond) || (kassert_print(#cond, __FILE__, __LINE__), 0))
#endif

#endif
