#ifndef _KERNEL_LOG_H_
#define _KERNEL_LOG_H_

#include <stdio.h>

#ifdef DEBUG
#define _DEBUG  1
#else
#define _DEBUG  0
#endif

/* kernel log macros - should be self explanatory */

#define kinfo(fmt, ...)     do { kprintf("[INF] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define kwarn(fmt, ...)     do { kprintf("[WRN] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define kerror(fmt, ...)    do { kprintf("[ERR] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)
#define kdebug(fmt, ...)    do { if(_DEBUG) kfprintf(kstderr, "[DBG] %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); } while(0)

#endif
