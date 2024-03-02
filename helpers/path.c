#include <helpers/path.h>

const char* path_traverse(const char* path, size_t* len) {
    size_t l = 0;
    for(; *path != '/' && *path != '\0'; path++, l++);
    if(*path == '/') path++; // skip past slash
    if(len) *len = l;
    return path;
}