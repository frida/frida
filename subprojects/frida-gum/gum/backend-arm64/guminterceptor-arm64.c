/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2022-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2026 inforcqb <fanjiawei080615@qq.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "guminterceptor-priv.h"

#include "gumarm64reader.h"
#include "gumarm64relocator.h"
#include "gumarm64writer.h"
#include "gumcloak.h"
#include "gumlibc.h"
#include "gummemory.h"
#ifdef HAVE_DARWIN
# include "gum/gumdarwin.h"
# include "gumdarwingrafter-priv.h"
#endif

#ifdef HAVE_DARWIN
# include <dlfcn.h>
# include <mach-o/dyld.h>
# include <mach-o/loader.h>
# include <stdlib.h>
#endif

#define GUM_INTERCEPTOR_FULL_REDIRECT_SIZE 16

#define GUM_ARM64_LOGICAL_PAGE_SIZE 4096

#define GUM_FRAME_OFFSET_CPU_CONTEXT 0
#define GUM_FRAME_OFFSET_NEXT_HOP \
    (GUM_FRAME_OFFSET_CPU_CONTEXT + sizeof (GumCpuContext))

#define GUM_FCDATA(context) \
    ((GumArm64FunctionContextData *) (context)->backend_data.storage)

typedef struct _GumArm64FunctionContextData GumArm64FunctionContextData;
typedef struct _GumThunkSet GumThunkSet;
typedef struct _GumEmitThunksContext GumEmitThunksContext;

struct _GumInterceptorBackend
{
  GRecMutex * mutex;
  GumCodeAllocator * allocator;

  GumArm64Writer writer;
  GumArm64Relocator relocator;

  GHashTable * thunks_by_scratch_reg;
};

struct _GumThunkSet
{
  gpointer page;
  gpointer enter_thunk;
  gpointer leave_thunk;
};

struct _GumEmitThunksContext
{
  GumInterceptorBackend * backend;
  GumThunkSet * thunks;
  arm64_reg scratch_reg;
};

struct _GumArm64FunctionContextData
{
  guint redirect_code_size;
  arm64_reg scratch_reg;
  guint available_space;
};

G_STATIC_ASSERT (sizeof (GumArm64FunctionContextData)
    <= sizeof (GumFunctionContextBackendData));

static gboolean gum_interceptor_backend_write_custom_redirect (
    GumInterceptorBackend * self, GumFunctionContext * ctx, gpointer target);

static GumThunkSet * gum_interceptor_backend_get_thunks (
    GumInterceptorBackend * self, arm64_reg scratch_reg);
static GumThunkSet * gum_thunk_set_new (GumInterceptorBackend * backend,
    arm64_reg scratch_reg);
static void gum_thunk_set_free (GumThunkSet * thunks);

static void gum_emit_thunks (gpointer mem, GumEmitThunksContext * ctx);
static void gum_emit_enter_thunk (GumArm64Writer * aw, arm64_reg scratch_reg);
static void gum_emit_leave_thunk (GumArm64Writer * aw, arm64_reg scratch_reg);

static void gum_emit_prolog (GumArm64Writer * aw);
static void gum_emit_epilog (GumArm64Writer * aw, arm64_reg scratch_reg);

GumInterceptorBackend *
_gum_interceptor_backend_create (GRecMutex * mutex,
                                 GumCodeAllocator * allocator)
{
  GumInterceptorBackend * backend;

  backend = g_slice_new0 (GumInterceptorBackend);
  backend->mutex = mutex;
  backend->allocator = allocator;

  if (gum_process_get_code_signing_policy () == GUM_CODE_SIGNING_OPTIONAL)
  {
    gum_arm64_writer_init (&backend->writer, NULL);
    gum_arm64_relocator_init (&backend->relocator, NULL, &backend->writer);

    backend->thunks_by_scratch_reg = g_hash_table_new_full (NULL, NULL, NULL,
        (GDestroyNotify) gum_thunk_set_free);
  }

  return backend;
}

void
_gum_interceptor_backend_destroy (GumInterceptorBackend * backend)
{
  if (backend->thunks_by_scratch_reg != NULL)
  {
    g_hash_table_unref (backend->thunks_by_scratch_reg);

    gum_arm64_relocator_clear (&backend->relocator);
    gum_arm64_writer_clear (&backend->writer);
  }

  g_slice_free (GumInterceptorBackend, backend);
}

#ifdef HAVE_DARWIN

typedef struct _GumImportTarget GumImportTarget;
typedef struct _GumImportEntry GumImportEntry;
typedef struct _GumClaimHookOperation GumClaimHookOperation;
typedef struct _GumGraftedSegmentPairDetails GumGraftedSegmentPairDetails;

typedef gboolean (* GumFoundGraftedSegmentPairFunc) (
    const GumGraftedSegmentPairDetails * details, gpointer user_data);

struct _GumImportTarget
{
  gpointer implementation;
  GumFunctionContext * ctx;
  GArray * entries;
};

struct _GumImportEntry
{
  const struct mach_header_64 * mach_header;
  GumGraftedImport * import;
};

struct _GumClaimHookOperation
{
  GumFunctionContext * ctx;
  guint32 code_offset;

  gboolean success;
};

struct _GumGraftedSegmentPairDetails
{
  const struct mach_header_64 * mach_header;

  GumGraftedHeader * header;

  GumGraftedHook * hooks;
  guint32 num_hooks;

  GumGraftedImport * imports;
  guint32 num_imports;
};

extern void _gum_interceptor_begin_invocation (void);
extern void _gum_interceptor_end_invocation (void);

static void gum_on_module_added (const struct mach_header * mh,
    intptr_t vmaddr_slide);
static void gum_on_module_removed (const struct mach_header * mh,
    intptr_t vmaddr_slide);
static gboolean gum_attach_segment_pair (
    const GumGraftedSegmentPairDetails * details, gpointer user_data);
static gboolean gum_detach_segment_pair (
    const GumGraftedSegmentPairDetails * details, gpointer user_data);
static gboolean gum_claim_hook_if_found_in_pair (
    const GumGraftedSegmentPairDetails * details, gpointer user_data);

static GumImportTarget * gum_import_target_register (gpointer implementation);
static void gum_import_target_link (GumImportTarget * self,
    GumFunctionContext * ctx);
static void gum_import_target_free (GumImportTarget * target);
static void gum_import_target_maybe_activate (GumImportTarget * self,
    const GumImportEntry * entry);
static void gum_import_target_activate (GumImportTarget * self,
    const GumImportEntry * entry);
static void gum_import_target_deactivate (GumImportTarget * self,
    const GumImportEntry * entry);

static void gum_enumerate_grafted_segment_pairs (gconstpointer mach_header,
    GumFoundGraftedSegmentPairFunc func, gpointer user_data);

static int gum_compare_grafted_hook (const void * element_a,
    const void * element_b);

static gboolean gum_is_system_module (const gchar * path);

static GumInterceptorBackend * gum_interceptor_backend = NULL;
static GHashTable * gum_import_targets = NULL;

gboolean
_gum_interceptor_backend_claim_grafted_trampoline (GumInterceptorBackend * self,
                                                   GumFunctionContext * ctx)
{
  GumImportTarget * target;
  Dl_info info;
  GumClaimHookOperation op;

  if (gum_interceptor_backend == NULL)
  {
    gum_interceptor_backend = self;
    gum_import_targets = g_hash_table_new_full (NULL, NULL, NULL,
        (GDestroyNotify) gum_import_target_free);

    _dyld_register_func_for_add_image (gum_on_module_added);
    _dyld_register_func_for_remove_image (gum_on_module_removed);
  }

  target = g_hash_table_lookup (gum_import_targets, ctx->function_address);
  if (target != NULL)
  {
    gum_import_target_link (target, ctx);
    return TRUE;
  }

  if (dladdr (ctx->function_address, &info) == 0)
    return FALSE;

  op.ctx = ctx;
  op.code_offset = (guint8 *) ctx->function_address - (guint8 *) info.dli_fbase;

  op.success = FALSE;

  gum_enumerate_grafted_segment_pairs (info.dli_fbase,
      gum_claim_hook_if_found_in_pair, &op);

  if (!op.success && gum_is_system_module (info.dli_fname))
  {
    target = gum_import_target_register (ctx->function_address);
    gum_import_target_link (target, ctx);
    return TRUE;
  }

  return op.success;
}

static void
gum_on_module_added (const struct mach_header * mh,
                     intptr_t vmaddr_slide)
{
  g_rec_mutex_lock (gum_interceptor_backend->mutex);
  gum_enumerate_grafted_segment_pairs (mh, gum_attach_segment_pair, NULL);
  g_rec_mutex_unlock (gum_interceptor_backend->mutex);
}

static void
gum_on_module_removed (const struct mach_header * mh,
                       intptr_t vmaddr_slide)
{
  g_rec_mutex_lock (gum_interceptor_backend->mutex);
  gum_enumerate_grafted_segment_pairs (mh, gum_detach_segment_pair, NULL);
  g_rec_mutex_unlock (gum_interceptor_backend->mutex);
}

static gboolean
gum_attach_segment_pair (const GumGraftedSegmentPairDetails * details,
                         gpointer user_data)
{
  const struct mach_header_64 * mach_header = details->mach_header;
  GumGraftedHeader * header = details->header;
  GumGraftedImport * imports = details->imports;
  guint32 i;

  header->begin_invocation =
      GPOINTER_TO_SIZE (_gum_interceptor_begin_invocation);
  header->end_invocation =
      GPOINTER_TO_SIZE (_gum_interceptor_end_invocation);

  for (i = 0; i != header->num_imports; i++)
  {
    GumGraftedImport * import = &imports[i];
    gpointer * slot, implementation;
    GumImportTarget * target;
    GumImportEntry entry;

    slot = (gpointer *) ((const guint8 *) mach_header + import->slot_offset);
    implementation = *slot;

    target = g_hash_table_lookup (gum_import_targets, implementation);
    if (target == NULL)
      target = gum_import_target_register (implementation);

    entry.mach_header = mach_header;
    entry.import = import;
    g_array_append_val (target->entries, entry);

    gum_import_target_maybe_activate (target, &entry);
  }

  return TRUE;
}

static gboolean
gum_detach_segment_pair (const GumGraftedSegmentPairDetails * details,
                         gpointer user_data)
{
  const struct mach_header_64 * mach_header = details->mach_header;
  GHashTableIter iter;
  gpointer implementation;
  GumImportTarget * target;
  GQueue empty_targets = G_QUEUE_INIT;
  GList * cur;

  g_hash_table_iter_init (&iter, gum_import_targets);
  while (g_hash_table_iter_next (&iter, &implementation, (gpointer *) &target))
  {
    GArray * entries = target->entries;
    gint i;

    for (i = 0; i < entries->len; i++)
    {
      GumImportEntry * entry = &g_array_index (entries, GumImportEntry, i);
      if (entry->mach_header == mach_header)
      {
        g_array_remove_index_fast (entries, i);
        i--;
      }
    }

    if (target->ctx == NULL && entries->len == 0)
    {
      g_queue_push_tail (&empty_targets, implementation);
    }
    else if (entries->len != 0)
    {
      gum_import_target_maybe_activate (target,
          &g_array_index (entries, GumImportEntry, 0));
    }
  }

  for (cur = empty_targets.head; cur != NULL; cur = cur->next)
  {
    g_hash_table_remove (gum_import_targets, cur->data);
  }

  g_queue_clear (&empty_targets);

  return TRUE;
}

static gboolean
gum_claim_hook_if_found_in_pair (const GumGraftedSegmentPairDetails * details,
                                 gpointer user_data)
{
  GumClaimHookOperation * op = user_data;
  GumFunctionContext * ctx = op->ctx;
  GumGraftedHook key = { 0, };
  GumGraftedHook * hook;
  guint8 * trampoline;

  key.code_offset = op->code_offset;
  hook = bsearch (&key, details->hooks, details->header->num_hooks,
      sizeof (GumGraftedHook), gum_compare_grafted_hook);
  if (hook == NULL)
    return TRUE;

  hook->user_data = GPOINTER_TO_SIZE (ctx);

  ctx->grafted_hook = hook;

  trampoline = (guint8 *) details->mach_header + hook->trampoline_offset;
  ctx->on_enter_trampoline =
      trampoline + GUM_GRAFTED_HOOK_ON_ENTER_OFFSET (hook);
  ctx->on_leave_trampoline =
      trampoline + GUM_GRAFTED_HOOK_ON_LEAVE_OFFSET (hook);
  ctx->on_invoke_trampoline =
      trampoline + GUM_GRAFTED_HOOK_ON_INVOKE_OFFSET (hook);

  op->success = TRUE;

  return FALSE;
}

static GumImportTarget *
gum_import_target_register (gpointer implementation)
{
  GumImportTarget * target;

  target = g_slice_new (GumImportTarget);
  target->implementation = implementation;
  target->ctx = NULL;
  target->entries = g_array_new (FALSE, FALSE, sizeof (GumImportEntry));

  g_hash_table_insert (gum_import_targets, implementation, target);

  return target;
}

static void
gum_import_target_link (GumImportTarget * self,
                        GumFunctionContext * ctx)
{
  self->ctx = ctx;
  ctx->import_target = self;
}

static void
gum_import_target_free (GumImportTarget * target)
{
  g_array_free (target->entries, TRUE);

  g_slice_free (GumImportTarget, target);
}

static void
gum_import_target_activate_all (GumImportTarget * self)
{
  GArray * entries = self->entries;
  guint i;

  for (i = 0; i != entries->len; i++)
  {
    const GumImportEntry * entry = &g_array_index (entries, GumImportEntry, i);
    gum_import_target_activate (self, entry);
  }
}

static void
gum_import_target_deactivate_all (GumImportTarget * self)
{
  GArray * entries = self->entries;
  guint i;

  for (i = 0; i != entries->len; i++)
  {
    const GumImportEntry * entry = &g_array_index (entries, GumImportEntry, i);
    gum_import_target_deactivate (self, entry);
  }
}

static void
gum_import_target_maybe_activate (GumImportTarget * self,
                                  const GumImportEntry * entry)
{
  GumFunctionContext * ctx = self->ctx;

  if (ctx == NULL || !ctx->activated)
    return;

  gum_import_target_activate (self, entry);
}

static void
gum_import_target_activate (GumImportTarget * self,
                            const GumImportEntry * entry)
{
  GumFunctionContext * ctx = self->ctx;
  GumGraftedImport * import = entry->import;
  gpointer * slot;
  guint8 * trampoline;
  mach_port_t self_task;
  GumPageProtection prot;
  gboolean flip_needed;

  import->user_data = GPOINTER_TO_SIZE (ctx);

  slot = (gpointer *) ((guint8 *) entry->mach_header + import->slot_offset);

  trampoline = (guint8 *) entry->mach_header + import->trampoline_offset;
  ctx->on_enter_trampoline =
      trampoline + GUM_GRAFTED_IMPORT_ON_ENTER_OFFSET (import);
  ctx->on_leave_trampoline =
      trampoline + GUM_GRAFTED_IMPORT_ON_LEAVE_OFFSET (import);
  ctx->on_invoke_trampoline = self->implementation;

  self_task = mach_task_self ();

  if (!gum_darwin_query_protection (self_task, GUM_ADDRESS (slot), &prot))
    return;

  flip_needed = (prot & GUM_PAGE_WRITE) == 0;
  if (flip_needed)
  {
    if (!gum_try_mprotect (slot, 4, prot | GUM_PAGE_WRITE))
      return;
  }

  *slot = ctx->on_enter_trampoline;

  if (flip_needed)
    gum_try_mprotect (slot, 4, prot);
}

static void
gum_import_target_deactivate (GumImportTarget * self,
                              const GumImportEntry * entry)
{
  mach_port_t self_task;
  GumPageProtection prot;
  gboolean flip_needed;
  gpointer * slot =
      (gpointer *) ((guint8 *) entry->mach_header + entry->import->slot_offset);

  self_task = mach_task_self ();

  if (!gum_darwin_query_protection (self_task, GUM_ADDRESS (slot), &prot))
    return;

  flip_needed = (prot & GUM_PAGE_WRITE) == 0;
  if (flip_needed)
  {
    if (!gum_try_mprotect (slot, 4, prot | GUM_PAGE_WRITE))
      return;
  }

  *slot = self->implementation;

  if (flip_needed)
    gum_try_mprotect (slot, 4, prot);
}

static void
gum_import_target_clear_user_data (GumImportTarget * self)
{
  GArray * entries = self->entries;
  guint i;

  for (i = 0; i != entries->len; i++)
  {
    const GumImportEntry * entry = &g_array_index (entries, GumImportEntry, i);
    entry->import->user_data = 0;
  }
}

static void
gum_enumerate_grafted_segment_pairs (gconstpointer mach_header,
                                     GumFoundGraftedSegmentPairFunc func,
                                     gpointer user_data)
{
  const struct mach_header_64 * mh;
  gconstpointer command;
  intptr_t slide;
  guint i;

  mh = mach_header;
  command = mh + 1;
  slide = 0;
  for (i = 0; i != mh->ncmds; i++)
  {
    const struct load_command * lc = command;

    if (lc->cmd == LC_SEGMENT_64)
    {
      const struct segment_command_64 * sc = command;

      if (strcmp (sc->segname, "__TEXT") == 0)
      {
        slide = (guint8 *) mach_header - (guint8 *) sc->vmaddr;
      }
      else if (g_str_has_prefix (sc->segname, "__FRIDA_DATA"))
      {
        GumGraftedHeader * header = GSIZE_TO_POINTER (sc->vmaddr + slide);

        if (header->abi_version == GUM_DARWIN_GRAFTER_ABI_VERSION)
        {
          GumGraftedSegmentPairDetails d;

          d.mach_header = mh;

          d.header = header;

          d.hooks = (GumGraftedHook *) (header + 1);
          d.num_hooks = header->num_hooks;

          d.imports = (GumGraftedImport *) (d.hooks + header->num_hooks);
          d.num_imports = header->num_imports;

          if (!func (&d, user_data))
            return;
        }
      }
    }

    command = (const guint8 *) command + lc->cmdsize;
  }
}

static int
gum_compare_grafted_hook (const void * element_a,
                          const void * element_b)
{
  const GumGraftedHook * a = element_a;
  const GumGraftedHook * b = element_b;

  return (gssize) a->code_offset - (gssize) b->code_offset;
}

static gboolean
gum_is_system_module (const gchar * path)
{
  gboolean has_system_prefix;
  static gboolean api_initialized = FALSE;
  static bool (* dsc_contains_path) (const char * path) = NULL;

  has_system_prefix = g_str_has_prefix (path, "/System/") ||
      g_str_has_prefix (path, "/usr/lib/") ||
      g_str_has_prefix (path, "/Developer/") ||
      g_str_has_prefix (path, "/private/preboot/");
  if (has_system_prefix)
    return TRUE;

  if (!api_initialized)
  {
    dsc_contains_path =
        dlsym (RTLD_DEFAULT, "_dyld_shared_cache_contains_path");
    api_initialized = TRUE;
  }

  if (dsc_contains_path != NULL)
    return dsc_contains_path (path);

  return FALSE;
}

#else

gboolean
_gum_interceptor_backend_claim_grafted_trampoline (GumInterceptorBackend * self,
                                                   GumFunctionContext * ctx)
{
  return FALSE;
}

#endif

static gboolean
gum_interceptor_backend_prepare_trampoline (GumInterceptorBackend * self,
                                            GumFunctionContext * ctx,
                                            gboolean force,
                                            gboolean * need_deflector)
{
  GumArm64FunctionContextData * data = GUM_FCDATA (ctx);
  gpointer function_address = ctx->function_address;
  GumRelocationScenario scenario =
      (ctx->scenario == GUM_INTERCEPTOR_SCENARIO_OFFLINE)
      ? GUM_SCENARIO_OFFLINE
      : GUM_SCENARIO_ONLINE;
  guint redirect_limit;

  *need_deflector = FALSE;

  data->scratch_reg = ctx->scratch_register;

  if (ctx->write_redirect != NULL)
  {
    guint scan_bytes;

    scan_bytes = (ctx->redirect_space_hint != 0)
        ? ctx->redirect_space_hint
        : GUM_INTERCEPTOR_MAX_REDIRECT_SIZE;
    gum_arm64_relocator_can_relocate (function_address, scan_bytes, scenario,
        ctx->relocation_policy, &data->available_space, &data->scratch_reg);
    if (ctx->redirect_space_hint != 0 &&
        data->available_space > ctx->redirect_space_hint)
      data->available_space = ctx->redirect_space_hint;
    if (data->available_space == 0)
      return FALSE;

    if (data->scratch_reg == ARM64_REG_INVALID)
    {
      data->scratch_reg = (ctx->scratch_register != ARM64_REG_INVALID)
          ? ctx->scratch_register
          : ARM64_REG_X16;
    }

    data->redirect_code_size = GUM_INTERCEPTOR_FULL_REDIRECT_SIZE;
    ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);
    ctx->redirect_code = g_malloc (data->available_space);

    return TRUE;
  }

  if (gum_arm64_relocator_can_relocate (function_address,
        GUM_INTERCEPTOR_FULL_REDIRECT_SIZE, scenario, ctx->relocation_policy,
        &redirect_limit, &data->scratch_reg))
  {
    data->redirect_code_size = GUM_INTERCEPTOR_FULL_REDIRECT_SIZE;

    ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);
  }
  else if (force)
  {
    data->redirect_code_size = GUM_INTERCEPTOR_FULL_REDIRECT_SIZE;

    ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);

    if (data->scratch_reg == ARM64_REG_INVALID)
    {
      data->scratch_reg = (ctx->scratch_register != ARM64_REG_INVALID)
          ? ctx->scratch_register
          : ARM64_REG_X16;
    }

    return TRUE;
  }
  else if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
  {
    /*
     * For fast interceptors, we must patch the target function to jump
     * directly to the replacement function, instead of using a trampoline.
     * This requires the jump instruction at the target site to reach the
     * replacement function's address.
     *
     * However, if there are only 4 or 8 bytes of space available for patching,
     * we cannot always emit a jump instruction that can reliably reach the
     * replacement function (since the distance may be too far for short-range
     * instructions). Therefore, we cannot proceed in this case.
     */
    return FALSE;
  }
  else
  {
    GumAddressSpec spec;
    gsize alignment;

    if (redirect_limit >= 8)
    {
      data->redirect_code_size = 8;

      spec.near_address = GSIZE_TO_POINTER (
          GPOINTER_TO_SIZE (function_address) &
          ~((gsize) (GUM_ARM64_LOGICAL_PAGE_SIZE - 1)));
      spec.max_distance = GUM_ARM64_ADRP_MAX_DISTANCE;
      alignment = GUM_ARM64_LOGICAL_PAGE_SIZE;
    }
    else if (redirect_limit >= 4)
    {
      data->redirect_code_size = 4;

      spec.near_address = function_address;
      spec.max_distance = GUM_ARM64_B_MAX_DISTANCE;
      alignment = 0;
    }
    else
    {
      return FALSE;
    }

    ctx->trampoline_slice = gum_code_allocator_try_alloc_slice_near (
        self->allocator, &spec, alignment);
    if (ctx->trampoline_slice == NULL)
    {
      ctx->trampoline_slice = gum_code_allocator_alloc_slice (self->allocator);
      *need_deflector = TRUE;
    }
  }

  if (data->scratch_reg == ARM64_REG_INVALID)
  {
    if (!force)
      goto no_scratch_reg;

    data->scratch_reg = (ctx->scratch_register != ARM64_REG_INVALID)
        ? ctx->scratch_register
        : ARM64_REG_X16;
  }

  return TRUE;

no_scratch_reg:
  {
    gum_code_slice_unref (ctx->trampoline_slice);
    ctx->trampoline_slice = NULL;
    return FALSE;
  }
}

gboolean
_gum_interceptor_backend_create_trampoline (GumInterceptorBackend * self,
                                            GumFunctionContext * ctx,
                                            gboolean force)
{
  GumArm64Writer * aw = &self->writer;
  GumArm64Relocator * ar = &self->relocator;
  gpointer function_address = ctx->function_address;
  GumArm64FunctionContextData * data = GUM_FCDATA (ctx);
  gboolean need_deflector;
  GumThunkSet * thunks = NULL;
  gpointer deflector_target;
  GString * signature;
  gboolean is_eligible_for_lr_rewriting;
  guint reloc_bytes;

  if (!gum_interceptor_backend_prepare_trampoline (self, ctx, force,
        &need_deflector))
    return FALSE;

  if (ctx->type != GUM_INTERCEPTOR_TYPE_FAST)
    thunks = gum_interceptor_backend_get_thunks (self, data->scratch_reg);

  gum_arm64_writer_reset (aw, ctx->trampoline_slice->data);
  aw->pc = GUM_ADDRESS (ctx->trampoline_slice->pc);

  if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
  {
    deflector_target = ctx->replacement_function;
  }
  else
  {
    ctx->on_enter_trampoline = gum_sign_code_pointer (
        (guint8 *) ctx->trampoline_slice->pc + gum_arm64_writer_offset (aw));
    deflector_target = ctx->on_enter_trampoline;
  }

  if (need_deflector)
  {
    GumAddressSpec caller;
    gpointer return_address;
    gboolean dedicated;

    caller.near_address =
        (guint8 *) function_address + data->redirect_code_size - 4;
    caller.max_distance = GUM_ARM64_B_MAX_DISTANCE;

    return_address = (guint8 *) function_address + data->redirect_code_size;

    dedicated = data->redirect_code_size == 4;

    ctx->trampoline_deflector = gum_code_allocator_alloc_deflector (
        self->allocator, &caller, return_address, deflector_target, dedicated);
    if (ctx->trampoline_deflector == NULL)
    {
      gum_code_slice_unref (ctx->trampoline_slice);
      ctx->trampoline_slice = NULL;
      return FALSE;
    }

    gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_X0, ARM64_REG_LR);
  }

  if (ctx->type != GUM_INTERCEPTOR_TYPE_FAST)
  {
    arm64_reg scratch_reg = data->scratch_reg;

    gum_arm64_writer_put_ldr_reg_address (aw, scratch_reg, GUM_ADDRESS (ctx));
    gum_arm64_writer_put_str_reg_reg_offset_mode (aw, scratch_reg,
        ARM64_REG_SP, -16, GUM_INDEX_PRE_ADJUST);
    gum_arm64_writer_put_ldr_reg_address (aw, scratch_reg,
        GUM_ADDRESS (gum_sign_code_pointer (thunks->enter_thunk)));
    gum_arm64_writer_put_br_reg (aw, scratch_reg);

    ctx->on_leave_trampoline =
        (guint8 *) ctx->trampoline_slice->pc + gum_arm64_writer_offset (aw);

    gum_arm64_writer_put_ldr_reg_address (aw, scratch_reg, GUM_ADDRESS (ctx));
    gum_arm64_writer_put_str_reg_reg_offset_mode (aw, scratch_reg,
        ARM64_REG_SP, -16, GUM_INDEX_PRE_ADJUST);
    gum_arm64_writer_put_ldr_reg_address (aw, scratch_reg,
        GUM_ADDRESS (gum_sign_code_pointer (thunks->leave_thunk)));
    gum_arm64_writer_put_br_reg (aw, scratch_reg);

    gum_arm64_writer_flush (aw);
    g_assert (gum_arm64_writer_offset (aw) <= ctx->trampoline_slice->size);
  }

  ctx->on_invoke_trampoline = gum_sign_code_pointer (
      (guint8 *) ctx->trampoline_slice->pc + gum_arm64_writer_offset (aw));

  if (ctx->write_redirect != NULL &&
      !gum_interceptor_backend_write_custom_redirect (self, ctx,
        deflector_target))
  {
    gum_code_slice_unref (ctx->trampoline_slice);
    ctx->trampoline_slice = NULL;
    return FALSE;
  }

  gum_arm64_relocator_reset (ar, function_address, aw);

  signature = g_string_sized_new (16);

  do
  {
    const cs_insn * insn;

    reloc_bytes = gum_arm64_relocator_read_one (ar, &insn);
    if (reloc_bytes == 0)
    {
      reloc_bytes = data->redirect_code_size;
      break;
    }

    if (signature->len != 0)
      g_string_append_c (signature, ';');
    g_string_append (signature, insn->mnemonic);
  }
  while (reloc_bytes < data->redirect_code_size);

  /*
   * Try to deal with minimal thunks that determine their caller and pass
   * it along to some inner function. This is important to support hooking
   * dlopen() on Android, where the dynamic linker uses the caller address
   * to decide on namespace and whether to allow the particular library to
   * be used by a particular caller.
   *
   * Because we potentially replace LR in order to trap the return, we end
   * up breaking dlopen() in such cases. We work around this by detecting
   * LR being read, and replace that instruction with a load of the actual
   * caller.
   *
   * This is however a bit risky done blindly, so we try to limit the
   * scope to the bare minimum. A potentially better longer term solution
   * is to analyze the function and patch each point of return, so we don't
   * have to replace LR on entry. That is however a bit complex, so we
   * opt for this simpler solution for now.
   */
  is_eligible_for_lr_rewriting = strcmp (signature->str, "mov;b") == 0 ||
      g_str_has_prefix (signature->str, "stp;mov;mov;bl");

  g_string_free (signature, TRUE);

  if (is_eligible_for_lr_rewriting)
  {
    const cs_insn * insn;

    while ((insn = gum_arm64_relocator_peek_next_write_insn (ar)) != NULL)
    {
      const cs_arm64_op * source_op = &insn->detail->arm64.operands[1];

      if (insn->id == ARM64_INS_MOV &&
          source_op->type == ARM64_OP_REG &&
          source_op->reg == ARM64_REG_LR)
      {
        arm64_reg dst_reg = insn->detail->arm64.operands[0].reg;
        const guint reg_size = sizeof (gpointer);
        const guint reg_pair_size = 2 * reg_size;
        guint dst_reg_index, dst_reg_slot_index, dst_reg_offset_in_frame;

        gum_arm64_writer_put_push_all_x_registers (aw);

        gum_arm64_writer_put_call_address_with_arguments (aw,
            GUM_ADDRESS (_gum_interceptor_translate_top_return_address), 1,
            GUM_ARG_REGISTER, ARM64_REG_LR);

        if (dst_reg >= ARM64_REG_X0 && dst_reg <= ARM64_REG_X28)
        {
          dst_reg_index = dst_reg - ARM64_REG_X0;
        }
        else
        {
          g_assert (dst_reg >= ARM64_REG_X29 && dst_reg <= ARM64_REG_X30);

          dst_reg_index = dst_reg - ARM64_REG_X29;
        }

        dst_reg_slot_index = (dst_reg_index * reg_size) / reg_pair_size;

        dst_reg_offset_in_frame = (15 - dst_reg_slot_index) * reg_pair_size;
        if (dst_reg_index % 2 != 0)
          dst_reg_offset_in_frame += reg_size;

        gum_arm64_writer_put_str_reg_reg_offset (aw, ARM64_REG_X0, ARM64_REG_SP,
            dst_reg_offset_in_frame);

        gum_arm64_writer_put_pop_all_x_registers (aw);

        gum_arm64_relocator_skip_one (ar);
      }
      else
      {
        gum_arm64_relocator_write_one (ar);
      }
    }
  }
  else
  {
    gum_arm64_relocator_write_all (ar);
  }

  if (!ar->eoi)
  {
    GumAddress resume_at;

    resume_at = gum_sign_code_address (
        GUM_ADDRESS (function_address) + reloc_bytes);
    gum_arm64_writer_put_ldr_reg_address (aw, data->scratch_reg, resume_at);
    gum_arm64_writer_put_jmp_reg (aw, data->scratch_reg);
  }

  gum_arm64_writer_flush (aw);
  g_assert (gum_arm64_writer_offset (aw) <= ctx->trampoline_slice->size);

  ctx->overwritten_prologue_len = reloc_bytes;
  ctx->overwritten_prologue = g_malloc (reloc_bytes);
  gum_memcpy (ctx->overwritten_prologue, function_address, reloc_bytes);

  return TRUE;
}

static gboolean
gum_interceptor_backend_write_custom_redirect (GumInterceptorBackend * self,
                                               GumFunctionContext * ctx,
                                               gpointer target)
{
  GumArm64FunctionContextData * data = GUM_FCDATA (ctx);
  GumRedirectWriteResult result;
  GumArm64Writer rw;
  GumRedirectWriteDetails details;

  gum_arm64_writer_init (&rw, ctx->redirect_code);
  rw.pc = GUM_ADDRESS (ctx->function_address);

  details.writer = &rw;
  details.target = target;
  details.scratch_register = data->scratch_reg;
  details.capacity = data->available_space;

  result = ctx->write_redirect (&details, ctx->write_redirect_data);

  gum_arm64_writer_flush (&rw);
  data->redirect_code_size = gum_arm64_writer_offset (&rw);
  gum_arm64_writer_clear (&rw);

  g_assert (data->redirect_code_size <= data->available_space);

  return result == GUM_REDIRECT_WRITTEN;
}

void
_gum_interceptor_backend_destroy_trampoline (GumInterceptorBackend * self,
                                             GumFunctionContext * ctx)
{
#ifdef HAVE_DARWIN
  if (ctx->grafted_hook != NULL)
  {
    GumGraftedHook * func = ctx->grafted_hook;
    func->user_data = 0;
    return;
  }

  if (ctx->import_target != NULL)
  {
    gum_import_target_clear_user_data (ctx->import_target);
    return;
  }
#endif

  gum_code_slice_unref (ctx->trampoline_slice);
  gum_code_deflector_unref (ctx->trampoline_deflector);
  ctx->trampoline_slice = NULL;
  ctx->trampoline_deflector = NULL;
}

void
_gum_interceptor_backend_activate_trampoline (GumInterceptorBackend * self,
                                              GumFunctionContext * ctx,
                                              gpointer prologue)
{
  GumArm64Writer * aw = &self->writer;
  GumArm64FunctionContextData * data = GUM_FCDATA (ctx);
  GumAddress on_enter, on_enter_bare;

  if (ctx->type == GUM_INTERCEPTOR_TYPE_FAST)
    on_enter = GUM_ADDRESS (ctx->replacement_function);
  else
    on_enter = GUM_ADDRESS (ctx->on_enter_trampoline);
  on_enter_bare =
      GUM_ADDRESS (gum_strip_code_pointer (GSIZE_TO_POINTER (on_enter)));

#ifdef HAVE_DARWIN
  if (ctx->grafted_hook != NULL)
  {
    _gum_grafted_hook_activate (ctx->grafted_hook);
    return;
  }

  if (ctx->import_target != NULL)
  {
    gum_import_target_activate_all (ctx->import_target);
    return;
  }
#endif

  gum_arm64_writer_reset (aw, prologue);
  aw->pc = GUM_ADDRESS (ctx->function_address);

  if (ctx->write_redirect != NULL)
  {
    gum_arm64_writer_put_bytes (aw, ctx->redirect_code,
        data->redirect_code_size);
  }
  else if (ctx->trampoline_deflector != NULL)
  {
    if (data->redirect_code_size == 8)
    {
      gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_X0, ARM64_REG_LR);
      gum_arm64_writer_put_bl_imm (aw,
          GUM_ADDRESS (ctx->trampoline_deflector->trampoline));
    }
    else
    {
      g_assert (data->redirect_code_size == 4);
      gum_arm64_writer_put_b_imm (aw,
          GUM_ADDRESS (ctx->trampoline_deflector->trampoline));
    }
  }
  else
  {
    switch (data->redirect_code_size)
    {
      case 4:
        gum_arm64_writer_put_b_imm (aw, on_enter_bare);
        break;
      case 8:
        gum_arm64_writer_put_adrp_reg_address (aw, data->scratch_reg,
            on_enter_bare);
        gum_arm64_writer_put_jmp_reg_no_auth (aw, data->scratch_reg);
        break;
      case GUM_INTERCEPTOR_FULL_REDIRECT_SIZE:
        gum_arm64_writer_put_ldr_reg_address (aw, data->scratch_reg, on_enter);
        gum_arm64_writer_put_jmp_reg (aw, data->scratch_reg);
        break;
      default:
        g_assert_not_reached ();
    }
  }

  gum_arm64_writer_flush (aw);
  g_assert (gum_arm64_writer_offset (aw) <= data->redirect_code_size);
}

void
_gum_interceptor_backend_deactivate_trampoline (GumInterceptorBackend * self,
                                                GumFunctionContext * ctx,
                                                gpointer prologue)
{
#ifdef HAVE_DARWIN
  if (ctx->grafted_hook != NULL)
  {
    _gum_grafted_hook_deactivate (ctx->grafted_hook);
    return;
  }

  if (ctx->import_target != NULL)
  {
    gum_import_target_deactivate_all (ctx->import_target);
    return;
  }
#endif

  gum_memcpy (prologue, ctx->overwritten_prologue,
      ctx->overwritten_prologue_len);
}

gpointer
_gum_interceptor_backend_get_function_address (GumFunctionContext * ctx)
{
  return ctx->function_address;
}

gpointer
_gum_interceptor_backend_resolve_redirect (GumInterceptorBackend * self,
                                           gpointer address)
{
  return gum_arm64_reader_try_get_relative_jump_target (address);
}

gsize
_gum_interceptor_backend_detect_hook_size (gconstpointer code,
                                           csh capstone,
                                           cs_insn * insn)
{
  const cs_arm64 * arm64 = &insn->detail->arm64;
  const uint8_t * start, * cursor;
  size_t size;
  uint64_t addr;
  arm64_reg expecting_branch_to_trampoline_in_reg = ARM64_REG_INVALID;
  gsize inline_data_size = 0;
  gboolean expecting_call_to_shared_deflector = FALSE;

  start = code;
  cursor = start;
  size = 16;
  addr = GPOINTER_TO_SIZE (cursor);

  if (!cs_disasm_iter (capstone, &cursor, &size, &addr, insn))
    return 0;
  switch (insn->id)
  {
    case ARM64_INS_B:
      return cursor - start;
    case ARM64_INS_ADRP:
    case ARM64_INS_LDR:
      expecting_branch_to_trampoline_in_reg = arm64->operands[0].reg;
      if (insn->id == ARM64_INS_LDR)
        inline_data_size = 8;
      break;
    case ARM64_INS_STP:
      expecting_call_to_shared_deflector =
          arm64->operands[0].reg == ARM64_REG_X0 &&
          arm64->operands[1].reg == ARM64_REG_LR;
      break;
    default:
      break;
  }

  if (!cs_disasm_iter (capstone, &cursor, &size, &addr, insn))
    return 0;
  switch (insn->id)
  {
    case ARM64_INS_BR:
    case ARM64_INS_BRAAZ:
      if (arm64->operands[0].reg == expecting_branch_to_trampoline_in_reg)
        return (cursor - start) + inline_data_size;
      break;
    case ARM64_INS_BL:
      if (expecting_call_to_shared_deflector)
        return cursor - start;
      break;
    default:
      break;
  }

  return 0;
}

static GumThunkSet *
gum_interceptor_backend_get_thunks (GumInterceptorBackend * self,
                                    arm64_reg scratch_reg)
{
  GumThunkSet * thunks;

  thunks = g_hash_table_lookup (self->thunks_by_scratch_reg,
      GINT_TO_POINTER (scratch_reg));
  if (thunks == NULL)
  {
    thunks = gum_thunk_set_new (self, scratch_reg);
    g_hash_table_insert (self->thunks_by_scratch_reg,
        GINT_TO_POINTER (scratch_reg), thunks);
  }

  return thunks;
}

static GumThunkSet *
gum_thunk_set_new (GumInterceptorBackend * backend,
                   arm64_reg scratch_reg)
{
  GumThunkSet * thunks;
  gsize page_size, code_size;
  GumPageProtection protection;
  GumMemoryRange range;
  GumEmitThunksContext ctx;

  thunks = g_slice_new (GumThunkSet);

  page_size = gum_query_page_size ();
  code_size = page_size;

  protection = gum_memory_can_remap_writable () ? GUM_PAGE_RX : GUM_PAGE_RW;

  thunks->page = gum_memory_allocate (NULL, code_size, page_size, protection);

  range.base_address = GUM_ADDRESS (thunks->page);
  range.size = code_size;
  gum_cloak_add_range (&range);

  ctx.backend = backend;
  ctx.thunks = thunks;
  ctx.scratch_reg = scratch_reg;
  gum_memory_patch_code (thunks->page, 1024,
      (GumMemoryPatchApplyFunc) gum_emit_thunks, &ctx);

  return thunks;
}

static void
gum_thunk_set_free (GumThunkSet * thunks)
{
  gum_memory_free (thunks->page, gum_query_page_size ());

  g_slice_free (GumThunkSet, thunks);
}

static void
gum_emit_thunks (gpointer mem,
                 GumEmitThunksContext * ctx)
{
  GumThunkSet * thunks = ctx->thunks;
  GumArm64Writer * aw = &ctx->backend->writer;
  arm64_reg scratch_reg = ctx->scratch_reg;

  thunks->enter_thunk = thunks->page;
  gum_arm64_writer_reset (aw, mem);
  aw->pc = GUM_ADDRESS (thunks->enter_thunk);
  gum_emit_enter_thunk (aw, scratch_reg);
  gum_arm64_writer_flush (aw);

  thunks->leave_thunk =
      (guint8 *) thunks->enter_thunk + gum_arm64_writer_offset (aw);
  gum_emit_leave_thunk (aw, scratch_reg);
  gum_arm64_writer_flush (aw);
}

static void
gum_emit_enter_thunk (GumArm64Writer * aw,
                      arm64_reg scratch_reg)
{
  gum_arm64_writer_put_bti (aw);

  gum_arm64_writer_put_ldr_reg_reg_offset (aw, ARM64_REG_X17, ARM64_REG_SP, 0);

  gum_emit_prolog (aw);

  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X1, ARM64_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X2, ARM64_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT + G_STRUCT_OFFSET (GumCpuContext, lr));
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X3, ARM64_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_arm64_writer_put_call_address_with_arguments (aw,
      GUM_ADDRESS (_gum_function_context_begin_invocation), 4,
      GUM_ARG_REGISTER, ARM64_REG_X17,
      GUM_ARG_REGISTER, ARM64_REG_X1,
      GUM_ARG_REGISTER, ARM64_REG_X2,
      GUM_ARG_REGISTER, ARM64_REG_X3);

  gum_emit_epilog (aw, scratch_reg);
}

static void
gum_emit_leave_thunk (GumArm64Writer * aw,
                      arm64_reg scratch_reg)
{
  gum_arm64_writer_put_bti (aw);

  gum_arm64_writer_put_ldr_reg_reg_offset (aw, ARM64_REG_X17, ARM64_REG_SP, 0);

  gum_emit_prolog (aw);

  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X1, ARM64_REG_SP,
      GUM_FRAME_OFFSET_CPU_CONTEXT);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X2, ARM64_REG_SP,
      GUM_FRAME_OFFSET_NEXT_HOP);

  gum_arm64_writer_put_call_address_with_arguments (aw,
      GUM_ADDRESS (_gum_function_context_end_invocation), 3,
      GUM_ARG_REGISTER, ARM64_REG_X17,
      GUM_ARG_REGISTER, ARM64_REG_X1,
      GUM_ARG_REGISTER, ARM64_REG_X2);

  gum_emit_epilog (aw, scratch_reg);
}

static void
gum_emit_prolog (GumArm64Writer * aw)
{
  gint i;

  /*
   * Set up our stack frame, with the next_hop slot already pushed by the
   * caller's dispatch sequence (also holding the function context pointer
   * on entry):
   *
   * [in: function context / frame pointer chain entry, out: next_hop]
   * [in/out: cpu_context]
   */

#ifndef G_OS_NONE
  /* Store vector registers */
  for (i = 30; i != -2; i -= 2)
    gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_Q0 + i, ARM64_REG_Q1 + i);
#endif

  /* Store X1-X28, FP, and LR */
  gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_FP, ARM64_REG_LR);
  for (i = 27; i != -1; i -= 2)
    gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_X0 + i, ARM64_REG_X1 + i);

  /* Store NZCV and X0 */
  gum_arm64_writer_put_mov_reg_nzcv (aw, ARM64_REG_X1);
  gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_X1, ARM64_REG_X0);

  /* PC placeholder and SP */
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_X0,
      ARM64_REG_SP, sizeof (GumCpuContext) -
      G_STRUCT_OFFSET (GumCpuContext, nzcv) + 16);
  gum_arm64_writer_put_push_reg_reg (aw, ARM64_REG_XZR, ARM64_REG_X0);

  /* Frame pointer chain entry */
  gum_arm64_writer_put_str_reg_reg_offset (aw, ARM64_REG_LR, ARM64_REG_SP,
      sizeof (GumCpuContext) + 8);
  gum_arm64_writer_put_str_reg_reg_offset (aw, ARM64_REG_FP, ARM64_REG_SP,
      sizeof (GumCpuContext) + 0);
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_FP, ARM64_REG_SP,
      sizeof (GumCpuContext));
}

static void
gum_emit_epilog (GumArm64Writer * aw,
                 arm64_reg scratch_reg)
{
  guint i;

  /* Skip PC and SP */
  gum_arm64_writer_put_add_reg_reg_imm (aw, ARM64_REG_SP, ARM64_REG_SP, 16);

  /* Restore NZCV and X0 */
  gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_X1, ARM64_REG_X0);
  gum_arm64_writer_put_mov_nzcv_reg (aw, ARM64_REG_X1);

  /* Restore X1-X28, FP, and LR */
  for (i = 1; i != 29; i += 2)
    gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_X0 + i, ARM64_REG_X1 + i);
  gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_FP, ARM64_REG_LR);

#ifndef G_OS_NONE
  /* Restore vector registers */
  for (i = 0; i != 32; i += 2)
    gum_arm64_writer_put_pop_reg_reg (aw, ARM64_REG_Q0 + i, ARM64_REG_Q1 + i);
#endif

  gum_arm64_writer_put_ldr_reg_reg_offset_mode (aw, scratch_reg, ARM64_REG_SP,
      16, GUM_INDEX_POST_ADJUST);
  gum_arm64_writer_put_jmp_reg (aw, scratch_reg);
}
