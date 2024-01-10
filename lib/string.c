#include <string.h>

__attribute__((weak)) void* memcpy(void* dest, const void* src, size_t n) {
    if((uintptr_t) dest == (uintptr_t) src) return dest;
    for(size_t i = 0; i < n; i++) {
        *(uint8_t*)((uintptr_t) dest + i) = *(uint8_t*)((uintptr_t) src + i);
    }
    return dest;
}

__attribute__((weak)) void* memmove(void* dest, const void* src, size_t n) {
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

__attribute__((weak)) void* memset(void* str, int c, size_t n) {
    for(size_t i = 0; i < n; i++) {
        *(uint8_t*)((uintptr_t) str + i) = (uint8_t) c;
    }
    return str;
}

__attribute__((weak)) void* memset16(void* str, uint16_t c, size_t n) {
    for(size_t i = 0; i < n; i++) {
        *(uint16_t*)((uintptr_t) str + i) = c;
    }
    return str;
}

__attribute__((weak)) void* memset32(void* str, uint32_t c, size_t n) {
    for(size_t i = 0; i < n; i++) {
        *(uint32_t*)((uintptr_t) str + i) = c;
    }
    return str;
}

__attribute__((weak)) void* memset64(void* str, uint64_t c, size_t n) {
    for(size_t i = 0; i < n; i++) {
        *(uint64_t*)((uintptr_t) str + i) = c;
    }
    return str;
}

__attribute__((weak)) int memcmp(const void* ptr1, const void* ptr2, size_t n) {
    const uint8_t* buf1 = ptr1;
    const uint8_t* buf2 = ptr2;
    int ret = 0;
    for(size_t i = 0; i < n && ret == 0; i++) {
        ret = buf1[i] - buf2[i];
    }
    return ret;
}

__attribute__((weak)) int strcmp(const char* str1, const char* str2) {
    if((uintptr_t) str1 == (uintptr_t) str2) return 0;
    int ret = 0;
    for(size_t i = 0; ret == 0; i++) {
        ret = str1[i] - str2[i];
        if(!str1[i] || !str2[i]) break;
    }
    return ret;
}

__attribute__((weak)) int strncmp(const char* str1, const char* str2, size_t n) {
    if((uintptr_t) str1 == (uintptr_t) str2) return 0;
    int ret = 0;
    for(size_t i = 0; i < n && ret == 0; i++) {
        ret = str1[i] - str2[i];
        if(!str1[i] || !str2[i]) break;
    }
    return ret;
}

__attribute__((weak)) size_t strlen(const char* str) {
    int ret = -1;
    while(str[++ret]);
    return (size_t) ret;
}

__attribute__((weak)) char* strcpy(char* dest, const char* src) {
    return memmove(dest, src, strlen(src) + 1);
}

__attribute__((weak)) char* strncpy(char* dest, const char* src, size_t n) {
    size_t srclen = strlen(src);
    return memmove(dest, src, (srclen < n) ? (srclen + 1) : n);
}
