#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

size_t strlen (const char * s);
int strcmp (const char * s1, const char * s2);
int strncmp (const char * s1, const char * s2, size_t n);
char * strstr (const char * haystack, const char * needle);
char * strchr (const char * s, int c);
char * strrchr (const char * s, int c);
void * memcpy (void * restrict dst, const void * restrict src, size_t n);
void * memmove (void * dst, const void * src, size_t len);
void * memset (void * b, int c, size_t len);

#endif
