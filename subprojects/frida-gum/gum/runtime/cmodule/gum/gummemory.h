#ifndef __GUM_MEMORY_H__
#define __GUM_MEMORY_H__

#include "gumdefs.h"

typedef guint GumPtrauthSupport;
typedef guint GumPageProtection;
typedef struct _GumMatchPattern GumMatchPattern;

enum _GumPtrauthSupport
{
  GUM_PTRAUTH_INVALID,
  GUM_PTRAUTH_UNSUPPORTED,
  GUM_PTRAUTH_SUPPORTED
};

enum _GumPageProtection
{
  GUM_PAGE_NO_ACCESS = 0,
  GUM_PAGE_READ      = (1 << 0),
  GUM_PAGE_WRITE     = (1 << 1),
  GUM_PAGE_EXECUTE   = (1 << 2),
};

typedef void (* GumMemoryPatchApplyFunc) (gpointer mem, gpointer user_data);
typedef gboolean (* GumMemoryScanMatchFunc) (GumAddress address, gsize size,
    gpointer user_data);

gpointer gum_sign_code_pointer (gpointer value);
gpointer gum_strip_code_pointer (gpointer value);
GumAddress gum_sign_code_address (GumAddress value);
GumAddress gum_strip_code_address (GumAddress value);
GumPtrauthSupport gum_query_ptrauth_support (void);
gboolean gum_memory_query_protection (gconstpointer address,
    GumPageProtection * prot);
guint8 * gum_memory_read (gconstpointer address, gsize len,
    gsize * n_bytes_read);
gboolean gum_memory_write (gpointer address, const guint8 * bytes, gsize len);
gboolean gum_memory_patch_code (gpointer address, gsize size,
    GumMemoryPatchApplyFunc apply, gpointer apply_data);
gboolean gum_memory_mark_code (gpointer address, gsize size);

void gum_memory_scan (const GumMemoryRange * range,
    const GumMatchPattern * pattern, GumMemoryScanMatchFunc func,
    gpointer user_data);

GumMatchPattern * gum_match_pattern_new_from_string (
    const gchar * pattern_str);
GumMatchPattern * gum_match_pattern_ref (GumMatchPattern * pattern);
void gum_match_pattern_unref (GumMatchPattern * pattern);
guint gum_match_pattern_get_size (const GumMatchPattern * pattern);

void gum_ensure_code_readable (gconstpointer address, gsize size);

void gum_mprotect (gpointer address, gsize size, GumPageProtection prot);
gboolean gum_try_mprotect (gpointer address, gsize size,
    GumPageProtection prot);

void gum_clear_cache (gpointer address, gsize size);

#endif
