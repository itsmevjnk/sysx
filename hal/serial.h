#ifndef _HAL_SERIAL_H_
#define _HAL_SERIAL_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <fs/devfs.h>

#ifndef NO_SERIAL

#define SER_PARITY_NONE         0
#define SER_PARITY_ODD          1
#define SER_PARITY_EVEN         2
#define SER_PARITY_MARK         3
#define SER_PARITY_SPACE        4

/* must be handled by the architecture code */

/*
 * size_t ser_getports()
 *  Returns the number of serial ports available.
 */
size_t ser_getports();

/*
 * bool ser_avail_read(size_t p)
 *  Returns true if there is data to be read from serial
 *  port p, and false otherwise.
 */
bool ser_avail_read(size_t p);

/*
 * bool ser_avail_write(size_t p)
 *  Returns true if the serial port p is ready to transmit,
 *  and false otherwise.
 */
bool ser_avail_write(size_t p);

/*
 * int ser_init(size_t p, size_t datbits, size_t stpbits, size_t parity, size_t baud)
 *  Initialize serial port p with datbits data bits, stpbits
 *  stop bit(s), parity type (see SER_PARITY_ above), and baud
 *  rate. Returns 0 on success.
 */
int ser_init(size_t p, size_t datbits, size_t stpbits, size_t parity, size_t baud);

/*
 * char ser_getc(size_t p)
 *  Waits for data to be available on serial port p and
 *  returns the first byte.
 */
char ser_getc(size_t p);

/*
 * void ser_putc(size_t p, char c)
 *  Waits until serial port p is available, then send
 *  character c for transmission.
 */
void ser_putc(size_t p, char c);

/* handled by hal/serial.c */

/*
 * void ser_puts(size_t p, const char* s)
 *  Outputs the null-terminated string s to serial port p.
 */
void ser_puts(size_t p, const char* s);

/*
 * size_t ser_getbuf(size_t p, char* buf, char stop)
 *  Reads a stream of data from serial port p into a buffer
 *  pointed by buf until the character stop is encountered.
 *  Returns the number of bytes read.
 */
size_t ser_getbuf(size_t p, char* buf, char stop);

/*
 * char* ser_gets(size_t p, char* buf)
 *  Reads an LF-terminated string from serial port p into
 *  a buffer pointed by buf, then null terminates and
 *  returns it.
 */
char* ser_gets(size_t p, char* buf);

/*
 * void ser_devfs_init(vfs_node_t* root)
 *  Creates serial port devices in the specified devfs root.
 */
void ser_devfs_init(vfs_node_t* root);

#endif

#endif
