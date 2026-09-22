/*
 * Copyright (C) 2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2026 Sam Sun <samsun@nvidia.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummoduleregistry.h"

#include "gumelfmodule.h"
#include "guminterceptor.h"
#include "testutil.h"

#ifdef HAVE_LINUX
# include "interceptor-callbacklistener.h"
# include <dlfcn.h>
# include <link.h>
#endif

#define TESTCASE(NAME) \
    void test_module_registry_ ## NAME (void)
#define TESTENTRY(NAME) \
    TESTENTRY_SIMPLE ("Core/ModuleRegistry", test_module_registry, NAME)

TESTLIST_BEGIN (module_registry)
  TESTENTRY (module_registry_should_emit_signal_on_add)
  TESTENTRY (hooks_should_be_discarded_when_module_unloads)
  TESTENTRY (relocated_program_headers_can_be_parsed)
  TESTENTRY (online_elf_should_allow_padded_inline_program_headers)
  TESTENTRY (online_elf_should_bound_inline_program_headers)
TESTLIST_END ()

static void on_module_added (GumModuleRegistry * registry, GumModule * module,
    gpointer user_data);
static void on_module_removed (GumModuleRegistry * registry, GumModule * module,
    gpointer user_data);

#ifdef HAVE_LINUX

# define GUM_TARGET_MODULE_FILENAME "module-registry-target.so"

typedef struct _TestModuleHooks TestModuleHooks;

struct _TestModuleHooks
{
  gboolean seen_add;
  gboolean seen_remove;
};

static void on_target_module_added (GumModuleRegistry * registry,
    GumModule * module, gpointer user_data);
static void on_target_module_removed (GumModuleRegistry * registry,
    GumModule * module, gpointer user_data);
static gboolean is_target_module (GumModule * module);

#endif

#if defined (HAVE_LINUX) && !defined (HAVE_ANDROID) && \
    defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
# define GUM_HAVE_RELOCATED_PHDR_TARGET 1
#endif

#ifdef HAVE_LINUX
# if GLIB_SIZEOF_VOID_P == 8
#  define GUM_TEST_ELF_CLASS ELFCLASS64
# else
#  define GUM_TEST_ELF_CLASS ELFCLASS32
# endif
# if G_BYTE_ORDER == G_LITTLE_ENDIAN
#  define GUM_TEST_ELF_ENCODING ELFDATA2LSB
# else
#  define GUM_TEST_ELF_ENCODING ELFDATA2MSB
# endif
# define GUM_TEST_UNOPENABLE_PATH "/nonexistent/gum-test-image.so"
#endif

#ifdef GUM_HAVE_RELOCATED_PHDR_TARGET

# define GUM_RELOCATED_PHDR_TARGET_FILENAME \
    "module-registry-target-relocated-phdr-linux-x86_64.so"
# define GUM_TARGET_MODULE_EXPORT "gum_module_registry_target_function"

typedef struct _TestRelocatedPhdrContext TestRelocatedPhdrContext;

struct _TestRelocatedPhdrContext
{
  gboolean seen;
  GumAddress export_address;
};

static void on_relocated_phdr_module_added (GumModuleRegistry * registry,
    GumModule * module, gpointer user_data);
static gboolean on_relocated_phdr_export_found (
    const GumExportDetails * details, gpointer user_data);
static void assert_relocated_phdr_layout (const gchar * path);

#endif

TESTCASE (module_registry_should_emit_signal_on_add)
{
  GumModuleRegistry * registry;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  registry = gum_module_registry_obtain ();
  g_signal_connect (registry, "module-added", G_CALLBACK (on_module_added),
      NULL);
  g_signal_connect (registry, "module-removed", G_CALLBACK (on_module_removed),
      NULL);
  g_printerr ("Sleeping in PID %u...\n", gum_process_get_id ());
  while (TRUE)
    g_usleep (60 * G_USEC_PER_SEC);
}

TESTCASE (hooks_should_be_discarded_when_module_unloads)
{
#ifdef HAVE_LINUX
  GumModuleRegistry * registry;
  GumInterceptor * interceptor;
  TestCallbackListener * listener;
  TestModuleHooks hooks = { 0, };
  gulong added_handler, removed_handler;
  gchar * data_dir, * target_path;
  void * handle;
  gpointer target;
  GumAttachReturn result;

  registry = gum_module_registry_obtain ();
  interceptor = gum_interceptor_obtain ();
  listener = test_callback_listener_new ();

  added_handler = g_signal_connect (registry, "module-added",
      G_CALLBACK (on_target_module_added), &hooks);
  removed_handler = g_signal_connect (registry, "module-removed",
      G_CALLBACK (on_target_module_removed), &hooks);

  data_dir = test_util_get_data_dir ();
  target_path = g_build_filename (data_dir, GUM_TARGET_MODULE_FILENAME, NULL);

  handle = dlopen (target_path, RTLD_NOW | RTLD_LOCAL);
  g_assert_nonnull (handle);
  g_assert_true (hooks.seen_add);

  target = dlsym (handle, "gum_module_registry_target_function");
  g_assert_nonnull (target);

  result = gum_interceptor_attach (interceptor, target,
      GUM_INVOCATION_LISTENER (listener), NULL);
  g_assert_cmpint (result, ==, GUM_ATTACH_OK);

  dlclose (handle);

  /*
   * If the C library unloaded the module (glibc does; musl keeps it resident),
   * its live hook must have been discarded without restoring the now-unmapped
   * prologue, so detaching afterwards has to be a safe no-op.
   */
  gum_interceptor_detach (interceptor, GUM_INVOCATION_LISTENER (listener));

  if (!hooks.seen_remove)
    g_test_skip ("dlclose did not unload the module on this platform");

  g_signal_handler_disconnect (registry, added_handler);
  g_signal_handler_disconnect (registry, removed_handler);
  g_object_unref (listener);

  g_free (target_path);
  g_free (data_dir);
#else
  g_test_skip ("only supported on Linux");
#endif
}

TESTCASE (relocated_program_headers_can_be_parsed)
{
#ifdef GUM_HAVE_RELOCATED_PHDR_TARGET
  GumModuleRegistry * registry;
  TestRelocatedPhdrContext ctx = { 0, };
  gulong handler;
  gchar * data_dir, * target_path;
  void * handle, * expected_export;

  data_dir = test_util_get_data_dir ();
  target_path = g_build_filename (data_dir,
      GUM_RELOCATED_PHDR_TARGET_FILENAME, NULL);
  assert_relocated_phdr_layout (target_path);

  registry = gum_module_registry_obtain ();
  handler = g_signal_connect (registry, "module-added",
      G_CALLBACK (on_relocated_phdr_module_added), &ctx);

  handle = dlopen (target_path, RTLD_NOW | RTLD_LOCAL);
  g_assert_nonnull (handle);

  g_signal_handler_disconnect (registry, handler);

  g_assert_true (ctx.seen);
  expected_export = dlsym (handle, GUM_TARGET_MODULE_EXPORT);
  g_assert_nonnull (expected_export);
  g_assert_cmphex (ctx.export_address, ==, GUM_ADDRESS (expected_export));

  dlclose (handle);
  g_free (target_path);
  g_free (data_dir);
#else
  g_test_skip ("only supported on Linux/x86-64");
#endif
}

TESTCASE (online_elf_should_allow_padded_inline_program_headers)
{
#ifdef HAVE_LINUX
  guint page_size;
  guint8 * page;
  ElfW(Ehdr) * ehdr;
  ElfW(Phdr) * phdr;
  GumElfModule * module;
  GError * error = NULL;

  page_size = gum_query_page_size ();
  page = gum_memory_allocate (NULL, page_size, page_size, GUM_PAGE_RW);
  g_assert_nonnull (page);

  memset (page, 0, page_size);
  ehdr = (ElfW(Ehdr) *) page;
  memcpy (ehdr->e_ident, ELFMAG, SELFMAG);
  ehdr->e_ident[EI_CLASS] = GUM_TEST_ELF_CLASS;
  ehdr->e_ident[EI_DATA] = GUM_TEST_ELF_ENCODING;
  ehdr->e_ident[EI_VERSION] = EV_CURRENT;
  ehdr->e_type = ET_DYN;
  ehdr->e_version = EV_CURRENT;
  ehdr->e_ehsize = sizeof (ElfW(Ehdr));
  ehdr->e_phoff = ehdr->e_ehsize + 16;
  ehdr->e_phentsize = sizeof (ElfW(Phdr));
  ehdr->e_phnum = 1;

  phdr = (ElfW(Phdr) *) (page + ehdr->e_phoff);
  phdr->p_type = PT_LOAD;
  phdr->p_flags = PF_R | PF_W;
  phdr->p_filesz = page_size;
  phdr->p_memsz = page_size;
  phdr->p_align = page_size;

  g_assert_cmpuint (ehdr->e_phoff, !=, ehdr->e_ehsize);
  g_assert_cmpuint (ehdr->e_phoff + ehdr->e_phentsize, <=, page_size);

  module = gum_elf_module_new_from_memory (GUM_TEST_UNOPENABLE_PATH,
      GUM_ADDRESS (page), &error);
  g_assert_no_error (error);
  g_assert_nonnull (module);
  g_assert_cmpuint (gum_elf_module_get_mapped_size (module), ==, page_size);

  g_object_unref (module);
  gum_memory_free (page, page_size);
#else
  g_test_skip ("only supported on Linux");
#endif
}

TESTCASE (online_elf_should_bound_inline_program_headers)
{
#ifdef HAVE_LINUX
  if (g_test_subprocess ())
  {
    guint page_size;
    guint8 * pages;
    ElfW(Ehdr) * ehdr;
    GumElfModule * module;
    GError * error = NULL;

    page_size = gum_query_page_size ();
    pages = gum_memory_allocate (NULL, 2 * page_size, page_size,
        GUM_PAGE_RW);
    g_assert_nonnull (pages);
    gum_mprotect (pages + page_size, page_size, GUM_PAGE_NO_ACCESS);

    memset (pages, 0, page_size);
    ehdr = (ElfW(Ehdr) *) pages;
    memcpy (ehdr->e_ident, ELFMAG, SELFMAG);
    ehdr->e_ident[EI_CLASS] = GUM_TEST_ELF_CLASS;
    ehdr->e_ident[EI_DATA] = GUM_TEST_ELF_ENCODING;
    ehdr->e_ident[EI_VERSION] = EV_CURRENT;
    ehdr->e_type = ET_DYN;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_ehsize = sizeof (ElfW(Ehdr));
    ehdr->e_phoff = ehdr->e_ehsize;
    ehdr->e_phentsize = (guint16) (page_size - ehdr->e_ehsize);
    ehdr->e_phnum = 2;
    g_assert_cmpuint (ehdr->e_phoff, ==, ehdr->e_ehsize);
    g_assert_cmpuint (ehdr->e_phoff + ehdr->e_phentsize, ==, page_size);

    module = gum_elf_module_new_from_memory (GUM_TEST_UNOPENABLE_PATH,
        GUM_ADDRESS (pages), &error);
    g_assert_null (module);
    g_assert_error (error, GUM_ERROR, GUM_ERROR_INVALID_ARGUMENT);

    g_clear_error (&error);
    gum_memory_free (pages, 2 * page_size);
  }
  else
  {
    g_test_trap_subprocess (NULL, 0, (GTestSubprocessFlags) 0);
    g_test_trap_assert_passed ();
  }
#else
  g_test_skip ("only supported on Linux");
#endif
}

static void
on_module_added (GumModuleRegistry * registry,
                 GumModule * module,
                 gpointer user_data)
{
  g_printerr ("%s: path=\"%s\"\n", G_STRFUNC, gum_module_get_path (module));
}

static void
on_module_removed (GumModuleRegistry * registry,
                   GumModule * module,
                   gpointer user_data)
{
  g_printerr ("%s: path=\"%s\"\n", G_STRFUNC, gum_module_get_path (module));
}

#ifdef HAVE_LINUX

static void
on_target_module_added (GumModuleRegistry * registry,
                        GumModule * module,
                        gpointer user_data)
{
  TestModuleHooks * hooks = user_data;

  if (is_target_module (module))
    hooks->seen_add = TRUE;
}

static void
on_target_module_removed (GumModuleRegistry * registry,
                          GumModule * module,
                          gpointer user_data)
{
  TestModuleHooks * hooks = user_data;

  if (is_target_module (module))
    hooks->seen_remove = TRUE;
}

static gboolean
is_target_module (GumModule * module)
{
  return g_str_has_suffix (gum_module_get_path (module),
      G_DIR_SEPARATOR_S GUM_TARGET_MODULE_FILENAME);
}

#endif

#ifdef GUM_HAVE_RELOCATED_PHDR_TARGET

static void
on_relocated_phdr_module_added (GumModuleRegistry * registry,
                                GumModule * module,
                                gpointer user_data)
{
  TestRelocatedPhdrContext * ctx = user_data;

  if (!g_str_has_suffix (gum_module_get_path (module),
      G_DIR_SEPARATOR_S GUM_RELOCATED_PHDR_TARGET_FILENAME))
    return;

  ctx->seen = TRUE;
  gum_module_enumerate_exports (module, on_relocated_phdr_export_found, ctx);
}

static gboolean
on_relocated_phdr_export_found (const GumExportDetails * details,
                                gpointer user_data)
{
  TestRelocatedPhdrContext * ctx = user_data;

  if (g_strcmp0 (details->name, GUM_TARGET_MODULE_EXPORT) != 0)
    return TRUE;

  ctx->export_address = details->address;

  return FALSE;
}

static void
assert_relocated_phdr_layout (const gchar * path)
{
  gchar * data;
  gsize size;
  const ElfW(Ehdr) * ehdr;
  const ElfW(Phdr) * phdrs, * pt_phdr;
  guint i;

  pt_phdr = NULL;

  g_assert_true (g_file_get_contents (path, &data, &size, NULL));
  g_assert_cmpuint (size, >=, sizeof (ElfW(Ehdr)));

  ehdr = (const ElfW(Ehdr) *) data;
  g_assert_cmpmem (ehdr->e_ident, SELFMAG, ELFMAG, SELFMAG);
  g_assert_cmpuint (ehdr->e_phentsize, ==, sizeof (ElfW(Phdr)));
  g_assert_cmpuint (ehdr->e_phoff +
      ((gsize) ehdr->e_phnum * ehdr->e_phentsize), <=, size);

  phdrs = (const ElfW(Phdr) *) (data + ehdr->e_phoff);
  for (i = 0; i != ehdr->e_phnum; i++)
  {
    if (phdrs[i].p_type == PT_PHDR)
    {
      pt_phdr = &phdrs[i];
      break;
    }
  }

  g_assert_nonnull (pt_phdr);
  g_assert_cmphex (pt_phdr->p_offset, ==, ehdr->e_phoff);
  g_assert_cmphex (pt_phdr->p_vaddr, !=, ehdr->e_phoff);

  g_free (data);
}

#endif
