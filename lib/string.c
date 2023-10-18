#include <string.h>

#ifndef ARCH_MEMCPY
void* memcpy(void* dest, const void* src, size_t n) {
    if((uintptr_t) dest == (uintptr_t) src) return dest;
    for(size_t i = 0; i < n; i++) {
        *(uint8_t*)((uintptr_t) dest + i) = *(uint8_t*)((uintptr_t) src + i);
    }
    return dest;
}
#endif

#ifndef ARCH_MEMMOVE
void* memmove(void* dest, const void* src, size_t n) {
    if((uintptr_t) dest == (uintptr_t) src) return dest;
    if((uintptr_t) dest < (uintptr_t) src) {
        for(size_t i = 0; i < n; i++) {
            *(uint8_t*)((uintptr_t) dest + i) = *(uint8_t*)((uintptr_t) src + i);
        }
    } else {
        for(int i = n - 1; i >= 0; i--) {
            *(uint8_t*)((uintptr_t) dest + i) = *(uint8_t*)((uintptr_t) src + i);
        }
    }
    return dest;
}
#endif

#ifndef ARCH_MEMCPY
void* memset(void* str, int c, size_t n) {
    for(size_t i = 0; i < n; i++) {
        *(uint8_t*)((uintptr_t) str + i) = (uint8_t) c;
    }
    return str;
}
#endif

#ifndef ARCH_MEMCMP
int memcmp(const void* ptr1, const void* ptr2, size_t n) {
    const uint8_t* buf1 = ptr1;
    const uint8_t* buf2 = ptr2;
    int ret = 0;
    for(size_t i = 0; i < n && ret == 0; i++) {
        ret = buf1[i] - buf2[i];
    }
    return ret;
}
#endif

#ifndef ARCH_STRCMP
int strcmp(const char* str1, const char* str2) {
    if((uintptr_t) str1 == (uintptr_t) str2) return 0;
    int ret = 0;
    for(size_t i = 0; ret == 0; i++) {
        ret = str1[i] - str2[i];
        if(!str1[i] || !str2[i]) break;
    }
    return ret;
}
#endif

#ifndef ARCH_STRNCMP
int strncmp(const char* str1, const char* str2, size_t n) {
    if((uintptr_t) str1 == (uintptr_t) str2) return 0;
    int ret = 0;
    for(size_t i = 0; i < n && ret == 0; i++) {
        ret = str1[i] - str2[i];
        if(!str1[i] || !str2[i]) break;
    }
    return ret;
}
#endif

#ifndef ARCH_STRLEN
size_t strlen(const char* str) {
    int ret = -1;
    while(str[++ret]);
    return (size_t) ret;
}
#endif

#ifndef ARCH_STRCPY
char* strcpy(char* dest, const char* src) {
    return memmove(dest, src, strlen(src) + 1);
}
#endif

#ifndef ARCH_STRNCPY
char* strncpy(char* dest, const char* src, size_t n) {
    size_t srclen = strlen(src);
    return memmove(dest, src, (srclen < n) ? (srclen + 1) : n);
}
#endif
