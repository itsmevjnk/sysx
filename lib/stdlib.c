#include <stdlib.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <mm/kheap.h>
#include <mm/pmm.h>

void* kvalloc(size_t size) {
    return kmemalign(pmm_framesz(), size);
}

void* kcalloc(size_t nitems, size_t size) {
    void* ptr = kmalloc(nitems * size);
    if(ptr) memset(ptr, 0, nitems * size);
    return ptr;
}

uint64_t strtoull(const char* str, char** endptr, int base) {
    if(*str == '+' || *str == '-') str++; // skip past sign
    if(!base) {
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
    for(; *str; str++) {
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

    if(endptr) *endptr = (char*)str;
    return result;
}

int64_t strtoll(const char* str, char** endptr, int base) {
    int64_t result = 0;
    for(; *str == '\t' || *str == '\n' || *str == '\v' || *str == '\f' || *str == '\r' || *str == ' '; str++); // skip whitespace characters
    bool sign = (*str == '-'); // set if the number is negative
    if(*str == '+' || *str == '-') str++; // skip past sign
    if(!base) {
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
    for(; *str; str++) {
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

    if(endptr) *endptr = (char*)str;
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

/* PSEUDORANDOM NUMBER GENERATOR */

#ifndef RAND_SEEDCNT
#define RAND_SEEDCNT                5 // number of seeds
#endif

#ifndef RAND_LCG_A
#define RAND_LCG_A                  1103515245UL
#endif

#ifndef RAND_LCG_C
#define RAND_LCG_C                  12345
#endif

unsigned int rand_seed[RAND_SEEDCNT] = {0};

void srand(unsigned int seed) {
    for(size_t i = 0; i < RAND_SEEDCNT; i++) {
        /* populate the seed array using a generic linear congruential generator (LCG) */
        seed = seed * RAND_LCG_A + RAND_LCG_C;
        rand_seed[i] += seed; // this allows for more entropy for each seeding attempt
    }
}

int rand() {
    /* modified Fibonacci + LCG */
    int ret = 0;
    for(size_t i = 0; i < RAND_SEEDCNT; i++) ret += rand_seed[i];
    ret = ret * RAND_LCG_A + RAND_LCG_C;
    for(size_t i = 1; i < RAND_SEEDCNT; i++) rand_seed[i - 1] = rand_seed[i];
    rand_seed[RAND_SEEDCNT - 1] = ret;
    return (ret % RAND_MAX);
}