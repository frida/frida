#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>

typedef struct _FILE FILE;

extern FILE * stdout;
extern FILE * stderr;

int puts (const char * s);
int fputs (const char * restrict s, FILE * restrict stream);
int fflush (FILE * stream);
int printf (const char * restrict format, ...);
int fprintf (FILE * restrict stream, const char * restrict format, ...);
int vfprintf (FILE * restrict stream, const char * restrict format, va_list ap);

#endif
