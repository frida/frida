/*
 * Copyright (C) 2015-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Alex Soler <asoler@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumkernel.h"

#include "gum-init.h"
#include "gum/gumdarwin.h"
#include "gumdarwin-priv.h"
#include "gummemory-priv.h"

#include <mach/mach.h>
#include <mach-o/loader.h>
#include <sys/sysctl.h>

#define GUM_KERNEL_SLIDE_OFFSET 0x1000000
#define GUM_KERNEL_SLIDE_SIZE 0x200000

typedef struct _GumKernelScanContext GumKernelScanContext;
typedef struct _GumKernelEnumerateModuleRangesContext
    GumKernelEnumerateModuleRangesContext;
typedef struct _GumKernelSearchKextContext GumKernelSearchKextContext;
typedef struct _GumKernelKextInfo GumKernelKextInfo;
typedef struct _GumKernelFindRangeByNameContext GumKernelFindRangeByNameContext;
typedef struct _GumEmitModuleContext GumEmitModuleContext;
typedef struct _GumKernelKextByNameContext GumKernelKextByNameContext;

struct _GumKernelScanContext
{
  GumMemoryScanMatchFunc func;
  gpointer user_data;

  GumAddress cursor_userland;
  GumAddress cursor_kernel;

  gboolean carry_on;
};

struct _GumKernelEnumerateModuleRangesContext
{
  GumPageProtection protection;
  GumFoundKernelModuleRangeFunc func;
  gpointer user_data;
};

struct _GumKernelSearchKextContext
{
  GHashTable * kexts;
};

struct _GumKernelKextInfo
{
  gchar name[0x41];
  GumAddress address;
};

struct _GumKernelFindRangeByNameContext
{
  const gchar * name;
  gboolean found;
  GumMemoryRange range;
};

struct _GumEmitModuleContext
{
  GumFoundKernelModuleFunc func;
  gpointer user_data;
};

struct _GumKernelKextByNameContext
{
  const gchar * module_name;
  gboolean found;
  GumDarwinModule * module;
};

typedef gboolean (* GumFoundKextFunc) (GumDarwinModule * module,
    gpointer user_data);

static gboolean gum_kernel_emit_match (GumAddress address, gsize size,
    GumKernelScanContext * ctx);
static void gum_kernel_enumerate_kexts (GumFoundKextFunc func,
    gpointer user_data);
static gboolean gum_kernel_scan_section (const gchar * section_name,
    const gchar * pattern_string, GumMemoryScanMatchFunc func,
    gpointer user_data);
static gboolean gum_kernel_emit_module_range (
    const GumDarwinSectionDetails * section, gpointer user_data);
static gboolean gum_kernel_emit_module (GumDarwinModule * module,
    gpointer user_data);
static gsize gum_darwin_module_estimate_size (
    GumDarwinModule * module);
static gboolean gum_kernel_range_by_name (GumMemoryRange * out_range,
    const gchar * name);
static gboolean gum_kernel_find_range_by_name (
    GumKernelModuleRangeDetails * details,
    GumKernelFindRangeByNameContext * ctx);
static gboolean gum_kernel_store_kext_addr (GumAddress address,
    gsize size, GumKernelSearchKextContext * ctx);
static gboolean gum_kernel_store_kext_name (GumAddress address,
    gsize size, GumKernelSearchKextContext * ctx);
static GumDarwinModule * gum_kernel_find_module_by_name (
    const gchar * module_name);
static gboolean gum_kernel_kext_by_name (GumDarwinModule * module,
    GumKernelKextByNameContext * ctx);
static GumDarwinModule * gum_kernel_get_module (void);
static GumAddress * gum_kernel_do_find_base_address (void);

#ifdef HAVE_ARM64

static float gum_kernel_get_version (void);
static GumAddress gum_kernel_get_base_from_all_image_info (void);
static GumAddress gum_kernel_bruteforce_base (GumAddress unslid_base);
static gboolean gum_kernel_is_header (GumAddress address);
static gboolean gum_kernel_has_kld (GumAddress address);
static gboolean gum_kernel_find_first_hit (GumAddress address, gsize size,
    gboolean * found);

#endif

mach_port_t gum_kernel_get_task (void);
static mach_port_t gum_kernel_do_init (void);
static void gum_kernel_do_deinit (void);

static GumDarwinModule * gum_kernel_cached_module = NULL;
static GumAddress gum_kernel_external_base = 0;

gboolean
gum_kernel_api_is_available (void)
{
  return gum_kernel_get_task () != MACH_PORT_NULL;
}

guint
gum_kernel_query_page_size (void)
{
  return vm_kernel_page_size;
}

GumAddress
gum_kernel_alloc_n_pages (guint n_pages)
{
  mach_vm_address_t result;
  mach_port_t task;
  gsize page_size, size;
  G_GNUC_UNUSED kern_return_t kr;
  G_GNUC_UNUSED gboolean written;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return 0;

  page_size = vm_kernel_page_size;
  size = (n_pages + 1) * page_size;

  result = 0;
  kr = mach_vm_allocate (task, &result, size, VM_FLAGS_ANYWHERE);
  g_assert (kr == KERN_SUCCESS);

  written = gum_darwin_write (task, result, (guint8 *) &size, sizeof (gsize));
  g_assert (written);

  kr = vm_protect (task, result + page_size, size - page_size,
      TRUE, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
  g_assert (kr == KERN_SUCCESS);

  return result + page_size;
}

void
gum_kernel_free_pages (GumAddress mem)
{
  mach_port_t task;
  gsize page_size;
  mach_vm_address_t address;
  mach_vm_size_t * size;
  gsize bytes_read;
  G_GNUC_UNUSED kern_return_t kr;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return;

  page_size = vm_kernel_page_size;

  address = mem - page_size;
  size = (mach_vm_size_t *) gum_kernel_read (address, sizeof (mach_vm_size_t),
      &bytes_read);
  if (size == NULL)
    return;
  if (bytes_read < sizeof (mach_vm_size_t))
  {
    g_free (size);
    return;
  }

  kr = mach_vm_deallocate (task, address, *size);
  g_free (size);
  g_assert (kr == KERN_SUCCESS);
}

gboolean
gum_kernel_try_mprotect (GumAddress address,
                         gsize size,
                         GumPageProtection prot)
{
  mach_port_t task;
  gsize page_size;
  GumAddress aligned_address;
  gsize aligned_size;
  vm_prot_t mach_prot;
  kern_return_t kr;

  g_assert (size != 0);

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return FALSE;

  page_size = vm_kernel_page_size;
  aligned_address = address & ~(page_size - 1);
  aligned_size =
      (1 + ((address + size - 1 - aligned_address) / page_size)) * page_size;
  mach_prot = gum_page_protection_to_mach (prot);

  kr = mach_vm_protect (task, aligned_address, aligned_size, FALSE, mach_prot);

  return kr == KERN_SUCCESS;
}

guint8 *
gum_kernel_read (GumAddress address,
                 gsize len,
                 gsize * n_bytes_read)
{
  mach_port_t task;
  guint page_size;
  guint8 * result;
  gsize offset;
  kern_return_t kr;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return NULL;

  /* Failsafe size, smaller than the kernel page size. */
  page_size = 2048;
  result = g_malloc (len);
  offset = 0;

  while (offset != len)
  {
    GumAddress chunk_address, page_address;
    gsize chunk_size, page_offset;

    chunk_address = address + offset;
    page_address = chunk_address & ~GUM_ADDRESS (page_size - 1);
    page_offset = chunk_address - page_address;
    chunk_size = MIN (len - offset, page_size - page_offset);

    mach_vm_size_t n_bytes_read;

    /* mach_vm_read corrupts memory on iOS */
    kr = mach_vm_read_overwrite (task, chunk_address, chunk_size,
        (vm_address_t) (result + offset), &n_bytes_read);
    if (kr != KERN_SUCCESS)
      break;
    g_assert (n_bytes_read == chunk_size);

    offset += chunk_size;
  }

  if (offset == 0)
  {
    g_free (result);
    result = NULL;
  }

  if (n_bytes_read != NULL)
    *n_bytes_read = offset;

  return result;
}

gboolean
gum_kernel_write (GumAddress address,
                  const guint8 * bytes,
                  gsize len)
{
  mach_port_t task;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return FALSE;

  return gum_darwin_write (task, address, bytes, len);
}

void
gum_kernel_enumerate_ranges (GumPageProtection prot,
                             GumFoundRangeFunc func,
                             gpointer user_data)
{
  mach_port_t task;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return;

  gum_darwin_enumerate_ranges (task, prot, func, user_data);
}

void
gum_kernel_scan (const GumMemoryRange * range,
                 const GumMatchPattern * pattern,
                 GumMemoryScanMatchFunc func,
                 gpointer user_data)
{
  GumKernelScanContext ctx;
  GumAddress cursor, end;
  guint pattern_size;
  gsize size, max_chunk_size;

  ctx.func = func;
  ctx.user_data = user_data;

  cursor = range->base_address;
  pattern_size = gum_match_pattern_get_size (pattern);
  size = range->size;
  max_chunk_size = MAX (pattern_size * 2, 2048 * 512);
  end = cursor + size - pattern_size;

  while (cursor <= end)
  {
    gsize chunk_size;
    guint8 * haystack;
    GumMemoryRange subrange;

    chunk_size = MIN (size, max_chunk_size);
    haystack = gum_kernel_read (cursor, chunk_size, NULL);
    if (haystack == NULL)
      return;

    subrange.base_address = GUM_ADDRESS (haystack);
    subrange.size = chunk_size;

    ctx.cursor_userland = GUM_ADDRESS (haystack);
    ctx.cursor_kernel = GUM_ADDRESS (cursor);

    gum_memory_scan (&subrange, pattern,
        (GumMemoryScanMatchFunc) gum_kernel_emit_match, &ctx);

    g_free (haystack);

    if (!ctx.carry_on)
      return;

    cursor += chunk_size - pattern_size + 1;
    size -= chunk_size - pattern_size + 1;
  }
}

static gboolean
gum_kernel_emit_match (GumAddress address,
                       gsize size,
                       GumKernelScanContext * ctx)
{
  GumAddress address_kernel = address - ctx->cursor_userland +
      ctx->cursor_kernel;

  ctx->carry_on = ctx->func (address_kernel, size, ctx->user_data);

  return ctx->carry_on;
}

void
gum_kernel_enumerate_modules (GumFoundKernelModuleFunc func,
                              gpointer user_data)
{
  GumEmitModuleContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  if (!gum_kernel_emit_module (gum_kernel_get_module (), &ctx))
    return;

  gum_kernel_enumerate_kexts (gum_kernel_emit_module, &ctx);
}

static gboolean
gum_kernel_emit_module (GumDarwinModule * module,
                        gpointer user_data)
{
  GumEmitModuleContext * ctx = user_data;
  GumKernelModuleDetails details;
  GumMemoryRange range;

  range.base_address = module->base_address;
  range.size = gum_darwin_module_estimate_size (module);

  details.name = module->name;
  details.range = &range;
  details.path = NULL;

  return ctx->func (&details, ctx->user_data);
}

static void
gum_kernel_enumerate_kexts (GumFoundKextFunc func,
                            gpointer user_data)
{
  mach_port_t task;
  GHashTable * kexts;
  GumKernelSearchKextContext kext_ctx;
  GHashTableIter iter;
  gpointer item;
  gpointer header_addr;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return;

  kexts = g_hash_table_new (NULL, NULL);
  kext_ctx.kexts = kexts;

  /* Search the first 8 bytes of mach0 header. */
  if (!gum_kernel_scan_section ("__PRELINK_TEXT.__text", "cffaedfe0c00000100",
        (GumMemoryScanMatchFunc) gum_kernel_store_kext_addr, &kext_ctx))
  {
    return;
  }

  /* Search for "com.apple" string. */
  if (!gum_kernel_scan_section ("__PRELINK_DATA.__data", "636f6d2e6170706c65",
        (GumMemoryScanMatchFunc) gum_kernel_store_kext_name, &kext_ctx))
  {
    if (!gum_kernel_scan_section ("__PRELINK_TEXT.__text", "636f6d2e6170706c65",
          (GumMemoryScanMatchFunc) gum_kernel_store_kext_name, &kext_ctx))
    {
      return;
    }
  }

  g_hash_table_iter_init (&iter, kexts);
  while (g_hash_table_iter_next (&iter, &header_addr, &item))
  {
    GumKernelKextInfo * kext = item;
    GumDarwinModule * module;

    if (*kext->name == '\0')
      continue;

    module = gum_darwin_module_new_from_memory (kext->name, task, kext->address,
        GUM_DARWIN_MODULE_FLAGS_NONE, NULL);

    if (module == NULL)
      continue;

    if (!func (module, user_data))
      break;
  }

  g_hash_table_unref (kexts);
}

static gboolean
gum_kernel_scan_section (const gchar * section_name,
                         const gchar * pattern_string,
                         GumMemoryScanMatchFunc func,
                         gpointer user_data)
{
  GumMemoryRange range;
  GumMatchPattern * pattern;

  if (!gum_kernel_range_by_name (&range, section_name))
    return FALSE;

  pattern = gum_match_pattern_new_from_string (pattern_string);
  if (pattern == NULL)
    return FALSE;

  gum_kernel_scan (&range, pattern, func, user_data);

  gum_match_pattern_unref (pattern);

  return TRUE;
}

static gsize
gum_darwin_module_estimate_size (GumDarwinModule * module)
{
  gsize index = 0, size = 0;

  do
  {
    const GumDarwinSegment * segment;

    segment = gum_darwin_module_get_nth_segment (module, index++);
    size += segment->vm_size;
  }
  while (index < module->segments->len);

  return size;
}

static gboolean
gum_kernel_range_by_name (GumMemoryRange * out_range,
                          const gchar * name)
{
  GumKernelFindRangeByNameContext ctx;

  ctx.name = name;
  ctx.found = FALSE;

  gum_kernel_enumerate_module_ranges ("Kernel", GUM_PAGE_NO_ACCESS,
      (GumFoundKernelModuleRangeFunc) gum_kernel_find_range_by_name, &ctx);

  if (ctx.found)
  {
    out_range->base_address = ctx.range.base_address;
    out_range->size = ctx.range.size;
    return TRUE;
  }

  return FALSE;
}

static gboolean
gum_kernel_find_range_by_name (GumKernelModuleRangeDetails * details,
                               GumKernelFindRangeByNameContext * ctx)
{
  if (strncmp (details->name, ctx->name, sizeof (details->name)) == 0)
  {
    ctx->range.base_address = details->address;
    ctx->range.size = details->size;
    ctx->found = TRUE;
    return FALSE;
  }

  return TRUE;
}

static gboolean
gum_kernel_store_kext_addr (GumAddress address,
                            gsize size,
                            GumKernelSearchKextContext * ctx)
{
  GumKernelKextInfo * kext;

  kext = g_slice_new0 (GumKernelKextInfo);
  kext->address = address;

  g_hash_table_insert (ctx->kexts, GSIZE_TO_POINTER (address), kext);

  return TRUE;
}

static gboolean
gum_kernel_store_kext_name (GumAddress address,
                            gsize size,
                            GumKernelSearchKextContext * ctx)
{
  GumKernelKextInfo * kext;
  guint8 * buf;

  /* Reference: osfmk/mach/kmod.h */
  buf = gum_kernel_read (address + 0x8c, 8, NULL);
  kext = g_hash_table_lookup (ctx->kexts, *((GumAddress **) buf));
  g_free (buf);

  if (kext == NULL)
    return TRUE;

  buf = gum_kernel_read (address, 0x40, NULL);
  strncpy (kext->name, (gchar*) buf, 0x40);
  kext->name[0x40] = 0;
  g_free (buf);

  return TRUE;
}

void
gum_kernel_enumerate_module_ranges (const gchar * module_name,
                                    GumPageProtection prot,
                                    GumFoundKernelModuleRangeFunc func,
                                    gpointer user_data)
{
  GumDarwinModule * module;
  GumKernelEnumerateModuleRangesContext ctx;

  module = gum_kernel_find_module_by_name (module_name);
  if (module == NULL)
    return;

  ctx.protection = prot;
  ctx.func = func;
  ctx.user_data = user_data;

  gum_darwin_module_enumerate_sections (module, gum_kernel_emit_module_range,
      &ctx);
}

static GumDarwinModule *
gum_kernel_find_module_by_name (const gchar * module_name)
{
  GumKernelKextByNameContext ctx;

  if (strcmp (module_name, "Kernel") == 0)
    return gum_kernel_get_module ();

  ctx.module_name = module_name;
  ctx.found = FALSE;

  gum_kernel_enumerate_kexts ((GumFoundKextFunc) gum_kernel_kext_by_name, &ctx);

  if (!ctx.found)
    return NULL;

  return ctx.module;
}

static gboolean
gum_kernel_kext_by_name (GumDarwinModule * module,
                         GumKernelKextByNameContext * ctx)
{
  ctx->found = strcmp (module->name, ctx->module_name) == 0;

  if (ctx->found)
    ctx->module = module;

  return !ctx->found;
}

static gboolean
gum_kernel_emit_module_range (const GumDarwinSectionDetails * section,
                              gpointer user_data)
{
  GumKernelEnumerateModuleRangesContext * ctx = user_data;
  GumPageProtection prot;
  GumKernelModuleRangeDetails details;

  prot = gum_page_protection_from_mach (section->protection);
  if ((prot & ctx->protection) != ctx->protection)
    return TRUE;

  g_snprintf (details.name, sizeof (details.name), "%s.%s",
      section->segment_name, section->section_name);
  details.address = section->vm_address;
  details.size = section->size;
  details.protection = prot;

  return ctx->func (&details, ctx->user_data);
}

static GumDarwinModule *
gum_kernel_get_module (void)
{
  mach_port_t task;
  GumAddress base;

  if (gum_kernel_cached_module != NULL)
    return gum_kernel_cached_module;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return NULL;

  base = gum_kernel_find_base_address ();

  gum_kernel_cached_module = gum_darwin_module_new_from_memory ("Kernel", task,
      base, GUM_DARWIN_MODULE_FLAGS_NONE, NULL);

  return gum_kernel_cached_module;
}

GumAddress
gum_kernel_find_base_address (void)
{
  static GOnce get_base_once = G_ONCE_INIT;

  if (gum_kernel_external_base != 0)
    return gum_kernel_external_base;

  g_once (&get_base_once, (GThreadFunc) gum_kernel_do_find_base_address, NULL);

  return *((GumAddress *) get_base_once.retval);
}

void
gum_kernel_set_base_address (GumAddress base)
{
  gum_kernel_external_base = base;
}

static GumAddress *
gum_kernel_do_find_base_address (void)
{
  GumAddress base = 0;

#ifdef HAVE_ARM64
  float version;

  base = gum_kernel_get_base_from_all_image_info ();
  if (base == 0)
  {
    version = gum_kernel_get_version ();
    if (version >= 16.0) /* iOS 10.0+ */
    {
      base = gum_kernel_bruteforce_base (
          G_GUINT64_CONSTANT (0xfffffff007004000));
    }
    else if (version >= 15.0) /* iOS 9.0+ */
    {
      base = gum_kernel_bruteforce_base (
          G_GUINT64_CONSTANT (0xffffff8004004000));
    }
  }
#endif

  return g_slice_dup (GumAddress, &base);
}

#ifdef HAVE_ARM64

static float
gum_kernel_get_version (void)
{
  char buf[256];
  size_t size;
  G_GNUC_UNUSED int res;
  float version;

  size = sizeof (buf);
  res = sysctlbyname ("kern.osrelease", buf, &size, NULL, 0);
  g_assert (res == 0);

  version = atof (buf);

  return version;
}

static GumAddress
gum_kernel_get_base_from_all_image_info (void)
{
  mach_port_t task;
  kern_return_t kr;
  DyldInfo info_raw;
  mach_msg_type_number_t info_count = DYLD_INFO_COUNT;

  task = gum_kernel_get_task ();
  if (task == MACH_PORT_NULL)
    return 0;

  kr = task_info (task, TASK_DYLD_INFO, (task_info_t) &info_raw, &info_count);
  if (kr != KERN_SUCCESS)
    return 0;

  if (info_raw.info_64.all_image_info_addr == 0 &&
      info_raw.info_64.all_image_info_size == 0)
  {
    return 0;
  }

  return info_raw.info_64.all_image_info_size +
      G_GUINT64_CONSTANT (0xfffffff007004000);
}

static gboolean
gum_kernel_is_debug (void)
{
  char buf[256];
  size_t size;
  G_GNUC_UNUSED int res;

  size = sizeof (buf);
  res = sysctlbyname ("kern.bootargs", buf, &size, NULL, 0);
  g_assert (res == 0);

  return strstr (buf, "debug") != NULL;
}

static GumAddress
gum_kernel_bruteforce_base (GumAddress unslid_base)
{
  /*
   * References & credits:
   * http://conference.hackinthebox.org/hitbsecconf2012kul/materials
   *    /D1T2%20-%20Mark%20Dowd%20&%20Tarjei%20Mandt%20-%20iOS6%20Security.pdf
   * https://www.theiphonewiki.com/wiki/Kernel_ASLR
   * https://www.wikiwand.com/en/Address_space_layout_randomization
   * https://www.slideshare.net/i0n1c
   *    /csw2013-stefan-esserios6exploitation280dayslater
   *    /19-KASLR_iOS_6_introduces_KASLR
   * http://people.oregonstate.edu/~jangye/assets/papers/2016/jang:drk-bh.pdf
   */

  gint slide_byte;
  gboolean is_debug;

  is_debug = gum_kernel_is_debug ();

  if (is_debug && gum_kernel_is_header (unslid_base))
    return unslid_base;

  if (gum_kernel_is_header (unslid_base + 0x21000000))
    return unslid_base + 0x21000000;

  for (slide_byte = 255; slide_byte > 0; slide_byte--)
  {
    GumAddress base = unslid_base;

    base += GUM_KERNEL_SLIDE_OFFSET +
        ((1 + slide_byte) * GUM_KERNEL_SLIDE_SIZE);

    if (gum_kernel_is_header (base))
      return base;
  }

  return 0;
}

static gboolean
gum_kernel_is_header (GumAddress address)
{
  gboolean result = FALSE;
  guint8 * header = NULL;
  gsize n_bytes_read;

  header = gum_kernel_read (address, 28, &n_bytes_read);
  if (n_bytes_read != 28 || header == NULL)
    goto bail_out;

  /* Magic */
  if (*((guint32*) (header + 0)) != MH_MAGIC_64)
    goto bail_out;

  /* Cpu type */
  if (*((guint32*) (header + 4)) != CPU_TYPE_ARM64)
    goto bail_out;

  /* File type */
  if (*((guint32*) (header + 12)) != MH_EXECUTE)
    goto bail_out;

  if (!gum_kernel_has_kld (address))
    goto bail_out;

  result = TRUE;

bail_out:
  g_free (header);

  return result;
}

static gboolean
gum_kernel_has_kld (GumAddress address)
{
  gboolean found = FALSE;
  GumMemoryRange range;
  GumMatchPattern * pattern;

  range.base_address = address;
  range.size = 2048;

  /* __KLD */
  pattern = gum_match_pattern_new_from_string ("5f 5f 4b 4c 44");
  if (pattern == NULL)
    return FALSE;

  gum_kernel_scan (&range, pattern,
      (GumMemoryScanMatchFunc) gum_kernel_find_first_hit, &found);

  gum_match_pattern_unref (pattern);

  return found;
}

static gboolean
gum_kernel_find_first_hit (GumAddress address,
                           gsize size,
                           gboolean * found)
{
  *found = TRUE;

  return FALSE;
}

#endif

mach_port_t
gum_kernel_get_task (void)
{
  static GOnce init_once = G_ONCE_INIT;

  g_once (&init_once, (GThreadFunc) gum_kernel_do_init, NULL);

  return (mach_port_t) GPOINTER_TO_SIZE (init_once.retval);
}

static mach_port_t
gum_kernel_do_init (void)
{
#if defined (HAVE_IOS) || defined (HAVE_TVOS)
  mach_port_t task;

  if (gum_darwin_query_hardened ())
    return MACH_PORT_NULL;

  task = MACH_PORT_NULL;
  task_for_pid (mach_task_self (), 0, &task);
  if (task == MACH_PORT_NULL)
  {
    /* Untested, but should work on iOS 9.1 with Pangu jailbreak */
    host_get_special_port (mach_host_self (), HOST_LOCAL_NODE, 4, &task);
  }

  if (task != MACH_PORT_NULL)
    _gum_register_destructor (gum_kernel_do_deinit);

  return task;
#else
  (void) gum_kernel_do_deinit;

  return MACH_PORT_NULL;
#endif
}

static void
gum_kernel_do_deinit (void)
{
  mach_port_deallocate (mach_task_self (), gum_kernel_get_task ());
}
