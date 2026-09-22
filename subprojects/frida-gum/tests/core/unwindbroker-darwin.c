/*
 * Copyright (C) 2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gum/gumdarwin.h"
#include "gum/gumdarwinmapper.h"
#include "gum/gummemory.h"
#include "gum/gumunwindbroker.h"
#include "testutil.h"

#include <mach/mach.h>
#include <unwind.h>

#define GUM_TEST_MH_MAGIC_64 0xfeedfacf

#define TESTCASE(NAME) \
    void test_unwind_broker_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/UnwindBroker", test_unwind_broker, NAME)

TESTLIST_BEGIN (unwind_broker)
  TESTENTRY (can_unwind_through_manually_mapped_dylib)
TESTLIST_END ()

typedef unsigned int (* GumTestMappedInvokeFunc) (
    void (* callback) (gpointer user_data), gpointer user_data);

typedef struct _GumTestUnwindProbe GumTestUnwindProbe;
typedef struct _GumTestDyldUnwindSections GumTestDyldUnwindSections;
typedef struct _GumTestFillContext GumTestFillContext;

struct _GumTestUnwindProbe
{
  GumMemoryRange mapped_range;
  gboolean saw_mapped_frame;
  guint frames_after_mapped;
};

struct _GumTestDyldUnwindSections
{
  const void * mh;
  const void * dwarf_section;
  uintptr_t dwarf_section_length;
  const void * compact_unwind_section;
  uintptr_t compact_unwind_section_length;
};

struct _GumTestFillContext
{
  GumTestDyldUnwindSections * sections;
  guint remaining;
};

#define GUM_TEST_TYPE_SECTIONS_PROVIDER \
    (gum_test_sections_provider_get_type ())
G_DECLARE_FINAL_TYPE (GumTestSectionsProvider, gum_test_sections_provider,
    GUM_TEST, SECTIONS_PROVIDER, GObject)

struct _GumTestSectionsProvider
{
  GObject parent;

  GumMemoryRange range;
  GumTestDyldUnwindSections sections;
};

static GumTestSectionsProvider * gum_test_sections_provider_new (
    const GumMemoryRange * range);
static void gum_test_sections_provider_iface_init (gpointer g_iface,
    gpointer iface_data);
static const GumMemoryRange * gum_test_sections_provider_get_range (
    GumUnwindSectionsProvider * provider);
static gboolean gum_test_sections_provider_fill (
    GumUnwindSectionsProvider * provider, GumAddress address, gpointer info);
static gboolean gum_test_collect_section (
    const GumDarwinSectionDetails * details, GumTestFillContext * ctx);

static void gum_test_backtrace_from_mapped_frame (gpointer user_data);
static _Unwind_Reason_Code gum_test_examine_frame (
    struct _Unwind_Context * context, gpointer user_data);

G_DEFINE_TYPE_EXTENDED (GumTestSectionsProvider, gum_test_sections_provider,
    G_TYPE_OBJECT, 0,
    G_IMPLEMENT_INTERFACE (GUM_TYPE_UNWIND_SECTIONS_PROVIDER,
        gum_test_sections_provider_iface_init))

TESTCASE (can_unwind_through_manually_mapped_dylib)
{
  GError * error = NULL;
  GumDarwinModuleResolver * resolver;
  GumDarwinMapper * mapper;
  gsize size;
  mach_vm_address_t base = 0;
  kern_return_t kr;
  GumMemoryRange range;
  GumTestSectionsProvider * provider;
  GumUnwindBroker * broker;
  void (* constructor) (void);
  GumTestMappedInvokeFunc invoke;
  GumTestUnwindProbe probe;

  resolver = gum_darwin_module_resolver_new (mach_task_self (), &error);
  g_assert_no_error (error);

  mapper = gum_darwin_mapper_new_from_file (GUM_TESTS_MAPPER_UNWIND_TARGET,
      resolver, &error);
  g_assert_no_error (error);

  size = gum_darwin_mapper_size (mapper);
  kr = mach_vm_allocate (mach_task_self (), &base, size, VM_FLAGS_ANYWHERE);
  g_assert_cmpint (kr, ==, KERN_SUCCESS);

  gum_darwin_mapper_map (mapper, base, &error);
  g_assert_no_error (error);

  range.base_address = base;
  range.size = size;

  provider = gum_test_sections_provider_new (&range);
  broker = gum_unwind_broker_obtain ();
  gum_unwind_broker_add_sections_provider (broker,
      GUM_UNWIND_SECTIONS_PROVIDER (provider));

  constructor =
      GSIZE_TO_POINTER (gum_darwin_mapper_constructor (mapper));
  constructor ();

  invoke = GSIZE_TO_POINTER (
      gum_darwin_mapper_resolve (mapper, "gum_test_mapped_invoke"));
  g_assert_nonnull (invoke);

  probe.mapped_range = range;
  probe.saw_mapped_frame = FALSE;
  probe.frames_after_mapped = 0;
  invoke (gum_test_backtrace_from_mapped_frame, &probe);

  g_assert_true (probe.saw_mapped_frame);
  g_assert_cmpuint (probe.frames_after_mapped, >, 0);

  gum_unwind_broker_remove_sections_provider (broker,
      GUM_UNWIND_SECTIONS_PROVIDER (provider));
  g_object_unref (broker);
  g_object_unref (provider);

  mach_vm_deallocate (mach_task_self (), base, size);
  g_object_unref (mapper);
  g_object_unref (resolver);
}

static void
gum_test_backtrace_from_mapped_frame (gpointer user_data)
{
  GumTestUnwindProbe * probe = user_data;

  _Unwind_Backtrace (gum_test_examine_frame, probe);
}

static _Unwind_Reason_Code
gum_test_examine_frame (struct _Unwind_Context * context,
                        gpointer user_data)
{
  GumTestUnwindProbe * probe = user_data;
  GumAddress ip;

  ip = gum_strip_code_address (GUM_ADDRESS (_Unwind_GetIP (context)));

  if (probe->saw_mapped_frame)
  {
    probe->frames_after_mapped++;
  }
  else if (ip >= probe->mapped_range.base_address &&
      ip < probe->mapped_range.base_address + probe->mapped_range.size)
  {
    probe->saw_mapped_frame = TRUE;
  }

  return _URC_NO_REASON;
}

static GumTestSectionsProvider *
gum_test_sections_provider_new (const GumMemoryRange * range)
{
  GumTestSectionsProvider * provider;
  gsize page_size;
  guint8 * header, * end;
  GumDarwinModule * module;
  GumTestFillContext ctx;

  provider = g_object_new (GUM_TEST_TYPE_SECTIONS_PROVIDER, NULL);
  provider->range = *range;

  page_size = gum_query_page_size ();
  header = GSIZE_TO_POINTER (range->base_address);
  end = GSIZE_TO_POINTER (range->base_address + range->size);
  while (header + sizeof (guint32) <= end &&
      *(guint32 *) header != GUM_TEST_MH_MAGIC_64)
  {
    header += page_size;
  }
  g_assert_cmphex (*(guint32 *) header, ==, GUM_TEST_MH_MAGIC_64);

  provider->sections.mh = header;

  module = gum_darwin_module_new_from_memory ("mapped", mach_task_self (),
      GPOINTER_TO_SIZE (header), GUM_DARWIN_MODULE_FLAGS_NONE, NULL);
  g_assert_nonnull (module);

  ctx.sections = &provider->sections;
  ctx.remaining = 2;
  gum_darwin_module_enumerate_sections (module,
      (GumFoundDarwinSectionFunc) gum_test_collect_section, &ctx);

  g_object_unref (module);

  return provider;
}

static gboolean
gum_test_collect_section (const GumDarwinSectionDetails * details,
                          GumTestFillContext * ctx)
{
  if (strcmp (details->segment_name, "__TEXT") != 0)
    return TRUE;

  if (strcmp (details->section_name, "__eh_frame") == 0)
  {
    ctx->sections->dwarf_section = GSIZE_TO_POINTER (details->vm_address);
    ctx->sections->dwarf_section_length = details->size;
    ctx->remaining--;
  }
  else if (strcmp (details->section_name, "__unwind_info") == 0)
  {
    ctx->sections->compact_unwind_section =
        GSIZE_TO_POINTER (details->vm_address);
    ctx->sections->compact_unwind_section_length = details->size;
    ctx->remaining--;
  }

  return ctx->remaining > 0;
}

static void
gum_test_sections_provider_iface_init (gpointer g_iface,
                                       gpointer iface_data)
{
  GumUnwindSectionsProviderInterface * iface = g_iface;

  iface->get_range = gum_test_sections_provider_get_range;
  iface->fill = gum_test_sections_provider_fill;
}

static const GumMemoryRange *
gum_test_sections_provider_get_range (GumUnwindSectionsProvider * provider)
{
  return &GUM_TEST_SECTIONS_PROVIDER (provider)->range;
}

static gboolean
gum_test_sections_provider_fill (GumUnwindSectionsProvider * provider,
                                 GumAddress address,
                                 gpointer info)
{
  GumTestSectionsProvider * self = GUM_TEST_SECTIONS_PROVIDER (provider);

  memcpy (info, &self->sections, sizeof (self->sections));

  return TRUE;
}

static void
gum_test_sections_provider_class_init (GumTestSectionsProviderClass * klass)
{
}

static void
gum_test_sections_provider_init (GumTestSectionsProvider * self)
{
}
