#include "frida-payload.h"

#ifdef HAVE_DARWIN

#include <gum/gumdarwin.h>
#include <gum/gummemory.h>

#define FRIDA_MH_MAGIC_64 0xfeedfacf

typedef struct _FridaFillInfoContext FridaFillInfoContext;
typedef struct _FridaDyldUnwindSections FridaDyldUnwindSections;
typedef struct _FridaCreateArgs FridaCreateArgs;

struct _FridaFillInfoContext
{
  FridaDyldUnwindSections * info;
  guint missing_info;
};

struct _FridaDyldUnwindSections
{
  const void * mh;
  const void * dwarf_section;
  uintptr_t dwarf_section_length;
  const void * compact_unwind_section;
  uintptr_t compact_unwind_section_length;
};

struct _FridaCreateArgs
{
  GumAddress range_start;
  GumAddress range_end;
};

static FridaDyldUnwindSections * frida_get_cached_sections (GumAddress range_start, GumAddress range_end);
static FridaDyldUnwindSections * frida_create_cached_sections (FridaCreateArgs * args);
static gboolean frida_fill_info (const GumDarwinSectionDetails * details, FridaFillInfoContext * ctx);

void
_frida_unwind_sitter_fill_unwind_sections (GumAddress invader_start, GumAddress invader_end, void * info)
{
  FridaDyldUnwindSections * unwind_sections = info;
  FridaDyldUnwindSections * cached;

  cached = frida_get_cached_sections (invader_start, invader_end);
  if (cached == NULL)
    return;

  memcpy (unwind_sections, cached, sizeof (FridaDyldUnwindSections));
}

static FridaDyldUnwindSections *
frida_get_cached_sections (GumAddress range_start, GumAddress range_end)
{
  static GOnce get_sections_once = G_ONCE_INIT;
  FridaCreateArgs args;

  args.range_start = range_start;
  args.range_end = range_end;

  g_once (&get_sections_once, (GThreadFunc) frida_create_cached_sections, &args);

  return (FridaDyldUnwindSections *) get_sections_once.retval;
}

static FridaDyldUnwindSections *
frida_create_cached_sections (FridaCreateArgs * args)
{
  FridaDyldUnwindSections * cached_sections;
  gsize page_size;
  gpointer header;
  GumPageProtection prot;
  GumDarwinModule * module;
  FridaFillInfoContext ctx;

  page_size = gum_query_page_size ();
  header = GSIZE_TO_POINTER (args->range_start);

  while ((gum_memory_query_protection (header, &prot) && (prot & GUM_PAGE_READ) == 0) ||
      (*(guint32 *) header != FRIDA_MH_MAGIC_64 && header + 4 <= GSIZE_TO_POINTER (args->range_end)))
  {
    header += page_size;
  }
  if (*(guint32 *) header != FRIDA_MH_MAGIC_64)
    return NULL;

  cached_sections = g_slice_new0 (FridaDyldUnwindSections);
  cached_sections->mh = header;

  module = gum_darwin_module_new_from_memory ("Frida", mach_task_self (), GPOINTER_TO_SIZE (header),
      GUM_DARWIN_MODULE_FLAGS_NONE, NULL);
  if (module == NULL)
    return cached_sections;

  ctx.info = cached_sections;
  ctx.missing_info = 2;
  gum_darwin_module_enumerate_sections (module, (GumFoundDarwinSectionFunc) frida_fill_info, &ctx);

  g_object_unref (module);

  return cached_sections;
}

static gboolean
frida_fill_info (const GumDarwinSectionDetails * details, FridaFillInfoContext * ctx)
{
  if (strcmp ("__TEXT", details->segment_name) != 0)
    return TRUE;

  if (strcmp ("__eh_frame", details->section_name) == 0)
  {
    ctx->missing_info--;
    ctx->info->dwarf_section = GSIZE_TO_POINTER (details->vm_address);
    ctx->info->dwarf_section_length = details->size;
  }
  else if (strcmp ("__unwind_info", details->section_name) == 0)
  {
    ctx->missing_info--;
    ctx->info->compact_unwind_section = GSIZE_TO_POINTER (details->vm_address);
    ctx->info->compact_unwind_section_length = details->size;
  }

  return ctx->missing_info > 0;
}

#endif
