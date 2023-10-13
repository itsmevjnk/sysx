#include <hal/serial.h>

/* support functions */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* IO port offset */
#define SER_DATA        0
#define SER_IER         1
#define SER_BAUD_L      0 // only if DLAB is set
#define SER_BAUD_H      1 // only if DLAB is set
#define SER_IID_FIFO    2
#define SER_LCR         3
#define SER_MCR         4
#define SER_LSR         5
#define SER_MSR         6
#define SER_SCRATCH     7

/* lookup tables */
static const uint16_t ser_iobase[4] = {0x3f8, 0x2f8, 0x3e8, 0x2e8};

size_t ser_getports() {
    return 4;
}

bool ser_avail_read(size_t p) {
    return (inb(ser_iobase[p] + SER_LSR) & 1);
}

bool ser_avail_write(size_t p) {
    return (inb(ser_iobase[p] + SER_LSR) & 0x20);
}

int ser_init(size_t p, size_t datbits, size_t stpbits, size_t parity, size_t baud) {
    /* sanity checks */
    if(p >= 4 || datbits < 5 || datbits > 8 || stpbits < 1 || stpbits > 2 || parity > SER_PARITY_SPACE || !baud) return -1;

    /* check scratchpad register */
    outb(ser_iobase[p] + SER_SCRATCH, 0x74); // any byte would work
    if(inb(ser_iobase[p] + SER_SCRATCH) != 0x74) return -2; // scratchpad register check failed

    /* initialize the port now that we know there's a working controller */
    outb(ser_iobase[p] + SER_IER, 0); // disable all interrupts
    outb(ser_iobase[p] + SER_LCR, (1 << 7)); // set DLAB so we can set baud rate
    uint16_t div = 115200 / baud;
    outb(ser_iobase[p] + SER_BAUD_L, div & 0xff);
    outb(ser_iobase[p] + SER_BAUD_H, div >> 8);
    outb(ser_iobase[p] + SER_LCR, (datbits - 5) | ((stpbits - 1) << 2) | ((parity) ? ((1 << 3) | (parity - 1) << 4) : 0)); // insanity
    outb(ser_iobase[p] + SER_IID_FIFO, 0xc7); // enable 14-byte FIFO and clear it

    /* test the port in loopback mode */
    outb(ser_iobase[p] + SER_MCR, 0x1e); // set to loopback mode
    outb(ser_iobase[p] + SER_DATA, 0x55);
    if(inb(ser_iobase[p] + SER_DATA) != 0x55) return -3; // loopback test failed

    outb(ser_iobase[p] + SER_MCR, 0x0f); // set RTS/DSR and turn off loopback mode
    // TODO: use interrupt mode instead of polling
    return 0;
}

char ser_getc(size_t p) {
    while(!ser_avail_read(p));
    return (char) inb(ser_iobase[p] + SER_DATA);
}

void ser_putc(size_t p, char c) {
    while(!ser_avail_write(p));
    outb(ser_iobase[p] + SER_DATA, (uint8_t) c);
}
