#ifndef HELPERS_BASECOL_H
#define HELPERS_BASECOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * INFO:
 *  The column name encoding specifies that:
 *   - Each digit is encoded with an alphabetical letter, i.e.
 *     A = 0, B = 1, etc.
 *   - If the number n to be encoded is larger than 26, the preceding
 *     digit(s) shall be (n % 26) - 1, e.g. 26 -> AA, 27 = AB, etc.
 *  This type of encoding is often used in Unix/Linux storage device
 *  names, such as /dev/sda.
 */

/*
 * size_t basecol_encode(size_t n, char* buf, bool upper)
 *  Encodes the specified integer with column name encoding
 *  (modified base-26) and stores the encoded string in the
 *  specified buffer.
 *  The upper parameter specifies whether the output shall be
 *  uppercase or lowercase.
 *  Returns the number of bytes written, excluding the mandatory
 *  NULL termination.
 */
size_t basecol_encode(size_t n, char* buf, bool upper);

/*
 * size_t base26alp_decode(const char* str, char** endptr)
 *  Decodes the specified column name encoded string
 *  and (optionally) stores the pointer to the first character
 *  after the encoded part in *endptr (similarly to strtoul).
 *  Returns the decoded number.
 */
size_t basecol_decode(const char* str, char** endptr);

#endif
