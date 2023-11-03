#include <fs/devfs.h>
#include <fs/vfs.h>
#include <hal/terminal.h>
#include <kernel/log.h>
#include <string.h>

uint64_t devfs_console_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    (void) node;
    (void) offset;
    for(uint64_t i = 0; i < size; i++) buffer[i] = (uint8_t) term_getc_noecho();
    return size;
}

uint64_t devfs_console_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    (void) node;
    (void) offset;
    for(uint64_t i = 0; i < size; i++) term_putc((char) buffer[i]);
    return size;
}

uint64_t devfs_null_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    (void) node;
    (void) offset;
    (void) buffer;
    return size;
}

uint64_t devfs_zero_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    (void) node;
    (void) offset;
    memset(buffer, size, 0);
    return size;
}

void devfs_std_init(vfs_node_t* root) {
    if(!vfs_is_valid(root)) {
        kdebug("specified root node is not a valid devfs node");
        return;
    }

    if(vfs_finddir(root, "null") == NULL) devfs_create(root, NULL, devfs_null_write, NULL, NULL, NULL, false, 0, "null");
    else kdebug("device named null exists, skipping null device creation");

    if(vfs_finddir(root, "zero") == NULL) devfs_create(root, devfs_zero_read, devfs_null_write, NULL, NULL, NULL, false, 0, "zero");
    else kdebug("device named zero exists, skipping zero device creation");

    if(vfs_finddir(root, "console") == NULL) devfs_create(root, devfs_console_read, devfs_console_write, NULL, NULL, NULL, false, 0, "console");
    else kdebug("device named console exists, skipping console device creation");
}
