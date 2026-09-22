/*
 * Copyright (C) 2013-2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8file.h"

#include "gumv8macros.h"
#include "gumv8scope.h"

#include <errno.h>
#include <string.h>

#define GUMJS_MODULE_NAME File

using namespace v8;

struct GumFile
{
  Global<Object> * wrapper;
  FILE * handle;
  GumV8File * module;
};

GUMJS_DECLARE_FUNCTION (gumjs_file_read_all_bytes)
GUMJS_DECLARE_FUNCTION (gumjs_file_read_all_text)
GUMJS_DECLARE_FUNCTION (gumjs_file_write_all_bytes)
GUMJS_DECLARE_FUNCTION (gumjs_file_write_all_text)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_file_construct)
GUMJS_DECLARE_FUNCTION (gumjs_file_tell)
GUMJS_DECLARE_FUNCTION (gumjs_file_seek)
GUMJS_DECLARE_FUNCTION (gumjs_file_read_bytes)
GUMJS_DECLARE_FUNCTION (gumjs_file_read_text)
GUMJS_DECLARE_FUNCTION (gumjs_file_read_line)
GUMJS_DECLARE_FUNCTION (gumjs_file_write)
GUMJS_DECLARE_FUNCTION (gumjs_file_flush)
GUMJS_DECLARE_FUNCTION (gumjs_file_close)

static GumFile * gum_file_new (Local<Object> wrapper, FILE * handle,
    GumV8File * module);
static void gum_file_free (GumFile * file);
static gboolean gum_file_check_open (GumFile * self, Isolate * isolate);
static void gum_file_close (GumFile * self);
static gsize gum_file_query_num_bytes_available (GumFile * self);
static gboolean gum_file_set_contents (const gchar * filename,
    const gchar * contents, gssize length, GError ** error);
static void gum_file_on_weak_notify (const WeakCallbackInfo<GumFile> & info);

static const GumV8Function gumjs_file_module_functions[] =
{
  { "readAllBytes", gumjs_file_read_all_bytes },
  { "readAllText", gumjs_file_read_all_text },
  { "writeAllBytes", gumjs_file_write_all_bytes },
  { "writeAllText", gumjs_file_write_all_text },

  { NULL, NULL }
};

static const GumV8Function gumjs_file_functions[] =
{
  { "tell", gumjs_file_tell },
  { "seek", gumjs_file_seek },
  { "readBytes", gumjs_file_read_bytes },
  { "readText", gumjs_file_read_text },
  { "readLine", gumjs_file_read_line },
  { "write", gumjs_file_write },
  { "flush", gumjs_file_flush },
  { "close", gumjs_file_close },

  { NULL, NULL }
};

void
_gum_v8_file_init (GumV8File * self,
                   GumV8Core * core,
                   Local<ObjectTemplate> scope)
{
  auto isolate = core->isolate;

  self->core = core;

  auto module = External::New (isolate, self);

  auto file = _gum_v8_create_class ("File", gumjs_file_construct, scope,
      module, isolate);
  file->Set (_gum_v8_string_new_ascii (isolate, "SEEK_SET"),
      Integer::New (isolate, SEEK_SET), ReadOnly);
  file->Set (_gum_v8_string_new_ascii (isolate, "SEEK_CUR"),
      Integer::New (isolate, SEEK_CUR), ReadOnly);
  file->Set (_gum_v8_string_new_ascii (isolate, "SEEK_END"),
      Integer::New (isolate, SEEK_END), ReadOnly);
  _gum_v8_class_add_static (file, gumjs_file_module_functions, module, isolate);
  _gum_v8_class_add (file, gumjs_file_functions, module, isolate);
}

void
_gum_v8_file_realize (GumV8File * self)
{
  self->files = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_file_free);
}

void
_gum_v8_file_dispose (GumV8File * self)
{
  g_hash_table_unref (self->files);
  self->files = NULL;
}

void
_gum_v8_file_finalize (GumV8File * self)
{
}

GUMJS_DEFINE_FUNCTION (gumjs_file_read_all_bytes)
{
  gchar * filename;
  if (!_gum_v8_args_parse (args, "s", &filename))
    return;

  gchar * contents;
  gsize length;
  GError * error = NULL;
  gboolean success = g_file_get_contents (filename, &contents, &length, &error);

  g_free (filename);

  if (!success)
  {
    _gum_v8_throw_literal (isolate, error->message);
    g_error_free (error);
    return;
  }

  auto result = ArrayBuffer::New (isolate, length);
  auto store = result.As<ArrayBuffer> ()->GetBackingStore ();
  memcpy (store->Data (), contents, length);
  info.GetReturnValue ().Set (result);

  g_free (contents);
}

GUMJS_DEFINE_FUNCTION (gumjs_file_read_all_text)
{
  gchar * filename;
  if (!_gum_v8_args_parse (args, "s", &filename))
    return;

  gchar * contents;
  gsize length;
  GError * error = NULL;
  gboolean success = g_file_get_contents (filename, &contents, &length, &error);

  g_free (filename);

  if (!success)
  {
    _gum_v8_throw_literal (isolate, error->message);
    g_error_free (error);
    return;
  }

  const gchar * end;
  if (g_utf8_validate (contents, length, &end))
  {
    info.GetReturnValue ().Set (
        String::NewFromUtf8 (isolate, contents, NewStringType::kNormal, length)
        .ToLocalChecked ());
  }
  else
  {
    _gum_v8_throw (isolate, "can't decode byte 0x%02x in position %u",
        (guint8) *end, (guint) (end - contents));
  }

  g_free (contents);
}

GUMJS_DEFINE_FUNCTION (gumjs_file_write_all_bytes)
{
  gchar * filename;
  GBytes * bytes;
  if (!_gum_v8_args_parse (args, "sB", &filename, &bytes))
    return;

  gsize size;
  gconstpointer data = g_bytes_get_data (bytes, &size);

  GError * error = NULL;
  gboolean success = gum_file_set_contents (filename, (const gchar *) data,
      size, &error);

  g_bytes_unref (bytes);
  g_free (filename);

  if (!success)
  {
    _gum_v8_throw_literal (isolate, error->message);
    g_error_free (error);
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_file_write_all_text)
{
  gchar * filename, * text;
  if (!_gum_v8_args_parse (args, "ss", &filename, &text))
    return;

  GError * error = NULL;
  gboolean success = gum_file_set_contents (filename, text, -1, &error);

  g_free (text);
  g_free (filename);

  if (!success)
  {
    _gum_v8_throw_literal (isolate, error->message);
    g_error_free (error);
  }
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_file_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new File()` to create a new instance");
    return;
  }

  gchar * filename, * mode;
  if (!_gum_v8_args_parse (args, "ss", &filename, &mode))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  auto handle = fopen (filename, mode);

  g_free (filename);
  g_free (mode);

  if (handle == NULL)
  {
    _gum_v8_throw_literal (isolate, g_strerror (errno));
    return;
  }

  auto file = gum_file_new (wrapper, handle, module);
  wrapper->SetAlignedPointerInInternalField (0, file);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_tell, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  info.GetReturnValue ().Set ((double) ftell (self->handle));
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_seek, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  gssize offset;
  gint whence = SEEK_SET;
  if (!_gum_v8_args_parse (args, "z|i", &offset, &whence))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  int result = fseek (self->handle, offset, whence);
  if (result == -1)
  {
    _gum_v8_throw_literal (isolate, g_strerror (errno));
    return;
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_read_bytes, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  gsize n = G_MAXSIZE;
  if (!_gum_v8_args_parse (args, "|Z", &n))
    return;

  if (n == G_MAXSIZE)
    n = gum_file_query_num_bytes_available (self);

  if (n == 0)
  {
    info.GetReturnValue ().Set (ArrayBuffer::New (isolate, 0));
    return;
  }

  auto result = ArrayBuffer::New (isolate, n);
  auto store = result.As<ArrayBuffer> ()->GetBackingStore ();

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  size_t num_bytes_read = fread (store->Data (), 1, n, self->handle);
  if (num_bytes_read < n)
  {
    auto r = ArrayBuffer::New (isolate, num_bytes_read);
    auto s = r.As<ArrayBuffer> ()->GetBackingStore ();
    memcpy (s->Data (), store->Data (), num_bytes_read);
    result = r;
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_read_text, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  gsize n = G_MAXSIZE;
  if (!_gum_v8_args_parse (args, "|Z", &n))
    return;

  if (n == G_MAXSIZE)
    n = gum_file_query_num_bytes_available (self);

  if (n == 0)
  {
    info.GetReturnValue ().Set (ArrayBuffer::New (isolate, 0));
    return;
  }

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  gchar * data = (gchar *) g_malloc (n);
  size_t num_bytes_read = fread (data, 1, n, self->handle);

  const gchar * end;
  if (g_utf8_validate (data, num_bytes_read, &end))
  {
    info.GetReturnValue ().Set (
        String::NewFromUtf8 (isolate, data, NewStringType::kNormal,
          (int) num_bytes_read).ToLocalChecked ());
  }
  else
  {
    _gum_v8_throw (isolate, "can't decode byte 0x%02x in position %u",
        (guint8) *end, (guint) (end - data));

    fseek (self->handle, -((long) num_bytes_read), SEEK_CUR);
  }

  g_free (data);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_read_line, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  gsize offset = 0;
  gsize capacity = 256;
  GString * buffer = g_string_sized_new (capacity);
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  while (TRUE)
  {
    g_string_set_size (buffer, capacity);

    if (fgets (buffer->str + offset, capacity - offset, self->handle) == NULL)
      break;

    gsize num_bytes_read = strlen (buffer->str + offset);
    offset += num_bytes_read;

    if (buffer->str[offset - 1] == '\n')
      break;

    if (offset == capacity - 1)
      capacity += 256;
    else
      break;
  }
  g_string_set_size (buffer, offset);

  const gchar * end;
  if (g_utf8_validate (buffer->str, buffer->len, &end))
  {
    info.GetReturnValue ().Set (
        String::NewFromUtf8 (isolate, buffer->str, NewStringType::kNormal,
          buffer->len).ToLocalChecked ());
  }
  else
  {
    _gum_v8_throw (isolate, "can't decode byte 0x%02x in position %u",
        (guint8) *end, (guint) (end - buffer->str));

    fseek (self->handle, -((long) buffer->len), SEEK_CUR);
  }

  g_string_free (buffer, TRUE);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_write, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  GBytes * bytes;
  if (!_gum_v8_args_parse (args, "B~", &bytes))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  gsize size;
  auto data = g_bytes_get_data (bytes, &size);
  fwrite (data, size, 1, self->handle);

  g_bytes_unref (bytes);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_flush, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  fflush (self->handle);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_file_close, GumFile)
{
  if (!gum_file_check_open (self, isolate))
    return;

  gum_file_close (self);
}

static GumFile *
gum_file_new (Local<Object> wrapper,
              FILE * handle,
              GumV8File * module)
{
  auto file = g_slice_new (GumFile);
  file->wrapper = new Global<Object> (module->core->isolate, wrapper);
  file->wrapper->SetWeak (file, gum_file_on_weak_notify,
      WeakCallbackType::kParameter);
  file->handle = handle;
  file->module = module;

  g_hash_table_add (module->files, file);

  return file;
}

static void
gum_file_free (GumFile * self)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  gum_file_close (self);

  delete self->wrapper;

  g_slice_free (GumFile, self);
}

static gboolean
gum_file_check_open (GumFile * self,
                     Isolate * isolate)
{
  if (self->handle == NULL)
  {
    _gum_v8_throw_ascii_literal (isolate, "file is closed");
    return FALSE;
  }

  return TRUE;
}

static void
gum_file_close (GumFile * self)
{
  g_clear_pointer (&self->handle, fclose);
}

static gsize
gum_file_query_num_bytes_available (GumFile * self)
{
  FILE * handle = self->handle;
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  long offset = ftell (handle);

  fseek (handle, 0, SEEK_END);
  long size = ftell (handle);

  fseek (handle, offset, SEEK_SET);

  return size - offset;
}

static gboolean
gum_file_set_contents (const gchar * filename,
                       const gchar * contents,
                       gssize length,
                       GError ** error)
{
#if GLIB_CHECK_VERSION (2, 66, 0)
  return g_file_set_contents_full (filename, contents, length,
      G_FILE_SET_CONTENTS_NONE, 0666, error);
#else
  return g_file_set_contents (filename, contents, length, error);
#endif
}

static void
gum_file_on_weak_notify (const WeakCallbackInfo<GumFile> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->module->files, self);
}
