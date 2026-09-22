/*
 * Copyright (C) 2025-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummoduleregistry-priv.h"

#include "gum/gumdarwin.h"
#include "gumdarwin-priv.h"
#include "guminterceptor.h"
#include "guminvocationlistener.h"
#include "gummodule-darwin.h"

#ifdef HAVE_I386
# include <gum/arch-x86/gumx86writer.h>
#else
# include <gum/arch-arm64/gumarm64writer.h>
#endif

#include <dlfcn.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>

typedef struct _GumDyldNotifierContext GumDyldNotifierContext;
typedef void (* DyldImageNotifier) (enum dyld_image_mode mode,
    guint32 info_count, const struct dyld_image_info info[]);
typedef struct _GumResidentNotifier GumResidentNotifier;
typedef void (* GumAddImageNotifier) (const struct mach_header * mh,
    intptr_t vmaddr_slide);
typedef struct _GumResidentStubLayout GumResidentStubLayout;

struct _GumDyldNotifierContext
{
  gpointer * slot;
  DyldImageNotifier original;
};

struct _GumResidentNotifier
{
  gpointer magic;
  GumAddImageNotifier handler;
  GumAddImageNotifier noop;
};

struct _GumResidentStubLayout
{
  gpointer pc;
  gpointer handler_slot;
  gsize entry_offset;
  gsize noop_offset;
};

#define GUM_RESIDENT_NOTIFIER_MAGIC G_GUINT64_CONSTANT (0x4d6f644e6f746679)

static void gum_module_registry_on_image_added (const struct mach_header * mh,
    intptr_t vmaddr_slide);
static void gum_module_registry_on_image_removed (const struct mach_header * mh,
    intptr_t vmaddr_slide);
static void gum_lldb_image_notifier (enum dyld_image_mode mode,
    guint32 info_count, const struct dyld_image_info info[]);

static GumNativeModule * gum_make_image_module (const struct mach_header * mh,
    const gchar * name);
static void gum_add_image (const struct mach_header * mh, const gchar * name);
static void gum_remove_image (const struct mach_header * mh);
static void gum_module_registry_synchronize_modules (void);
static void gum_module_registry_on_rtld_notification (
    GumInvocationContext * ic, gpointer user_data);

static gboolean gum_try_drive_registry_via_dyld_internals (
    const GumDarwinAllImageInfos * infos);
static GumAddress gum_resolve_dyld_dec_dl_ref_count (GumAddress dyld_base);
static gboolean gum_collect_dyld_dec_dl_ref_count (
    const GumDarwinSymbolDetails * details, gpointer user_data);
static void gum_arm_resident_add_notifier (void);
static GumResidentNotifier * gum_find_resident_notifier (void);
static GumResidentNotifier * gum_create_resident_notifier (void);
static void gum_write_resident_stub (gpointer mem, gpointer user_data);
static void gum_resident_add_notifier_on_image_event (
    const struct mach_header * mh, intptr_t vmaddr_slide);

static void gum_module_registry_begin_tracking (gboolean on_leave);
static void gum_module_registry_snapshot_modules (void);
static void gum_module_registry_start_change_tracking (gboolean on_leave);
static void gum_module_registry_attach_notifier (gpointer location);

static gsize gum_detect_macho_size (const struct mach_header * mh);

static GumModuleRegistry * gum_registry;
static GumDarwinModuleResolver * gum_resolver;
static GumDyldNotifierContext * gum_dyld_notifier_context;
static GumResidentNotifier * gum_resident_notifier;
static GHashTable * gum_current_modules;
static GumInterceptor * gum_rtld_interceptor;
static GumInvocationListener * gum_rtld_handler;

void
_gum_module_registry_activate (GumModuleRegistry * self)
{
  GumDarwinAllImageInfos infos = { 0, };

  gum_registry = self;
  gum_resolver = gum_darwin_module_resolver_new_with_loader (mach_task_self (),
      (GumDarwinModuleResolverLoadFunc) _gum_module_registry_get_modules,
      gum_registry, NULL, NULL);

  if (!gum_darwin_query_all_image_infos (mach_task_self (), &infos, NULL))
    return;

  {
    const guint * offsets;
    guint n_offsets;

    offsets = _gum_module_registry_get_rtld_notifier_offsets (&n_offsets);
    if (n_offsets != 0)
    {
      GumAddress dyld_base = infos.dyld_image_load_address;
      guint i;

      gum_module_registry_begin_tracking (FALSE);

      gum_interceptor_begin_transaction (gum_rtld_interceptor);
      for (i = 0; i != n_offsets; i++)
        gum_module_registry_attach_notifier (
            GSIZE_TO_POINTER (dyld_base + offsets[i]));
      gum_interceptor_end_transaction (gum_rtld_interceptor);

      gum_add_image (GSIZE_TO_POINTER (dyld_base), NULL);

      gum_module_registry_synchronize_modules ();

      return;
    }
  }

  if (gum_process_get_teardown_requirement () == GUM_TEARDOWN_REQUIREMENT_FULL)
  {
    gpointer * slot;
    uint32_t count, i;

    if (gum_try_drive_registry_via_dyld_internals (&infos))
    {
      gum_add_image (GSIZE_TO_POINTER (infos.dyld_image_load_address), NULL);
      return;
    }

    g_assert (infos.dyld_all_image_infos_address != 0);

    if (infos.format == TASK_DYLD_ALL_IMAGE_INFO_64)
    {
      slot = gum_strip_code_pointer (
          GSIZE_TO_POINTER (infos.dyld_all_image_infos_address +
              offsetof (DyldAllImageInfos64, notification)));
    }
    else
    {
      slot = GSIZE_TO_POINTER (infos.dyld_all_image_infos_address +
          offsetof (DyldAllImageInfos32, notification));
    }

    gum_dyld_notifier_context = g_slice_new (GumDyldNotifierContext);

    gum_dyld_notifier_context->slot = slot;
    gum_dyld_notifier_context->original = *slot;

    *slot = gum_sign_code_pointer (
        gum_strip_code_pointer (gum_lldb_image_notifier));

    do
    {
      _gum_module_registry_reset (gum_registry);

      count = _dyld_image_count ();
      for (i = 0; i != count; i++)
        gum_add_image (_dyld_get_image_header (i), _dyld_get_image_name (i));
    }
    while (_dyld_image_count () != count);
  }
  else
  {
    _dyld_register_func_for_add_image (gum_module_registry_on_image_added);
    _dyld_register_func_for_remove_image (gum_module_registry_on_image_removed);
  }

  gum_add_image (GSIZE_TO_POINTER (infos.dyld_image_load_address), NULL);
}

void
_gum_module_registry_deactivate (GumModuleRegistry * self)
{
  if (gum_rtld_interceptor != NULL)
  {
    gum_interceptor_detach (gum_rtld_interceptor, gum_rtld_handler);

    g_object_unref (gum_rtld_handler);
    gum_rtld_handler = NULL;

    g_object_unref (gum_rtld_interceptor);
    gum_rtld_interceptor = NULL;

    g_hash_table_unref (gum_current_modules);
    gum_current_modules = NULL;
  }

  if (gum_dyld_notifier_context != NULL)
  {
    GumDyldNotifierContext * context = gum_dyld_notifier_context;
    gum_dyld_notifier_context = NULL;

    *context->slot = context->original;
    g_slice_free (GumDyldNotifierContext, context);
  }

  if (gum_resident_notifier != NULL)
  {
    g_atomic_pointer_set (&gum_resident_notifier->handler,
        gum_resident_notifier->noop);
    gum_resident_notifier = NULL;
  }

  g_clear_object (&gum_resolver);
}

static void
gum_module_registry_on_image_added (const struct mach_header * mh,
                                    intptr_t vmaddr_slide)
{
  gum_add_image (mh, NULL);
}

static void
gum_module_registry_on_image_removed (const struct mach_header * mh,
                                      intptr_t vmaddr_slide)
{
  gum_remove_image (mh);
}

static void
gum_lldb_image_notifier (enum dyld_image_mode mode,
                         guint32 info_count,
                         const struct dyld_image_info info[])
{
  uint32_t i;

  for (i = 0; i != info_count; i++)
  {
    if (mode == dyld_image_adding)
      gum_add_image (info[i].imageLoadAddress, info[i].imageFilePath);
    else
      gum_remove_image (info[i].imageLoadAddress);
  }

  return gum_dyld_notifier_context->original (mode, info_count, info);
}

static GumNativeModule *
gum_make_image_module (const struct mach_header * mh,
                       const gchar * name)
{
  Dl_info info;
  GumFileMapping file;
  struct proc_regionwithpathinfo region;
  GumMemoryRange range;
  const gchar * sysroot;

  if (name == NULL)
  {
    if (dladdr (mh, &info) != 0)
    {
      name = info.dli_fname;
    }
    else
    {
      G_GNUC_UNUSED gboolean resolved;

      resolved = _gum_darwin_fill_file_mapping (getpid (),
          GPOINTER_TO_SIZE (mh), &file, &region);
      g_assert (resolved);

      name = file.path;
    }
  }

  sysroot = gum_darwin_query_sysroot ();
  if (sysroot != NULL && g_str_has_prefix (name, sysroot))
    name += strlen (sysroot);

  range.base_address = GUM_ADDRESS (mh);
  range.size = gum_detect_macho_size (mh);

  return _gum_native_module_make (name, &range, gum_resolver);
}

static void
gum_add_image (const struct mach_header * mh,
               const gchar * name)
{
  GumNativeModule * mod = gum_make_image_module (mh, name);

  _gum_module_registry_register (gum_registry, GUM_MODULE (mod));

  g_object_unref (mod);
}

static void
gum_remove_image (const struct mach_header * mh)
{
  _gum_module_registry_unregister (gum_registry, GUM_ADDRESS (mh));
}

static void
gum_module_registry_synchronize_modules (void)
{
  GHashTable * modules;
  uint32_t count, i;
  GHashTableIter iter;
  gpointer base_address;
  GumModule * module;
  GQueue added = G_QUEUE_INIT;
  GQueue removed = G_QUEUE_INIT;

  modules = g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  count = _dyld_image_count ();
  for (i = 0; i != count; i++)
  {
    const struct mach_header * mh = _dyld_get_image_header (i);

    g_hash_table_insert (modules, GSIZE_TO_POINTER (GUM_ADDRESS (mh)),
        gum_make_image_module (mh, _dyld_get_image_name (i)));
  }

  gum_module_registry_lock (gum_registry);

  g_hash_table_iter_init (&iter, modules);
  while (g_hash_table_iter_next (&iter, &base_address, (gpointer *) &module))
  {
    if (!g_hash_table_contains (gum_current_modules, base_address))
      g_queue_push_tail (&added, g_object_ref (module));
  }

  g_hash_table_iter_init (&iter, gum_current_modules);
  while (g_hash_table_iter_next (&iter, &base_address, NULL))
  {
    if (!g_hash_table_contains (modules, base_address))
      g_queue_push_tail (&removed, base_address);
  }

  g_hash_table_unref (gum_current_modules);
  gum_current_modules = modules;

  gum_module_registry_unlock (gum_registry);

  while ((base_address = g_queue_pop_head (&removed)) != NULL)
    _gum_module_registry_unregister (gum_registry, GUM_ADDRESS (base_address));

  while ((module = g_queue_pop_head (&added)) != NULL)
  {
    _gum_module_registry_register (gum_registry, module);
    g_object_unref (module);
  }
}

static void
gum_module_registry_on_rtld_notification (GumInvocationContext * ic,
                                          gpointer user_data)
{
  gum_module_registry_synchronize_modules ();
}

static gboolean
gum_try_drive_registry_via_dyld_internals (
    const GumDarwinAllImageInfos * infos)
{
  GumAddress dec_dl_ref_count;

  dec_dl_ref_count =
      gum_resolve_dyld_dec_dl_ref_count (infos->dyld_image_load_address);
  if (dec_dl_ref_count == 0)
    return FALSE;

  gum_module_registry_begin_tracking (TRUE);

  gum_interceptor_begin_transaction (gum_rtld_interceptor);
  gum_module_registry_attach_notifier (GSIZE_TO_POINTER (dec_dl_ref_count));
  gum_interceptor_end_transaction (gum_rtld_interceptor);

  gum_arm_resident_add_notifier ();

  gum_module_registry_synchronize_modules ();

  return TRUE;
}

static GumAddress
gum_resolve_dyld_dec_dl_ref_count (GumAddress dyld_base)
{
  GumAddress result = 0;
  GumDarwinModule * dyld;

  dyld = gum_darwin_module_new_from_memory (NULL, mach_task_self (), dyld_base,
      GUM_DARWIN_MODULE_FLAGS_NONE, NULL);
  if (dyld == NULL)
    return 0;

  gum_darwin_module_enumerate_symbols (dyld, gum_collect_dyld_dec_dl_ref_count,
      &result);

  g_object_unref (dyld);

  return result;
}

static gboolean
gum_collect_dyld_dec_dl_ref_count (const GumDarwinSymbolDetails * details,
                                   gpointer user_data)
{
  GumAddress * result = user_data;
  const gchar * name = details->name;

  if (g_str_has_prefix (name, "__ZN") &&
      strstr (name, "RuntimeState") != NULL &&
      strstr (name, "decDlRefCount") != NULL &&
      strstr (name, "block_invoke") == NULL &&
      strstr (name, ".cold") == NULL)
  {
    *result = details->address;
    return FALSE;
  }

  return TRUE;
}

static void
gum_arm_resident_add_notifier (void)
{
  GumResidentNotifier * notifier;

  notifier = gum_find_resident_notifier ();
  if (notifier == NULL)
    notifier = gum_create_resident_notifier ();

  gum_resident_notifier = notifier;
}

static GumResidentNotifier *
gum_find_resident_notifier (void)
{
  gpointer handler;
  mach_port_t self;
  mach_vm_address_t address = 0;

  handler = gum_sign_code_pointer (gum_strip_code_pointer (
      GUM_FUNCPTR_TO_POINTER (gum_resident_add_notifier_on_image_event)));
  self = mach_task_self ();

  while (TRUE)
  {
    mach_vm_size_t size = 0;
    natural_t depth = 0;
    vm_region_submap_info_data_64_t info;
    mach_msg_type_number_t info_count = VM_REGION_SUBMAP_INFO_COUNT_64;
    GumResidentNotifier * candidate;

    if (mach_vm_region_recurse (self, &address, &size, &depth,
          (vm_region_recurse_info_t) &info, &info_count) != KERN_SUCCESS)
    {
      return NULL;
    }

    candidate = GSIZE_TO_POINTER (address);

    if (info.protection == (VM_PROT_READ | VM_PROT_WRITE) &&
        info.share_mode == SM_PRIVATE &&
        size >= sizeof (GumResidentNotifier) &&
        g_atomic_pointer_get (&candidate->magic) ==
            GSIZE_TO_POINTER (GUM_RESIDENT_NOTIFIER_MAGIC) &&
        g_atomic_pointer_compare_and_exchange (&candidate->handler,
            candidate->noop, handler))
    {
      return candidate;
    }

    address += size;
  }
}

static GumResidentNotifier *
gum_create_resident_notifier (void)
{
  GumResidentNotifier * notifier;
  gsize page_size;
  GumPageProtection code_prot;
  gpointer code;
  GumResidentStubLayout layout;

  page_size = gum_query_page_size ();

  notifier = gum_memory_allocate (NULL, page_size, page_size,
      GUM_PAGE_READ | GUM_PAGE_WRITE);

  code_prot = gum_memory_can_remap_writable () ? GUM_PAGE_RX : GUM_PAGE_RW;
  code = gum_memory_allocate (NULL, page_size, page_size, code_prot);

  layout.pc = code;
  layout.handler_slot = &notifier->handler;
  gum_memory_patch_code (code, page_size, gum_write_resident_stub, &layout);

  notifier->noop = GUM_POINTER_TO_FUNCPTR (GumAddImageNotifier,
      gum_sign_code_pointer ((guint8 *) code + layout.noop_offset));
  notifier->handler = notifier->noop;

  _dyld_register_func_for_add_image (GUM_POINTER_TO_FUNCPTR (
      GumAddImageNotifier,
      gum_sign_code_pointer ((guint8 *) code + layout.entry_offset)));

  notifier->handler = GUM_POINTER_TO_FUNCPTR (GumAddImageNotifier,
      gum_sign_code_pointer (gum_strip_code_pointer (
          GUM_FUNCPTR_TO_POINTER (gum_resident_add_notifier_on_image_event))));
  g_atomic_pointer_set (&notifier->magic,
      GSIZE_TO_POINTER (GUM_RESIDENT_NOTIFIER_MAGIC));

  return notifier;
}

static void
gum_write_resident_stub (gpointer mem,
                         gpointer user_data)
{
  GumResidentStubLayout * layout = user_data;

#ifdef HAVE_I386
  GumX86Writer cw;

  gum_x86_writer_init (&cw, mem);
  cw.pc = GUM_ADDRESS (layout->pc);

  layout->entry_offset = gum_x86_writer_offset (&cw);
  gum_x86_writer_put_mov_reg_address (&cw, GUM_X86_XAX,
      GUM_ADDRESS (layout->handler_slot));
  gum_x86_writer_put_jmp_reg_offset_ptr (&cw, GUM_X86_XAX, 0);

  layout->noop_offset = gum_x86_writer_offset (&cw);
  gum_x86_writer_put_ret (&cw);

  gum_x86_writer_clear (&cw);
#else
  GumArm64Writer cw;

  gum_arm64_writer_init (&cw, mem);
  cw.pc = GUM_ADDRESS (layout->pc);

  layout->entry_offset = gum_arm64_writer_offset (&cw);
  gum_arm64_writer_put_ldr_reg_address (&cw, ARM64_REG_X16,
      GUM_ADDRESS (layout->handler_slot));
  gum_arm64_writer_put_ldr_reg_reg_offset (&cw, ARM64_REG_X16, ARM64_REG_X16,
      0);
  gum_arm64_writer_put_br_reg (&cw, ARM64_REG_X16);

  layout->noop_offset = gum_arm64_writer_offset (&cw);
  gum_arm64_writer_put_ret (&cw);

  gum_arm64_writer_clear (&cw);
#endif
}

static void
gum_resident_add_notifier_on_image_event (const struct mach_header * mh,
                                          intptr_t vmaddr_slide)
{
  gum_module_registry_synchronize_modules ();
}

static void
gum_module_registry_begin_tracking (gboolean on_leave)
{
  gum_module_registry_snapshot_modules ();
  gum_module_registry_start_change_tracking (on_leave);
}

static void
gum_module_registry_snapshot_modules (void)
{
  uint32_t count, i;

  gum_current_modules =
      g_hash_table_new_full (NULL, NULL, NULL, g_object_unref);

  _gum_module_registry_reset (gum_registry);

  count = _dyld_image_count ();
  for (i = 0; i != count; i++)
  {
    const struct mach_header * mh = _dyld_get_image_header (i);
    GumNativeModule * mod =
        gum_make_image_module (mh, _dyld_get_image_name (i));

    g_hash_table_insert (gum_current_modules,
        GSIZE_TO_POINTER (GUM_ADDRESS (mh)), g_object_ref (mod));
    _gum_module_registry_register (gum_registry, GUM_MODULE (mod));

    g_object_unref (mod);
  }
}

static void
gum_module_registry_start_change_tracking (gboolean on_leave)
{
  gum_rtld_interceptor = gum_interceptor_obtain ();
  gum_rtld_handler = on_leave
      ? gum_make_call_listener (NULL,
          gum_module_registry_on_rtld_notification, NULL, NULL)
      : gum_make_probe_listener (
          gum_module_registry_on_rtld_notification, NULL, NULL);
}

static void
gum_module_registry_attach_notifier (gpointer location)
{
  GumAttachOptions options = {
    .ignorability = GUM_INVOCATION_UNIGNORABLE
  };

  if (gum_interceptor_attach (gum_rtld_interceptor, location, gum_rtld_handler,
        &options) == GUM_ATTACH_WRONG_SIGNATURE)
  {
    options.instrumentation.relocation_policy = GUM_RELOCATION_FORCED;
    gum_interceptor_attach (gum_rtld_interceptor, location, gum_rtld_handler,
        &options);
  }
}

static gsize
gum_detect_macho_size (const struct mach_header * mh)
{
  gconstpointer first_command, p;
  guint i;

#if GLIB_SIZEOF_VOID_P == 8
  first_command = (const guint8 *) mh + sizeof (struct mach_header_64);
#else
  first_command = (const guint8 *) mh + sizeof (struct mach_header);
#endif

  p = first_command;
  for (i = 0; i != mh->ncmds; i++)
  {
    const struct load_command * lc = p;

    if (lc->cmd == LC_SEGMENT)
    {
      const struct segment_command * sc = p;
      if (strcmp (sc->segname, "__TEXT") == 0)
        return sc->vmsize;
    }
    else if (lc->cmd == LC_SEGMENT_64)
    {
      const struct segment_command_64 * sc = p;
      if (strcmp (sc->segname, "__TEXT") == 0)
        return sc->vmsize;
    }

    p += lc->cmdsize;
  }

  g_assert_not_reached ();
}
