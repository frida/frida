#define FRIDA_PRINTF_BUFFER_SIZE (512 * 1024)
#define _GNU_SOURCE

#include <errno.h>
#include <gum/gum.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef HAVE_WINDOWS
# include <assert.h>
# include <fcntl.h>
# include <unistd.h>
# include <wchar.h>
#endif
#if defined (HAVE_DARWIN)
# include <sys/attr.h>
#elif defined (HAVE_LINUX)
# include <sys/syscall.h>
#endif
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif
#ifdef HAVE_XLOCALE_H
# include <xlocale.h>
#endif

#undef __srget
#undef feof
#undef ferror
#undef fgetwc
#undef fopen
#undef fopen64
#undef fputwc
#undef fseek
#undef fseeko
#undef fseeko64
#undef ftell
#undef ftello
#undef ftello64
#undef getc
#undef getc_unlocked
#undef getwc
#undef memcpy
#undef putc
#undef putchar
#undef putwc
#undef setbuf
#undef setbuffer
#undef setlinebuf
#undef snprintf
#undef sprintf
#undef stderr
#undef stdin
#undef stdout
#undef ungetc
#undef ungetwc
#undef vsnprintf

#ifdef HAVE_MUSL
# define FRIDA_STDIO_OPAQUE_FILE 1
#endif

static gboolean frida_deinit_expected = FALSE;

void
frida_libc_shim_prepare_to_deinit (void)
{
  frida_deinit_expected = TRUE;
}

#if defined (HAVE_WINDOWS) || defined (HAVE_ASAN)

void
frida_run_atexit_handlers (void)
{
}

# ifdef _MSC_VER
G_GNUC_INTERNAL
# else
__attribute__ ((constructor)) static
# endif
void
frida_libc_shim_init (void)
{
# ifdef HAVE_ASAN
  gum_init ();
# else
  gum_init_embedded ();
# endif
}

# if defined (_MSC_VER) || !defined (HAVE_DARWIN)
G_GNUC_INTERNAL
# else
__attribute__ ((destructor)) static
# endif
void
frida_libc_shim_deinit (void)
{
  if (!frida_deinit_expected)
    return;

# ifdef HAVE_ASAN
  gum_deinit ();
# else
  gum_deinit_embedded ();
# endif
}

#else

#define FRIDA_SHIM_LOCK() gum_spinlock_acquire (&frida_shim_lock)
#define FRIDA_SHIM_UNLOCK() gum_spinlock_release (&frida_shim_lock)

#ifndef _IONBF
# define _IONBF 0
# define _IOFBF 1
# define _IOLBF 2
#endif

#ifndef O_DIRECTORY
# define O_DIRECTORY 0
#endif

#define FRIDA_FILE_MAGIC 0x46524944u /* 'FRID' */
#define FRIDA_DIR_MAGIC  0x46444952u /* 'FDIR' */

#define FRIDA_STDIO_BUFSIZE 4096
#define FRIDA_GETLINE_INITIAL_SIZE 128

#if defined (HAVE_LINUX) || defined (HAVE_DARWIN)
# define HAVE_FRIDA_DIR
#endif

#if !defined (SYS_getdents64) && defined (__NR_getdents64)
# define SYS_getdents64 __NR_getdents64
#endif

#if !defined (SYS_getdirentries64) && defined (__NR_getdirentries64)
# define SYS_getdirentries64 __NR_getdirentries64
#endif

typedef struct _FridaExitEntry FridaExitEntry;
typedef void (* FridaExitFunc) (gpointer user_data);

typedef struct _FridaFile FridaFile;
typedef struct _FridaFileHandle FridaFileHandle;
typedef struct _FridaDir FridaDir;

struct _FridaExitEntry
{
  FridaExitFunc func;
  gpointer user_data;
};

struct _FridaFile
{
  int fd;
  gboolean close_fd;

  int buf_mode;

  gboolean eof;
  int err;

  guint8 * rbuf;
  gboolean owns_rbuf;
  size_t rcap;
  size_t rpos;
  size_t rlen;

  guint8 * wbuf;
  gboolean owns_wbuf;
  size_t wcap;
  size_t wlen;

  guint8 ungot_buf[8];
  guint ungot_len;
};

struct _FridaFileHandle
{
  guint32 magic;
  FridaFile * impl;
};

#ifdef HAVE_FRIDA_DIR

struct _FridaDir
{
  guint32 magic;

  int fd;
  gboolean close_fd;

  guint8 * buf;
  size_t cap;
  size_t pos;
  size_t len;

  struct dirent cur;
};

#endif

static FILE * frida_fopen_impl (const char * pathname, const char * mode);
static int frida_fseek_impl (FILE * stream, off_t offset, int whence);
static off_t frida_ftell_impl (FILE * stream);

static void frida_stdio_register_stream (FILE * stream);
static void frida_stdio_unregister_stream (FILE * stream);
#ifdef HAVE_FRIDA_DIR
static void frida_stdio_register_dir (DIR * dirp);
static void frida_stdio_unregister_dir (DIR * dirp);
#endif
static void frida_flush_all_streams (int * result);

static FridaFile * frida_file_get_impl (FILE * stream);
static void frida_file_bind_slot (FILE * slot, FridaFile * impl);
static FILE * frida_file_wrap (FridaFile * impl);
static void frida_file_unwrap (FILE * stream);

static FridaFile * frida_file_new (int fd, gboolean close_fd, int buf_mode);
static void frida_file_free (FridaFile * f);
static int frida_file_flush_write (FridaFile * f);
static ssize_t frida_file_fill_read (FridaFile * f);

static void frida_parse_fopen_mode (const char * mode, int * oflags);

static int frida_write_formatted_to_fd (int fd, const char * format, va_list args);

static gboolean frida_utf8_expected_len (guint8 first, guint * out_len);

#ifdef HAVE_FRIDA_DIR
static FridaDir * frida_dir_get_impl (DIR * dirp);
static DIR * frida_dir_wrap (FridaDir * impl);
static void frida_dir_free (FridaDir * d);

# ifdef HAVE_DARWIN
static int frida_dir_refill_darwin (FridaDir * d);
static guint8 frida_darwin_objtype_to_dtype (guint32 objtype);
# else
static ssize_t frida_getdirents_nointr (int fd, void * buf, size_t size);
# endif
#endif

static int frida_open_nointr (const char * pathname, int flags, mode_t mode);
static ssize_t frida_read_nointr (int fd, void * buf, size_t count);
static ssize_t frida_write_nointr (int fd, const void * buf, size_t count);
static int frida_close_nointr (int fd);
static off_t frida_lseek_nointr (int fd, off_t offset, int whence);

#ifndef FRIDA_STDIO_OPAQUE_FILE
G_GNUC_INTERNAL FILE __sF[3];

G_GNUC_INTERNAL FILE * stdin = &__sF[0];
G_GNUC_INTERNAL FILE * stdout = &__sF[1];
G_GNUC_INTERNAL FILE * stderr = &__sF[2];

# ifdef HAVE_DARWIN
G_GNUC_INTERNAL FILE * __stdinp = &__sF[0];
G_GNUC_INTERNAL FILE * __stdoutp = &__sF[1];
G_GNUC_INTERNAL FILE * __stderrp = &__sF[2];
# endif
#else
static FridaFileHandle frida_stdio[3];

G_GNUC_INTERNAL FILE * const stdin = (FILE *) &frida_stdio[0];
G_GNUC_INTERNAL FILE * const stdout = (FILE *) &frida_stdio[1];
G_GNUC_INTERNAL FILE * const stderr = (FILE *) &frida_stdio[2];
#endif

static gboolean frida_libc_shim_initialized = FALSE;

static FridaExitEntry * frida_atexit_entries = NULL;
static guint frida_atexit_count = 0;

static GumSpinlock frida_shim_lock = GUM_SPINLOCK_INIT;

G_LOCK_DEFINE_STATIC (frida_stdio);

static GHashTable * frida_streams = NULL;
#ifdef HAVE_FRIDA_DIR
static GHashTable * frida_dirs = NULL;
#endif

__attribute__ ((constructor)) static void
frida_libc_shim_init (void)
{
  FridaFile * f0, * f1, * f2;

  if (frida_libc_shim_initialized)
    return;

  gum_init_embedded ();

  f0 = NULL;
  f1 = NULL;
  f2 = NULL;

  G_LOCK (frida_stdio);

  frida_streams = g_hash_table_new (g_direct_hash, g_direct_equal);
#ifdef HAVE_FRIDA_DIR
  frida_dirs = g_hash_table_new (g_direct_hash, g_direct_equal);
#endif

  G_UNLOCK (frida_stdio);

  f0 = frida_file_new (0, FALSE, _IOFBF);
#ifndef FRIDA_STDIO_OPAQUE_FILE
  frida_file_bind_slot (&__sF[0], f0);
#else
  frida_file_bind_slot ((FILE *) &frida_stdio[0], f0);
#endif

  f1 = frida_file_new (1, FALSE, _IOLBF);
#ifndef FRIDA_STDIO_OPAQUE_FILE
  frida_file_bind_slot (&__sF[1], f1);
#else
  frida_file_bind_slot ((FILE *) &frida_stdio[1], f1);
#endif

  f2 = frida_file_new (2, FALSE, _IONBF);
#ifndef FRIDA_STDIO_OPAQUE_FILE
  frida_file_bind_slot (&__sF[2], f2);
#else
  frida_file_bind_slot ((FILE *) &frida_stdio[2], f2);
#endif

  frida_libc_shim_initialized = TRUE;
}

/*
 * Avoid destructors on i/macOS as modern toolchain versions now emit a
 * constructor per destructor, each calling __cxa_atexit().
 *
 * We want to make sure we release our heap as the very last thing we do,
 * so we shim __cxa_atexit() to make sure any destructors registered that
 * way will be run before we deallocate our internal heap.
 */

#ifdef HAVE_DARWIN
void
#else
__attribute__ ((destructor)) static void
#endif
frida_libc_shim_deinit (void)
{
  FridaFileHandle * sh;
  GHashTableIter iter;
  gpointer key;

  assert (frida_libc_shim_initialized);

  fflush (NULL);

  if (!frida_deinit_expected)
    return;

  G_LOCK (frida_stdio);

  g_hash_table_iter_init (&iter, frida_streams);

  while (g_hash_table_iter_next (&iter, &key, NULL))
  {
    FILE * stream = key;
    FridaFile * f;

    f = frida_file_get_impl (stream);

    frida_file_flush_write (f);

    if (f->close_fd)
      frida_close_nointr (f->fd);

    frida_file_free (f);
    frida_file_unwrap (stream);

    g_hash_table_iter_remove (&iter);
  }

  g_clear_pointer (&frida_streams, g_hash_table_unref);

#ifdef HAVE_FRIDA_DIR
  g_hash_table_iter_init (&iter, frida_dirs);

  while (g_hash_table_iter_next (&iter, &key, NULL))
  {
    DIR * dirp = key;
    FridaDir * d;

    d = frida_dir_get_impl (dirp);

    if (d->close_fd)
      frida_close_nointr (d->fd);

    frida_dir_free (d);

    g_hash_table_iter_remove (&iter);
  }

  g_clear_pointer (&frida_dirs, g_hash_table_unref);
#endif

  G_UNLOCK (frida_stdio);

  sh = (FridaFileHandle *) stdin;
  g_clear_pointer (&sh->impl, frida_file_free);
  sh->magic = 0;

  sh = (FridaFileHandle *) stdout;
  g_clear_pointer (&sh->impl, frida_file_free);
  sh->magic = 0;

  sh = (FridaFileHandle *) stderr;
  g_clear_pointer (&sh->impl, frida_file_free);
  sh->magic = 0;

  gum_deinit_embedded ();
}

void
frida_run_atexit_handlers (void)
{
  gint i;

  for (i = (gint) frida_atexit_count - 1; i >= 0; i--)
  {
    const FridaExitEntry * entry = &frida_atexit_entries[i];

    entry->func (entry->user_data);
  }

  gum_free (frida_atexit_entries);
  frida_atexit_entries = 0;
  frida_atexit_count = 0;
}

G_GNUC_INTERNAL int
__cxa_atexit (void (* func) (void *), void * arg, void * dso_handle)
{
  FridaExitEntry * entry;

  frida_libc_shim_init ();

  FRIDA_SHIM_LOCK ();
  frida_atexit_count++;
  frida_atexit_entries = gum_realloc (frida_atexit_entries, frida_atexit_count * sizeof (FridaExitEntry));
  entry = &frida_atexit_entries[frida_atexit_count - 1];
  FRIDA_SHIM_UNLOCK ();

  entry->func = func;
  entry->user_data = arg;

  return 0;
}

#ifdef HAVE_DARWIN

G_GNUC_INTERNAL int
atexit (void (* func) (void))
{
  __cxa_atexit ((FridaExitFunc) func, NULL, NULL);

  return 0;
}

#endif

G_GNUC_INTERNAL void *
malloc (size_t size)
{
  return gum_malloc (size);
}

G_GNUC_INTERNAL void *
calloc (size_t count, size_t size)
{
  return gum_calloc (count, size);
}

G_GNUC_INTERNAL void *
realloc (void * ptr, size_t size)
{
  return gum_realloc (ptr, size);
}

G_GNUC_INTERNAL void *
memalign (size_t alignment, size_t size)
{
  return gum_memalign (alignment, size);
}

G_GNUC_INTERNAL int
posix_memalign (void ** memptr, size_t alignment, size_t size)
{
  gpointer result;

  result = gum_memalign (alignment, size);
  if (result == NULL)
    return ENOMEM;

  *memptr = result;
  return 0;
}

G_GNUC_INTERNAL void
free (void * ptr)
{
  gum_free (ptr);
}

G_GNUC_INTERNAL size_t
malloc_size (const void * ptr)
{
  return gum_malloc_usable_size (ptr);
}

G_GNUC_INTERNAL size_t
malloc_usable_size (const void * ptr)
{
  return gum_malloc_usable_size (ptr);
}

G_GNUC_INTERNAL void *
memcpy (void * dst, const void * src, size_t n)
{
  return gum_memcpy (dst, src, n);
}

G_GNUC_INTERNAL char *
strdup (const char * s)
{
  return g_strdup (s);
}

G_GNUC_INTERNAL int
printf (const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = frida_write_formatted_to_fd (1, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
fprintf (FILE * stream, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = frida_write_formatted_to_fd (frida_file_get_impl (stream)->fd, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
sprintf (char * string, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = gum_vsnprintf (string, FRIDA_PRINTF_BUFFER_SIZE, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
snprintf (char * string, size_t size, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = gum_vsnprintf (string, size, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
vprintf (const char * format, va_list args)
{
  return frida_write_formatted_to_fd (1, format, args);
}

G_GNUC_INTERNAL int
vfprintf (FILE * stream, const char * format, va_list args)
{
  return frida_write_formatted_to_fd (frida_file_get_impl (stream)->fd, format, args);
}

G_GNUC_INTERNAL int
vsnprintf (char * string, size_t size, const char * format, va_list args)
{
  return gum_vsnprintf (string, size, format, args);
}

G_GNUC_INTERNAL int
__sprintf_chk (char * string, int flag, size_t size, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = gum_vsnprintf (string, size, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
__snprintf_chk (char * string, size_t size, int flags, size_t len, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = gum_vsnprintf (string, size, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
__vsnprintf_chk (char * string, size_t size, int flags, size_t len, const char * format, va_list args)
{
  return gum_vsnprintf (string, size, format, args);
}

#ifdef HAVE_XLOCALE_H

G_GNUC_INTERNAL int
sprintf_l (char * string, locale_t loc, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = gum_vsnprintf (string, FRIDA_PRINTF_BUFFER_SIZE, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
snprintf_l (char * string, size_t size, locale_t loc, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = gum_vsnprintf (string, size, format, args);
  va_end (args);

  return result;
}

G_GNUC_INTERNAL int
asprintf_l (char ** ret, locale_t loc, const char * format, ...)
{
  int result;
  va_list args;

  va_start (args, format);
  result = gum_vasprintf (ret, format, args);
  va_end (args);

  return result;
}

#endif

#ifdef HAVE_GLIBC

G_GNUC_INTERNAL FILE *
frida_fopen_glibc_225 (const char * pathname, const char * mode)
{
  return frida_fopen_impl (pathname, mode);
}
__asm__ (".symver frida_fopen_glibc_225,fopen@@GLIBC_2.2.5");

G_GNUC_INTERNAL FILE *
frida_fopen64_glibc_225 (const char * pathname, const char * mode)
{
  return frida_fopen_impl (pathname, mode);
}
__asm__ (".symver frida_fopen64_glibc_225,fopen64@@GLIBC_2.2.5");

#else

G_GNUC_INTERNAL FILE *
fopen (const char * pathname, const char * mode)
{
  return frida_fopen_impl (pathname, mode);
}

#endif

static FILE *
frida_fopen_impl (const char * pathname, const char * mode)
{
  FILE * result;
  int oflags, fd;
  FridaFile * impl;

  frida_parse_fopen_mode (mode, &oflags);

  fd = frida_open_nointr (pathname, oflags, 0666);
  if (fd == -1)
    return NULL;

  impl = frida_file_new (fd, TRUE, _IOFBF);

  result = frida_file_wrap (impl);
  frida_stdio_register_stream (result);

  return result;
}

G_GNUC_INTERNAL FILE *
fdopen (int fd, const char * mode)
{
  FILE * result;
  int oflags;
  FridaFile * impl;

  frida_parse_fopen_mode (mode, &oflags);

  impl = frida_file_new (fd, TRUE, _IOFBF);

  result = frida_file_wrap (impl);
  frida_stdio_register_stream (result);

  return result;
}

G_GNUC_INTERNAL int
fclose (FILE * stream)
{
  FridaFile * f;

  f = frida_file_get_impl (stream);

  fflush (stream);

  if (f->close_fd)
  {
    if (frida_close_nointr (f->fd) == -1)
      return EOF;
  }

  frida_stdio_unregister_stream (stream);

  frida_file_free (f);
  frida_file_unwrap (stream);

  return 0;
}

G_GNUC_INTERNAL int
setvbuf (FILE * stream, char * buf, int mode, size_t size)
{
  FridaFile * f;

  f = frida_file_get_impl (stream);

  if (fflush (stream) != 0)
    return -1;

  f->buf_mode = mode;

  if (mode == _IONBF)
  {
    if (f->owns_rbuf)
      g_clear_pointer (&f->rbuf, g_free);
    else
      f->rbuf = NULL;
    f->owns_rbuf = FALSE;

    if (f->owns_wbuf)
      g_clear_pointer (&f->wbuf, g_free);
    else
      f->wbuf = NULL;
    f->owns_wbuf = FALSE;

    f->rcap = 0;
    f->wcap = 0;
    f->rpos = 0;
    f->rlen = 0;
    f->wlen = 0;

    return 0;
  }

  if (size == 0)
    size = BUFSIZ;

  if (!f->owns_rbuf)
  {
    f->rbuf = NULL;
    f->rcap = 0;
  }

  f->rbuf = g_realloc (f->rbuf, size);
  f->owns_rbuf = TRUE;
  f->rcap = size;

  if (buf != NULL)
  {
    if (f->owns_wbuf)
      g_clear_pointer (&f->wbuf, g_free);

    f->wbuf = (guint8 *) buf;
    f->owns_wbuf = FALSE;
    f->wcap = size;
  }
  else
  {
    if (!f->owns_wbuf)
    {
      f->wbuf = NULL;
      f->wcap = 0;
    }

    f->wbuf = g_realloc (f->wbuf, size);
    f->owns_wbuf = TRUE;
    f->wcap = size;
  }

  f->rpos = 0;
  f->rlen = 0;
  f->wlen = 0;

  return 0;
}

G_GNUC_INTERNAL void
setbuf (FILE * stream, char * buf)
{
  if (buf == NULL)
    (void) setvbuf (stream, NULL, _IONBF, 0);
  else
    (void) setvbuf (stream, buf, _IOFBF, BUFSIZ);
}

#if defined (HAVE_DARWIN) || defined (HAVE_ANDROID) || defined (HAVE_FREEBSD)
G_GNUC_INTERNAL int
setlinebuf (FILE * stream)
{
  return setvbuf (stream, NULL, _IOLBF, 0);
}
#else
G_GNUC_INTERNAL void
setlinebuf (FILE * stream)
{
  setvbuf (stream, NULL, _IOLBF, 0);
}
#endif

G_GNUC_INTERNAL void
#if defined (HAVE_DARWIN) || defined (HAVE_ANDROID) || defined (HAVE_FREEBSD)
setbuffer (FILE * stream, char * buf, int size)
#else
setbuffer (FILE * stream, char * buf, size_t size)
#endif
{
  (void) setvbuf (stream, buf, (buf != NULL) ? _IOFBF : _IONBF, size);
}

G_GNUC_INTERNAL int
fflush (FILE * stream)
{
  int result = 0;

  if (stream != NULL)
  {
    FridaFile * f = frida_file_get_impl (stream);

    if (frida_file_flush_write (f) != 0)
      result = EOF;

    return result;
  }

  {
    FridaFile * f0, * f1, * f2;

    f0 = frida_file_get_impl (stdin);
    f1 = frida_file_get_impl (stdout);
    f2 = frida_file_get_impl (stderr);

    if (frida_file_flush_write (f0) != 0)
      result = EOF;
    if (frida_file_flush_write (f1) != 0)
      result = EOF;
    if (frida_file_flush_write (f2) != 0)
      result = EOF;
  }

  frida_flush_all_streams (&result);

  return result;
}

G_GNUC_INTERNAL int
fileno (FILE * stream)
{
  return frida_file_get_impl (stream)->fd;
}

G_GNUC_INTERNAL int
feof (FILE * stream)
{
  return frida_file_get_impl (stream)->eof ? 1 : 0;
}

G_GNUC_INTERNAL int
ferror (FILE * stream)
{
  return (frida_file_get_impl (stream)->err != 0) ? 1 : 0;
}

G_GNUC_INTERNAL int
getc_unlocked (FILE * stream)
{
  FridaFile * f;

  f = frida_file_get_impl (stream);

  if (f->ungot_len != 0)
    return f->ungot_buf[--f->ungot_len];

  if (f->buf_mode == _IONBF)
  {
    unsigned char ch;
    ssize_t n;

    n = frida_read_nointr (f->fd, &ch, 1);
    if (n == -1)
    {
      f->err = errno;
      return EOF;
    }

    if (n == 0)
    {
      f->eof = TRUE;
      return EOF;
    }

    return ch;
  }

  if (f->rpos == f->rlen)
  {
    if (frida_file_fill_read (f) == -1)
      return EOF;

    if (f->rlen == 0)
      return EOF;
  }

  return f->rbuf[f->rpos++];
}

G_GNUC_INTERNAL int
getc (FILE * stream)
{
  return getc_unlocked (stream);
}

G_GNUC_INTERNAL int
__srget (FILE * stream)
{
  return getc_unlocked (stream);
}

G_GNUC_INTERNAL wint_t
getwc (FILE * stream)
{
  return fgetwc (stream);
}

G_GNUC_INTERNAL wint_t
fgetwc (FILE * stream)
{
  char bytes[4];
  guint need, have;
  int c;
  gunichar ch;

  c = getc_unlocked (stream);
  if (c == EOF)
    return WEOF;

  bytes[0] = c;

  if (!frida_utf8_expected_len (bytes[0], &need))
  {
    errno = EILSEQ;
    frida_file_get_impl (stream)->err = errno;
    return WEOF;
  }

  have = 1;
  while (have != need)
  {
    c = getc_unlocked (stream);
    if (c == EOF)
    {
      errno = EILSEQ;
      frida_file_get_impl (stream)->err = errno;
      return WEOF;
    }

    bytes[have++] = c;
  }

  ch = g_utf8_get_char_validated (bytes, (gssize) need);
  if (ch == (gunichar) -1 || ch == (gunichar) -2)
  {
    errno = EILSEQ;
    frida_file_get_impl (stream)->err = errno;
    return WEOF;
  }

  return (wint_t) ch;
}

G_GNUC_INTERNAL int
ungetc (int c, FILE * stream)
{
  FridaFile * f;

  f = frida_file_get_impl (stream);

  if (c == EOF)
    return EOF;

  if (f->ungot_len == G_N_ELEMENTS (f->ungot_buf))
    return EOF;

  f->ungot_buf[f->ungot_len++] = c;
  f->eof = FALSE;

  return c;
}

G_GNUC_INTERNAL wint_t
ungetwc (wint_t wc, FILE * stream)
{
  FridaFile * f;
  char tmp[4];
  int len;
  int i;

  if (wc == WEOF)
    return WEOF;

  f = frida_file_get_impl (stream);

  len = g_unichar_to_utf8 ((gunichar) wc, tmp);
  if (len <= 0 || len > 4)
  {
    errno = EILSEQ;
    f->err = errno;
    return WEOF;
  }

  if (f->ungot_len + len > G_N_ELEMENTS (f->ungot_buf))
    return WEOF;

  for (i = 0; i != len; i++)
    f->ungot_buf[f->ungot_len++] = tmp[i];

  f->eof = FALSE;
  return wc;
}

G_GNUC_INTERNAL size_t
fread (void * ptr, size_t size, size_t nmemb, FILE * stream)
{
  FridaFile * f;
  size_t want, got;
  guint8 * out;

  f = frida_file_get_impl (stream);

  if (size == 0 || nmemb == 0)
    return 0;

  want = size * nmemb;
  got = 0;
  out = ptr;

  while (got != want)
  {
    if (f->ungot_len != 0)
    {
      out[got++] = f->ungot_buf[--f->ungot_len];
      continue;
    }

    if (f->buf_mode == _IONBF)
    {
      ssize_t n;

      n = frida_read_nointr (f->fd, out + got, want - got);
      if (n == -1)
      {
        f->err = errno;
        break;
      }

      if (n == 0)
      {
        f->eof = TRUE;
        break;
      }

      got += n;
      continue;
    }

    if (f->rpos == f->rlen)
    {
      if (frida_file_fill_read (f) == -1)
        break;

      if (f->rlen == 0)
        break;
    }

    {
      size_t available, take;

      available = f->rlen - f->rpos;
      take = MIN (want - got, available);

      memcpy (out + got, f->rbuf + f->rpos, take);

      f->rpos += take;
      got += take;
    }
  }

  return got / size;
}

G_GNUC_INTERNAL size_t
fwrite (const void * ptr, size_t size, size_t nmemb, FILE * stream)
{
  FridaFile * f;
  size_t total, off;
  const guint8 * in;

  f = frida_file_get_impl (stream);

  if (size == 0 || nmemb == 0)
    return 0;

  total = size * nmemb;
  off = 0;
  in = ptr;

  if (f->buf_mode == _IONBF)
  {
    while (off != total)
    {
      ssize_t n;

      n = frida_write_nointr (f->fd, in + off, total - off);
      if (n == -1)
      {
        f->err = errno;
        break;
      }

      off += n;
    }

    return off / size;
  }

  while (off != total)
  {
    size_t space, take;

    space = f->wcap - f->wlen;

    if (space == 0)
    {
      if (frida_file_flush_write (f) != 0)
        break;

      space = f->wcap;
    }

    take = MIN (total - off, space);

    memcpy (f->wbuf + f->wlen, in + off, take);

    f->wlen += take;
    off += take;

    if (f->buf_mode == _IOLBF)
    {
      if (memchr (in + (off - take), '\n', take) != NULL)
      {
        if (frida_file_flush_write (f) != 0)
          break;
      }
    }
  }

  return off / size;
}

G_GNUC_INTERNAL int
putc (int c, FILE * stream)
{
  return fputc (c, stream);
}

G_GNUC_INTERNAL int
fputc (int c, FILE * stream)
{
  unsigned char ch = c;
  size_t n;

  n = fwrite (&ch, 1, 1, stream);
  if (n != 1)
    return EOF;

  return ch;
}

G_GNUC_INTERNAL wint_t
putwc (wchar_t wc, FILE * stream)
{
  return fputwc (wc, stream);
}

G_GNUC_INTERNAL wint_t
fputwc (wchar_t wc, FILE * stream)
{
  char tmp[4];
  int len;
  size_t n;

  if (wc == WEOF)
    return WEOF;

  len = g_unichar_to_utf8 ((gunichar) wc, tmp);
  if (len <= 0 || len > 4)
  {
    errno = EILSEQ;
    frida_file_get_impl (stream)->err = errno;
    return WEOF;
  }

  n = fwrite (tmp, 1, len, stream);
  if (n != len)
    return WEOF;

  return (wint_t) wc;
}

G_GNUC_INTERNAL int
fputs (const char * s, FILE * stream)
{
  size_t len, n;

  len = strlen (s);

  n = fwrite (s, 1, len, stream);
  if (n != len)
    return EOF;

  return len;
}

G_GNUC_INTERNAL char *
fgets (char * s, int size, FILE * stream)
{
  int i = 0;

  while (i < size - 1)
  {
    int c;

    c = getc_unlocked (stream);
    if (c == EOF)
      break;

    s[i++] = c;

    if (c == '\n')
      break;
  }

  if (i == 0)
    return NULL;

  s[i] = '\0';

  return s;
}

G_GNUC_INTERNAL int
fseek (FILE * stream, long offset, int whence)
{
  return frida_fseek_impl (stream, (off_t) offset, whence);
}

G_GNUC_INTERNAL long
ftell (FILE * stream)
{
  off_t pos = frida_ftell_impl (stream);
  if (pos == (off_t) -1)
    return -1;

  return (long) pos;
}

#ifdef HAVE_GLIBC

G_GNUC_INTERNAL int
frida_fseeko_glibc_225 (FILE * stream, off_t offset, int whence)
{
  return frida_fseek_impl (stream, offset, whence);
}
__asm__ (".symver frida_fseeko_glibc_225,fseeko@@GLIBC_2.2.5");

G_GNUC_INTERNAL int
frida_fseeko64_glibc_225 (FILE * stream, off_t offset, int whence)
{
  return frida_fseek_impl (stream, offset, whence);
}
__asm__ (".symver frida_fseeko64_glibc_225,fseeko64@@GLIBC_2.2.5");

G_GNUC_INTERNAL off_t
frida_ftello_glibc_225 (FILE * stream)
{
  return frida_ftell_impl (stream);
}
__asm__ (".symver frida_ftello_glibc_225,ftello@@GLIBC_2.2.5");

G_GNUC_INTERNAL off_t
frida_ftello64_glibc_225 (FILE * stream)
{
  return frida_ftell_impl (stream);
}
__asm__ (".symver frida_ftello64_glibc_225,ftello64@@GLIBC_2.2.5");

#else

G_GNUC_INTERNAL int
fseeko (FILE * stream, off_t offset, int whence)
{
  return frida_fseek_impl (stream, offset, whence);
}

G_GNUC_INTERNAL off_t
ftello (FILE * stream)
{
  return frida_ftell_impl (stream);
}

# ifndef HAVE_QNX

G_GNUC_INTERNAL int
fseeko64 (FILE * stream, off_t offset, int whence)
{
  return frida_fseek_impl (stream, offset, whence);
}

G_GNUC_INTERNAL off_t
ftello64 (FILE * stream)
{
  return frida_ftell_impl (stream);
}

# endif

#endif

static int
frida_fseek_impl (FILE * stream, off_t offset, int whence)
{
  FridaFile * f;
  off_t r;

  f = frida_file_get_impl (stream);

  if (fflush (stream) != 0)
    return -1;

  f->rpos = 0;
  f->rlen = 0;
  f->ungot_len = 0;
  f->eof = FALSE;

  r = frida_lseek_nointr (f->fd, offset, whence);
  if (r == (off_t) -1)
  {
    f->err = errno;
    return -1;
  }

  return 0;
}

static off_t
frida_ftell_impl (FILE * stream)
{
  FridaFile * f;
  off_t pos;

  f = frida_file_get_impl (stream);

  pos = frida_lseek_nointr (f->fd, 0, SEEK_CUR);
  if (pos == (off_t) -1)
  {
    f->err = errno;
    return -1;
  }

  if (f->buf_mode != _IONBF)
    pos -= (off_t) (f->rlen - f->rpos);

  pos -= f->ungot_len;

  return pos;
}

G_GNUC_INTERNAL void
rewind (FILE * stream)
{
  FridaFile * f;

  fseek (stream, 0, SEEK_SET);

  f = frida_file_get_impl (stream);
  f->err = 0;
  f->eof = FALSE;
}

G_GNUC_INTERNAL ssize_t
getdelim (char ** lineptr, size_t * n, int delimiter, FILE * stream)
{
  size_t len;

  if (*lineptr == NULL || *n == 0)
  {
    *n = FRIDA_GETLINE_INITIAL_SIZE;
    *lineptr = g_realloc (*lineptr, *n);
  }

  len = 0;

  while (TRUE)
  {
    int c;

    c = getc_unlocked (stream);
    if (c == EOF)
    {
      if (len == 0)
        return -1;

      break;
    }

    if (len + 2 > *n)
    {
      size_t new_n;

      new_n = (*n) * 2;
      if (new_n < len + 2)
        new_n = len + 2;

      *lineptr = g_realloc (*lineptr, new_n);
      *n = new_n;
    }

    (*lineptr)[len++] = (char) c;

    if (c == delimiter)
      break;
  }

  (*lineptr)[len] = '\0';

  return len;
}

G_GNUC_INTERNAL ssize_t
getline (char ** lineptr, size_t * n, FILE * stream)
{
  return getdelim (lineptr, n, '\n', stream);
}

G_GNUC_INTERNAL FILE *
tmpfile (void)
{
  FILE * result;
  int fd;
  gchar * path;
  FridaFile * impl;

  fd = g_file_open_tmp ("frida-XXXXXX", &path, NULL);
  if (fd == -1)
    return NULL;

  (void) unlink (path);

  g_free (path);

  impl = frida_file_new (fd, TRUE, _IOFBF);

  result = frida_file_wrap (impl);
  frida_stdio_register_stream (result);

  return result;
}

G_GNUC_INTERNAL int
putchar (int c)
{
  unsigned char ch = c;
  ssize_t n;

  n = frida_write_nointr (1, &ch, 1);
  if (n != 1)
    return EOF;

  return ch;
}

G_GNUC_INTERNAL int
puts (const char * s)
{
  size_t len;
  ssize_t n;

  len = strlen (s);

  n = frida_write_nointr (1, s, len);
  if (n != (ssize_t) len)
    return EOF;

  n = frida_write_nointr (1, "\n", 1);
  if (n != 1)
    return EOF;

  return 1;
}

#ifdef HAVE_FRIDA_DIR

G_GNUC_INTERNAL DIR *
opendir (const char * name)
{
  DIR * result;
  int fd;
  FridaDir * d;

  fd = frida_open_nointr (name, O_RDONLY | O_DIRECTORY, 0);
  if (fd == -1)
    return NULL;

  d = g_new0 (FridaDir, 1);
  d->magic = FRIDA_DIR_MAGIC;
  d->fd = fd;
  d->close_fd = TRUE;
  d->cap = FRIDA_STDIO_BUFSIZE;
  d->buf = g_malloc (d->cap);

  result = frida_dir_wrap (d);
  frida_stdio_register_dir (result);

  return result;
}

G_GNUC_INTERNAL DIR *
fdopendir (int fd)
{
  DIR * result;
  FridaDir * d;

  d = g_new0 (FridaDir, 1);
  d->magic = FRIDA_DIR_MAGIC;
  d->fd = fd;
  d->close_fd = TRUE;
  d->cap = FRIDA_STDIO_BUFSIZE;
  d->buf = g_malloc (d->cap);

  result = frida_dir_wrap (d);
  frida_stdio_register_dir (result);

  return result;
}

G_GNUC_INTERNAL int
closedir (DIR * dirp)
{
  FridaDir * d;

  d = frida_dir_get_impl (dirp);

  if (d->close_fd)
  {
    if (frida_close_nointr (d->fd) == -1)
      return -1;
  }

  frida_stdio_unregister_dir (dirp);

  frida_dir_free (d);

  return 0;
}

G_GNUC_INTERNAL struct dirent *
readdir (DIR * dirp)
{
  FridaDir * d;

  d = frida_dir_get_impl (dirp);

  while (TRUE)
  {
    if (d->pos == d->len)
    {
#ifdef HAVE_DARWIN
      int res = frida_dir_refill_darwin (d);
      if (res == -1 || res == 0)
        return NULL;
#else
      ssize_t n;

      d->pos = 0;
      d->len = 0;

      n = frida_getdirents_nointr (d->fd, d->buf, d->cap);
      if (n == -1)
        return NULL;

      if (n == 0)
        return NULL;

      d->len = n;
#endif
    }

#ifdef HAVE_DARWIN
    {
      guint8 * group, * base, * p;
      guint32 group_len;
      G_GNUC_UNUSED attribute_set_t returned;
      attrreference_t * name_refp;
      const char * name;
      size_t name_len, copy_len;
      guint32 objtype;
      fsobj_id_t objid;

      group = d->buf + d->pos;

      group_len = *(guint32 *) group;
      d->pos += group_len;

      base = group + 4;

      returned = *(attribute_set_t *) base;
      assert ((returned.commonattr & (ATTR_CMN_NAME | ATTR_CMN_OBJTYPE | ATTR_CMN_OBJID)) ==
          (ATTR_CMN_NAME | ATTR_CMN_OBJTYPE | ATTR_CMN_OBJID));

      p = base + sizeof (attribute_set_t);

      name_refp = (attrreference_t *) p;
      p += sizeof (attrreference_t);

      name = (const char *) ((guint8 *) name_refp + name_refp->attr_dataoffset);
      name_len = (size_t) name_refp->attr_length;

      if (name_len != 0 && name[name_len - 1] == '\0')
        name_len--;

      objtype = *(guint32 *) p;
      p += sizeof (guint32);

      p = (guint8 *) (((gsize) p + 3) & ~((gsize) 3));
      objid = *(fsobj_id_t *) p;

      copy_len = name_len;
      if (copy_len > sizeof (d->cur.d_name) - 1)
        copy_len = sizeof (d->cur.d_name) - 1;

      d->cur.d_ino = (ino_t) objid.fid_objno;
#if __DARWIN_64_BIT_INO_T
      d->cur.d_seekoff = 0;
#endif
      d->cur.d_reclen = sizeof (d->cur);
      d->cur.d_namlen = copy_len;
      d->cur.d_type = frida_darwin_objtype_to_dtype (objtype);

      memcpy (d->cur.d_name, name, copy_len);
      d->cur.d_name[copy_len] = '\0';

      return &d->cur;
    }
#elif defined (SYS_getdents64)
    {
      struct linux_dirent64
      {
        guint64 d_ino;
        gint64  d_off;
        guint16 d_reclen;
        guint8  d_type;
        char    d_name[];
      };
      struct linux_dirent64 * e;
      size_t name_len;

      e = (struct linux_dirent64 *) (d->buf + d->pos);
      d->pos += e->d_reclen;

      memset (&d->cur, 0, sizeof (d->cur));

      d->cur.d_ino = (ino_t) e->d_ino;
      d->cur.d_type = e->d_type;

      name_len = strnlen (e->d_name, sizeof (d->cur.d_name) - 1);
      memcpy (d->cur.d_name, e->d_name, name_len);
      d->cur.d_name[name_len] = '\0';

      return &d->cur;
    }
#else
    {
      struct dirent * e;
      size_t reclen;

      e = (struct dirent *) (d->buf + d->pos);
      reclen = e->d_reclen;

      if (reclen == 0)
        return NULL;

      d->pos += reclen;

      memcpy (&d->cur, e, sizeof (struct dirent));

      return &d->cur;
    }
#endif
  }
}

#ifdef HAVE_DARWIN

static int
frida_dir_refill_darwin (FridaDir * d)
{
  int n_entries;
  guint8 * p;
  int i;

  d->pos = 0;
  d->len = 0;

  while (TRUE)
  {
    static const struct attrlist attrlist =
    {
      .bitmapcount = ATTR_BIT_MAP_COUNT,
      .reserved = 0,
      .commonattr = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_NAME | ATTR_CMN_OBJTYPE | ATTR_CMN_OBJID,
      .volattr = 0,
      .dirattr = 0,
      .fileattr = 0,
      .forkattr = 0
    };

    n_entries = getattrlistbulk (d->fd, (struct attrlist *) &attrlist, d->buf, d->cap, 0);
    if (n_entries != -1)
      break;

    if (errno == EINTR)
      continue;

    return -1;
  }

  if (n_entries == 0)
    return 0;

  p = d->buf;

  for (i = 0; i != n_entries; i++)
  {
    guint32 group_len;

    group_len = *(guint32 *) p;

    if (group_len == 0)
      return -1;

    d->len += group_len;
    p += group_len;
  }

  return 1;
}

static guint8
frida_darwin_objtype_to_dtype (guint32 objtype)
{
  switch (objtype)
  {
    case 1:  /* VREG  */
      return DT_REG;
    case 2:  /* VDIR  */
      return DT_DIR;
    case 3:  /* VBLK  */
      return DT_BLK;
    case 4:  /* VCHR  */
      return DT_CHR;
    case 5:  /* VLNK  */
      return DT_LNK;
    case 6:  /* VSOCK */
      return DT_SOCK;
    case 7:  /* VFIFO */
      return DT_FIFO;
    default:
      return DT_UNKNOWN;
  }
}

#else

static ssize_t
frida_getdirents_nointr (int fd, void * buf, size_t size)
{
  while (TRUE)
  {
#if defined (HAVE_DARWIN)
    ssize_t n;
    long basep;

    n = getdirentries (fd, buf, size, &basep);
    if (n != -1)
      return n;

    if (errno == EINTR)
      continue;

    return -1;
#elif defined (SYS_getdents64)
    ssize_t n;

    n = (ssize_t) syscall (SYS_getdents64, fd, buf, size);
    if (n != -1)
      return n;

    if (errno == EINTR)
      continue;

    return -1;
#elif defined (SYS_getdirentries64)
    ssize_t n;
    off_t basep;

    n = (ssize_t) syscall (SYS_getdirentries64, fd, buf, size, &basep);
    if (n != -1)
      return n;

    if (errno == EINTR)
      continue;

    return -1;
#endif
  }
}

#endif

#endif

static void
frida_stdio_register_stream (FILE * stream)
{
  G_LOCK (frida_stdio);

  g_hash_table_add (frida_streams, stream);

  G_UNLOCK (frida_stdio);
}

static void
frida_stdio_unregister_stream (FILE * stream)
{
  G_LOCK (frida_stdio);

  g_hash_table_remove (frida_streams, stream);

  G_UNLOCK (frida_stdio);
}

#ifdef HAVE_FRIDA_DIR

static void
frida_stdio_register_dir (DIR * dirp)
{
  G_LOCK (frida_stdio);

  g_hash_table_add (frida_dirs, dirp);

  G_UNLOCK (frida_stdio);
}

static void
frida_stdio_unregister_dir (DIR * dirp)
{
  G_LOCK (frida_stdio);

  g_hash_table_remove (frida_dirs, dirp);

  G_UNLOCK (frida_stdio);
}

#endif

static void
frida_flush_all_streams (int * result)
{
  GHashTableIter iter;
  gpointer key;

  G_LOCK (frida_stdio);

  g_hash_table_iter_init (&iter, frida_streams);

  while (g_hash_table_iter_next (&iter, &key, NULL))
  {
    FILE * s = key;
    FridaFile * f;

    f = frida_file_get_impl (s);

    if (frida_file_flush_write (f) != 0)
      *result = EOF;
  }

  G_UNLOCK (frida_stdio);
}

static FridaFile *
frida_file_get_impl (FILE * stream)
{
  FridaFileHandle * h;

  assert (stream != NULL);

  h = (FridaFileHandle *) stream;

  assert (h->magic == FRIDA_FILE_MAGIC);
  assert (h->impl != NULL);

  return h->impl;
}

static void
frida_file_bind_slot (FILE * slot, FridaFile * impl)
{
  FridaFileHandle * h;

  assert (slot != NULL);
  assert (impl != NULL);

#ifndef FRIDA_STDIO_OPAQUE_FILE
  assert (sizeof (FILE) >= sizeof (FridaFileHandle));
#endif

  h = (FridaFileHandle *) slot;

  h->magic = FRIDA_FILE_MAGIC;
  h->impl = impl;
}

static FILE *
frida_file_wrap (FridaFile * impl)
{
  FILE * stream;
  FridaFileHandle * h;

  assert (impl != NULL);

#ifndef FRIDA_STDIO_OPAQUE_FILE
  assert (sizeof (FILE) >= sizeof (FridaFileHandle));
  stream = g_new0 (FILE, 1);
#else
  stream = (FILE *) g_new0 (FridaFileHandle, 1);
#endif

  h = (FridaFileHandle *) stream;
  h->magic = FRIDA_FILE_MAGIC;
  h->impl = impl;

  return stream;
}

static void
frida_file_unwrap (FILE * stream)
{
  FridaFileHandle * h;

  assert (stream != NULL);

  h = (FridaFileHandle *) stream;

  assert (h->magic == FRIDA_FILE_MAGIC);

  h->magic = 0;
  h->impl = NULL;

  g_free (stream);
}

static FridaFile *
frida_file_new (int fd, gboolean close_fd, int buf_mode)
{
  FridaFile * f;

  f = g_new0 (FridaFile, 1);

  f->fd = fd;
  f->close_fd = close_fd;
  f->buf_mode = buf_mode;

  if (buf_mode != _IONBF)
  {
    f->rcap = FRIDA_STDIO_BUFSIZE;
    f->rbuf = g_malloc (f->rcap);
    f->owns_rbuf = TRUE;

    f->wcap = FRIDA_STDIO_BUFSIZE;
    f->wbuf = g_malloc (f->wcap);
    f->owns_wbuf = TRUE;
  }

  return f;
}

static void
frida_file_free (FridaFile * f)
{
  if (f == NULL)
    return;

  if (f->owns_rbuf)
    g_clear_pointer (&f->rbuf, g_free);
  else
    f->rbuf = NULL;

  if (f->owns_wbuf)
    g_clear_pointer (&f->wbuf, g_free);
  else
    f->wbuf = NULL;

  g_free (f);
}

static int
frida_file_flush_write (FridaFile * f)
{
  size_t off = 0;

  while (off != f->wlen)
  {
    ssize_t n;

    n = frida_write_nointr (f->fd, f->wbuf + off, f->wlen - off);
    if (n == -1)
      goto io_failed;

    off += n;
  }

  f->wlen = 0;

  return 0;

io_failed:
  {
    f->err = errno;
    return -1;
  }
}

static ssize_t
frida_file_fill_read (FridaFile * f)
{
  ssize_t n;

  f->rpos = 0;
  f->rlen = 0;

  if (f->buf_mode == _IONBF)
    return 0;

  n = frida_read_nointr (f->fd, f->rbuf, f->rcap);
  if (n == -1)
    goto io_failed;

  if (n == 0)
  {
    f->eof = TRUE;
    return 0;
  }

  f->rlen = (size_t) n;

  return n;

io_failed:
  {
    f->err = errno;
    return -1;
  }
}

static void
frida_parse_fopen_mode (const char * mode, int * oflags)
{
  char c0;
  gboolean plus;

  c0 = mode[0];
  plus = (strchr (mode, '+') != NULL);

  if (c0 == 'r')
  {
    *oflags = plus ? O_RDWR : O_RDONLY;
    return;
  }

  if (c0 == 'w')
  {
    *oflags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC;
    return;
  }

  if (c0 == 'a')
  {
    *oflags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND;
    return;
  }

  g_assert_not_reached ();
}

static int
frida_write_formatted_to_fd (int fd, const char * format, va_list args)
{
  int result = 0;
  gchar * message;
  size_t len, off;

  gum_vasprintf (&message, format, args);

  len = strlen (message);
  off = 0;

  while (off != len)
  {
    ssize_t n;

    n = frida_write_nointr (fd, message + off, len - off);
    if (n == -1)
      goto io_failed;

    off += n;
  }

  goto beach;

io_failed:
  {
    result = -1;
    goto beach;
  }
beach:
  {
    g_free (message);

    return result;
  }
}

static gboolean
frida_utf8_expected_len (guint8 first, guint * out_len)
{
  if (first < 0x80)
  {
    *out_len = 1;
    return TRUE;
  }

  if ((first & 0xe0) == 0xc0)
  {
    *out_len = 2;
    return TRUE;
  }

  if ((first & 0xf0) == 0xe0)
  {
    *out_len = 3;
    return TRUE;
  }

  if ((first & 0xf8) == 0xf0)
  {
    *out_len = 4;
    return TRUE;
  }

  return FALSE;
}

#ifdef HAVE_FRIDA_DIR

static FridaDir *
frida_dir_get_impl (DIR * dirp)
{
  FridaDir * d;

  assert (dirp != NULL);

  d = (FridaDir *) dirp;

  assert (d->magic == FRIDA_DIR_MAGIC);

  return d;
}

static DIR *
frida_dir_wrap (FridaDir * impl)
{
  assert (impl != NULL);
  assert (impl->magic == FRIDA_DIR_MAGIC);

  return (DIR *) impl;
}

static void
frida_dir_free (FridaDir * d)
{
  if (d == NULL)
    return;

  assert (d->magic == FRIDA_DIR_MAGIC);

  d->magic = 0;

  g_clear_pointer (&d->buf, g_free);

  g_free (d);
}

#endif

static int
frida_open_nointr (const char * pathname, int flags, mode_t mode)
{
  while (TRUE)
  {
    int fd;

    if ((flags & O_CREAT) != 0)
      fd = open (pathname, flags, mode);
    else
      fd = open (pathname, flags);

    if (fd != -1)
      return fd;

    if (errno == EINTR)
      continue;

    return -1;
  }
}

static ssize_t
frida_read_nointr (int fd, void * buf, size_t count)
{
  while (TRUE)
  {
    ssize_t n;

    n = read (fd, buf, count);
    if (n != -1)
      return n;

    if (errno == EINTR)
      continue;

    return -1;
  }
}

static ssize_t
frida_write_nointr (int fd, const void * buf, size_t count)
{
  while (TRUE)
  {
    ssize_t n;

    n = write (fd, buf, count);
    if (n != -1)
      return n;

    if (errno == EINTR)
      continue;

    return -1;
  }
}

static int
frida_close_nointr (int fd)
{
  while (TRUE)
  {
    int r;

    r = close (fd);
    if (r != -1)
      return 0;

    if (errno == EINTR)
      continue;

    return -1;
  }
}

static off_t
frida_lseek_nointr (int fd, off_t offset, int whence)
{
  while (TRUE)
  {
    off_t r;

    r = lseek (fd, offset, whence);
    if (r != (off_t) -1)
      return r;

    if (errno == EINTR)
      continue;

    return -1;
  }
}

#endif

#ifdef HAVE_DARWIN

/*
 * Get rid of the -lresolv dependency until we actually need it, i.e. if/when
 * we expose GLib's resolvers to JavaScript. This is however not needed for
 * our current Socket.connect() API, which is neat.
 */

#include <resolv.h>

G_GNUC_INTERNAL int
res_9_init (void)
{
  g_assert_not_reached ();
  return -1;
}

G_GNUC_INTERNAL int
res_9_ninit (res_9_state state)
{
  g_assert_not_reached ();
  return -1;
}

G_GNUC_INTERNAL void
res_9_ndestroy (res_9_state state)
{
  g_assert_not_reached ();
}

G_GNUC_INTERNAL int
res_9_nquery (res_9_state state, const char * dname, int klass, int type, u_char * answer, int anslen)
{
  g_assert_not_reached ();
  return -1;
}

G_GNUC_INTERNAL int
res_9_dn_expand (const u_char * msg, const u_char * eomorig, const u_char * comp_dn, char * exp_dn, int length)
{
  g_assert_not_reached ();
  return -1;
}

#endif

#ifdef HAVE_LINUX

#ifndef __NR_dup3
# if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
#  define __NR_dup3 330
# elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
#  define __NR_dup3 292
# elif defined (HAVE_ARM)
#  define __NR_dup3 (__NR_SYSCALL_BASE + 358)
# elif defined (HAVE_MIPS)
#  if _MIPS_SIM == _MIPS_SIM_ABI32
#   define __NR_dup3 4327
#  elif _MIPS_SIM == _MIPS_SIM_ABI64
#   define __NR_dup3 5286
#  elif _MIPS_SIM == _MIPS_SIM_NABI32
#   define __NR_dup3 6290
#  else
#   error Unexpected MIPS ABI
#  endif
# elif defined (HAVE_RISCV)
#  if GLIB_SIZEOF_VOID_P == 8
#   define __NR_dup3 24
#  else
#   error RISC-V 32-bit not yet supported
#  endif
# endif
#endif

int dup3 (int old_fd, int new_fd, int flags);

G_GNUC_INTERNAL int
dup (int old_fd)
{
  return syscall (__NR_dup, old_fd);
}

G_GNUC_INTERNAL int
dup2 (int old_fd, int new_fd)
{
  if (new_fd == old_fd)
  {
    if (fcntl (new_fd, F_GETFD) == -1)
      return -1;
    return new_fd;
  }

  return dup3 (old_fd, new_fd, 0);
}

G_GNUC_INTERNAL int
dup3 (int old_fd, int new_fd, int flags)
{
  return syscall (__NR_dup3, old_fd, new_fd, flags);
}

G_GNUC_INTERNAL long
_frida_set_errno (int n)
{
  errno = n;

  return -1;
}

#endif

#if defined (HAVE_DARWIN) && GLIB_SIZEOF_VOID_P == 8

# undef read
# undef write
# undef mmap
# undef munmap

ssize_t
read (int fildes, void * buf, size_t nbyte)
{
  ssize_t result;

# ifdef HAVE_I386
  register          gint rdi asm ("rdi") = fildes;
  register gconstpointer rsi asm ("rsi") = buf;
  register         gsize rdx asm ("rdx") = nbyte;
  register         guint eax asm ("eax") = 0x2000003;

  asm volatile (
      "syscall\n\t"
      : "=a" (result)
      : "r" (rdi),
        "r" (rsi),
        "r" (rdx),
        "r" (eax)
      : "rcx", "r11", "cc", "memory"
  );
# else
  asm volatile (
      "sub sp, sp, #16 * 2\n\t"
      "stp x0, x1, [sp, #16 * 0]\n\t"
      "stp x2, x16, [sp, #16 * 1]\n\t"
      "mov x0, %1\n\t"
      "mov x1, %2\n\t"
      "mov x2, %3\n\t"
      "mov x16, 0x3\n\t"
      "svc 0x80\n\t"
      "mov %0, x0\n\t"
      "ldp x0, x1, [sp, #16 * 0]\n\t"
      "ldp x2, x16, [sp, #16 * 1]\n\t"
      "add sp, sp, #16 * 2\n\t"
      : "=r" (result)
      : "r" ((gsize) fildes),
        "r" (buf),
        "r" (nbyte)
      : "x0", "x1", "x2", "x16"
  );
# endif

  return result;
}

ssize_t
write (int fildes, const void * buf, size_t nbyte)
{
  ssize_t result;

# ifdef HAVE_I386
  register          gint rdi asm ("rdi") = fildes;
  register gconstpointer rsi asm ("rsi") = buf;
  register         gsize rdx asm ("rdx") = nbyte;
  register         guint eax asm ("eax") = 0x2000004;

  asm volatile (
      "syscall\n\t"
      : "=a" (result)
      : "r" (rdi),
        "r" (rsi),
        "r" (rdx),
        "r" (eax)
      : "rcx", "r11", "cc", "memory"
  );
# else
  asm volatile (
      "sub sp, sp, #16 * 2\n\t"
      "stp x0, x1, [sp, #16 * 0]\n\t"
      "stp x2, x16, [sp, #16 * 1]\n\t"
      "mov x0, %1\n\t"
      "mov x1, %2\n\t"
      "mov x2, %3\n\t"
      "mov x16, 0x4\n\t"
      "svc 0x80\n\t"
      "mov %0, x0\n\t"
      "ldp x0, x1, [sp, #16 * 0]\n\t"
      "ldp x2, x16, [sp, #16 * 1]\n\t"
      "add sp, sp, #16 * 2\n\t"
      : "=r" (result)
      : "r" ((gsize) fildes),
        "r" (buf),
        "r" (nbyte)
      : "x0", "x1", "x2", "x16"
  );
# endif

  return result;
}

void *
mmap (void * addr, size_t len, int prot, int flags, int fd, off_t offset)
{
  void * result;

# ifdef HAVE_I386
  register      gpointer rdi asm ("rdi") = addr;
  register         gsize rsi asm ("rsi") = len;
  register         gsize rdx asm ("rdx") = (gsize) prot;
  register         gsize r10 asm ("r10") = (gsize) flags;
  register         gsize  r8 asm ( "r8") = (gsize) fd;
  register         gsize  r9 asm ( "r9") = offset;
  register         guint eax asm ("eax") = 0x20000c5;

  asm volatile (
      "syscall\n\t"
      : "=a" (result)
      : "r" (rdi),
        "r" (rsi),
        "r" (rdx),
        "r" (r10),
        "r" (r8),
        "r" (r9),
        "r" (eax)
      : "rcx", "r11", "cc", "memory"
  );
# else
  asm volatile (
      "sub sp, sp, #16 * 4\n\t"
      "stp x0, x1, [sp, #16 * 0]\n\t"
      "stp x2, x3, [sp, #16 * 1]\n\t"
      "stp x4, x5, [sp, #16 * 2]\n\t"
      "str x16, [sp, #16 * 3]\n\t"
      "mov x0, %1\n\t"
      "mov x1, %2\n\t"
      "mov x2, %3\n\t"
      "mov x3, %4\n\t"
      "mov x4, %5\n\t"
      "mov x5, %6\n\t"
      "mov x16, 0xc5\n\t"
      "svc 0x80\n\t"
      "mov %0, x0\n\t"
      "ldp x0, x1, [sp, #16 * 0]\n\t"
      "ldp x2, x3, [sp, #16 * 1]\n\t"
      "ldp x4, x5, [sp, #16 * 2]\n\t"
      "ldr x16, [sp, #16 * 3]\n\t"
      "add sp, sp, #16 * 4\n\t"
      : "=r" (result)
      : "r" (addr),
        "r" (len),
        "r" ((gsize) prot),
        "r" ((gsize) flags),
        "r" ((gsize) fd),
        "r" (offset)
      : "x0", "x1", "x2", "x3", "x4", "x5", "x16"
  );
# endif

  return result;
}

int
munmap (void * addr, size_t len)
{
  int result;

# ifdef HAVE_I386
  register      gpointer rdi asm ("rdi") = addr;
  register         gsize rsi asm ("rsi") = len;
  register         guint eax asm ("eax") = 0x2000049;

  asm volatile (
      "syscall\n\t"
      : "=a" (result)
      : "r" (rdi),
        "r" (rsi),
        "r" (eax)
      : "rcx", "r11", "cc", "memory"
  );
# else
  asm volatile (
      "sub sp, sp, #16 * 2\n\t"
      "stp x0, x1, [sp, #16 * 0]\n\t"
      "str x16, [sp, #16 * 1]\n\t"
      "mov x0, %1\n\t"
      "mov x1, %2\n\t"
      "mov x16, 0x49\n\t"
      "svc 0x80\n\t"
      "mov %w0, w0\n\t"
      "ldp x0, x1, [sp, #16 * 0]\n\t"
      "ldr x16, [sp, #16 * 1]\n\t"
      "add sp, sp, #16 * 2\n\t"
      : "=r" (result)
      : "r" (addr),
        "r" (len)
      : "x0", "x1", "x16"
  );
# endif

  return result;
}

#endif
