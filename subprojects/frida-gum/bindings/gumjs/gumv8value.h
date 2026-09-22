/*
 * Copyright (C) 2016-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_VALUE_H__
#define __GUM_V8_VALUE_H__

#include "gumv8core.h"

struct GumV8Args
{
  const v8::FunctionCallbackInfo<v8::Value> * info;
  GumV8Core * core;
};

struct GumV8Property
{
  const gchar * name;
  v8::AccessorNameGetterCallback getter;
  v8::AccessorNameSetterCallback setter;
};

struct GumV8Function
{
  const gchar * name;
  v8::FunctionCallback callback;
};

G_GNUC_INTERNAL gboolean _gum_v8_args_parse (const GumV8Args * args,
    const gchar * format, ...);

G_GNUC_INTERNAL v8::Local<v8::String> _gum_v8_string_new_ascii (
    v8::Isolate * isolate, const gchar * str);

G_GNUC_INTERNAL v8::Local<v8::ArrayBuffer> _gum_v8_array_buffer_new_take (
    v8::Isolate * isolate, gpointer data, gsize size);

G_GNUC_INTERNAL GBytes * _gum_v8_bytes_get (v8::Local<v8::Value> value,
    GumV8Core * core);
G_GNUC_INTERNAL GBytes * _gum_v8_bytes_parse (v8::Local<v8::Value> value,
    GumV8Core * core);
G_GNUC_INTERNAL GBytes * _gum_v8_bytes_try_get (v8::Local<v8::Value> value,
    GumV8Core * core);

G_GNUC_INTERNAL GumV8NativeResource * _gum_v8_native_resource_new (
    gpointer data, gsize size, GDestroyNotify notify, GumV8Core * core);
G_GNUC_INTERNAL GumV8NativeResource * _gum_v8_native_resource_new_from_pages (
    gpointer data, gsize size, GumV8Core * core);
G_GNUC_INTERNAL void _gum_v8_native_resource_free (GumV8NativeResource * block);

G_GNUC_INTERNAL GumV8KernelResource * _gum_v8_kernel_resource_new (
    GumAddress data, gsize size, GumV8KernelDestroyNotify notify,
    GumV8Core * core);
G_GNUC_INTERNAL void _gum_v8_kernel_resource_free (GumV8KernelResource * block);

G_GNUC_INTERNAL gboolean _gum_v8_int_get (v8::Local<v8::Value> value, gint * i,
    GumV8Core * core);

G_GNUC_INTERNAL gboolean _gum_v8_uint_get (v8::Local<v8::Value> value,
    guint * u, GumV8Core * core);

G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_int64_new (gint64 value,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_int64_get (v8::Local<v8::Value> value,
    gint64 * i, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_int64_parse (v8::Local<v8::Value> value,
    gint64 * i, GumV8Core * core);
G_GNUC_INTERNAL gint64 _gum_v8_int64_get_value (v8::Local<v8::Object> object);
G_GNUC_INTERNAL void _gum_v8_int64_set_value (v8::Local<v8::Object> object,
    gint64 value, v8::Isolate * isolate);

G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_uint64_new (guint64 value,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_uint64_get (v8::Local<v8::Value> value,
    guint64 * u, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_uint64_parse (v8::Local<v8::Value> value,
    guint64 * u, GumV8Core * core);
G_GNUC_INTERNAL guint64 _gum_v8_uint64_get_value (
    v8::Local<v8::Object> object);
G_GNUC_INTERNAL void _gum_v8_uint64_set_value (v8::Local<v8::Object> object,
    guint64 value, v8::Isolate * isolate);

G_GNUC_INTERNAL gboolean _gum_v8_size_get (v8::Local<v8::Value> value,
    gsize * size, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_ssize_get (v8::Local<v8::Value> value,
    gssize * size, GumV8Core * core);

G_GNUC_INTERNAL v8::Local<v8::String> _gum_v8_enum_new (v8::Isolate * isolate,
    gint value, GType type);
G_GNUC_INTERNAL gboolean _gum_v8_enum_get (v8::Local<v8::Value> value,
    GType type, gint * result, GumV8Core * core);

G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_native_pointer_new (
    gpointer address, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_native_pointer_get (
    v8::Local<v8::Value> value, gpointer * ptr, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_native_pointer_parse (
    v8::Local<v8::Value> value, gpointer * ptr, GumV8Core * core);

G_GNUC_INTERNAL void _gum_v8_throw (v8::Isolate * isolate, const gchar * format,
    ...);
G_GNUC_INTERNAL void _gum_v8_throw_literal (v8::Isolate * isolate,
    const gchar * message);
G_GNUC_INTERNAL void _gum_v8_throw_ascii (v8::Isolate * isolate,
    const gchar * format, ...);
G_GNUC_INTERNAL void _gum_v8_throw_ascii_literal (v8::Isolate * isolate,
    const gchar * message);
G_GNUC_INTERNAL void _gum_v8_throw_native (GumExceptionDetails * details,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_maybe_throw (v8::Isolate * isolate,
    GError ** error);

G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_cpu_context_new_immutable (
    const GumCpuContext * cpu_context, GumV8Core * core);
G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_cpu_context_new_mutable (
    GumCpuContext * cpu_context, GumV8Core * core);
G_GNUC_INTERNAL void _gum_v8_cpu_context_free_later (
    v8::Global<v8::Object> * cpu_context, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_cpu_context_get (
    v8::Local<v8::Value> value, GumCpuContext ** context, GumV8Core * core);

G_GNUC_INTERNAL void _gum_v8_parse_exception_details (
    GumExceptionDetails * details, v8::Local<v8::Object> & exception,
    v8::Local<v8::Object> & cpu_context, GumV8Core * core);

G_GNUC_INTERNAL v8::Local<v8::Value> _gum_v8_error_new_take_error (
    v8::Isolate * isolate, GError ** error);
G_GNUC_INTERNAL gchar * _gum_v8_error_get_message (v8::Isolate * isolate,
    v8::Local<v8::Value> error);

G_GNUC_INTERNAL const gchar * _gum_v8_thread_state_to_string (
    GumThreadState state);
G_GNUC_INTERNAL const gchar * _gum_v8_memory_operation_to_string (
    GumMemoryOperation operation);

G_GNUC_INTERNAL gboolean _gum_v8_object_set (v8::Local<v8::Object> object,
    const gchar * key, v8::Local<v8::Value> value, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_int (v8::Local<v8::Object> object,
    const gchar * key, gint value, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_uint (v8::Local<v8::Object> object,
    const gchar * key, guint value, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_pointer (
    v8::Local<v8::Object> object, const gchar * key, gpointer value,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_pointer (
    v8::Local<v8::Object> object, const gchar * key, GumAddress value,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_uint64 (
    v8::Local<v8::Object> object, const gchar * key, GumAddress value,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_enum (v8::Local<v8::Object> object,
    const gchar * key, gint value, GType type, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_ascii (
    v8::Local<v8::Object> object, const gchar * key, const gchar * value,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_utf8 (v8::Local<v8::Object> object,
    const gchar * key, const gchar * value, GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_object_set_page_protection (
    v8::Local<v8::Object> object, const gchar * key, GumPageProtection prot,
    GumV8Core * core);

G_GNUC_INTERNAL v8::Local<v8::Object> _gum_v8_range_details_new (
    const GumRangeDetails * details, GumV8Core * core);

G_GNUC_INTERNAL GArray * _gum_v8_memory_ranges_get (v8::Local<v8::Value> value,
    GumV8Core * core);
G_GNUC_INTERNAL gboolean _gum_v8_memory_range_get (v8::Local<v8::Value> value,
    GumMemoryRange * range, GumV8Core * core);

G_GNUC_INTERNAL v8::Local<v8::String> _gum_v8_page_protection_new (
    v8::Isolate * isolate, GumPageProtection prot);
G_GNUC_INTERNAL gboolean _gum_v8_page_protection_get (
    v8::Local<v8::Value> prot_val, GumPageProtection * prot,
    GumV8Core * core);

G_GNUC_INTERNAL v8::Local<v8::ObjectTemplate> _gum_v8_create_module (
    const gchar * name, v8::Local<v8::ObjectTemplate> scope,
    v8::Isolate * isolate);
G_GNUC_INTERNAL void _gum_v8_module_add (v8::Local<v8::External> module,
    v8::Local<v8::ObjectTemplate> object, const GumV8Property * properties,
    v8::Isolate * isolate);
G_GNUC_INTERNAL void _gum_v8_module_add (v8::Local<v8::External> module,
    v8::Local<v8::ObjectTemplate> object, const GumV8Function * functions,
    v8::Isolate * isolate);
G_GNUC_INTERNAL v8::Local<v8::FunctionTemplate> _gum_v8_create_class (
    const gchar * name, v8::FunctionCallback ctor,
    v8::Local<v8::ObjectTemplate> scope, v8::Local<v8::External> module,
    v8::Isolate * isolate);
G_GNUC_INTERNAL void _gum_v8_class_add_static (
    v8::Local<v8::FunctionTemplate> klass, const GumV8Property * properties,
    v8::Local<v8::External> module, v8::Isolate * isolate);
G_GNUC_INTERNAL void _gum_v8_class_add_static (
    v8::Local<v8::FunctionTemplate> klass, const GumV8Function * functions,
    v8::Local<v8::External> module, v8::Isolate * isolate);
G_GNUC_INTERNAL void _gum_v8_class_add (v8::Local<v8::FunctionTemplate> klass,
    const GumV8Property * properties, v8::Local<v8::External> module,
    v8::Isolate * isolate);
G_GNUC_INTERNAL void _gum_v8_class_add (v8::Local<v8::FunctionTemplate> klass,
    const GumV8Function * functions, v8::Local<v8::External> module,
    v8::Isolate * isolate);

template <typename T> void _gum_v8_ignore_result (T unused_result) {}

#endif
