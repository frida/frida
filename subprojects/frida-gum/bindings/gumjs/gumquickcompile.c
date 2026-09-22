#include <gio/gio.h>
#include <quickjs.h>

typedef struct _GumESAsset GumESAsset;

struct _GumESAsset
{
  char * name;
  char * source;
  size_t size;
  GFile * output_file;
};

static GumESAsset * gum_es_asset_new_from_file (const char * name,
    GError ** error);
static char * gum_es_name_from_filesystem_path (const char * path);
static JSValue gum_compile_module (JSContext * ctx, const GumESAsset * asset);
static char * gum_normalize_module_name (JSContext * ctx,
    const char * base_name, const char * name, void * opaque);
static JSModuleDef * gum_load_module (JSContext * ctx, const char * module_name,
    void * opaque);

static int extra_write_flags = 0;
static const char * output_dir;
static const char * input_dir;
static GHashTable * es_assets;

int
main (int argc,
      char * argv[])
{
  int exit_code = 1;
  const char * entrypoint_name;
  GumESAsset * entrypoint = NULL;
  GError * error = NULL;
  int i;
  JSRuntime * rt = NULL;
  JSContext * ctx = NULL;
  JSValue val = JS_NULL;

#ifdef HAVE_FRIDA_GLIB
  glib_init ();
#endif

  if (argc >= 2 && strcmp (argv[1], "--bswap") == 0)
  {
    extra_write_flags = JS_WRITE_OBJ_BSWAP;

    argc--;
    argv[1] = argv[0];
    argv++;
  }

  if (argc < 4)
    goto bad_usage;

  output_dir = argv[1];
  input_dir = argv[2];
  entrypoint_name = argv[3];

  entrypoint = gum_es_asset_new_from_file (entrypoint_name, &error);
  if (entrypoint == NULL)
    goto propagate_error;

  es_assets = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 4; i != argc; i++)
  {
    GumESAsset * asset;

    asset = gum_es_asset_new_from_file (argv[i], &error);
    if (asset == NULL)
      goto propagate_error;

    g_hash_table_insert (es_assets, asset->name, asset);
  }

  rt = JS_NewRuntime ();
  JS_SetModuleLoaderFunc (rt, gum_normalize_module_name, gum_load_module, NULL);

  ctx = JS_NewContext (rt);

  val = gum_compile_module (ctx, entrypoint);
  if (JS_IsException (val))
    goto compilation_failed;

  exit_code = 0;
  goto beach;

bad_usage:
  {
    g_printerr ("Usage: %s [--bswap] <output_dir> <input_dir> <entrypoint.js> "
        "[<script1.js> <script2.js> ...]\n", argv[0]);
    goto beach;
  }
propagate_error:
  {
    g_printerr ("%s\n", error->message);
    goto beach;
  }
compilation_failed:
  {
    JSValue exception_val;
    const char * message;

    exception_val = JS_GetException (ctx);

    message = JS_ToCString (ctx, exception_val);

    fprintf (stderr, "%s: %s\n", entrypoint_name, message);

    JS_FreeCString (ctx, message);
    JS_FreeValue (ctx, exception_val);

    goto beach;
  }
beach:
  {
    if (!JS_IsNull (val))
      JS_FreeValue (ctx, val);

    g_clear_pointer (&ctx, JS_FreeContext);

    g_clear_pointer (&rt, JS_FreeRuntime);

    g_clear_error (&error);

    return exit_code;
  }
}

static GumESAsset *
gum_es_asset_new_from_file (const char * name,
                            GError ** error)
{
  GumESAsset * asset = NULL;
  char * path, * source;
  size_t size;
  char * stem, * outname;

  path = g_build_filename (input_dir, name, NULL);

  if (!g_file_get_contents (path, &source, &size, error))
    goto beach;

  stem = g_strdup (name);
  *strrchr (stem, '.') = '\0';

  outname = g_strconcat (strchr (stem, G_DIR_SEPARATOR) + 1, ".qjs", NULL);

  asset = g_slice_new (GumESAsset);
  asset->name = gum_es_name_from_filesystem_path (name);
  asset->source = source;
  asset->size = size;
  asset->output_file = g_file_new_build_filename (output_dir, outname, NULL);

  g_free (outname);
  g_free (stem);

beach:
  g_free (path);

  return asset;
}

static char *
gum_es_name_from_filesystem_path (const char * path)
{
  char * name;
  char dir_sep[2] = { G_DIR_SEPARATOR, '\0' };
  char ** parts, * rel_name;

  parts = g_strsplit (path, dir_sep, 0);
  g_free (parts[0]);
  parts[0] = g_strdup ("frida/runtime");

  rel_name = g_strjoinv ("/", parts);
  name = g_strconcat ("/", rel_name, NULL);
  g_free (rel_name);

  g_strfreev (parts);

  return name;
}

static JSValue
gum_compile_module (JSContext * ctx,
                    const GumESAsset * asset)
{
  JSValue val;
  uint8_t * code;
  size_t size;
  GFile * parent_dir;
  char * output_path;
  GError * error = NULL;

  val = JS_Eval (ctx, asset->source, asset->size, asset->name,
      JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY);
  if (JS_IsException (val))
    goto beach;

  code = JS_WriteObject (ctx, &size, val,
      JS_WRITE_OBJ_BYTECODE | extra_write_flags);

  parent_dir = g_file_get_parent (asset->output_file);
  g_file_make_directory_with_parents (parent_dir, NULL, NULL);
  g_object_unref (parent_dir);

  output_path = g_file_get_path (asset->output_file);

  if (!g_file_set_contents (output_path, (char *) code, size, &error))
  {
    g_printerr ("%s\n", error->message);
    exit (1);
  }

  g_free (output_path);

  js_free (ctx, code);

beach:
  return val;
}

static char *
gum_normalize_module_name (JSContext * ctx,
                           const char * base_name,
                           const char * name,
                           void * opaque)
{
  char * result;
  const char * base_dir_end;
  guint base_dir_length;
  const char * cursor;

  if (name[0] != '.')
  {
    GumESAsset * asset;

    asset = g_hash_table_lookup (es_assets, name);
    if (asset != NULL)
      return js_strdup (ctx, asset->name);

    return js_strdup (ctx, name);
  }

  /* The following mimics QuickJS' default implementation: */

  base_dir_end = strrchr (base_name, '/');
  if (base_dir_end != NULL)
    base_dir_length = base_dir_end - base_name;
  else
    base_dir_length = 0;

  result = js_malloc (ctx, base_dir_length + 1 + strlen (name) + 1);
  memcpy (result, base_name, base_dir_length);
  result[base_dir_length] = '\0';

  cursor = name;
  while (TRUE)
  {
    if (g_str_has_prefix (cursor, "./"))
    {
      cursor += 2;
    }
    else if (g_str_has_prefix (cursor, "../"))
    {
      char * new_end;

      if (result[0] == '\0')
        break;

      new_end = strrchr (result, '/');
      if (new_end != NULL)
        new_end++;
      else
        new_end = result;

      if (strcmp (new_end, ".") == 0 || strcmp (new_end, "..") == 0)
        break;

      if (new_end > result)
        new_end--;

      *new_end = '\0';

      cursor += 3;
    }
    else
    {
      break;
    }
  }

  strcat (result, "/");
  strcat (result, cursor);

  return result;
}

static JSModuleDef *
gum_load_module (JSContext * ctx,
                 const char * module_name,
                 void * opaque)
{
  GumESAsset * asset;
  JSValue val;

  asset = g_hash_table_lookup (es_assets, module_name);
  if (asset == NULL)
    goto not_found;

  val = gum_compile_module (ctx, asset);
  if (JS_IsException (val))
    goto malformed_module;

  JS_FreeValue (ctx, val);

  return JS_VALUE_GET_PTR (val);

not_found:
malformed_module:
  return NULL;
}
