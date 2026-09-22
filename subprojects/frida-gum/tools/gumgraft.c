/*
 * Copyright (C) 2021-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include <gum/gum.h>

static gchar ** input_paths = NULL;
static gchar ** code_offsets = NULL;
static gboolean ingest_function_starts = FALSE;
static gboolean ingest_imports = FALSE;
static gboolean transform_lazy_binds = FALSE;

static GOptionEntry options[] =
{
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &input_paths,
      "Mach-O binary to instrument", "BINARY" },
  { "instrument", 'i', 0, G_OPTION_ARG_STRING_ARRAY, &code_offsets,
      "Include instrumentation for a specific code offset", "0x1234" },
  { "ingest-function-starts", 's', 0, G_OPTION_ARG_NONE, &ingest_function_starts,
      "Include instrumentation for offsets retrieved from LC_FUNCTION_STARTS",
      NULL },
  { "ingest-imports", 'm', 0, G_OPTION_ARG_NONE, &ingest_imports,
      "Include instrumentation for imports", NULL },
  { "transform-lazy-binds", 'z', 0, G_OPTION_ARG_NONE, &transform_lazy_binds,
      "Transform lazy binds into regular binds (experimental)", NULL },
  { NULL }
};

int
main (int argc,
      char * argv[])
{
  GOptionContext * context;
  const gchar * input_path;
  GumDarwinGrafterFlags flags;
  GumDarwinGrafter * grafter;
  GError * error;

  gum_init ();

  context = g_option_context_new ("- graft instrumentation into Mach-O binaries");
  g_option_context_add_main_entries (context, options, "gum-graft");
  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    g_printerr ("%s\n", error->message);
    return 1;
  }

  if (input_paths == NULL || g_strv_length (input_paths) != 1)
  {
    g_printerr ("Usage: %s <path/to/binary>\n", argv[0]);
    return 2;
  }
  input_path = input_paths[0];

  flags = GUM_DARWIN_GRAFTER_FLAGS_NONE;
  if (ingest_function_starts)
    flags |= GUM_DARWIN_GRAFTER_FLAGS_INGEST_FUNCTION_STARTS;
  if (ingest_imports)
    flags |= GUM_DARWIN_GRAFTER_FLAGS_INGEST_IMPORTS;
  if (transform_lazy_binds)
    flags |= GUM_DARWIN_GRAFTER_FLAGS_TRANSFORM_LAZY_BINDS;

  grafter = gum_darwin_grafter_new_from_file (input_path, flags);

  if (code_offsets != NULL)
  {
    gchar * const * cursor;

    for (cursor = code_offsets; *cursor != NULL; cursor++)
    {
      const gchar * raw_offset = *cursor;
      guint base;
      guint64 offset;

      if (g_str_has_prefix (raw_offset, "0x"))
      {
        raw_offset += 2;
        base = 16;
      }
      else
      {
        base = 10;
      }

      if (!g_ascii_string_to_unsigned (raw_offset, base, 4096, G_MAXUINT32,
          &offset, &error))
      {
        g_printerr ("%s\n", error->message);
        return 3;
      }

      if (offset % sizeof (guint32) != 0)
      {
        g_printerr ("%" G_GINT64_MODIFIER "x: Offset is not aligned on a "
            "4-byte boundary\n", offset);
        return 4;
      }

      gum_darwin_grafter_add (grafter, offset);
    }
  }

  error = NULL;
  gum_darwin_grafter_graft (grafter, &error);
  if (error != NULL)
  {
    if (g_error_matches (error, GUM_ERROR, GUM_ERROR_EXISTS))
    {
      g_print ("%s: Already grafted. Assuming it contains the desired "
          "instrumentation.\n", input_path);
      return 0;
    }

    g_printerr ("%s\n", error->message);
    return 5;
  }

  return 0;
}
