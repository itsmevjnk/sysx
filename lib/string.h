#ifndef _STRING_H_
#define _STRING_H_

#include <stddef.h>
#include <stdint.h>

/*
 * void* memcpy(void* dest, const void* src, size_t n)
 *  Copies n characters from src to dest and returns dest.
 *  This is a highly inefficient implementation (due to the
 *  compilation), and an architecture-specific low level
 *  implementation is recommended.
 */
void* memcpy(void* dest, const void* src, size_t n);

/*
 * void* memmove(void* dest, const void* src, size_t n)
 *  Copies n characters from src to dest and returns dest.
 *  This is a highly inefficient implementation (due to the
 *  compilation and added safety), and an architecture-specific
 *  low level implementation is recommended.
 */
void* memmove(void* dest, const void* src, size_t n);

/*
 * void* memset(void* str, int c, size_t n)
 *  Copies the character c to the first n characters of the
 *  string str, then returns str.
 */
void* memset(void* str, int c, size_t n);

/*
 * int memcmp(const void* ptr1, const void* ptr2, size_t n)
 *  Compares the first n bytes of ptr1 and ptr2 until they
 *  differ. Returns the difference at the stop point.
 */
int memcmp(const void* ptr1, const void* ptr2, size_t n);

/*
 * int strcmp(const char* str1, const char* str2)
 *  Compares the str1 and str2 strings until str1[i] and
 *  str2[i] differ, or one of the 2 strings reaches the
 *  end. Returns the difference at the stop point.
 */
int strcmp(const char* str1, const char* str2);

/*
 * int strncmp(const char* str1, const char* str2, size_t n)
 *  Compares up to n characters of the str1 and str2 strings
 *  until str1[i] and str2[i] differ, or one of the 2 strings
 *  reaches the end. Returns the difference at the stop point.
 */
int strncmp(const char* str1, const char* str2, size_t n);

/*
 * size_t strlen(const char* str)
 *  Returns the length of the string str.
 */
size_t strlen(const char* str);

/*
 * char* strcpy(char* dest, const char* src)
 *  Copies the string src to dest, then returns dest.
 */
char* strcpy(char* dest, const char* src);

/*
 * char* strncpy(char* dest, const char* src, size_t n)
 *  Copies up to n characters from the string src to dest.
 */
char* strncpy(char* dest, const char* src, size_t n);

#endif
