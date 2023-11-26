#include <fs/devfs.h>
#include <fs/vfs.h>
#include <hal/terminal.h>
#include <kernel/log.h>
#include <string.h>
#include <stdlib.h>

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

uint64_t devfs_random_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    (void) node;
    (void) offset;
    for(uint64_t i = 0; i < size; i += sizeof(unsigned int)) {
        unsigned int seed = 0;
        for(uint64_t j = i; j < sizeof(unsigned int) && i + j < size; j++) seed += buffer[j]; // collate multiple bytes to speed up seeding
        srand(seed);
    }
    return size;
}

uint64_t devfs_random_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    (void) node;
    (void) offset;
    for(uint64_t i = 0; i < size; i++) buffer[i] = rand() & 0xFF;
    return size;
}

vfs_node_t* dev_console = NULL;
vfs_node_t* dev_zero = NULL;
vfs_node_t* dev_null = NULL;
vfs_node_t* dev_random = NULL;
vfs_node_t* dev_urandom = NULL;

void devfs_std_init(vfs_node_t* root) {
    if(!vfs_is_valid(root)) {
        kdebug("specified root node is not a valid devfs node");
        return;
    }

    if(vfs_finddir(root, "null") == NULL) dev_null = devfs_create(root, NULL, devfs_null_write, NULL, NULL, NULL, false, 0, "null");
    else kdebug("device named null exists, skipping null device creation");

    if(vfs_finddir(root, "zero") == NULL) dev_zero = devfs_create(root, devfs_zero_read, devfs_null_write, NULL, NULL, NULL, false, 0, "zero");
    else kdebug("device named zero exists, skipping zero device creation");

    if(vfs_finddir(root, "console") == NULL) dev_console = devfs_create(root, devfs_console_read, devfs_console_write, NULL, NULL, NULL, false, 0, "console");
    else kdebug("device named console exists, skipping console device creation");

    if(vfs_finddir(root, "random") == NULL) dev_random = devfs_create(root, devfs_random_read, devfs_random_write, NULL, NULL, NULL, false, 0, "random");
    else kdebug("device named random exists, skipping random device creation");

    if(vfs_finddir(root, "urandom") == NULL) dev_urandom = devfs_create(root, devfs_random_read, devfs_random_write, NULL, NULL, NULL, false, 0, "urandom");
    else kdebug("device named urandom exists, skipping urandom device creation");
}
