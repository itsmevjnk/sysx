#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <mm/kheap.h>

void* kmalloc(size_t size) {
    return kmalloc_ext(size, 1, NULL);
}

void* kcalloc(size_t nitems, size_t size) {
    void* ptr = kmalloc(nitems * size);
    if(ptr != NULL) memset(ptr, 0, nitems * size);
    return ptr;
}

void* krealloc(void* ptr, size_t size) {
    return krealloc_ext(ptr, size, 1, NULL);
}

uint64_t strtoull(const char* str, char** endptr, int base) {
    if(*str == '+' || *str == '-') str++; // skip past sign
    if(base == 0) {
        /* autodetect base */
        if(*str == '0') {
            /* octal or hexadecimal */
            str++;
            if(*str == 'x' || *str == 'X') {
                str++;
                base = 16;
            } else base = 8;
        } else base = 10;
    }
    uint64_t result = 0;
    for(; *str == '\t' || *str == '\n' || *str == '\v' || *str == '\f' || *str == '\r' || *str == ' '; str++); // skip whitespace characters
    for(; *str != 0; str++) {
        char c = *str;
        if(c >= 'a' && c <= 'z') c -= 'a' - 'A'; // convert to uppercase

        if(c >= '0' && c <= '9') c = c - '0';
        else if(c >= 'A' && c <= 'Z') c = c - 'A' + 10;
        else break; // invalid character

        if(c >= base) break; // character is out of range
        
        if(result > UINT64_MAX / base) return UINT64_MAX; // will overflow
        result *= base;
        if(result > UINT64_MAX - c) return UINT64_MAX; // will overflow
        result += c;
    }

    if(endptr != NULL) *endptr = (char*)str;
    return result;
}

int64_t strtoll(const char* str, char** endptr, int base) {
    int64_t result = 0;
    for(; *str == '\t' || *str == '\n' || *str == '\v' || *str == '\f' || *str == '\r' || *str == ' '; str++); // skip whitespace characters
    bool sign = (*str == '-'); // set if the number is negative
    if(*str == '+' || *str == '-') str++; // skip past sign
    if(base == 0) {
        /* autodetect base */
        if(*str == '0') {
            /* octal or hexadecimal */
            str++;
            if(*str == 'x' || *str == 'X') {
                str++;
                base = 16;
            } else base = 8;
        } else base = 10;
    }
    for(; *str != 0; str++) {
        char c = *str;
        if(c >= 'a' && c <= 'z') c -= 'a' - 'A'; // convert to uppercase

        if(c >= '0' && c <= '9') c = c - '0';
        else if(c >= 'A' && c <= 'Z') c = c - 'A' + 10;
        else break; // invalid character

        if(c >= base) break; // character is out of range
        
        if(!sign) {
            if(result > INT64_MAX / base) return INT64_MAX; // will overflow
            result *= base;
            if(result > INT64_MAX - c) return INT64_MAX; // will overflow
            result += c;
        } else {
            if(result < INT64_MIN / base) return INT64_MIN; // will underflow
            result *= base;
            if(result < INT64_MIN + c) return INT64_MAX; // will underflow
            result -= c;
        }
    }

    if(endptr != NULL) *endptr = (char*)str;
    return result;
}

uint32_t strtoul(const char* str, char** endptr, int base) {
    uint64_t result = strtoull(str, endptr, base);
    if(result > UINT32_MAX) return UINT32_MAX;
    return result;
}

int32_t strtol(const char* str, char** endptr, int base) {
    int64_t result = strtoull(str, endptr, base);
    if(result > INT32_MAX) return INT32_MAX;
    if(result < INT32_MIN) return INT32_MIN;
    return result;
}