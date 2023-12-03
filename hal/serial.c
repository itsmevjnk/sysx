/* architecture independent portion */
#include <hal/serial.h>
#include <kernel/log.h>

#ifndef NO_SERIAL

void ser_puts(size_t p, const char* s) {
    while(*s) {
        ser_putc(p, *s);
        s++;
    }
}

size_t ser_getbuf(size_t p, char* buf, char stop) {
    size_t read = 0;
    while(1) {
        char t = ser_getc(p);
        if(t == stop) break;
        buf[read++] = t;
    }
    return read;
}

char* ser_gets(size_t p, char* buf) {
    buf[ser_getbuf(p, buf, '\n')] = 0;
    return buf;
}

uint64_t ser_devfs_read(vfs_node_t* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    (void) offset;
    for(size_t i = 0; i < size; i++) buffer[i] = (uint8_t) ser_getc(node->impl);
    return size;
}

uint64_t ser_devfs_write(vfs_node_t* node, uint64_t offset, uint64_t size, const uint8_t* buffer) {
    (void) offset;
    for(size_t i = 0; i < size; i++) ser_putc(node->impl, (uint8_t) buffer[i]);
    return size;
}

void ser_devfs_init(vfs_node_t* root) {
    size_t ports = ser_getports();
    for(size_t i = 0; i < ports; i++) {
        vfs_node_t* node = devfs_create(root, &ser_devfs_read, &ser_devfs_write, NULL, NULL, NULL, false, 0, "ttyS%u", i);
        if(node == NULL) kerror("cannot create device for serial port %u", i);
        else node->impl = i; // we'll use the impl field for storing the serial port number
    }
}

#endif
