/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummoduleregistry-elf.h"

#include "gum-init.h"
#include "gumlinux-priv.h"
#include "gummodule-elf.h"
#include "valgrind.h"
#include "gum/gumandroid.h"
#include "gum/gumlinux.h"

#include <sys/stat.h>
#include <sys/sysmacros.h>
#ifdef HAVE_LINK_H
# include <link.h>
#endif

#define GUM_PAGE_START(value, page_size) \
    (GUM_ADDRESS (value) & ~GUM_ADDRESS (page_size - 1))

typedef struct _GumEnumerateModulesContext GumEnumerateModulesContext;

typedef gint (* GumFoundDlPhdrFunc) (struct dl_phdr_info * info,
    gsize size, gpointer data);
typedef void (* GumDlIteratePhdrImpl) (GumFoundDlPhdrFunc func, gpointer data);

typedef struct _GumProgramRanges GumProgramRanges;
typedef ElfW(auxv_t) * (* GumReadAuxvFunc) (void);

typedef struct _GumFileId GumFileId;

#ifdef HAVE_MUSL
typedef struct _GumRtldNotifierScan GumRtldNotifierScan;
#endif

struct _GumEnumerateModulesContext
{
  GumFoundModuleFunc func;
  gpointer user_data;

  GHashTable * named_ranges;
};

struct _GumProgramRanges
{
  GumMemoryRange program;
  GumMemoryRange interpreter;
  GumMemoryRange vdso;
};

struct _GumFileId
{
  dev_t device;
  ino_t inode;
};

#ifdef HAVE_MUSL
struct _GumRtldNotifierScan
{
  GumAddress base;
  GumAddress stub_offset;
  gconstpointer file_data;
  gsize file_size;
  GumFoundRtldNotifierFunc func;
  gpointer user_data;
  gboolean found;
};
#endif

static void gum_enumerate_modules_using_libc (GumDlIteratePhdrImpl iterate_phdr,
    GumFoundModuleFunc func, gpointer user_data);
static gint gum_emit_module_from_phdr (struct dl_phdr_info * info, gsize size,
    gpointer user_data);
static void gum_enumerate_modules_using_r_debug (const GumProgramModules * pm,
    GumFoundModuleFunc func, gpointer user_data);
static gpointer gum_link_map_as_module_handle (GumNativeModule * module,
    gpointer user_data);
static void gum_enumerate_modules_using_proc_maps (GumFoundModuleFunc func,
    gpointer user_data);
static gpointer gum_create_module_handle (GumNativeModule * module,
    gpointer user_data);
static gboolean gum_find_r_debug (GumModule * module, gpointer user_data);
static gboolean gum_find_debug_entry (const GumElfDynamicEntryDetails * details,
    gpointer user_data);

static void gum_deinit_program_modules (void);
static gboolean gum_query_program_ranges (GumReadAuxvFunc read_auxv,
    GumProgramRanges * ranges);
static ElfW(auxv_t) * gum_read_auxv_from_proc (void);
static ElfW(auxv_t) * gum_read_auxv_from_stack (void);
static gboolean gum_query_main_thread_stack_range (GumMemoryRange * range);
static gboolean gum_compute_elf_range_from_ehdr (const ElfW(Ehdr) * ehdr,
    GumMemoryRange * range);
static gboolean gum_phdrs_mapped_at_offset (const ElfW(Phdr) * phdrs,
    ElfW(Half) phdr_size, ElfW(Half) phdr_count, ElfW(Off) offset,
    GumAddress * lowest);
static void gum_compute_elf_range_from_phdrs (const ElfW(Phdr) * phdrs,
    ElfW(Half) phdr_size, ElfW(Half) phdr_count, GumAddress bias,
    GumMemoryRange * range);
static gboolean gum_detect_interpreter_exec_wrapper (
    const gchar * main_image_path, gchar ** payload_path);
static gboolean gum_read_argv_from_cmdline (GPtrArray ** argv);

static gboolean gum_paths_refer_to_same_file (const gchar * a, const gchar * b);
static gboolean gum_get_file_id (const gchar * path, GumFileId * id);
static gchar * gum_find_mapped_path_by_start (GumAddress wanted_start);
static gboolean gum_find_range_for_file_id_offset0 (GumFileId * wanted,
    GumMemoryRange * range);

static GumAddress gum_query_rtld_base (void);
#ifdef HAVE_MUSL
static gboolean gum_emit_rtld_notifier_call_sites (gpointer stub,
    GumModule * linker, GumFoundRtldNotifierFunc func, gpointer user_data);
static gboolean gum_emit_rtld_notifier_call_sites_in_segment (
    const GumElfSegmentDetails * segment, gpointer user_data);
#endif

static struct r_debug * gum_r_debug;
static GumProgramModules gum_program_modules;
static gboolean gum_syncing_modules_from_rtld;

void
_gum_module_registry_enumerate_loaded_modules (GumFoundModuleFunc func,
                                               gpointer user_data)
{
  const GumProgramModules * pm;
  static gsize iterate_phdr_value = 0;
  GumDlIteratePhdrImpl iterate_phdr;

  pm = _gum_query_program_modules ();

  if (pm->rtld == GUM_PROGRAM_RTLD_NONE)
  {
    if (!func (pm->program, user_data))
      return;

    if (pm->vdso != NULL)
      func (pm->vdso, user_data);

    return;
  }

#ifdef HAVE_ANDROID
  if (gum_android_get_linker_flavor () == GUM_ANDROID_LINKER_NATIVE)
  {
    gum_android_enumerate_modules (func, user_data);
    return;
  }
#endif

  if (gum_syncing_modules_from_rtld && gum_r_debug != NULL)
  {
    gum_enumerate_modules_using_r_debug (pm, func, user_data);
    return;
  }

  if (g_once_init_enter (&iterate_phdr_value))
  {
    gpointer libc, impl;

    libc = dlopen (_gum_process_get_libc_info ()->dli_fname,
        RTLD_LAZY | RTLD_GLOBAL);
    g_assert (libc != NULL);

    impl = dlsym (libc, "dl_iterate_phdr");

    dlclose (libc);

    g_once_init_leave (&iterate_phdr_value, GPOINTER_TO_SIZE (impl) + 1);
  }

  iterate_phdr = GSIZE_TO_POINTER (iterate_phdr_value - 1);
  if (iterate_phdr != NULL)
    gum_enumerate_modules_using_libc (iterate_phdr, func, user_data);
  else
    gum_enumerate_modules_using_proc_maps (func, user_data);
}

static void
gum_enumerate_modules_using_libc (GumDlIteratePhdrImpl iterate_phdr,
                                  GumFoundModuleFunc func,
                                  gpointer user_data)
{
  GumEnumerateModulesContext ctx;

  ctx.func = func;
  ctx.user_data = user_data;

  ctx.named_ranges = gum_linux_collect_named_ranges ();

  iterate_phdr (gum_emit_module_from_phdr, &ctx);

  g_hash_table_unref (ctx.named_ranges);
}

static gint
gum_emit_module_from_phdr (struct dl_phdr_info * info,
                           gsize size,
                           gpointer user_data)
{
  GumEnumerateModulesContext * ctx = user_data;
  GumMemoryRange range;
  GumLinuxNamedRange * named_range;
  const gchar * path;
  GumNativeModule * module;
  gboolean carry_on;

  gum_compute_elf_range_from_phdrs (info->dlpi_phdr, sizeof (ElfW(Phdr)),
      info->dlpi_phnum, info->dlpi_addr, &range);

  named_range = g_hash_table_lookup (ctx->named_ranges,
      GSIZE_TO_POINTER (range.base_address));

  path = (named_range != NULL) ? named_range->name : info->dlpi_name;

  module = _gum_native_module_make (path, &range, gum_create_module_handle,
      NULL, NULL, (GDestroyNotify) dlclose);

  carry_on = ctx->func (GUM_MODULE (module), ctx->user_data);

  g_object_unref (module);

  return carry_on ? 0 : 1;
}

static void
gum_enumerate_modules_using_r_debug (const GumProgramModules * pm,
                                     GumFoundModuleFunc func,
                                     gpointer user_data)
{
  GHashTable * named_ranges = NULL;
  const struct link_map * lm;
  gboolean carry_on = TRUE;

  for (lm = gum_r_debug->r_map; lm != NULL && carry_on; lm = lm->l_next)
  {
    GumMemoryRange range;
    GumNativeModule * module;

    if (lm->l_name[0] == '\0')
    {
      carry_on = func (pm->program, user_data);
      continue;
    }

    if (!gum_compute_elf_range_from_ehdr ((const ElfW(Ehdr) *) lm->l_addr,
        &range))
    {
      GumLinuxNamedRange * named_range;

      if (named_ranges == NULL)
        named_ranges = gum_linux_collect_named_ranges ();

      named_range = g_hash_table_lookup (named_ranges,
          GSIZE_TO_POINTER (lm->l_addr));
      range.base_address = GUM_ADDRESS (named_range->base);
      range.size = named_range->size;
    }

    module = _gum_native_module_make (lm->l_name, &range,
        gum_link_map_as_module_handle, (gpointer) lm, NULL, NULL);

    carry_on = func (GUM_MODULE (module), user_data);

    g_object_unref (module);
  }

  if (carry_on && pm->vdso != NULL)
    func (pm->vdso, user_data);

  if (named_ranges != NULL)
    g_hash_table_unref (named_ranges);
}

static gpointer
gum_link_map_as_module_handle (GumNativeModule * module,
                               gpointer user_data)
{
  return user_data;
}

static void
gum_enumerate_modules_using_proc_maps (GumFoundModuleFunc func,
                                       gpointer user_data)
{
  GumProcMapsIter iter;
  gchar * path, * next_path;
  const gchar * line;
  gboolean carry_on = TRUE;
  gboolean got_line = FALSE;

  gum_proc_maps_iter_init_for_self (&iter);

  path = g_malloc (PATH_MAX);
  next_path = g_malloc (PATH_MAX);

  do
  {
    const guint8 elf_magic[] = { 0x7f, 'E', 'L', 'F' };
    GumMemoryRange range;
    GumAddress end;
    gchar perms[5] = { 0, };
    gint n;
    gboolean is_vdso, readable, shared;
    GumNativeModule * module;

    if (!got_line)
    {
      if (!gum_proc_maps_iter_next (&iter, &line))
        break;
    }
    else
    {
      got_line = FALSE;
    }

    n = sscanf (line,
        "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x "
        "%4c "
        "%*x %*s %*d "
        "%[^\n]",
        &range.base_address, &end,
        perms,
        path);
    if (n == 3)
      continue;
    g_assert (n == 4);

    is_vdso = _gum_try_translate_vdso_name (path);

    readable = perms[0] == 'r';
    shared = perms[3] == 's';
    if (!readable || shared)
      continue;
    else if ((path[0] != '/' && !is_vdso) || g_str_has_prefix (path, "/dev/"))
      continue;
    else if (RUNNING_ON_VALGRIND && strstr (path, "/valgrind/") != NULL)
      continue;
    else if (memcmp (GSIZE_TO_POINTER (range.base_address), elf_magic,
        sizeof (elf_magic)) != 0)
      continue;

    range.size = end - range.base_address;

    while (gum_proc_maps_iter_next (&iter, &line))
    {
      n = sscanf (line,
          "%*x-%" G_GINT64_MODIFIER "x %*c%*c%*c%*c %*x %*s %*d %[^\n]",
          &end,
          next_path);
      if (n == 1)
      {
        continue;
      }
      else if (n == 2 && next_path[0] == '[')
      {
        if (!_gum_try_translate_vdso_name (next_path))
          continue;
      }

      if (n == 2 && strcmp (next_path, path) == 0)
      {
        range.size = end - range.base_address;
      }
      else
      {
        got_line = TRUE;
        break;
      }
    }

    module = _gum_native_module_make (path, &range, gum_create_module_handle,
        NULL, NULL, (GDestroyNotify) dlclose);

    carry_on = func (GUM_MODULE (module), user_data);

    g_object_unref (module);
  }
  while (carry_on);

  g_free (path);
  g_free (next_path);

  gum_proc_maps_iter_destroy (&iter);
}

static gpointer
gum_create_module_handle (GumNativeModule * module,
                          gpointer user_data)
{
#if defined (HAVE_MUSL)
  struct link_map * cur;

  for (cur = dlopen (NULL, 0); cur != NULL; cur = cur->l_next)
  {
    if (gum_linux_module_path_matches (cur->l_name, module->path))
      return cur;
  }

  for (cur = dlopen (NULL, 0); cur != NULL; cur = cur->l_next)
  {
    gchar * target, * parent_dir, * canonical_path;
    gboolean is_match;

    target = g_file_read_link (cur->l_name, NULL);
    if (target == NULL)
      continue;
    parent_dir = g_path_get_dirname (cur->l_name);
    canonical_path = g_canonicalize_filename (target, parent_dir);

    is_match = gum_linux_module_path_matches (canonical_path, module->path);

    g_free (canonical_path);
    g_free (parent_dir);
    g_free (target);

    if (is_match)
      return cur;
  }

  return NULL;
#else
  return dlopen (module->path, RTLD_LAZY | RTLD_NOLOAD);
#endif
}

void
_gum_module_registry_enumerate_rtld_notifiers (GumFoundRtldNotifierFunc func,
                                               gpointer user_data)
{
  struct r_debug * dbg = NULL;
  const guint * offsets;
  guint n_offsets;
  GumAddress linker_base;
  GumRtldNotifierDetails notifier;

  _gum_module_registry_enumerate_loaded_modules (gum_find_r_debug, &dbg);
  if (dbg == NULL)
    return;

  gum_r_debug = dbg;

  offsets = _gum_module_registry_get_rtld_notifier_offsets (&n_offsets);
  linker_base = (n_offsets != 0) ? gum_query_rtld_base () : 0;
  if (linker_base != 0)
  {
    guint i;

    notifier.point_cut = GUM_POINT_ENTER;
    for (i = 0; i != n_offsets; i++)
    {
      notifier.location = GSIZE_TO_POINTER (linker_base + offsets[i]);
      func (&notifier, user_data);
    }

    return;
  }

#ifdef HAVE_MUSL
  {
    const GumProgramModules * pm = _gum_query_program_modules ();

    gum_emit_rtld_notifier_call_sites (GSIZE_TO_POINTER (dbg->r_brk),
        pm->interpreter, func, user_data);
  }
#else
  notifier.location = GSIZE_TO_POINTER (dbg->r_brk);
  notifier.point_cut = GUM_POINT_ENTER;
  func (&notifier, user_data);
#endif
}

static GumAddress
gum_query_rtld_base (void)
{
  const GumProgramModules * pm = _gum_query_program_modules ();

  if (pm->rtld == GUM_PROGRAM_RTLD_NONE || pm->interpreter == NULL)
    return 0;

  return gum_module_get_range (pm->interpreter)->base_address;
}

#ifdef HAVE_MUSL

static gboolean
gum_emit_rtld_notifier_call_sites (gpointer stub,
                                   GumModule * linker,
                                   GumFoundRtldNotifierFunc func,
                                   gpointer user_data)
{
  GumRtldNotifierScan scan;
  GumElfModule * image;

  image = gum_elf_module_new_from_file (gum_module_get_path (linker), NULL);
  if (image == NULL)
    return FALSE;

  scan.base = gum_module_get_range (linker)->base_address;
  scan.stub_offset = GUM_ADDRESS (stub) - scan.base;
  scan.file_data = gum_elf_module_get_file_data (image, &scan.file_size);
  scan.func = func;
  scan.user_data = user_data;
  scan.found = FALSE;

  gum_elf_module_enumerate_segments (image,
      gum_emit_rtld_notifier_call_sites_in_segment, &scan);

  g_object_unref (image);

  return scan.found;
}

static gboolean
gum_emit_rtld_notifier_call_sites_in_segment (
    const GumElfSegmentDetails * segment,
    gpointer user_data)
{
  GumRtldNotifierScan * scan = user_data;
  const guint8 * code, * end, * cursor;

  if ((segment->protection & GUM_PAGE_EXECUTE) == 0)
    return TRUE;

  code = (const guint8 *) scan->file_data + segment->file_offset;
  end = code + segment->file_size;

# if defined (HAVE_I386)
  {
    const guint8 call_near_relative = 0xe8;

    for (cursor = code; end - cursor >= 5; cursor++)
    {
      GumAddress site_offset, target_offset;
      GumRtldNotifierDetails notifier;

      if (*cursor != call_near_relative)
        continue;

      site_offset = segment->vm_address + (cursor - code);
      target_offset = site_offset + 5 +
          GINT32_FROM_LE (*((const gint32 *) (cursor + 1)));
      if (target_offset != scan->stub_offset)
        continue;

      notifier.location = GSIZE_TO_POINTER (scan->base + site_offset);
      notifier.point_cut = GUM_POINT_ENTER;
      scan->func (&notifier, scan->user_data);

      scan->found = TRUE;
    }
  }
# elif defined (HAVE_ARM64)
  {
    const guint32 branch_with_link = 0x25;

    for (cursor = code; end - cursor >= 4; cursor += 4)
    {
      guint32 insn;
      gint32 imm26;
      GumAddress site_offset, target_offset;
      GumRtldNotifierDetails notifier;

      insn = GUINT32_FROM_LE (*((const guint32 *) cursor));
      if ((insn >> 26) != branch_with_link)
        continue;

      imm26 = (gint32) (insn << 6) >> 6;
      site_offset = segment->vm_address + (cursor - code);
      target_offset = site_offset + (gssize) imm26 * 4;
      if (target_offset != scan->stub_offset)
        continue;

      notifier.location = GSIZE_TO_POINTER (scan->base + site_offset);
      notifier.point_cut = GUM_POINT_ENTER;
      scan->func (&notifier, scan->user_data);

      scan->found = TRUE;
    }
  }
# endif

  return TRUE;
}

#endif

static gboolean
gum_find_r_debug (GumModule * module,
                  gpointer user_data)
{
  struct r_debug ** dbg = user_data;
  GumElfModule * elf;

  elf = _gum_native_module_get_elf_module (GUM_NATIVE_MODULE (module));
  if (elf == NULL)
    return TRUE;

  gum_elf_module_enumerate_dynamic_entries (elf, gum_find_debug_entry, dbg);

  return *dbg == NULL;
}

static gboolean
gum_find_debug_entry (const GumElfDynamicEntryDetails * details,
                      gpointer user_data)
{
  struct r_debug ** dbg = user_data;

  if (details->tag == GUM_ELF_DYNAMIC_DEBUG)
  {
    *dbg = GSIZE_TO_POINTER (details->val);
    return FALSE;
  }

  return TRUE;
}

void
_gum_module_registry_handle_rtld_notification (GumSynchronizeModulesFunc sync,
                                               GumInvocationContext * ic)
{
  if (gum_r_debug->r_state == RT_CONSISTENT)
  {
    gum_syncing_modules_from_rtld = TRUE;
    sync ();
    gum_syncing_modules_from_rtld = FALSE;
  }
}

const GumProgramModules *
_gum_query_program_modules (void)
{
  static gsize modules_value = 0;

  if (g_once_init_enter (&modules_value))
  {
    static GumProgramRanges ranges;
    gboolean got_kern, got_user;
    GumProgramRanges kern, user;
    GumProcMapsIter iter;
    gchar * path;
    const gchar * line;

    got_kern = gum_query_program_ranges (gum_read_auxv_from_proc, &kern);
    got_user = gum_query_program_ranges (gum_read_auxv_from_stack, &user);
    if (got_kern && got_user &&
        user.program.base_address != kern.program.base_address)
    {
      ranges = user;
      ranges.interpreter = kern.program;
    }
    else if (got_kern)
      ranges = kern;
    else
      ranges = user;

    if (ranges.interpreter.base_address == 0)
    {
      gchar * main_path, * payload_path;

      main_path = gum_find_mapped_path_by_start (ranges.program.base_address);

      if (gum_detect_interpreter_exec_wrapper (main_path, &payload_path))
      {
        GumFileId interp_id, prog_id;
        GumMemoryRange interp_range, prog_range;

        if (gum_get_file_id (main_path, &interp_id) &&
            gum_get_file_id (payload_path, &prog_id) &&
            gum_find_range_for_file_id_offset0 (&interp_id, &interp_range) &&
            gum_find_range_for_file_id_offset0 (&prog_id, &prog_range))
        {
          ranges.interpreter = interp_range;
          ranges.program = prog_range;
        }

        g_free (payload_path);
      }

      g_free (main_path);
    }

    gum_program_modules.rtld = (ranges.interpreter.base_address == 0)
        ? GUM_PROGRAM_RTLD_NONE
        : GUM_PROGRAM_RTLD_SHARED;

    gum_proc_maps_iter_init_for_self (&iter);
    path = g_malloc (PATH_MAX);

    while (gum_proc_maps_iter_next (&iter, &line))
    {
      GumAddress start;
      GumModule ** m;
      const GumMemoryRange * r;

      sscanf (line, "%" G_GINT64_MODIFIER "x-", &start);

      if (start == ranges.program.base_address)
      {
        m = &gum_program_modules.program;
        r = &ranges.program;
      }
      else if (start == ranges.interpreter.base_address)
      {
        m = &gum_program_modules.interpreter;
        r = &ranges.interpreter;
      }
      else
        continue;

      sscanf (line, "%*x-%*x %*c%*c%*c%*c %*x %*s %*d %[^\n]", path);

      *m = GUM_MODULE (_gum_native_module_make_handleless (path, r));
    }

    g_free (path);
    gum_proc_maps_iter_destroy (&iter);

    if (ranges.vdso.base_address != 0)
    {
      /* FIXME: Parse soname instead of hardcoding: */
      gum_program_modules.vdso = GUM_MODULE (
          _gum_native_module_make_handleless ("linux-vdso.so.1", &ranges.vdso));
    }

    _gum_register_destructor (gum_deinit_program_modules);

    g_once_init_leave (&modules_value, GPOINTER_TO_SIZE (&gum_program_modules));
  }

  return GSIZE_TO_POINTER (modules_value);
}

static void
gum_deinit_program_modules (void)
{
  GumProgramModules * m = &gum_program_modules;

  g_object_unref (m->program);
  if (m->interpreter != NULL)
    g_object_unref (m->interpreter);
  if (m->vdso != NULL)
    g_object_unref (m->vdso);
}

static gboolean
gum_query_program_ranges (GumReadAuxvFunc read_auxv,
                          GumProgramRanges * ranges)
{
  gboolean success = FALSE;
  ElfW(auxv_t) * auxv;
  const ElfW(Phdr) * phdrs;
  ElfW(Half) phdr_size, phdr_count;
  const ElfW(Ehdr) * interpreter, * vdso;
  ElfW(auxv_t) * entry;

  bzero (ranges, sizeof (GumProgramRanges));

  auxv = read_auxv ();
  if (auxv == NULL)
    goto beach;

  phdrs = NULL;
  phdr_size = 0;
  phdr_count = 0;
  interpreter = NULL;
  vdso = NULL;
  for (entry = auxv; entry->a_type != AT_NULL; entry++)
  {
    switch (entry->a_type)
    {
      case AT_PHDR:
        phdrs = (ElfW(Phdr) *) entry->a_un.a_val;
        break;
      case AT_PHENT:
        phdr_size = entry->a_un.a_val;
        break;
      case AT_PHNUM:
        phdr_count = entry->a_un.a_val;
        break;
      case AT_BASE:
        interpreter = (const ElfW(Ehdr) *) entry->a_un.a_val;
        break;
      case AT_SYSINFO_EHDR:
        vdso = (const ElfW(Ehdr) *) entry->a_un.a_val;
        break;
    }
  }
  if (phdrs == NULL || phdr_size == 0 || phdr_count == 0)
    goto beach;

  gum_compute_elf_range_from_phdrs (phdrs, phdr_size, phdr_count, 0,
      &ranges->program);
  gum_compute_elf_range_from_ehdr (interpreter, &ranges->interpreter);
  gum_compute_elf_range_from_ehdr (vdso, &ranges->vdso);

  success = TRUE;

beach:
  g_free (auxv);

  return success;
}

static ElfW(auxv_t) *
gum_read_auxv_from_proc (void)
{
  ElfW(auxv_t) * auxv = NULL;

  _gum_acquire_dumpability ();

  g_file_get_contents ("/proc/self/auxv", (gchar **) &auxv, NULL, NULL);

  _gum_release_dumpability ();

  return auxv;
}

static ElfW(auxv_t) *
gum_read_auxv_from_stack (void)
{
  GumMemoryRange stack;
  gpointer stack_start, stack_end;
  ElfW(auxv_t) needle;
  const ElfW(auxv_t) * match, * last_match;
  gsize offset;
  const ElfW(auxv_t) * cursor, * auxv_start, * auxv_end;
  gsize page_size;

  if (!gum_query_main_thread_stack_range (&stack))
    return NULL;
  stack_start = GSIZE_TO_POINTER (stack.base_address);
  stack_end = stack_start + stack.size;

  needle.a_type = AT_PHENT;
  needle.a_un.a_val = sizeof (ElfW(Phdr));

  match = NULL;
  last_match = NULL;
  offset = 0;
  while (offset != stack.size)
  {
    match = memmem (GSIZE_TO_POINTER (stack.base_address) + offset,
        stack.size - offset, &needle, sizeof (needle));
    if (match == NULL)
      break;

    last_match = match;
    offset = (GUM_ADDRESS (match) - stack.base_address) + 1;
  }
  if (last_match == NULL)
    return NULL;

  auxv_start = NULL;
  page_size = gum_query_page_size ();
  for (cursor = last_match - 1;
      (gpointer) cursor >= stack_start;
      cursor--)
  {
    gboolean probably_an_invalid_type = cursor->a_type >= page_size;
    if (probably_an_invalid_type)
    {
      auxv_start = cursor + 1;
      break;
    }
  }

  auxv_end = NULL;
  for (cursor = last_match + 1;
      (gpointer) cursor <= stack_end - sizeof (ElfW(auxv_t));
      cursor++)
  {
    if (cursor->a_type == AT_NULL)
    {
      auxv_end = cursor + 1;
      break;
    }
  }
  if (auxv_end == NULL)
    return NULL;

  return g_memdup (auxv_start, (guint8 *) auxv_end - (guint8 *) auxv_start);
}

static gboolean
gum_query_main_thread_stack_range (GumMemoryRange * range)
{
  GumProcMapsIter iter;
  GumAddress stack_bottom, stack_top;
  const gchar * line;

  gum_proc_maps_iter_init_for_self (&iter);

  stack_bottom = 0;
  stack_top = 0;

  while (gum_proc_maps_iter_next (&iter, &line))
  {
    if (g_str_has_suffix (line, " [stack]"))
    {
      sscanf (line,
          "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x ",
          &stack_bottom,
          &stack_top);
      break;
    }
  }

  range->base_address = stack_bottom;
  range->size = stack_top - stack_bottom;

  gum_proc_maps_iter_destroy (&iter);

  return range->size != 0;
}

static gboolean
gum_compute_elf_range_from_ehdr (const ElfW(Ehdr) * ehdr,
                                 GumMemoryRange * range)
{
  const ElfW(Phdr) * phdrs;
  gsize phdrs_size;
  GumAddress lowest;

  range->base_address = 0;
  range->size = 0;

  if (ehdr == NULL)
    return TRUE;

  phdrs = (gconstpointer) ehdr + ehdr->e_phoff;
  phdrs_size = (gsize) ehdr->e_phnum * ehdr->e_phentsize;

  if (ehdr->e_phoff + phdrs_size > gum_query_page_size () &&
      !gum_memory_is_readable (phdrs, phdrs_size))
    return FALSE;

  if (!gum_phdrs_mapped_at_offset (phdrs, ehdr->e_phentsize, ehdr->e_phnum,
      ehdr->e_phoff, &lowest))
    return FALSE;

  gum_compute_elf_range_from_phdrs (phdrs, ehdr->e_phentsize, ehdr->e_phnum,
      GUM_ADDRESS (ehdr) - lowest, range);
  return TRUE;
}

static gboolean
gum_phdrs_mapped_at_offset (const ElfW(Phdr) * phdrs,
                            ElfW(Half) phdr_size,
                            ElfW(Half) phdr_count,
                            ElfW(Off) offset,
                            GumAddress * lowest)
{
  const ElfW(Phdr) * holder;
  GumAddress lowest_vaddr;
  gsize page_size;
  ElfW(Half) i;
  const ElfW(Phdr) * phdr;

  holder = NULL;
  lowest_vaddr = ~0;
  page_size = gum_query_page_size ();

  for (i = 0, phdr = phdrs;
      i != phdr_count;
      i++, phdr = (gconstpointer) phdr + phdr_size)
  {
    if (phdr->p_type != PT_LOAD)
      continue;

    lowest_vaddr = MIN (GUM_PAGE_START (phdr->p_vaddr, page_size),
        lowest_vaddr);

    if (offset >= phdr->p_offset && offset < phdr->p_offset + phdr->p_filesz)
      holder = phdr;
  }

  if (holder == NULL)
    return FALSE;

  *lowest = lowest_vaddr;

  return holder->p_vaddr - holder->p_offset == lowest_vaddr;
}

static void
gum_compute_elf_range_from_phdrs (const ElfW(Phdr) * phdrs,
                                  ElfW(Half) phdr_size,
                                  ElfW(Half) phdr_count,
                                  GumAddress bias,
                                  GumMemoryRange * range)
{
  gboolean bias_known;
  GumAddress lowest, highest;
  gsize page_size;
  ElfW(Half) i;
  const ElfW(Phdr) * phdr;

  bias_known = bias != 0;
  lowest = ~0;
  highest = 0;
  page_size = gum_query_page_size ();

  for (i = 0, phdr = phdrs;
      i != phdr_count;
      i++, phdr = (gconstpointer) phdr + phdr_size)
  {
    if (phdr->p_type == PT_PHDR && !bias_known)
    {
      bias = GPOINTER_TO_SIZE (phdrs) - phdr->p_vaddr;
      bias_known = TRUE;
    }

    if (phdr->p_type == PT_LOAD)
    {
      lowest = MIN (GUM_PAGE_START (phdr->p_vaddr, page_size), lowest);
      highest = MAX (phdr->p_vaddr + phdr->p_memsz, highest);
    }
  }

  if (!bias_known)
    bias = GUM_PAGE_START (phdrs, page_size) - lowest;

  range->base_address = bias + lowest;
  range->size = highest - lowest;
}

static gboolean
gum_detect_interpreter_exec_wrapper (const gchar * main_image_path,
                                     gchar ** payload_path)
{
  gboolean found = FALSE;
  GPtrArray * argv;
  guint i;

  *payload_path = NULL;

  if (!gum_read_argv_from_cmdline (&argv))
    return FALSE;

  for (i = 1; i != argv->len && !found; i++)
  {
    const gchar * candidate;
    GumElfModule * m;
    const gchar * interp;

    candidate = g_ptr_array_index (argv, i);
    if (candidate[0] == '\0' || candidate[0] == '-')
      continue;

    m = gum_elf_module_new_from_file (candidate, NULL);
    if (m == NULL)
      continue;

    interp = gum_elf_module_get_interpreter (m);

    if (interp != NULL &&
        gum_paths_refer_to_same_file (interp, main_image_path))
    {
      found = TRUE;
      *payload_path = g_strdup (candidate);
    }

    g_object_unref (m);
  }

  g_ptr_array_unref (argv);

  return found;
}

static gboolean
gum_read_argv_from_cmdline (GPtrArray ** argv)
{
  gchar * contents;
  gsize length;
  GPtrArray * arr;
  gsize cursor;

  if (!g_file_get_contents ("/proc/self/cmdline", &contents, &length, NULL))
    return FALSE;

  arr = g_ptr_array_new_with_free_func (g_free);

  cursor = 0;
  while (cursor != length)
  {
    gsize n = strlen (contents + cursor);
    if (n == 0)
      break;

    g_ptr_array_add (arr, g_strdup (contents + cursor));
    cursor += n + 1;
  }

  g_free (contents);

  if (arr->len == 0)
  {
    g_ptr_array_unref (arr);
    return FALSE;
  }

  *argv = arr;
  return TRUE;
}

static gboolean
gum_paths_refer_to_same_file (const gchar * a,
                              const gchar * b)
{
  GumFileId id_a, id_b;

  if (!gum_get_file_id (a, &id_a) || !gum_get_file_id (b, &id_b))
    return FALSE;

  return id_a.device == id_b.device && id_a.inode == id_b.inode;
}

static gboolean
gum_get_file_id (const gchar * path,
                 GumFileId * id)
{
  struct stat st;

  if (stat (path, &st) != 0)
    return FALSE;

  id->device = st.st_dev;
  id->inode = st.st_ino;
  return TRUE;
}

static gchar *
gum_find_mapped_path_by_start (GumAddress wanted_start)
{
  gchar * result = NULL;
  GumProcMapsIter iter;
  const gchar * line;

  gum_proc_maps_iter_init_for_self (&iter);

  while (gum_proc_maps_iter_next (&iter, &line))
  {
    GumAddress start;
    gchar path[PATH_MAX];

    if (sscanf (line, "%" G_GINT64_MODIFIER "x-", &start) != 1)
      continue;

    if (start != wanted_start)
      continue;

    path[0] = '\0';
    sscanf (line, "%*x-%*x %*c%*c%*c%*c %*x %*s %*d %[^\n]", path);

    if (path[0] != '\0')
      result = g_strdup (path);
    break;
  }

  gum_proc_maps_iter_destroy (&iter);

  return result;
}

static gboolean
gum_find_range_for_file_id_offset0 (GumFileId * wanted,
                                    GumMemoryRange * range)
{
  gboolean success = FALSE;
  GumProcMapsIter iter;
  const gchar * line;

  gum_proc_maps_iter_init_for_self (&iter);

  while (gum_proc_maps_iter_next (&iter, &line))
  {
    int n;
    GumAddress start, end;
    guint64 offset;
    guint dev_major, dev_minor;
    guint64 inode;
    dev_t dev;

    n = sscanf (line,
        "%" G_GINT64_MODIFIER "x-%" G_GINT64_MODIFIER "x %*4s %"
        G_GINT64_MODIFIER "x %x:%x %" G_GINT64_MODIFIER "u",
        &start, &end, &offset, &dev_major, &dev_minor, &inode);
    if (n != 6)
      continue;

    if (offset != 0)
      continue;

    dev = makedev (dev_major, dev_minor);
    if (dev != wanted->device)
      continue;

    if ((ino_t) inode != wanted->inode)
      continue;

    range->base_address = start;
    range->size = end - start;

    success = TRUE;
    break;
  }

  gum_proc_maps_iter_destroy (&iter);

  return success;
}
