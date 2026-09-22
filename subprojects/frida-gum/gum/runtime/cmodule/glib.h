#ifndef __G_LIB_H__
#define __G_LIB_H__

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#undef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#undef ABS
#define ABS(a) (((a) < 0) ? -(a) : (a))

#undef CLAMP
#define CLAMP(x, low, high) \
    (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define G_APPROX_VALUE(a, b, epsilon) \
    (((a) > (b) ? (a) - (b) : (b) - (a)) < (epsilon))

#define G_N_ELEMENTS(arr) (sizeof (arr) / sizeof ((arr)[0]))

#define G_STRUCT_OFFSET(struct_type, member) \
    ((glong) offsetof (struct_type, member))

#define G_BEGIN_DECLS
#define G_END_DECLS

typedef char gchar;
typedef short gshort;
typedef long glong;
typedef int gint;
typedef gint gboolean;

typedef unsigned char guchar;
typedef unsigned short gushort;
typedef unsigned long gulong;
typedef unsigned int guint;

typedef float gfloat;
typedef double gdouble;

typedef void * gpointer;
typedef const void * gconstpointer;

typedef int8_t gint8;
typedef uint8_t guint8;

typedef int16_t gint16;
typedef uint16_t guint16;

typedef int32_t gint32;
typedef uint32_t guint32;

typedef int64_t gint64;
typedef uint64_t guint64;

typedef ssize_t gssize;
typedef size_t gsize;

#define GPOINTER_TO_SIZE(p) ((gsize) (p))
#define GSIZE_TO_POINTER(s) ((gpointer) (gsize) (s))

#if defined (__ILP32__)
# define GPOINTER_TO_INT(p) ((gint) (p))
# define GPOINTER_TO_UINT(p) ((guint) (p))
# define GINT_TO_POINTER(i) ((gpointer) (gint) (i))
# define GUINT_TO_POINTER(u) ((gpointer) (guint) (u))
typedef signed int gintptr;
typedef unsigned int guintptr;
#elif defined (__LLP64__)
# define GPOINTER_TO_INT(p) ((gint) (gint64) (p))
# define GPOINTER_TO_UINT(p) ((guint) (guint64) (p))
# define GINT_TO_POINTER(i) ((gpointer) (gint64) (i))
# define GUINT_TO_POINTER(u) ((gpointer) (guint64) (u))
typedef signed long long gintptr;
typedef unsigned long long guintptr;
#elif defined (__LP64__)
# define GPOINTER_TO_INT(p) ((gint) (glong) (p))
# define GPOINTER_TO_UINT(p) ((guint) (gulong) (p))
# define GINT_TO_POINTER(i) ((gpointer) (glong) (i))
# define GUINT_TO_POINTER(u) ((gpointer) (gulong) (u))
typedef signed long gintptr;
typedef unsigned long guintptr;
#endif

typedef guint32 gunichar;
typedef guint16 gunichar2;

typedef void (* GCallback) (void);
typedef void (* GFunc) (gpointer data, gpointer user_data);
typedef gpointer (* GCopyFunc) (gconstpointer src, gpointer data);
typedef void (* GDestroyNotify) (gpointer data);
typedef gboolean (* GEqualFunc) (gconstpointer a, gconstpointer b);
typedef gint (* GCompareDataFunc) (gconstpointer a, gconstpointer b,
    gpointer user_data);

gchar * g_strdup (const gchar * str);
gchar * g_strndup (const gchar * str, gsize n);
gchar * g_strdup_printf (const gchar * format, ...);
gchar * g_strdup_vprintf (const gchar * format, va_list args);
gboolean g_str_has_prefix (const gchar * str, const gchar * prefix);
gboolean g_str_has_suffix (const gchar * str, const gchar * suffix);

gchar * g_utf8_strup (const gchar * str, gssize len);
gchar * g_utf8_strdown (const gchar * str, gssize len);
gchar * g_utf8_casefold (const gchar * str, gssize len);

#define g_new(struct_type, n_structs) \
    g_malloc (n_structs * sizeof (struct_type))
#define g_new0(struct_type, n_structs) \
    g_malloc0 (n_structs * sizeof (struct_type))
#define g_renew(struct_type, mem, n_structs) \
    g_realloc (mem, n_structs * sizeof (struct_type))
gpointer g_malloc (gsize n_bytes);
gpointer g_malloc0 (gsize n_bytes);
gpointer g_realloc (gpointer mem, gsize n_bytes);
gpointer g_memdup (gconstpointer mem, guint byte_size); /* Deprecated. */
gpointer g_memdup2 (gconstpointer mem, gsize byte_size);
void g_free (gpointer mem);

typedef struct _GThread GThread;

typedef gpointer (* GThreadFunc) (gpointer data);
GThread * g_thread_new (const gchar * name, GThreadFunc func,
    gpointer data);
gpointer g_thread_join (GThread * thread);
GThread * g_thread_ref (GThread * thread);
void g_thread_unref (GThread * thread);
void g_thread_yield (void);

typedef union _GMutex GMutex;
typedef struct _GCond GCond;

union _GMutex
{
  gpointer p;
  guint i[2];
};

struct _GCond
{
  gpointer p;
  guint i[2];
};

void g_mutex_init (GMutex * mutex);
void g_mutex_clear (GMutex * mutex);
void g_mutex_lock (GMutex * mutex);
void g_mutex_unlock (GMutex * mutex);
gboolean g_mutex_trylock (GMutex * mutex);

void g_cond_init (GCond * cond);
void g_cond_clear (GCond * cond);
void g_cond_wait (GCond * cond, GMutex * mutex);
void g_cond_signal (GCond * cond);
void g_cond_broadcast (GCond * cond);

gint g_atomic_int_add (volatile gint * atomic, gint val);
gssize g_atomic_pointer_add (volatile void * atomic, gssize val);

typedef struct _GString GString;

struct _GString
{
  gchar * str;
  gsize len;
  gsize allocated_len;
};

GString * g_string_new (const gchar * init);
GString * g_string_new_len (const gchar * init, gssize len);
GString * g_string_sized_new (gsize dfl_size);
gchar * g_string_free (GString * string, gboolean free_segment);
gboolean g_string_equal (const GString * v, const GString * v2);
guint g_string_hash (const GString * str);
GString * g_string_assign (GString * string, const gchar * rval);
GString * g_string_truncate (GString * string, gsize len);
GString * g_string_set_size (GString * string, gsize len);
GString * g_string_insert_len (GString * string, gssize pos, const gchar * val,
    gssize len);
GString * g_string_append (GString * string, const gchar * val);
GString * g_string_append_len (GString * string, const gchar * val, gssize len);
GString * g_string_append_c (GString * string, gchar c);
GString * g_string_append_unichar (GString * string, gunichar wc);
GString * g_string_prepend (GString * string, const gchar * val);
GString * g_string_prepend_c (GString * string, gchar c);
GString * g_string_prepend_unichar (GString * string, gunichar wc);
GString * g_string_prepend_len (GString * string, const gchar * val,
    gssize len);
GString * g_string_insert (GString * string, gssize pos, const gchar * val);
GString * g_string_insert_c (GString * string, gssize pos, gchar c);
GString * g_string_insert_unichar (GString * string, gssize pos, gunichar wc);
GString * g_string_overwrite (GString * string, gsize pos, const gchar * val);
GString * g_string_overwrite_len (GString * string, gsize pos,
    const gchar * val, gssize len);
GString * g_string_erase (GString * string, gssize pos, gssize len);
GString * g_string_ascii_down (GString * string);
GString * g_string_ascii_up (GString * string);
void g_string_vprintf (GString * string, const gchar * format, va_list args);
void g_string_printf (GString * string, const gchar * format, ...);
void g_string_append_vprintf (GString * string, const gchar * format,
    va_list args);
void g_string_append_printf (GString * string, const gchar * format, ...);

typedef struct _GPatternSpec GPatternSpec;

GPatternSpec * g_pattern_spec_new (const gchar * pattern);
void g_pattern_spec_free (GPatternSpec * pspec);
gboolean g_pattern_spec_match (GPatternSpec * pspec, gsize string_length,
    const gchar * string, const gchar * string_reversed);
gboolean g_pattern_spec_match_string (GPatternSpec * pspec,
    const gchar * string);

/* These two are deprecated: */
gboolean g_pattern_match (GPatternSpec * pspec, guint string_length,
    const gchar * string, const gchar * string_reversed);
gboolean g_pattern_match_string (GPatternSpec * pspec, const gchar * string);

typedef struct _GArray GArray;

struct _GArray
{
  gchar * data;
  guint len;
};

#define g_array_append_val(a, v) g_array_append_vals (a, &(v), 1)
#define g_array_prepend_val(a, v) g_array_prepend_vals (a, &(v), 1)
#define g_array_insert_val(a, i, v) g_array_insert_vals (a, i, &(v), 1)
#define g_array_index(a, t, i) (((t *) (void *) (a)->data) [(i)])

GArray * g_array_new (gboolean zero_terminated, gboolean clear_,
    guint element_size);
GArray * g_array_sized_new (gboolean zero_terminated, gboolean clear_,
    guint element_size, guint reserved_size);
gchar * g_array_free (GArray * array, gboolean free_segment);
GArray * g_array_ref (GArray * array);
void g_array_unref (GArray * array);
guint g_array_get_element_size (GArray * array);
GArray * g_array_append_vals (GArray * array, gconstpointer data, guint len);
GArray * g_array_prepend_vals (GArray * array, gconstpointer data, guint len);
GArray * g_array_insert_vals (GArray * array, guint index_, gconstpointer data,
    guint len);
GArray * g_array_set_size (GArray * array, guint length);
GArray * g_array_remove_index (GArray * array, guint index_);
GArray * g_array_remove_index_fast (GArray * array, guint index_);
GArray * g_array_remove_range (GArray * array, guint index_, guint length);
void g_array_sort_with_data (GArray * array, GCompareDataFunc compare_func,
    gpointer user_data);
void g_array_set_clear_func (GArray * array, GDestroyNotify clear_func);

typedef struct _GPtrArray GPtrArray;

struct _GPtrArray
{
  gpointer * pdata;
  guint len;
};

#define g_ptr_array_index(a, i) ((a)->pdata)[i]

GPtrArray * g_ptr_array_new (void);
GPtrArray * g_ptr_array_new_with_free_func (GDestroyNotify element_free_func);
gpointer * g_ptr_array_steal (GPtrArray * array, gsize * len);
GPtrArray * g_ptr_array_copy (GPtrArray * array, GCopyFunc func,
    gpointer user_data);
GPtrArray * g_ptr_array_sized_new (guint reserved_size);
GPtrArray * g_ptr_array_new_full (guint reserved_size,
    GDestroyNotify element_free_func);
GPtrArray * g_ptr_array_new_null_terminated (guint reserved_size,
    GDestroyNotify element_free_func, gboolean null_terminated);
gpointer * g_ptr_array_free (GPtrArray * array, gboolean free_seg);
GPtrArray * g_ptr_array_ref (GPtrArray * array);
void g_ptr_array_unref (GPtrArray * array);
void g_ptr_array_set_free_func (GPtrArray * array,
    GDestroyNotify element_free_func);
void g_ptr_array_set_size (GPtrArray * array, gint length);
gpointer g_ptr_array_remove_index (GPtrArray * array, guint index_);
gpointer g_ptr_array_remove_index_fast (GPtrArray * array, guint index_);
gpointer g_ptr_array_steal_index (GPtrArray * array, guint index_);
gpointer g_ptr_array_steal_index_fast (GPtrArray * array, guint index_);
gboolean g_ptr_array_remove (GPtrArray * array, gpointer data);
gboolean g_ptr_array_remove_fast (GPtrArray * array, gpointer data);
GPtrArray * g_ptr_array_remove_range (GPtrArray * array, guint index_,
    guint length);
void g_ptr_array_add (GPtrArray * array, gpointer data);
void g_ptr_array_extend (GPtrArray * array_to_extend, GPtrArray * array,
    GCopyFunc func, gpointer user_data);
void g_ptr_array_extend_and_steal (GPtrArray * array_to_extend,
    GPtrArray * array);
void g_ptr_array_insert (GPtrArray * array, gint index_, gpointer data);
void g_ptr_array_sort_with_data (GPtrArray * array,
    GCompareDataFunc compare_func, gpointer user_data);
void g_ptr_array_foreach (GPtrArray * array, GFunc func, gpointer user_data);
gboolean g_ptr_array_find (GPtrArray * haystack, gconstpointer needle,
    guint * index_);
gboolean g_ptr_array_find_with_equal_func (GPtrArray * haystack,
    gconstpointer needle, GEqualFunc equal_func, guint * index_);

typedef struct _GHashTable GHashTable;
typedef struct _GHashTableIter GHashTableIter;

typedef guint (* GHashFunc) (gconstpointer key);

struct _GHashTableIter
{
  gpointer dummy1;
  gpointer dummy2;
  gpointer dummy3;
  int dummy4;
  gboolean dummy5;
  gpointer dummy6;
};

GHashTable * g_hash_table_new_full (GHashFunc hash_func,
    GEqualFunc key_equal_func, GDestroyNotify key_destroy_func,
    GDestroyNotify value_destroy_func);
gboolean g_hash_table_insert (GHashTable * hash_table, gpointer key,
    gpointer value);
gboolean g_hash_table_replace (GHashTable * hash_table, gpointer key,
    gpointer value);
gboolean g_hash_table_add (GHashTable * hash_table, gpointer key);
gboolean g_hash_table_remove (GHashTable * hash_table, gconstpointer key);
void g_hash_table_remove_all (GHashTable * hash_table);
gpointer g_hash_table_lookup (GHashTable * hash_table, gconstpointer key);
gboolean g_hash_table_contains (GHashTable * hash_table, gconstpointer key);
gboolean g_hash_table_lookup_extended (GHashTable * hash_table,
    gconstpointer lookup_key, gpointer * orig_key, gpointer * value);
guint g_hash_table_size (GHashTable * hash_table);

void g_hash_table_iter_init (GHashTableIter * iter, GHashTable * hash_table);
gboolean g_hash_table_iter_next (GHashTableIter * iter, gpointer * key,
    gpointer * value);
GHashTable * g_hash_table_iter_get_hash_table (GHashTableIter * iter);
void g_hash_table_iter_remove (GHashTableIter * iter);
void g_hash_table_iter_replace (GHashTableIter * iter, gpointer value);
void g_hash_table_iter_steal (GHashTableIter * iter);

GHashTable * g_hash_table_ref (GHashTable * hash_table);
void g_hash_table_unref (GHashTable * hash_table);

gboolean g_str_equal (gconstpointer v1, gconstpointer v2);
guint g_str_hash (gconstpointer v);

gboolean g_int_equal (gconstpointer v1, gconstpointer v2);
guint g_int_hash (gconstpointer v);

gboolean g_int64_equal (gconstpointer v1, gconstpointer v2);
guint g_int64_hash (gconstpointer v);

gboolean g_double_equal (gconstpointer v1, gconstpointer v2);
guint g_double_hash (gconstpointer v);

guint g_direct_hash (gconstpointer v);
gboolean g_direct_equal (gconstpointer v1, gconstpointer v2);

typedef struct _GTimer GTimer;

#define G_USEC_PER_SEC 1000000

GTimer * g_timer_new (void);
void g_timer_destroy (GTimer * timer);
void g_timer_start (GTimer * timer);
void g_timer_stop (GTimer * timer);
void g_timer_continue (GTimer * timer);
gdouble g_timer_elapsed (GTimer * timer, gulong * microseconds);

void g_usleep (gulong microseconds);

gint64 g_get_monotonic_time (void);
gint64 g_get_real_time (void);

gpointer g_object_ref (gpointer object);
void g_object_unref (gpointer object);

gsize g_base64_encode_step (const guchar * in, gsize len, gboolean break_lines,
    gchar * out, gint * state, gint * save);
gsize g_base64_encode_close (gboolean break_lines, gchar * out, gint * state,
    gint * save);
gchar * g_base64_encode (const guchar * data, gsize len);
gsize g_base64_decode_step (const gchar * in, gsize len, guchar * out,
    gint * state, guint * save);
guchar * g_base64_decode (const gchar * text, gsize * out_len);
guchar * g_base64_decode_inplace (gchar * text, gsize * out_len);

typedef enum {
  G_CHECKSUM_MD5,
  G_CHECKSUM_SHA1,
  G_CHECKSUM_SHA256,
  G_CHECKSUM_SHA512,
  G_CHECKSUM_SHA384
} GChecksumType;

typedef struct _GChecksum GChecksum;

struct _GChecksum
{
  guint8 opaque[296];
};

gssize g_checksum_type_get_length (GChecksumType checksum_type);
GChecksum * g_checksum_new (GChecksumType checksum_type);
GChecksum * g_checksum_copy (const GChecksum * checksum);
void g_checksum_free (GChecksum * checksum);
void g_checksum_reset (GChecksum * checksum);
void g_checksum_update (GChecksum * checksum, const guchar * data,
    gssize length);
const gchar * g_checksum_get_string (GChecksum * checksum);
void g_checksum_get_digest (GChecksum * checksum, guint8 * buffer,
    gsize * digest_len);

typedef guint32 GQuark;
typedef struct _GError GError;

struct _GError
{
  GQuark domain;
  gint code;
  gchar * message;
};

void g_error_free (GError * error);
void g_clear_error (GError ** error);

typedef struct _GIConv * GIConv;

GIConv g_iconv_open (const gchar * to_codeset, const gchar * from_codeset);
gsize g_iconv (GIConv converter,
    gchar ** inbuf, gsize * inbytes_left,
    gchar ** outbuf, gsize * outbytes_left);
gint g_iconv_close (GIConv converter);

gchar * g_convert (const gchar * str, gssize len,
    const gchar * to_codeset, const gchar * from_codeset,
    gsize * bytes_read, gsize * bytes_written, GError ** error);
gchar * g_convert_with_iconv (const gchar * str, gssize len, GIConv converter,
    gsize * bytes_read, gsize * bytes_written, GError ** error);
gchar * g_convert_with_fallback (const gchar * str, gssize len,
    const gchar * to_codeset, const gchar * from_codeset,
    const gchar * fallback, gsize * bytes_read, gsize * bytes_written,
    GError ** error);

#endif
