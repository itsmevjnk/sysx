#include <helpers/basecol.h>

size_t basecol_encode(size_t n, char* buf, bool upper) {
    size_t off = 0;
    
    /* encode the string in reverse */
    while(1) {
        buf[off++] = (n % 26) + ((upper) ? 'A' : 'a');

        n /= 26;
        if(n == 0) break; // no more digits to encode
        n--;
    }
    buf[off] = '\0';

    /* reverse the string */
    for(size_t i = 0; i < (off / 2); i++) {
        char t = buf[i];
        buf[i] = buf[off - 1 - i];
        buf[off - 1 - i] = t;
    }

    return off;
}

size_t basecol_decode(const char* str, char** endptr) {
    size_t ret = 0, n = 0;
    for(;; str++, n++) {
        if(*str >= 'A' && *str <= 'Z') {
            if(n) ret = (ret + 1) * 26; // not the first character
            ret += *str - 'A';
        } else if(*str >= 'a' && *str <= 'z') {
            if(n) ret = (ret + 1) * 26; // not the first character
            ret += *str - 'a';
        } else {
            /* invalid character */
            if(endptr != NULL) *endptr = (char*) str;
            break;
        }
    }
    
    return ret;
}