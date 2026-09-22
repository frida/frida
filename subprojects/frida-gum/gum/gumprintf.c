/* $Id: snprintf.c,v 1.9 2008/01/20 14:02:00 holger Exp $ */

/*
 * Copyright (c) 1995 Patrick Powell.
 *
 * This code is based on code written by Patrick Powell <papowell@astart.com>.
 * It may be used for any purpose as long as this notice remains intact on all
 * source code distributions.
 */

/*
 * Copyright (c) 2008 Holger Weiss.
 *
 * This version of the code is maintained by Holger Weiss <holger@jhweiss.de>.
 * My changes to the code may freely be used, modified and/or redistributed for
 * any purpose.  It would be nice if additions and fixes to this file (including
 * trivial code cleanups) would be sent back in order to let me include them in
 * the version available at <http://www.jhweiss.de/software/snprintf.html>.
 * However, this is not a requirement for using or redistributing (possibly
 * modified) versions of this file, nor is leaving this notice intact mandatory.
 */

/*
 * History
 *
 * 2014-05-01 Ole André Vadla Ravnås <oleavr@nowsecure.com> for frida-gum:
 *
 *   - Use frida-gum's memory allocator
 *   - Drop locale support
 *   - Use GLib
 *   - Rely on modern toolchains
 *
 * 2008-01-20 Holger Weiss <holger@jhweiss.de> for C99-snprintf 1.1:
 *
 *   Fixed the detection of infinite floating point values on IRIX (and
 *   possibly other systems) and applied another few minor cleanups.
 *
 * 2008-01-06 Holger Weiss <holger@jhweiss.de> for C99-snprintf 1.0:
 *
 *   Added a lot of new features, fixed many bugs, and incorporated various
 *   improvements done by Andrew Tridgell <tridge@samba.org>, Russ Allbery
 *   <rra@stanford.edu>, Hrvoje Niksic <hniksic@xemacs.org>, Damien Miller
 *   <djm@mindrot.org>, and others for the Samba, INN, Wget, and OpenSSH
 *   projects.  The additions include: support the "e", "E", "g", "G", and
 *   "F" conversion specifiers (and use conversion style "f" or "F" for the
 *   still unsupported "a" and "A" specifiers); support the "hh", "ll", "j",
 *   "t", and "z" length modifiers; support the "#" flag and the (non-C99)
 *   "'" flag; use localeconv(3) (if available) to get both the current
 *   locale's decimal point character and the separator between groups of
 *   digits; fix the handling of various corner cases of field width and
 *   precision specifications; fix various floating point conversion bugs;
 *   handle infinite and NaN floating point values; don't attempt to write to
 *   the output buffer (which may be NULL) if a size of zero was specified;
 *   check for integer overflow of the field width, precision, and return
 *   values and during the floating point conversion; use the OUTCHAR() macro
 *   instead of a function for better performance; provide asprintf(3) and
 *   vasprintf(3) functions; add new test cases.  The replacement functions
 *   have been renamed to use an "rpl_" prefix, the function calls in the
 *   main project (and in this file) must be redefined accordingly for each
 *   replacement function which is needed (by using Autoconf or other means).
 *   Various other minor improvements have been applied and the coding style
 *   was cleaned up for consistency.
 *
 * 2007-07-23 Holger Weiss <holger@jhweiss.de> for Mutt 1.5.13:
 *
 *   C99 compliant snprintf(3) and vsnprintf(3) functions return the number
 *   of characters that would have been written to a sufficiently sized
 *   buffer (excluding the '\0').  The original code simply returned the
 *   length of the resulting output string, so that's been fixed.
 *
 * 1998-03-05 Michael Elkins <me@mutt.org> for Mutt 0.90.8:
 *
 *   The original code assumed that both snprintf(3) and vsnprintf(3) were
 *   missing.  Some systems only have snprintf(3) but not vsnprintf(3), so
 *   the code is now broken down under HAVE_SNPRINTF and HAVE_VSNPRINTF.
 *
 * 1998-01-27 Thomas Roessler <roessler@does-not-exist.org> for Mutt 0.89i:
 *
 *   The PGP code was using unsigned hexadecimal formats.  Unfortunately,
 *   unsigned formats simply didn't work.
 *
 * 1997-10-22 Brandon Long <blong@fiction.net> for Mutt 0.87.1:
 *
 *   Ok, added some minimal floating point support, which means this probably
 *   requires libm on most operating systems.  Don't yet support the exponent
 *   (e,E) and sigfig (g,G).  Also, fmtint() was pretty badly broken, it just
 *   wasn't being exercised in ways which showed it, so that's been fixed.
 *   Also, formatted the code to Mutt conventions, and removed dead code left
 *   over from the original.  Also, there is now a builtin-test, run with:
 *   gcc -DTEST_SNPRINTF -o snprintf snprintf.c -lm && ./snprintf
 *
 * 2996-09-15 Brandon Long <blong@fiction.net> for Mutt 0.43:
 *
 *   This was ugly.  It is still ugly.  I opted out of floating point
 *   numbers, but the formatter understands just about everything from the
 *   normal C string format, at least as far as I can tell from the Solaris
 *   2.5 printf(3S) man page.
 */

/*
 * ToDo
 *
 * - Add wide character support.
 * - Add support for "%a" and "%A" conversions.
 * - Create test routines which predefine the expected results.  Our test cases
 *   usually expose bugs in system implementations rather than in ours :-)
 */

#include "gumprintf.h"

#include "gummemory.h"

#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#if defined (HAVE_UNSIGNED_LONG_LONG_INT) || defined (_MSC_VER)
#define ULLONG unsigned long long int
#else
#define ULLONG unsigned long int
#endif

#if defined (HAVE_LONG_DOUBLE)
# define LDOUBLE long double
# define LDOUBLE_MIN_10_EXP LDBL_MIN_10_EXP
# define LDOUBLE_MAX_10_EXP LDBL_MAX_10_EXP
#else
# define LDOUBLE double
# define LDOUBLE_MIN_10_EXP DBL_MIN_10_EXP
# define LDOUBLE_MAX_10_EXP DBL_MAX_10_EXP
#endif

#if defined (HAVE_LONG_LONG_INT)
# define LLONG long long int
#else
# define LLONG long int
#endif

#ifndef ERANGE
# define ERANGE E2BIG
#endif
#ifndef EOVERFLOW
# define EOVERFLOW ERANGE
#endif

/*
 * Buffer size to hold the octal string representation of UINT128_MAX without
 * nul-termination ("3777777777777777777777777777777777777777777").
 */
#define MAX_CONVERT_LENGTH      43

/* Format read states. */
#define PRINT_S_DEFAULT         0
#define PRINT_S_FLAGS           1
#define PRINT_S_WIDTH           2
#define PRINT_S_DOT             3
#define PRINT_S_PRECISION       4
#define PRINT_S_MOD             5
#define PRINT_S_CONV            6

/* Format flags. */
#define PRINT_F_MINUS           (1 << 0)
#define PRINT_F_PLUS            (1 << 1)
#define PRINT_F_SPACE           (1 << 2)
#define PRINT_F_NUM             (1 << 3)
#define PRINT_F_ZERO            (1 << 4)
#define PRINT_F_QUOTE           (1 << 5)
#define PRINT_F_UP              (1 << 6)
#define PRINT_F_UNSIGNED        (1 << 7)
#define PRINT_F_TYPE_G          (1 << 8)
#define PRINT_F_TYPE_E          (1 << 9)

/* Conversion flags. */
#define PRINT_C_CHAR            1
#define PRINT_C_SHORT           2
#define PRINT_C_LONG            3
#define PRINT_C_LLONG           4
#define PRINT_C_LDOUBLE         5
#define PRINT_C_SIZE            6
#define PRINT_C_PTRDIFF         7
#define PRINT_C_INTMAX          8

#ifndef CHARTOINT
# define CHARTOINT(ch) (ch - '0')
#endif
#ifndef ISDIGIT
# define ISDIGIT(ch) ('0' <= (guchar) ch && (guchar) ch <= '9')
#endif
#ifndef ISNAN
# define ISNAN(x) (x != x)
#endif
#ifndef ISINF
# define ISINF(x) (x != 0.0 && x + x == x)
#endif

#define OUTCHAR(str, len, size, ch)                                            \
do                                                                             \
{                                                                              \
  if (len + 1 < size)                                                          \
    str[len] = ch;                                                             \
  (len)++;                                                                     \
}                                                                              \
while (/* CONSTCOND */ 0)

#ifndef GUM_USE_SYSTEM_ALLOC
static void fmtstr (gchar *, gsize *, gsize, const gchar *, gint, gint, gint);
static void fmtint (gchar *, gsize *, gsize, intmax_t, gint, gint, gint, gint);
static void fmtflt (gchar *, gsize *, gsize, LDOUBLE, gint, gint, gint, gint *);
static void printsep (gchar *, gsize *, gsize);
static gint getnumsep (gint);
static gint getexponent (LDOUBLE);
static gint convert (uintmax_t, gchar *, gsize, gint, gint);
static uintmax_t cast (LDOUBLE);
static uintmax_t myround (LDOUBLE);
static LDOUBLE mypow10 (gint);
#endif

gint
gum_vsnprintf (gchar * str,
               gsize size,
               const gchar * format,
               va_list args)
{
#ifndef GUM_USE_SYSTEM_ALLOC
  LDOUBLE fvalue;
  intmax_t value;
  guchar cvalue;
  const gchar * strvalue;
  intmax_t * intmaxptr;
  ptrdiff_t * ptrdiffptr;
  gssize * sizeptr;
  LLONG * llongptr;
  long int * longptr;
  gint * intptr;
  gshort * shortptr;
  gint8 * charptr;
  gsize len = 0;
  gint overflow = 0;
  gint base = 0;
  gint cflags = 0;
  gint flags = 0;
  gint width = 0;
  gint precision = -1;
  gint state = PRINT_S_DEFAULT;
  char ch = *format++;

  /*
   * C99 says: "If `n' is zero, nothing is written, and `s' may be a null
   * pointer." (7.19.6.5, 2)  We're forgiving and allow a NULL pointer
   * even if a size larger than zero was specified.  At least NetBSD's
   * snprintf(3) does the same, as well as other versions of this file.
   * (Though some of these versions will write to a non-NULL buffer even
   * if a size of zero was specified, which violates the standard.)
   */
  if (str == NULL && size != 0)
    size = 0;

  while (ch != '\0')
  {
    switch (state)
    {
      case PRINT_S_DEFAULT:
        if (ch == '%')
          state = PRINT_S_FLAGS;
        else
          OUTCHAR (str, len, size, ch);
        ch = *format++;
        break;
      case PRINT_S_FLAGS:
        switch (ch)
        {
          case '-':
            flags |= PRINT_F_MINUS;
            ch = *format++;
            break;
          case '+':
            flags |= PRINT_F_PLUS;
            ch = *format++;
            break;
          case ' ':
            flags |= PRINT_F_SPACE;
            ch = *format++;
            break;
          case '#':
            flags |= PRINT_F_NUM;
            ch = *format++;
            break;
          case '0':
            flags |= PRINT_F_ZERO;
            ch = *format++;
            break;
          case '\'': /* SUSv2 flag (not in C99). */
            flags |= PRINT_F_QUOTE;
            ch = *format++;
            break;
          default:
            state = PRINT_S_WIDTH;
            break;
        }
        break;
      case PRINT_S_WIDTH:
        if (ISDIGIT (ch))
        {
          ch = CHARTOINT (ch);
          if (width > (INT_MAX - ch) / 10)
          {
            overflow = 1;
            goto out;
          }
          width = 10 * width + ch;
          ch = *format++;
        }
        else if (ch == '*')
        {
          /*
           * C99 says: "A negative field width argument is
           * taken as a `-' flag followed by a positive
           * field width." (7.19.6.1, 5)
           */
          if ((width = va_arg (args, gint)) < 0)
          {
            flags |= PRINT_F_MINUS;
            width = -width;
          }
          ch = *format++;
          state = PRINT_S_DOT;
        }
        else
        {
          state = PRINT_S_DOT;
        }
        break;
      case PRINT_S_DOT:
        if (ch == '.')
        {
          state = PRINT_S_PRECISION;
          ch = *format++;
        }
        else
        {
          state = PRINT_S_MOD;
        }
        break;
      case PRINT_S_PRECISION:
        if (precision == -1)
          precision = 0;
        if (ISDIGIT (ch))
        {
          ch = CHARTOINT (ch);
          if (precision > (INT_MAX - ch) / 10)
          {
            overflow = 1;
            goto out;
          }
          precision = 10 * precision + ch;
          ch = *format++;
        }
        else if (ch == '*')
        {
          /*
           * C99 says: "A negative precision argument is
           * taken as if the precision were omitted."
           * (7.19.6.1, 5)
           */
          if ((precision = va_arg (args, gint)) < 0)
            precision = -1;
          ch = *format++;
          state = PRINT_S_MOD;
        }
        else
        {
          state = PRINT_S_MOD;
        }
        break;
      case PRINT_S_MOD:
        switch (ch)
        {
          case 'h':
            ch = *format++;
            if (ch == 'h') /* It's a char. */
            {
              ch = *format++;
              cflags = PRINT_C_CHAR;
            }
            else
            {
              cflags = PRINT_C_SHORT;
            }
            break;
          case 'l':
            ch = *format++;
            if (ch == 'l') /* It's a long long. */
            {
              ch = *format++;
              cflags = PRINT_C_LLONG;
            }
            else
            {
              cflags = PRINT_C_LONG;
            }
            break;
          case 'L':
            cflags = PRINT_C_LDOUBLE;
            ch = *format++;
            break;
          case 'j':
            cflags = PRINT_C_INTMAX;
            ch = *format++;
            break;
          case 't':
            cflags = PRINT_C_PTRDIFF;
            ch = *format++;
            break;
          case 'z':
            cflags = PRINT_C_SIZE;
            ch = *format++;
            break;
        }
        state = PRINT_S_CONV;
        break;
      case PRINT_S_CONV:
        switch (ch)
        {
          case 'd':
            /* FALLTHROUGH */
          case 'i':
            switch (cflags)
            {
              case PRINT_C_CHAR:
                value = (gchar) va_arg (args, gint);
                break;
              case PRINT_C_SHORT:
                value = (gshort) va_arg (args, gint);
                break;
              case PRINT_C_LONG:
                value = va_arg (args, long int);
                break;
              case PRINT_C_LLONG:
                value = va_arg (args, LLONG);
                break;
              case PRINT_C_SIZE:
                value = va_arg (args, gssize);
                break;
              case PRINT_C_INTMAX:
                value = va_arg (args, intmax_t);
                break;
              case PRINT_C_PTRDIFF:
                value = va_arg (args, ptrdiff_t);
                break;
              default:
                value = va_arg (args, gint);
                break;
            }
            fmtint (str, &len, size, value, 10, width, precision, flags);
            break;
          case 'X':
            flags |= PRINT_F_UP;
            /* FALLTHROUGH */
          case 'x':
            base = 16;
            /* FALLTHROUGH */
          case 'o':
            if (base == 0)
              base = 8;
            /* FALLTHROUGH */
          case 'u':
            if (base == 0)
              base = 10;
            flags |= PRINT_F_UNSIGNED;
            switch (cflags)
            {
              case PRINT_C_CHAR:
                value = (guchar) va_arg (args, guint);
                break;
              case PRINT_C_SHORT:
                value = (gushort) va_arg (args, guint);
                break;
              case PRINT_C_LONG:
                value = va_arg (args, unsigned long int);
                break;
              case PRINT_C_LLONG:
                value = va_arg (args, ULLONG);
                break;
              case PRINT_C_SIZE:
                value = va_arg (args, gsize);
                break;
              case PRINT_C_INTMAX:
                value = va_arg (args, uintmax_t);
                break;
              case PRINT_C_PTRDIFF:
                value = va_arg (args, ptrdiff_t);
                break;
              default:
                value = va_arg (args, guint);
                break;
            }
            fmtint (str, &len, size, value, base, width, precision, flags);
            break;
          case 'A':
            /* Not yet supported, we'll use "%F". */
            /* FALLTHROUGH */
          case 'F':
            flags |= PRINT_F_UP;
          case 'a':
            /* Not yet supported, we'll use "%f". */
            /* FALLTHROUGH */
          case 'f':
            if (cflags == PRINT_C_LDOUBLE)
              fvalue = va_arg (args, LDOUBLE);
            else
              fvalue = va_arg (args, double);
            fmtflt (str, &len, size, fvalue, width, precision, flags,
                &overflow);
            if (overflow)
              goto out;
            break;
          case 'E':
            flags |= PRINT_F_UP;
            /* FALLTHROUGH */
          case 'e':
            flags |= PRINT_F_TYPE_E;
            if (cflags == PRINT_C_LDOUBLE)
              fvalue = va_arg (args, LDOUBLE);
            else
              fvalue = va_arg (args, double);
            fmtflt (str, &len, size, fvalue, width, precision, flags,
                &overflow);
            if (overflow)
              goto out;
            break;
          case 'G':
            flags |= PRINT_F_UP;
            /* FALLTHROUGH */
          case 'g':
            flags |= PRINT_F_TYPE_G;
            if (cflags == PRINT_C_LDOUBLE)
              fvalue = va_arg (args, LDOUBLE);
            else
              fvalue = va_arg (args, double);
            /*
             * If the precision is zero, it is treated as
             * one (cf. C99: 7.19.6.1, 8).
             */
            if (precision == 0)
              precision = 1;
            fmtflt (str, &len, size, fvalue, width, precision, flags,
                &overflow);
            if (overflow)
              goto out;
            break;
          case 'c':
            cvalue = va_arg (args, gint);
            OUTCHAR (str, len, size, cvalue);
            break;
          case 's':
            strvalue = va_arg (args, gchar *);
            fmtstr (str, &len, size, strvalue, width, precision, flags);
            break;
          case 'p':
            /*
             * C99 says: "The value of the pointer is
             * converted to a sequence of printing
             * characters, in an implementation-defined
             * manner." (C99: 7.19.6.1, 8)
             */
            if ((strvalue = va_arg (args, void *)) == NULL)
            {
              /*
               * We use the glibc format.  BSD prints
               * "0x0", SysV "0".
               */
              fmtstr (str, &len, size, "(nil)", width, -1, flags);
            }
            else
            {
              /*
               * We use the BSD/glibc format.  SysV
               * omits the "0x" prefix (which we emit
               * using the PRINT_F_NUM flag).
               */
              flags |= PRINT_F_NUM;
              flags |= PRINT_F_UNSIGNED;
              fmtint (str, &len, size, (uintptr_t) strvalue, 16, width,
                  precision, flags);
            }
            break;
          case 'n':
            switch (cflags)
            {
              case PRINT_C_CHAR:
                charptr = va_arg (args, gint8 *);
                *charptr = len;
                break;
              case PRINT_C_SHORT:
                shortptr = va_arg (args, gshort *);
                *shortptr = len;
                break;
              case PRINT_C_LONG:
                longptr = va_arg (args, long int *);
                *longptr = len;
                break;
              case PRINT_C_LLONG:
                llongptr = va_arg (args, LLONG *);
                *llongptr = len;
                break;
              case PRINT_C_SIZE:
                /*
                 * C99 says that with the "z" length
                 * modifier, "a following `n' conversion
                 * specifier applies to a pointer to a
                 * signed integer type corresponding to
                 * gsize argument." (7.19.6.1, 7)
                 */
                sizeptr = va_arg (args, gssize *);
                *sizeptr = len;
                break;
              case PRINT_C_INTMAX:
                intmaxptr = va_arg (args, intmax_t *);
                *intmaxptr = len;
                break;
              case PRINT_C_PTRDIFF:
                ptrdiffptr = va_arg (args, ptrdiff_t *);
                *ptrdiffptr = len;
                break;
              default:
                intptr = va_arg (args, gint *);
                *intptr = len;
                break;
            }
            break;
          case '%': /* Print a "%" character verbatim. */
            OUTCHAR (str, len, size, ch);
            break;
          default: /* Skip other characters. */
            break;
        }
        ch = *format++;
        state = PRINT_S_DEFAULT;
        base = cflags = flags = width = 0;
        precision = -1;
        break;
    }
  }

out:
  if (len < size)
    str[len] = '\0';
  else if (size > 0)
    str[size - 1] = '\0';

  if (overflow || len >= INT_MAX)
  {
    errno = overflow ? EOVERFLOW : ERANGE;
    return -1;
  }
  return (gint) len;
#else
  return g_vsnprintf (str, size, format, args);
#endif
}

gint
gum_vasprintf (gchar ** ret,
               const gchar * format,
               va_list ap)
{
  gsize size;
  gint len;
  va_list aq;

  va_copy (aq, ap);
  len = gum_vsnprintf (NULL, 0, format, aq);
  va_end (aq);
  if (len < 0 || (*ret = g_malloc (size = len + 1)) == NULL)
    return -1;

  return gum_vsnprintf (*ret, size, format, ap);
}

gint
gum_snprintf (gchar * str,
              gsize size,
              const gchar * format,
              ...)
{
  va_list ap;
  gint len;

  va_start (ap, format);
  len = gum_vsnprintf (str, size, format, ap);
  va_end (ap);

  return len;
}

gint
gum_asprintf (gchar ** ret,
              const gchar * format,
              ...)
{
  va_list ap;
  gint len;

  va_start (ap, format);
  len = gum_vasprintf (ret, format, ap);
  va_end (ap);

  return len;
}

#ifndef GUM_USE_SYSTEM_ALLOC

static void
fmtstr (gchar * str,
        gsize * len,
        gsize size,
        const gchar * value,
        gint width,
        gint precision,
        gint flags)
{
  gint padlen, strln; /* Amount to pad. */
  gint noprecision = (precision == -1);

  if (value == NULL) /* We're forgiving. */
    value = "(null)";

  /* If a precision was specified, don't read the string past it. */
  for (strln = 0; value[strln] != '\0' &&
      (noprecision || strln < precision); strln++)
    continue;

  if ((padlen = width - strln) < 0)
    padlen = 0;
  if (flags & PRINT_F_MINUS) /* Left justify. */
    padlen = -padlen;

  while (padlen > 0) /* Leading spaces. */
  {
    OUTCHAR (str, *len, size, ' ');
    padlen--;
  }
  while (*value != '\0' && (noprecision || precision-- > 0))
  {
    OUTCHAR (str, *len, size, *value);
    value++;
  }
  while (padlen < 0) /* Trailing spaces. */
  {
    OUTCHAR (str, *len, size, ' ');
    padlen++;
  }
}

static void
fmtint (gchar * str,
        gsize * len,
        gsize size,
        intmax_t value,
        gint base,
        gint width,
        gint precision,
        gint flags)
{
  uintmax_t uvalue;
  char iconvert[MAX_CONVERT_LENGTH];
  char sign = 0;
  char hexprefix = 0;
  gint spadlen = 0; /* Amount to space pad. */
  gint zpadlen = 0; /* Amount to zero pad. */
  gint pos;
  gint separators = (flags & PRINT_F_QUOTE);
  gint noprecision = (precision == -1);

  if (flags & PRINT_F_UNSIGNED)
  {
    uvalue = value;
  }
  else
  {
    uvalue = (value >= 0) ? value : -value;
    if (value < 0)
      sign = '-';
    else if (flags & PRINT_F_PLUS) /* Do a sign. */
      sign = '+';
    else if (flags & PRINT_F_SPACE)
      sign = ' ';
  }

  pos = convert (uvalue, iconvert, sizeof (iconvert), base, flags & PRINT_F_UP);

  if (flags & PRINT_F_NUM && uvalue != 0)
  {
    /*
     * C99 says: "The result is converted to an `alternative form'.
     * For `o' conversion, it increases the precision, if and only
     * if necessary, to force the first digit of the result to be a
     * zero (if the value and precision are both 0, a single 0 is
     * printed).  For `x' (or `X') conversion, a nonzero result has
     * `0x' (or `0X') prefixed to it." (7.19.6.1, 6)
     */
    switch (base)
    {
      case 8:
        if (precision <= pos)
          precision = pos + 1;
        break;
      case 16:
        hexprefix = (flags & PRINT_F_UP) ? 'X' : 'x';
        break;
    }
  }

  if (separators) /* Get the number of group separators we'll print. */
    separators = getnumsep (pos);

  zpadlen = precision - pos - separators;
  spadlen = width                         /* Minimum field width. */
      - separators                        /* Number of separators. */
      - MAX(precision, pos)               /* Number of integer digits. */
      - ((sign != 0) ? 1 : 0)             /* Will we print a sign? */
      - ((hexprefix != 0) ? 2 : 0);       /* Will we print a prefix? */

  if (zpadlen < 0)
    zpadlen = 0;
  if (spadlen < 0)
    spadlen = 0;

  /*
   * C99 says: "If the `0' and `-' flags both appear, the `0' flag is
   * ignored.  For `d', `i', `o', `u', `x', and `X' conversions, if a
   * precision is specified, the `0' flag is ignored." (7.19.6.1, 6)
   */
  if (flags & PRINT_F_MINUS) /* Left justify. */
  {
    spadlen = -spadlen;
  }
  else if (flags & PRINT_F_ZERO && noprecision)
  {
    zpadlen += spadlen;
    spadlen = 0;
  }
  while (spadlen > 0) /* Leading spaces. */
  {
    OUTCHAR (str, *len, size, ' ');
    spadlen--;
  }
  if (sign != 0) /* Sign. */
    OUTCHAR (str, *len, size, sign);
  if (hexprefix != 0) /* A "0x" or "0X" prefix. */
  {
    OUTCHAR (str, *len, size, '0');
    OUTCHAR (str, *len, size, hexprefix);
  }
  while (zpadlen > 0) /* Leading zeros. */
  {
    OUTCHAR (str, *len, size, '0');
    zpadlen--;
  }
  while (pos > 0) /* The actual digits. */
  {
    pos--;
    OUTCHAR (str, *len, size, iconvert[pos]);
    if (separators > 0 && pos > 0 && pos % 3 == 0)
      printsep (str, len, size);
  }
  while (spadlen < 0) /* Trailing spaces. */
  {
    OUTCHAR (str, *len, size, ' ');
    spadlen++;
  }
}

static void
fmtflt (gchar * str,
        gsize * len,
        gsize size,
        LDOUBLE fvalue,
        gint width,
        gint precision,
        gint flags,
        gint * overflow)
{
  LDOUBLE ufvalue;
  uintmax_t intpart;
  uintmax_t fracpart;
  uintmax_t mask;
  const gchar * infnan = NULL;
  char iconvert[MAX_CONVERT_LENGTH];
  char fconvert[MAX_CONVERT_LENGTH];
  char econvert[5]; /* "e-308" (without nul-termination). */
  char esign = 0;
  char sign = 0;
  gint leadfraczeros = 0;
  gint exponent = 0;
  gint emitpoint = 0;
  gint omitzeros = 0;
  gint omitcount = 0;
  gint padlen = 0;
  gint epos = 0;
  gint fpos = 0;
  gint ipos = 0;
  gint separators = (flags & PRINT_F_QUOTE);
  gint estyle = (flags & PRINT_F_TYPE_E);

  /*
   * AIX' man page says the default is 0, but C99 and at least Solaris'
   * and NetBSD's man pages say the default is 6, and sprintf(3) on AIX
   * defaults to 6.
   */
  if (precision == -1)
    precision = 6;

  if (fvalue < 0.0)
    sign = '-';
  else if (flags & PRINT_F_PLUS) /* Do a sign. */
    sign = '+';
  else if (flags & PRINT_F_SPACE)
    sign = ' ';

  if (ISNAN (fvalue))
    infnan = (flags & PRINT_F_UP) ? "NAN" : "nan";
  else if (ISINF (fvalue))
    infnan = (flags & PRINT_F_UP) ? "INF" : "inf";

  if (infnan != NULL)
  {
    if (sign != 0)
      iconvert[ipos++] = sign;
    while (*infnan != '\0')
      iconvert[ipos++] = *infnan++;
    fmtstr (str, len, size, iconvert, width, ipos, flags);
    return;
  }

  /* "%e" (or "%E") or "%g" (or "%G") conversion. */
  if (flags & PRINT_F_TYPE_E || flags & PRINT_F_TYPE_G)
  {
    if (flags & PRINT_F_TYPE_G)
    {
      /*
       * For "%g" (and "%G") conversions, the precision
       * specifies the number of significant digits, which
       * includes the digits in the integer part.  The
       * conversion will or will not be using "e-style" (like
       * "%e" or "%E" conversions) depending on the precision
       * and on the exponent.  However, the exponent can be
       * affected by rounding the converted value, so we'll
       * leave this decision for later.  Until then, we'll
       * assume that we're going to do an "e-style" conversion
       * (in order to get the exponent calculated).  For
       * "e-style", the precision must be decremented by one.
       */
      precision--;
      /*
       * For "%g" (and "%G") conversions, trailing zeros are
       * removed from the fractional portion of the result
       * unless the "#" flag was specified.
       */
      if (!(flags & PRINT_F_NUM))
        omitzeros = 1;
    }
    exponent = getexponent (fvalue);
    estyle = 1;
  }

again:
  /*
   * Sorry, we only support 9, 19, or 38 digits (that is, the number of
   * digits of the 32-bit, the 64-bit, or the 128-bit UINTMAX_MAX value
   * minus one) past the decimal point due to our conversion method.
   */
  switch (sizeof (uintmax_t))
  {
    case 16:
      if (precision > 38)
        precision = 38;
      break;
    case 8:
      if (precision > 19)
        precision = 19;
      break;
    default:
      if (precision > 9)
        precision = 9;
      break;
  }

  ufvalue = (fvalue >= 0.0) ? fvalue : -fvalue;
  if (estyle) /* We want exactly one integer digit. */
    ufvalue /= mypow10 (exponent);

  if ((intpart = cast (ufvalue)) == UINTMAX_MAX)
  {
    *overflow = 1;
    return;
  }

  /*
   * Factor of ten with the number of digits needed for the fractional
   * part.  For example, if the precision is 3, the mask will be 1000.
   */
  mask = (uintmax_t) mypow10 (precision);
  /*
   * We "cheat" by converting the fractional part to integer by
   * multiplying by a factor of ten.
   */
  if ((fracpart = myround (mask * (ufvalue - intpart))) >= mask)
  {
    /*
     * For example, ufvalue = 2.99962, intpart = 2, and mask = 1000
     * (because precision = 3).  Now, myround (1000 * 0.99962) will
     * return 1000.  So, the integer part must be incremented by one
     * and the fractional part must be set to zero.
     */
    intpart++;
    fracpart = 0;
    if (estyle && intpart == 10)
    {
      /*
       * The value was rounded up to ten, but we only want one
       * integer digit if using "e-style".  So, the integer
       * part must be set to one and the exponent must be
       * incremented by one.
       */
      intpart = 1;
      exponent++;
    }
  }

  /*
   * Now that we know the real exponent, we can check whether or not to
   * use "e-style" for "%g" (and "%G") conversions.  If we don't need
   * "e-style", the precision must be adjusted and the integer and
   * fractional parts must be recalculated from the original value.
   *
   * C99 says: "Let P equal the precision if nonzero, 6 if the precision
   * is omitted, or 1 if the precision is zero.  Then, if a conversion
   * with style `E' would have an exponent of X:
   *
   * - if P > X >= -4, the conversion is with style `f' (or `F') and
   *   precision P - (X + 1).
   *
   * - otherwise, the conversion is with style `e' (or `E') and precision
   *   P - 1." (7.19.6.1, 8)
   *
   * Note that we had decremented the precision by one.
   */
  if (flags & PRINT_F_TYPE_G && estyle &&
      precision + 1 > exponent && exponent >= -4)
  {
    precision -= exponent;
    estyle = 0;
    goto again;
  }

  if (estyle)
  {
    if (exponent < 0)
    {
      exponent = -exponent;
      esign = '-';
    }
    else
    {
      esign = '+';
    }

    /*
     * Convert the exponent.  The sizeof (econvert) is 5.  So, the
     * econvert buffer can hold e.g. "e+999" and "e-999".  We don't
     * support an exponent which contains more than three digits.
     * Therefore, the following stores are safe.
     */
    epos = convert (exponent, econvert, 3, 10, 0);
    /*
     * C99 says: "The exponent always contains at least two digits,
     * and only as many more digits as necessary to represent the
     * exponent." (7.19.6.1, 8)
     */
    if (epos == 1)
      econvert[epos++] = '0';
    econvert[epos++] = esign;
    econvert[epos++] = (flags & PRINT_F_UP) ? 'E' : 'e';
  }

  /* Convert the integer part and the fractional part. */
  ipos = convert (intpart, iconvert, sizeof (iconvert), 10, 0);
  if (fracpart != 0) /* convert () would return 1 if fracpart == 0. */
    fpos = convert (fracpart, fconvert, sizeof (fconvert), 10, 0);

  leadfraczeros = precision - fpos;

  if (omitzeros)
  {
    if (fpos > 0) /* Omit trailing fractional part zeros. */
    {
      while (omitcount < fpos && fconvert[omitcount] == '0')
        omitcount++;
    }
    else /* The fractional part is zero, omit it completely. */
    {
      omitcount = precision;
      leadfraczeros = 0;
    }
    precision -= omitcount;
  }

  /*
   * Print a decimal point if either the fractional part is non-zero
   * and/or the "#" flag was specified.
   */
  if (precision > 0 || flags & PRINT_F_NUM)
    emitpoint = 1;
  if (separators) /* Get the number of group separators we'll print. */
    separators = getnumsep (ipos);

  padlen = width                  /* Minimum field width. */
      - ipos                      /* Number of integer digits. */
      - epos                      /* Number of exponent characters. */
      - precision                 /* Number of fractional digits. */
      - separators                /* Number of group separators. */
      - (emitpoint ? 1 : 0)       /* Will we print a decimal point? */
      - ((sign != 0) ? 1 : 0);    /* Will we print a sign character? */

  if (padlen < 0)
    padlen = 0;

  /*
   * C99 says: "If the `0' and `-' flags both appear, the `0' flag is
   * ignored." (7.19.6.1, 6)
   */
  if (flags & PRINT_F_MINUS) /* Left justifty. */
  {
    padlen = -padlen;
  }
  else if (flags & PRINT_F_ZERO && padlen > 0)
  {
    if (sign != 0) /* Sign. */
    {
      OUTCHAR (str, *len, size, sign);
      sign = 0;
    }
    while (padlen > 0) /* Leading zeros. */
    {
      OUTCHAR (str, *len, size, '0');
      padlen--;
    }
  }
  while (padlen > 0) /* Leading spaces. */
  {
    OUTCHAR (str, *len, size, ' ');
    padlen--;
  }
  if (sign != 0) /* Sign. */
    OUTCHAR (str, *len, size, sign);
  while (ipos > 0) /* Integer part. */
  {
    ipos--;
    OUTCHAR (str, *len, size, iconvert[ipos]);
    if (separators > 0 && ipos > 0 && ipos % 3 == 0)
      printsep (str, len, size);
  }
  if (emitpoint) /* Decimal point. */
  {
    OUTCHAR (str, *len, size, '.');
  }
  while (leadfraczeros > 0) /* Leading fractional part zeros. */
  {
    OUTCHAR (str, *len, size, '0');
    leadfraczeros--;
  }
  while (fpos > omitcount) /* The remaining fractional part. */
  {
    fpos--;
    OUTCHAR (str, *len, size, fconvert[fpos]);
  }
  while (epos > 0) /* Exponent. */
  {
    epos--;
    OUTCHAR (str, *len, size, econvert[epos]);
  }
  while (padlen < 0) /* Trailing spaces. */
  {
    OUTCHAR (str, *len, size, ' ');
    padlen++;
  }
}

static void
printsep (gchar * str,
          gsize * len,
          gsize size)
{
  OUTCHAR (str, *len, size, ',');
}

static gint
getnumsep (gint digits)
{
  return (digits - ((digits % 3 == 0) ? 1 : 0)) / 3;
}

static gint
getexponent (LDOUBLE value)
{
  LDOUBLE tmp = (value >= 0.0) ? value : -value;
  gint exponent = 0;

  /*
   * We check for LDOUBLE_MAX_10_EXP > exponent > LDOUBLE_MIN_10_EXP in
   * order to work around possible endless loops which could happen
   * (at least) in the second loop (at least) if we're called with an
   * infinite value.  However, we checked for infinity before calling
   * this function using our ISINF() macro, so this might be somewhat
   * paranoid.
   */
  while (tmp < 1.0 && tmp > 0.0 && --exponent >= LDOUBLE_MIN_10_EXP)
    tmp *= 10;
  while (tmp >= 10.0 && ++exponent <= LDOUBLE_MAX_10_EXP)
    tmp /= 10;

  return exponent;
}

static gint
convert (uintmax_t value, gchar * buf, gsize size, gint base, gint caps)
{
  const gchar * digits = caps ? "0123456789ABCDEF" : "0123456789abcdef";
  gsize pos = 0;

  /* We return an unterminated buffer with the digits in reverse order. */
  do
  {
    buf[pos++] = digits[value % base];
    value /= base;
  }
  while (value != 0 && pos < size);

  return (gint) pos;
}

static uintmax_t
cast (LDOUBLE value)
{
  uintmax_t result;

  /*
   * We check for ">=" and not for ">" because if UINTMAX_MAX cannot be
   * represented exactly as an LDOUBLE value (but is less than LDBL_MAX),
   * it may be increased to the nearest higher representable value for the
   * comparison (cf. C99: 6.3.1.4, 2).  It might then equal the LDOUBLE
   * value although converting the latter to uintmax_t would overflow.
   */
  if (value >= (LDOUBLE) UINTMAX_MAX)
    return UINTMAX_MAX;

  result = (uintmax_t) value;

  /*
   * At least on NetBSD/sparc64 3.0.2 and 4.99.30, casting long double to
   * an integer type converts e.g. 1.9 to 2 instead of 1 (which violates
   * the standard).  Sigh.
   */
  return (result <= value) ? result : result - 1;
}

static uintmax_t
myround (LDOUBLE value)
{
  uintmax_t intpart = cast (value);

  return ((value -= intpart) < 0.5) ? intpart : intpart + 1;
}

static LDOUBLE
mypow10 (gint exponent)
{
  LDOUBLE result = 1;

  while (exponent > 0)
  {
    result *= 10;
    exponent--;
  }

  while (exponent < 0)
  {
    result /= 10;
    exponent++;
  }

  return result;
}

#endif
