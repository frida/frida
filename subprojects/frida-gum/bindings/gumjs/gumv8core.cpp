/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2015 Asger Hautop Drewsen <asgerdrewsen@gmail.com>
 * Copyright (C) 2015 Marc Hartmayer <hello@hartmayer.com>
 * Copyright (C) 2020-2022 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2020 Marcus Mengs <mame8282@googlemail.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 * Copyright (C) 2024 Simon Zuckerbraun <Simon_Zuckerbraun@trendmicro.com>
 * Copyright (C) 2026 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8core.h"

#include "gumansi.h"
#include "gumffi.h"
#include "gumsourcemap.h"
#include "gumv8macros.h"
#include "gumv8scope.h"
#include "gumv8script-priv.h"

#include <ffi.h>
#include <string.h>
#include <glib/gprintf.h>
#include <gum/gum-init.h>
#ifdef _MSC_VER
# include <intrin.h>
#endif
#ifdef HAVE_PTRAUTH
# include <ptrauth.h>
#endif

#define GUMJS_MODULE_NAME Core

using namespace v8;

typedef guint8 GumV8SchedulingBehavior;
typedef guint8 GumV8ExceptionsBehavior;
typedef guint8 GumV8CodeTraps;
typedef guint8 GumV8ReturnValueShape;

struct GumV8FlushCallback
{
  GumV8FlushNotify func;
  GumV8Script * script;
};

enum GumMemoryValueType
{
  GUM_MEMORY_VALUE_POINTER,
  GUM_MEMORY_VALUE_S8,
  GUM_MEMORY_VALUE_U8,
  GUM_MEMORY_VALUE_S16,
  GUM_MEMORY_VALUE_U16,
  GUM_MEMORY_VALUE_S32,
  GUM_MEMORY_VALUE_U32,
  GUM_MEMORY_VALUE_S64,
  GUM_MEMORY_VALUE_U64,
  GUM_MEMORY_VALUE_LONG,
  GUM_MEMORY_VALUE_ULONG,
  GUM_MEMORY_VALUE_FLOAT,
  GUM_MEMORY_VALUE_DOUBLE,
  GUM_MEMORY_VALUE_BYTE_ARRAY,
  GUM_MEMORY_VALUE_C_STRING,
  GUM_MEMORY_VALUE_UTF8_STRING,
  GUM_MEMORY_VALUE_UTF16_STRING,
  GUM_MEMORY_VALUE_ANSI_STRING
};

struct GumV8WeakRef
{
  guint id;
  Global<Value> * target;
  Global<Function> * callback;

  GumV8Core * core;
};

struct GumV8ScheduledCallback
{
  gint id;
  gboolean repeat;
  Global<Function> * func;
  GSource * source;

  GumV8Core * core;
};

struct GumV8ExceptionSink
{
  Global<Function> * callback;
  Isolate * isolate;
};

struct GumV8MessageSink
{
  Global<Function> * callback;
  Isolate * isolate;
};

struct GumV8NativeFunctionParams
{
  GCallback implementation;
  Local<Value> return_type;
  Local<Array> argument_types;
  Local<Value> abi;
  GumV8SchedulingBehavior scheduling;
  GumV8ExceptionsBehavior exceptions;
  GumV8CodeTraps traps;
  GumV8ReturnValueShape return_shape;
};

enum _GumV8SchedulingBehavior
{
  GUM_V8_SCHEDULING_COOPERATIVE,
  GUM_V8_SCHEDULING_EXCLUSIVE
};

enum _GumV8ExceptionsBehavior
{
  GUM_V8_EXCEPTIONS_STEAL,
  GUM_V8_EXCEPTIONS_PROPAGATE
};

enum _GumV8CodeTraps
{
  GUM_V8_CODE_TRAPS_DEFAULT,
  GUM_V8_CODE_TRAPS_NONE,
  GUM_V8_CODE_TRAPS_ALL
};

enum _GumV8ReturnValueShape
{
  GUM_V8_RETURN_PLAIN,
  GUM_V8_RETURN_DETAILED
};

struct GumV8NativeFunction
{
  Global<Object> * wrapper;

  GCallback implementation;
  GumV8SchedulingBehavior scheduling;
  GumV8ExceptionsBehavior exceptions;
  GumV8CodeTraps traps;
  GumV8ReturnValueShape return_shape;
  ffi_cif cif;
  ffi_type ** atypes;
  gsize arglist_size;
  gboolean is_variadic;
  uint32_t nargs_fixed;
  ffi_abi abi;
  GSList * data;

  GumV8Core * core;
};

struct GumV8NativeCallback
{
  gint ref_count;

  v8::Global<v8::Object> * wrapper;
  gpointer ptr_value;

  v8::Global<v8::Function> * func;
  ffi_closure * closure;
  ffi_cif cif;
  ffi_type ** atypes;
  GSList * data;

  GumV8Core * core;
};

struct GumV8CallbackContext
{
  Global<Object> * wrapper;
  Global<Object> * cpu_context;
  gint * system_error;
  GumAddress return_address;
  GumAddress raw_return_address;
};

struct GumV8MatchPattern
{
  Global<Object> * wrapper;
  GumMatchPattern * handle;
};

struct GumV8SourceMap
{
  Global<Object> * wrapper;
  GumSourceMap * handle;

  GumV8Core * core;
};

static gboolean gum_v8_core_handle_crashed_js (GumExceptionDetails * details,
    gpointer user_data);

static void gum_v8_core_clear_weak_refs (GumV8Core * self);
static void gum_v8_flush_callback_free (GumV8FlushCallback * self);
static gboolean gum_v8_flush_callback_notify (GumV8FlushCallback * self);

GUMJS_DECLARE_FUNCTION (gumjs_set_timeout)
GUMJS_DECLARE_FUNCTION (gumjs_set_interval)
static void gum_v8_core_schedule_callback (GumV8Core * self,
    const GumV8Args * args, gboolean repeat);
static GumV8ScheduledCallback * gum_v8_core_try_steal_scheduled_callback (
    GumV8Core * self, gint id);
GUMJS_DECLARE_FUNCTION (gumjs_clear_timer)
static GumV8ScheduledCallback * gum_v8_scheduled_callback_new (guint id,
    gboolean repeat, GSource * source, GumV8Core * core);
static void gum_v8_scheduled_callback_free (GumV8ScheduledCallback * callback);
static gboolean gum_v8_scheduled_callback_invoke (
    GumV8ScheduledCallback * self);
GUMJS_DECLARE_FUNCTION (gumjs_send)
GUMJS_DECLARE_FUNCTION (gumjs_set_unhandled_exception_callback)
GUMJS_DECLARE_FUNCTION (gumjs_set_incoming_message_callback)
GUMJS_DECLARE_FUNCTION (gumjs_wait_for_event)

static void gumjs_global_get (Local<Name> property,
    const PropertyCallbackInfo<Value> & info);

GUMJS_DECLARE_GETTER (gumjs_frida_get_heap_size)

GUMJS_DECLARE_FUNCTION (gumjs_script_evaluate)
GUMJS_DECLARE_FUNCTION (gumjs_script_load)
GUMJS_DECLARE_FUNCTION (gumjs_script_register_source_map)
GUMJS_DECLARE_FUNCTION (gumjs_script_find_source_map)
static gchar * gum_query_script_for_inline_source_map (Isolate * isolate,
    Local<Script> script);
GUMJS_DECLARE_FUNCTION (gumjs_script_next_tick)
GUMJS_DECLARE_FUNCTION (gumjs_script_pin)
GUMJS_DECLARE_FUNCTION (gumjs_script_unpin)
GUMJS_DECLARE_FUNCTION (gumjs_script_bind_weak)
GUMJS_DECLARE_FUNCTION (gumjs_script_unbind_weak)
static GumV8WeakRef * gum_v8_weak_ref_new (guint id, Local<Value> target,
    Local<Function> callback, GumV8Core * core);
static void gum_v8_weak_ref_clear (GumV8WeakRef * ref);
static void gum_v8_weak_ref_free (GumV8WeakRef * ref);
static void gum_v8_weak_ref_on_weak_notify (
    const WeakCallbackInfo<GumV8WeakRef> & info);
static gboolean gum_v8_core_invoke_pending_weak_callbacks_in_idle (
    GumV8Core * self);
static void gum_v8_core_invoke_pending_weak_callbacks (GumV8Core * self,
    ScriptScope * scope);
GUMJS_DECLARE_FUNCTION (gumjs_script_set_global_access_handler)

GUMJS_DECLARE_FUNCTION (gumjs_int64_construct)
GUMJS_DECLARE_FUNCTION (gumjs_int64_add)
GUMJS_DECLARE_FUNCTION (gumjs_int64_sub)
GUMJS_DECLARE_FUNCTION (gumjs_int64_and)
GUMJS_DECLARE_FUNCTION (gumjs_int64_or)
GUMJS_DECLARE_FUNCTION (gumjs_int64_xor)
GUMJS_DECLARE_FUNCTION (gumjs_int64_shr)
GUMJS_DECLARE_FUNCTION (gumjs_int64_shl)
GUMJS_DECLARE_FUNCTION (gumjs_int64_not)
GUMJS_DECLARE_FUNCTION (gumjs_int64_compare)
GUMJS_DECLARE_FUNCTION (gumjs_int64_to_number)
GUMJS_DECLARE_FUNCTION (gumjs_int64_to_string)
GUMJS_DECLARE_FUNCTION (gumjs_int64_to_json)
GUMJS_DECLARE_FUNCTION (gumjs_int64_value_of)

GUMJS_DECLARE_FUNCTION (gumjs_uint64_construct)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_add)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_sub)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_and)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_or)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_xor)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_shr)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_shl)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_not)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_compare)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_to_number)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_to_string)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_to_json)
GUMJS_DECLARE_FUNCTION (gumjs_uint64_value_of)

GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_construct)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_is_null)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_add)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_sub)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_and)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_or)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_xor)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_shr)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_shl)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_not)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_sign)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_strip)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_blend)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_compare)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_to_int32)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_to_uint32)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_to_string)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_to_json)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_to_match_pattern)

static void gumjs_native_pointer_handle_read (
    const FunctionCallbackInfo<Value> & info, GumMemoryValueType type,
    const GumV8Args * args);
static void gumjs_native_pointer_handle_write (
    const FunctionCallbackInfo<Value> & info, GumMemoryValueType type,
    const GumV8Args * args);

#define GUMJS_DEFINE_NATIVE_POINTER_READ(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_read_##T) \
    { \
      gumjs_native_pointer_handle_read (info, GUM_MEMORY_VALUE_##T, args); \
    }
#define GUMJS_DEFINE_NATIVE_POINTER_WRITE(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_write_##T) \
    { \
      gumjs_native_pointer_handle_write (info, GUM_MEMORY_VALUE_##T, args); \
    }
#define GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE(T) \
    GUMJS_DEFINE_NATIVE_POINTER_READ (T); \
    GUMJS_DEFINE_NATIVE_POINTER_WRITE (T)

#define GUMJS_EXPORT_MEMORY_READ(N, T) \
    { "read" N, gumjs_native_pointer_read_##T }
#define GUMJS_EXPORT_MEMORY_WRITE(N, T) \
    { "write" N, gumjs_native_pointer_write_##T }
#define GUMJS_EXPORT_MEMORY_READ_WRITE(N, T) \
    GUMJS_EXPORT_MEMORY_READ (N, T), \
    GUMJS_EXPORT_MEMORY_WRITE (N, T)

GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (POINTER)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (S8)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (U8)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (S16)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (U16)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (S32)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (U32)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (S64)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (U64)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (LONG)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (ULONG)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (FLOAT)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (DOUBLE)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (BYTE_ARRAY)
GUMJS_DEFINE_NATIVE_POINTER_READ (C_STRING)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (UTF8_STRING)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (UTF16_STRING)
GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE (ANSI_STRING)

GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_read_volatile)
GUMJS_DECLARE_FUNCTION (gumjs_native_pointer_write_volatile)

GUMJS_DECLARE_FUNCTION (gumjs_array_buffer_wrap)
GUMJS_DECLARE_FUNCTION (gumjs_array_buffer_unwrap)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_native_function_construct)
GUMJS_DECLARE_FUNCTION (gumjs_native_function_invoke)
GUMJS_DECLARE_FUNCTION (gumjs_native_function_call)
GUMJS_DECLARE_FUNCTION (gumjs_native_function_apply)
static gboolean gumjs_native_function_get (
    const FunctionCallbackInfo<Value> & info, Local<Object> receiver,
    GumV8Core * core, GumV8NativeFunction ** func, GCallback * implementation);
static GumV8NativeFunction * gumjs_native_function_init (Local<Object> wrapper,
    const GumV8NativeFunctionParams * params, GumV8Core * core);
static void gum_v8_native_function_free (GumV8NativeFunction * self);
static void gum_v8_native_function_invoke (GumV8NativeFunction * self,
    GCallback implementation, const FunctionCallbackInfo<Value> & info,
    uint32_t argc, Local<Value> * argv);
static void gum_v8_native_function_on_weak_notify (
    const WeakCallbackInfo<GumV8NativeFunction> & info);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_system_function_construct)

static gboolean gum_v8_native_function_params_init (
    GumV8NativeFunctionParams * params, GumV8ReturnValueShape return_shape,
    const GumV8Args * args);
static gboolean gum_v8_scheduling_behavior_parse (Local<Value> value,
    GumV8SchedulingBehavior * behavior, Isolate * isolate);
static gboolean gum_v8_exceptions_behavior_parse (Local<Value> value,
    GumV8ExceptionsBehavior * behavior, Isolate * isolate);
static gboolean gum_v8_code_traps_parse (Local<Value> value,
    GumV8CodeTraps * traps, Isolate * isolate);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_native_callback_construct)
static void gum_v8_native_callback_on_weak_notify (
    const WeakCallbackInfo<GumV8NativeCallback> & info);
static GumV8NativeCallback * gum_v8_native_callback_ref (
    GumV8NativeCallback * callback);
static void gum_v8_native_callback_unref (GumV8NativeCallback * callback);
static void gum_v8_native_callback_clear (GumV8NativeCallback * self);
static void gum_v8_native_callback_invoke (ffi_cif * cif,
    void * return_value, void ** args, void * user_data);

static GumV8CallbackContext * gum_v8_callback_context_new_persistent (
    GumV8Core * core, GumCpuContext * cpu_context, gint * system_error,
    GumAddress raw_return_address);
static void gum_v8_callback_context_free (GumV8CallbackContext * self);
GUMJS_DECLARE_GETTER (gumjs_callback_context_get_return_address)
GUMJS_DECLARE_GETTER (gumjs_callback_context_get_cpu_context)
GUMJS_DECLARE_GETTER (gumjs_callback_context_get_system_error)
GUMJS_DECLARE_SETTER (gumjs_callback_context_set_system_error)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_cpu_context_construct)
GUMJS_DECLARE_GETTER (gumjs_cpu_context_get_gpr)
GUMJS_DECLARE_SETTER (gumjs_cpu_context_set_gpr)
G_GNUC_UNUSED GUMJS_DECLARE_GETTER (gumjs_cpu_context_get_vector)
G_GNUC_UNUSED GUMJS_DECLARE_SETTER (gumjs_cpu_context_set_vector)
#ifdef HAVE_I386
GUMJS_DECLARE_GETTER (gumjs_cpu_context_get_xmm)
GUMJS_DECLARE_SETTER (gumjs_cpu_context_set_xmm)
#endif
G_GNUC_UNUSED GUMJS_DECLARE_GETTER (gumjs_cpu_context_get_double)
G_GNUC_UNUSED GUMJS_DECLARE_SETTER (gumjs_cpu_context_set_double)
G_GNUC_UNUSED GUMJS_DECLARE_GETTER (gumjs_cpu_context_get_float)
G_GNUC_UNUSED GUMJS_DECLARE_SETTER (gumjs_cpu_context_set_float)
G_GNUC_UNUSED GUMJS_DECLARE_GETTER (gumjs_cpu_context_get_flags)
G_GNUC_UNUSED GUMJS_DECLARE_SETTER (gumjs_cpu_context_set_flags)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_match_pattern_construct)
static GumV8MatchPattern * gum_v8_match_pattern_new (Local<Object> wrapper,
    GumMatchPattern * pattern, GumV8Core * core);
static void gum_v8_match_pattern_free (GumV8MatchPattern * self);

static MaybeLocal<Object> gumjs_source_map_new (const gchar * json,
    GumV8Core * core);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_source_map_construct)
GUMJS_DECLARE_FUNCTION (gumjs_source_map_resolve)
static GumV8SourceMap * gum_v8_source_map_new (Local<Object> wrapper,
    GumSourceMap * handle, GumV8Core * core);
static void gum_v8_source_map_free (GumV8SourceMap * self);
static void gum_v8_source_map_on_weak_notify (
    const WeakCallbackInfo<GumV8SourceMap> & info);

static GumV8ExceptionSink * gum_v8_exception_sink_new (
    Local<Function> callback, Isolate * isolate);
static void gum_v8_exception_sink_free (GumV8ExceptionSink * sink);
static void gum_v8_exception_sink_handle_exception (GumV8ExceptionSink * self,
    Local<Value> exception);

static GumV8MessageSink * gum_v8_message_sink_new (Local<Function> callback,
    Isolate * isolate);
static void gum_v8_message_sink_free (GumV8MessageSink * sink);
static void gum_v8_message_sink_post (GumV8MessageSink * self,
    const gchar * message, GBytes * data);
static void gum_delete_bytes_reference (void * data, size_t length,
    void * deleter_data);

static gboolean gum_v8_ffi_type_get (GumV8Core * core, Local<Value> name,
    ffi_type ** type, GSList ** data);
static gboolean gum_v8_ffi_abi_get (GumV8Core * core, Local<Value> name,
    ffi_abi * abi);
static gboolean gum_v8_value_to_ffi_type (GumV8Core * core,
    const Local<Value> svalue, GumFFIArg * value, const ffi_type * type);
static gboolean gum_v8_value_from_ffi_type (GumV8Core * core,
    Local<Value> * svalue, const GumFFIRet * value, const ffi_type * type);

static const GumV8Function gumjs_global_functions[] =
{
  { "_setTimeout", gumjs_set_timeout, },
  { "_setInterval", gumjs_set_interval },
  { "clearTimeout", gumjs_clear_timer },
  { "clearInterval", gumjs_clear_timer },
  { "_send", gumjs_send },
  { "_setUnhandledExceptionCallback", gumjs_set_unhandled_exception_callback },
  { "_setIncomingMessageCallback", gumjs_set_incoming_message_callback },
  { "_waitForEvent", gumjs_wait_for_event },

  { NULL, NULL }
};

static const GumV8Property gumjs_frida_values[] =
{
  { "heapSize", gumjs_frida_get_heap_size, NULL },

  { NULL, NULL }
};

static const GumV8Function gumjs_script_functions[] =
{
  { "evaluate", gumjs_script_evaluate },
  { "_load", gumjs_script_load },
  { "registerSourceMap", gumjs_script_register_source_map },
  { "_findSourceMap", gumjs_script_find_source_map },
  { "_nextTick", gumjs_script_next_tick },
  { "pin", gumjs_script_pin },
  { "unpin", gumjs_script_unpin },
  { "bindWeak", gumjs_script_bind_weak },
  { "unbindWeak", gumjs_script_unbind_weak },
  { "setGlobalAccessHandler", gumjs_script_set_global_access_handler },

  { NULL, NULL }
};

static const GumV8Function gumjs_int64_functions[] =
{
  { "add", gumjs_int64_add },
  { "sub", gumjs_int64_sub },
  { "and", gumjs_int64_and },
  { "or", gumjs_int64_or },
  { "xor", gumjs_int64_xor },
  { "shr", gumjs_int64_shr },
  { "shl", gumjs_int64_shl },
  { "not", gumjs_int64_not },
  { "compare", gumjs_int64_compare },
  { "toNumber", gumjs_int64_to_number },
  { "toString", gumjs_int64_to_string },
  { "toJSON", gumjs_int64_to_json },
  { "valueOf", gumjs_int64_value_of },

  { NULL, NULL }
};

static const GumV8Function gumjs_uint64_functions[] =
{
  { "add", gumjs_uint64_add },
  { "sub", gumjs_uint64_sub },
  { "and", gumjs_uint64_and },
  { "or", gumjs_uint64_or },
  { "xor", gumjs_uint64_xor },
  { "shr", gumjs_uint64_shr },
  { "shl", gumjs_uint64_shl },
  { "not", gumjs_uint64_not },
  { "compare", gumjs_uint64_compare },
  { "toNumber", gumjs_uint64_to_number },
  { "toString", gumjs_uint64_to_string },
  { "toJSON", gumjs_uint64_to_json },
  { "valueOf", gumjs_uint64_value_of },

  { NULL, NULL }
};

static const GumV8Function gumjs_native_pointer_functions[] =
{
  { "isNull", gumjs_native_pointer_is_null },
  { "add", gumjs_native_pointer_add },
  { "sub", gumjs_native_pointer_sub },
  { "and", gumjs_native_pointer_and },
  { "or", gumjs_native_pointer_or },
  { "xor", gumjs_native_pointer_xor },
  { "shr", gumjs_native_pointer_shr },
  { "shl", gumjs_native_pointer_shl },
  { "not", gumjs_native_pointer_not },
  { "sign", gumjs_native_pointer_sign },
  { "strip", gumjs_native_pointer_strip },
  { "blend", gumjs_native_pointer_blend },
  { "compare", gumjs_native_pointer_compare },
  { "toInt32", gumjs_native_pointer_to_int32 },
  { "toUInt32", gumjs_native_pointer_to_uint32 },
  { "toString", gumjs_native_pointer_to_string },
  { "toJSON", gumjs_native_pointer_to_json },
  { "toMatchPattern", gumjs_native_pointer_to_match_pattern },
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Pointer", POINTER),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("S8", S8),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U8", U8),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("S16", S16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U16", U16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("S32", S32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U32", U32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("S64", S64),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("U64", U64),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Short", S16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("UShort", U16),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Int", S32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("UInt", U32),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Long", LONG),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("ULong", ULONG),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Float", FLOAT),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Double", DOUBLE),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("ByteArray", BYTE_ARRAY),
  GUMJS_EXPORT_MEMORY_READ ("CString", C_STRING),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Utf8String", UTF8_STRING),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("Utf16String", UTF16_STRING),
  GUMJS_EXPORT_MEMORY_READ_WRITE ("AnsiString", ANSI_STRING),
  { "readVolatile", gumjs_native_pointer_read_volatile },
  { "writeVolatile", gumjs_native_pointer_write_volatile },

  { NULL, NULL }
};

static const GumV8Function gumjs_native_function_functions[] =
{
  { "call", gumjs_native_function_call },
  { "apply", gumjs_native_function_apply },

  { NULL, NULL }
};

static const GumV8Property gumjs_callback_context_values[] =
{
  {
    "returnAddress",
    gumjs_callback_context_get_return_address,
    NULL
  },
  {
    "context",
    gumjs_callback_context_get_cpu_context,
    NULL
  },
  {
    GUMJS_SYSTEM_ERROR_FIELD,
    gumjs_callback_context_get_system_error,
    gumjs_callback_context_set_system_error
  },

  { NULL, NULL, NULL }
};

static const GumV8Function gumjs_source_map_functions[] =
{
  { "_resolve", gumjs_source_map_resolve },

  { NULL, NULL }
};

void
_gum_v8_core_init (GumV8Core * self,
                   GumV8Script * script,
                   GumV8MessageEmitter message_emitter,
                   GumScriptScheduler * scheduler,
                   Isolate * isolate,
                   Local<ObjectTemplate> scope)
{
  self->script = script;
  self->backend = script->backend;
  self->core = self;
  self->message_emitter = message_emitter;
  self->scheduler = scheduler;
  self->exceptor = gum_exceptor_obtain ();
  self->isolate = isolate;

  self->current_scope = nullptr;
  self->current_owner = GUM_THREAD_ID_INVALID;
  self->transaction_released_owner = GUM_THREAD_ID_INVALID;
  self->usage_count = 0;
  self->flush_notify = NULL;

  self->event_loop = g_main_loop_new (
      gum_script_scheduler_get_js_context (scheduler), FALSE);
  g_mutex_init (&self->event_mutex);
  g_cond_init (&self->event_cond);
  self->event_count = 0;
  self->event_source_available = TRUE;

  self->weak_refs = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_weak_ref_free);

  self->scheduled_callbacks = g_hash_table_new (NULL, NULL);
  self->next_callback_id = 1;

  auto module = External::New (isolate, self);

  _gum_v8_module_add (module, scope, gumjs_global_functions, isolate);

  NamedPropertyHandlerConfiguration global_access;
  global_access.getter = gumjs_global_get;
  global_access.data = module;
  global_access.flags = (PropertyHandlerFlags) (
        (int) PropertyHandlerFlags::kNonMasking |
        (int) PropertyHandlerFlags::kOnlyInterceptStrings
      );
  scope->SetHandler (global_access);

  auto frida = _gum_v8_create_module ("Frida", scope, isolate);
  _gum_v8_module_add (module, frida, gumjs_frida_values, isolate);
  frida->Set (_gum_v8_string_new_ascii (isolate, "version"),
      _gum_v8_string_new_ascii (isolate, FRIDA_VERSION), ReadOnly);

  auto script_module = _gum_v8_create_module ("Script", scope, isolate);
  _gum_v8_module_add (module, script_module, gumjs_script_functions, isolate);
  script_module->Set (_gum_v8_string_new_ascii (isolate, "runtime"),
      _gum_v8_string_new_ascii (isolate, "V8"), ReadOnly);

  auto int64 = _gum_v8_create_class ("Int64", gumjs_int64_construct, scope,
      module, isolate);
  _gum_v8_class_add (int64, gumjs_int64_functions, module, isolate);
  int64->InstanceTemplate ()->SetInternalFieldCount (1);
  self->int64 = new Global<FunctionTemplate> (isolate, int64);

  auto uint64 = _gum_v8_create_class ("UInt64", gumjs_uint64_construct, scope,
      module, isolate);
  _gum_v8_class_add (uint64, gumjs_uint64_functions, module, isolate);
  uint64->InstanceTemplate ()->SetInternalFieldCount (1);
  self->uint64 = new Global<FunctionTemplate> (isolate, uint64);

  auto native_pointer = _gum_v8_create_class ("NativePointer",
      gumjs_native_pointer_construct, scope, module, isolate);
  _gum_v8_class_add (native_pointer, gumjs_native_pointer_functions, module,
      isolate);
  self->native_pointer = new Global<FunctionTemplate> (isolate, native_pointer);

  auto native_function = _gum_v8_create_class ("NativeFunction",
      gumjs_native_function_construct, scope, module, isolate);
  native_function->Inherit (native_pointer);
  _gum_v8_class_add (native_function, gumjs_native_function_functions, module,
      isolate);
  auto native_function_object = native_function->InstanceTemplate ();
  native_function_object->SetCallAsFunctionHandler (
      gumjs_native_function_invoke, module);
  native_function_object->SetInternalFieldCount (2);
  self->native_function =
      new Global<FunctionTemplate> (isolate, native_function);

  auto system_function = _gum_v8_create_class ("SystemFunction",
      gumjs_system_function_construct, scope, module, isolate);
  system_function->Inherit (native_function);
  auto system_function_object = system_function->InstanceTemplate ();
  system_function_object->SetCallAsFunctionHandler (
      gumjs_native_function_invoke, module);
  system_function_object->SetInternalFieldCount (2);

  auto native_callback = _gum_v8_create_class ("NativeCallback",
      gumjs_native_callback_construct, scope, module, isolate);
  native_callback->Inherit (native_pointer);
  native_callback->InstanceTemplate ()->SetInternalFieldCount (2);

  auto cc = _gum_v8_create_class ("CallbackContext", nullptr, scope,
      module, isolate);
  _gum_v8_class_add (cc, gumjs_callback_context_values, module, isolate);
  self->callback_context = new Global<FunctionTemplate> (isolate, cc);

  auto cpu_context = _gum_v8_create_class ("CpuContext",
      gumjs_cpu_context_construct, scope, module, isolate);
  auto cpu_context_object = cpu_context->InstanceTemplate ();
  cpu_context_object->SetInternalFieldCount (3);
  self->cpu_context = new Global<FunctionTemplate> (isolate, cpu_context);

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED(A, R) \
    cpu_context_object->SetAccessor ( \
        _gum_v8_string_new_ascii (isolate, G_STRINGIFY (A)), \
        gumjs_cpu_context_get_gpr, \
        gumjs_cpu_context_set_gpr, \
        Integer::NewFromUnsigned (isolate, \
            G_STRUCT_OFFSET (GumCpuContext, R)), \
        DEFAULT, \
        DontDelete)
#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR(R) \
    GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (R, R)

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR(A, R) \
    cpu_context_object->SetAccessor ( \
        _gum_v8_string_new_ascii (isolate, G_STRINGIFY (A)), \
        gumjs_cpu_context_get_vector, \
        gumjs_cpu_context_set_vector, \
        Integer::NewFromUnsigned (isolate, \
            G_STRUCT_OFFSET (GumCpuContext, R) << 8 | \
              sizeof (((GumCpuContext *) NULL)->R)), \
        DEFAULT, \
        DontDelete)

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM(A, INDEX) \
    cpu_context_object->SetAccessor ( \
        _gum_v8_string_new_ascii (isolate, G_STRINGIFY (A)), \
        gumjs_cpu_context_get_xmm, \
        gumjs_cpu_context_set_xmm, \
        Integer::NewFromUnsigned (isolate, INDEX), \
        DEFAULT, \
        DontDelete)

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE(A, R) \
    cpu_context_object->SetAccessor ( \
        _gum_v8_string_new_ascii (isolate, G_STRINGIFY (A)), \
        gumjs_cpu_context_get_double, \
        gumjs_cpu_context_set_double, \
        Integer::NewFromUnsigned (isolate, \
            G_STRUCT_OFFSET (GumCpuContext, R)), \
        DEFAULT, \
        DontDelete)

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT(A, R) \
    cpu_context_object->SetAccessor ( \
        _gum_v8_string_new_ascii (isolate, G_STRINGIFY (A)), \
        gumjs_cpu_context_get_float, \
        gumjs_cpu_context_set_float, \
        Integer::NewFromUnsigned (isolate, \
            G_STRUCT_OFFSET (GumCpuContext, R)), \
        DEFAULT, \
        DontDelete)

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLAGS(A, R) \
    cpu_context_object->SetAccessor ( \
        _gum_v8_string_new_ascii (isolate, G_STRINGIFY (A)), \
        gumjs_cpu_context_get_flags, \
        gumjs_cpu_context_set_flags, \
        Integer::NewFromUnsigned (isolate, \
            G_STRUCT_OFFSET (GumCpuContext, R)), \
        DEFAULT, \
        DontDelete)

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (pc, eip);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (sp, esp);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (eax);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ecx);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (edx);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ebx);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (esp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ebp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (esi);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (edi);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (eip);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm0, 0);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm1, 1);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm2, 2);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm3, 3);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm4, 4);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm5, 5);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm6, 6);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm7, 7);
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (pc, rip);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (sp, rsp);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rax);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rcx);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rdx);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rbx);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rsp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rbp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rsi);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rdi);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r8);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r9);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r10);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r11);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r12);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r13);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r14);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r15);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rip);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm0, 0);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm1, 1);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm2, 2);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm3, 3);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm4, 4);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm5, 5);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm6, 6);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm7, 7);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm8, 8);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm9, 9);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm10, 10);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm11, 11);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm12, 12);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm13, 13);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm14, 14);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm15, 15);
#elif defined (HAVE_ARM)
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (pc);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (sp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLAGS (cpsr, cpsr);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r0, r[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r1, r[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r2, r[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r3, r[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r4, r[4]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r5, r[5]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r6, r[6]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r7, r[7]);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r8);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r9);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r10);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r11);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r12);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (lr);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q0, v[0].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q1, v[1].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q2, v[2].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q3, v[3].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q4, v[4].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q5, v[5].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q6, v[6].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q7, v[7].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q8, v[8].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q9, v[9].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q10, v[10].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q11, v[11].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q12, v[12].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q13, v[13].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q14, v[14].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q15, v[15].q);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d0, v[0].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d1, v[0].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d2, v[1].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d3, v[1].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d4, v[2].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d5, v[2].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d6, v[3].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d7, v[3].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d8, v[4].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d9, v[4].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d10, v[5].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d11, v[5].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d12, v[6].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d13, v[6].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d14, v[7].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d15, v[7].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d16, v[8].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d17, v[8].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d18, v[9].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d19, v[9].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d20, v[10].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d21, v[10].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d22, v[11].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d23, v[11].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d24, v[12].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d25, v[12].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d26, v[13].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d27, v[13].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d28, v[14].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d29, v[14].d[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d30, v[15].d[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d31, v[15].d[1]);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s0, v[0].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s1, v[0].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s2, v[0].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s3, v[0].s[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s4, v[1].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s5, v[1].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s6, v[1].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s7, v[1].s[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s8, v[2].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s9, v[2].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s10, v[2].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s11, v[2].s[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s12, v[3].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s13, v[3].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s14, v[3].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s15, v[3].s[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s16, v[4].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s17, v[4].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s18, v[4].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s19, v[4].s[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s20, v[5].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s21, v[5].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s22, v[5].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s23, v[5].s[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s24, v[6].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s25, v[6].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s26, v[6].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s27, v[6].s[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s28, v[7].s[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s29, v[7].s[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s30, v[7].s[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s31, v[7].s[3]);
#elif defined (HAVE_ARM64)
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (pc);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (sp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLAGS (nzcv, nzcv);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x0, x[0]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x1, x[1]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x2, x[2]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x3, x[3]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x4, x[4]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x5, x[5]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x6, x[6]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x7, x[7]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x8, x[8]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x9, x[9]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x10, x[10]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x11, x[11]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x12, x[12]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x13, x[13]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x14, x[14]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x15, x[15]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x16, x[16]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x17, x[17]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x18, x[18]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x19, x[19]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x20, x[20]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x21, x[21]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x22, x[22]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x23, x[23]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x24, x[24]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x25, x[25]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x26, x[26]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x27, x[27]);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x28, x[28]);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (fp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (lr);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q0, v[0].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q1, v[1].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q2, v[2].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q3, v[3].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q4, v[4].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q5, v[5].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q6, v[6].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q7, v[7].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q8, v[8].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q9, v[9].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q10, v[10].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q11, v[11].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q12, v[12].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q13, v[13].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q14, v[14].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q15, v[15].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q16, v[16].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q17, v[17].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q18, v[18].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q19, v[19].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q20, v[20].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q21, v[21].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q22, v[22].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q23, v[23].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q24, v[24].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q25, v[25].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q26, v[26].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q27, v[27].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q28, v[28].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q29, v[29].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q30, v[30].q);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q31, v[31].q);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d0, v[0].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d1, v[1].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d2, v[2].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d3, v[3].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d4, v[4].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d5, v[5].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d6, v[6].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d7, v[7].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d8, v[8].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d9, v[9].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d10, v[10].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d11, v[11].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d12, v[12].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d13, v[13].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d14, v[14].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d15, v[15].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d16, v[16].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d17, v[17].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d18, v[18].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d19, v[19].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d20, v[20].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d21, v[21].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d22, v[22].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d23, v[23].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d24, v[24].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d25, v[25].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d26, v[26].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d27, v[27].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d28, v[28].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d29, v[29].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d30, v[30].d);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d31, v[31].d);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s0, v[0].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s1, v[1].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s2, v[2].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s3, v[3].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s4, v[4].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s5, v[5].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s6, v[6].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s7, v[7].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s8, v[8].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s9, v[9].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s10, v[10].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s11, v[11].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s12, v[12].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s13, v[13].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s14, v[14].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s15, v[15].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s16, v[16].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s17, v[17].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s18, v[18].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s19, v[19].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s20, v[20].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s21, v[21].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s22, v[22].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s23, v[23].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s24, v[24].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s25, v[25].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s26, v[26].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s27, v[27].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s28, v[28].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s29, v[29].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s30, v[30].s);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s31, v[31].s);
#elif defined (HAVE_MIPS)
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (pc);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (gp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (sp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (fp);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ra);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (hi);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (lo);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (at);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (v0);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (v1);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a0);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a1);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a2);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a3);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t0);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t1);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t2);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t3);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t4);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t5);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t6);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t7);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t8);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t9);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s0);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s1);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s2);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s3);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s4);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s5);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s6);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s7);

  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (k0);
  GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (k1);
#endif

  auto match_pattern = _gum_v8_create_class ("MatchPattern",
      gumjs_match_pattern_construct, scope, module, isolate);
  self->match_pattern = new Global<FunctionTemplate> (isolate, match_pattern);

  auto source_map = _gum_v8_create_class ("SourceMap",
      gumjs_source_map_construct, scope, module, isolate);
  _gum_v8_class_add (source_map, gumjs_source_map_functions, module, isolate);
  self->source_map = new Global<FunctionTemplate> (isolate, source_map);

  gum_exceptor_add (self->exceptor, gum_v8_core_handle_crashed_js, self);
}

static gboolean
gum_v8_core_handle_crashed_js (GumExceptionDetails * details,
                               gpointer user_data)
{
  GumV8Core * self = (GumV8Core *) user_data;
  GumThreadId thread_id = details->thread_id;

  if (gum_exceptor_has_scope (self->exceptor, thread_id))
    return FALSE;

  if (self->current_owner == thread_id &&
      self->transaction_released_owner != thread_id)
  {
    gum_interceptor_end_transaction (self->script->interceptor.interceptor);
    self->transaction_released_owner = thread_id;
    gum_v8_script_backend_mark_scope_mutex_trapped (self->backend);
  }

  return FALSE;
}

void
_gum_v8_core_realize (GumV8Core * self)
{
  auto isolate = self->isolate;
  auto context = isolate->GetCurrentContext ();

  auto module = External::New (isolate, self);

  auto global = context->Global ();

  auto array_buffer = global->Get (context,
      _gum_v8_string_new_ascii (isolate, "ArrayBuffer")).ToLocalChecked ()
      .As<Object> ();
  array_buffer->Set (context, _gum_v8_string_new_ascii (isolate, "wrap"),
      Function::New (context, gumjs_array_buffer_wrap, module)
      .ToLocalChecked ()).Check ();
  auto array_buffer_proto = array_buffer->Get (context,
      _gum_v8_string_new_ascii (isolate, "prototype")).ToLocalChecked ()
      .As<Object> ();
  array_buffer_proto->Set (context,
      _gum_v8_string_new_ascii (isolate, "unwrap"),
      Function::New (context, gumjs_array_buffer_unwrap, module)
      .ToLocalChecked ()).Check ();

  self->native_functions = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_native_function_free);

  self->native_callbacks = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_native_callback_clear);

  self->native_resources = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) _gum_v8_native_resource_free);
  self->kernel_resources = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) _gum_v8_kernel_resource_free);

  self->match_patterns = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_match_pattern_free);

  self->source_maps = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_v8_source_map_free);

  Local<Value> zero = Integer::New (isolate, 0);

  auto int64 = Local<FunctionTemplate>::New (isolate, *self->int64);
  auto int64_value = int64->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 1, &zero).ToLocalChecked ();
  self->int64_value = new Global<Object> (isolate, int64_value);

  auto uint64 = Local<FunctionTemplate>::New (isolate, *self->uint64);
  auto uint64_value = uint64->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, 1, &zero).ToLocalChecked ();
  self->uint64_value = new Global<Object> (isolate, uint64_value);

  auto native_pointer = Local<FunctionTemplate>::New (isolate,
      *self->native_pointer);
  auto native_pointer_value = native_pointer->GetFunction (context)
      .ToLocalChecked ()->NewInstance (context, 1, &zero).ToLocalChecked ();
  self->native_pointer_value = new Global<Object> (isolate,
      native_pointer_value);
  self->handle_key = new Global<String> (isolate,
      _gum_v8_string_new_ascii (isolate, "handle"));

  self->abi_key = new Global<String> (isolate,
      _gum_v8_string_new_ascii (isolate, "abi"));
  self->scheduling_key = new Global<String> (isolate,
      _gum_v8_string_new_ascii (isolate, "scheduling"));
  self->exceptions_key = new Global<String> (isolate,
      _gum_v8_string_new_ascii (isolate, "exceptions"));
  self->traps_key = new Global<String> (isolate,
      _gum_v8_string_new_ascii (isolate, "traps"));
  auto value_key = _gum_v8_string_new_ascii (isolate, "value");
  self->value_key = new Global<String> (isolate, value_key);
  auto system_error_key =
      _gum_v8_string_new_ascii (isolate, GUMJS_SYSTEM_ERROR_FIELD);
  self->system_error_key = new Global<String> (isolate, system_error_key);

  auto native_return_value = Object::New (isolate);
  native_return_value->Set (context, value_key, zero).Check ();
  native_return_value->Set (context, system_error_key, zero).Check ();
  self->native_return_value = new Global<Object> (isolate, native_return_value);

  auto callback_context = Local<FunctionTemplate>::New (isolate,
      *self->callback_context);
  auto callback_context_value = callback_context->GetFunction (context)
      .ToLocalChecked ()->NewInstance (context, 0, nullptr).ToLocalChecked ();
  self->callback_context_value = new Global<Object> (isolate,
      callback_context_value);

  auto cpu_context = Local<FunctionTemplate>::New (isolate, *self->cpu_context);
  auto cpu_context_value = cpu_context->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context).ToLocalChecked ();
  self->cpu_context_value = new Global<Object> (isolate, cpu_context_value);
}

gboolean
_gum_v8_core_flush (GumV8Core * self,
                    GumV8FlushNotify flush_notify)
{
  gboolean done;

  self->flush_notify = flush_notify;

  g_mutex_lock (&self->event_mutex);
  self->event_source_available = FALSE;
  g_cond_broadcast (&self->event_cond);
  g_mutex_unlock (&self->event_mutex);
  g_main_loop_quit (self->event_loop);

  if (self->usage_count > 1)
    return FALSE;

  do
  {
    GHashTableIter iter;
    GumV8ScheduledCallback * callback;

    g_hash_table_iter_init (&iter, self->scheduled_callbacks);
    while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &callback))
    {
      _gum_v8_core_pin (self);
      g_source_destroy (callback->source);
    }
    g_hash_table_remove_all (self->scheduled_callbacks);

    if (self->usage_count > 1)
      return FALSE;

    gum_v8_core_clear_weak_refs (self);
  }
  while (g_hash_table_size (self->scheduled_callbacks) > 0 ||
      g_hash_table_size (self->weak_refs) > 0);

  done = self->usage_count == 1;
  if (done)
    self->flush_notify = NULL;

  return done;
}

static void
gum_v8_core_clear_weak_refs (GumV8Core * self)
{
  GHashTableIter iter;
  GumV8WeakRef * ref;

  g_hash_table_iter_init (&iter, self->weak_refs);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &ref))
  {
    gum_v8_weak_ref_clear (ref);
  }

  g_hash_table_remove_all (self->weak_refs);

  ScriptScope scope (self->script);
  gum_v8_core_invoke_pending_weak_callbacks (self, &scope);
}

void
_gum_v8_core_notify_flushed (GumV8Core * self,
                             GumV8FlushNotify func)
{
  auto callback = g_slice_new (GumV8FlushCallback);
  callback->func = func;
  callback->script = GUM_V8_SCRIPT (g_object_ref (self->script));

  auto source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) gum_v8_flush_callback_notify,
      callback, (GDestroyNotify) gum_v8_flush_callback_free);
  g_source_attach (source,
      gum_script_scheduler_get_js_context (self->scheduler));
  g_source_unref (source);
}

static void
gum_v8_flush_callback_free (GumV8FlushCallback * self)
{
  g_object_unref (self->script);

  g_slice_free (GumV8FlushCallback, self);
}

static gboolean
gum_v8_flush_callback_notify (GumV8FlushCallback * self)
{
  self->func (self->script);
  return FALSE;
}

void
_gum_v8_core_dispose (GumV8Core * self)
{
  g_hash_table_unref (self->source_maps);
  self->source_maps = NULL;

  g_hash_table_unref (self->match_patterns);
  self->match_patterns = NULL;

  g_hash_table_unref (self->kernel_resources);
  self->kernel_resources = NULL;
  g_hash_table_unref (self->native_resources);
  self->native_resources = NULL;

  g_hash_table_unref (self->native_callbacks);
  self->native_callbacks = NULL;

  g_hash_table_unref (self->native_functions);
  self->native_functions = NULL;

  g_clear_pointer (&self->unhandled_exception_sink, gum_v8_exception_sink_free);

  g_clear_pointer (&self->incoming_message_sink, gum_v8_message_sink_free);

  delete self->on_global_get;
  delete self->global_receiver;
  self->on_global_get = nullptr;
  self->global_receiver = nullptr;

  delete self->int64_value;
  self->int64_value = nullptr;

  delete self->uint64_value;
  self->uint64_value = nullptr;

  delete self->handle_key;
  delete self->native_pointer_value;
  self->handle_key = nullptr;
  self->native_pointer_value = nullptr;

  delete self->abi_key;
  delete self->scheduling_key;
  delete self->exceptions_key;
  delete self->traps_key;
  delete self->value_key;
  delete self->system_error_key;
  self->abi_key = nullptr;
  self->scheduling_key = nullptr;
  self->exceptions_key = nullptr;
  self->traps_key = nullptr;
  self->value_key = nullptr;
  self->system_error_key = nullptr;

  delete self->native_return_value;
  self->native_return_value = nullptr;

  delete self->callback_context_value;
  self->callback_context_value = nullptr;

  delete self->cpu_context_value;
  self->cpu_context_value = nullptr;
}

void
_gum_v8_core_finalize (GumV8Core * self)
{
  g_hash_table_unref (self->scheduled_callbacks);
  self->scheduled_callbacks = NULL;

  g_hash_table_unref (self->weak_refs);
  self->weak_refs = NULL;

  delete self->source_map;
  self->source_map = nullptr;

  delete self->match_pattern;
  self->match_pattern = nullptr;

  delete self->cpu_context;
  self->cpu_context = nullptr;

  delete self->callback_context;
  self->callback_context = nullptr;

  delete self->native_function;
  self->native_function = nullptr;

  delete self->native_pointer;
  self->native_pointer = nullptr;

  delete self->uint64;
  self->uint64 = nullptr;

  delete self->int64;
  self->int64 = nullptr;

  gum_exceptor_remove (self->exceptor, gum_v8_core_handle_crashed_js, self);
  g_object_unref (self->exceptor);
  self->exceptor = NULL;

  g_main_loop_unref (self->event_loop);
  self->event_loop = NULL;
  g_mutex_clear (&self->event_mutex);
  g_cond_clear (&self->event_cond);
}

void
_gum_v8_core_pin (GumV8Core * self)
{
  self->usage_count++;
}

void
_gum_v8_core_unpin (GumV8Core * self)
{
  self->usage_count--;
}

void
_gum_v8_core_on_unhandled_exception (GumV8Core * self,
                                     Local<Value> exception)
{
  if (self->unhandled_exception_sink == NULL)
    return;

  gum_v8_exception_sink_handle_exception (self->unhandled_exception_sink,
      exception);
}

void
_gum_v8_core_post (GumV8Core * self,
                   const gchar * message,
                   GBytes * data)
{
  gboolean delivered = FALSE;

  {
    Locker locker (self->isolate);

    if (self->incoming_message_sink != NULL)
    {
      ScriptScope scope (self->script);
      gum_v8_message_sink_post (self->incoming_message_sink, message, data);
      delivered = TRUE;
    }
  }

  if (delivered)
  {
    g_mutex_lock (&self->event_mutex);
    self->event_count++;
    g_cond_broadcast (&self->event_cond);
    g_mutex_unlock (&self->event_mutex);

    g_main_loop_quit (self->event_loop);
  }
  else
  {
    g_bytes_unref (data);
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_set_timeout)
{
  gum_v8_core_schedule_callback (core, args, FALSE);
}

GUMJS_DEFINE_FUNCTION (gumjs_set_interval)
{
  gum_v8_core_schedule_callback (core, args, TRUE);
}

static void
gum_v8_core_schedule_callback (GumV8Core * self,
                               const GumV8Args * args,
                               gboolean repeat)
{
  Local<Function> func;
  gsize delay;

  if (repeat)
  {
    if (!_gum_v8_args_parse (args, "FZ", &func, &delay))
      return;
  }
  else
  {
    delay = 0;
    if (!_gum_v8_args_parse (args, "F|Z", &func, &delay))
      return;
  }

  auto id = self->next_callback_id++;
  GSource * source;
  if (delay == 0)
    source = g_idle_source_new ();
  else
    source = g_timeout_source_new ((guint) delay);
  auto callback = gum_v8_scheduled_callback_new (id, repeat, source, self);
  callback->func = new Global<Function> (self->isolate, func);
  g_source_set_callback (source, (GSourceFunc) gum_v8_scheduled_callback_invoke,
      callback, (GDestroyNotify) gum_v8_scheduled_callback_free);

  g_hash_table_insert (self->scheduled_callbacks, GINT_TO_POINTER (id),
      callback);
  self->current_scope->AddScheduledSource (source);

  args->info->GetReturnValue ().Set (id);
}

static GumV8ScheduledCallback *
gum_v8_core_try_steal_scheduled_callback (GumV8Core * self,
                                          gint id)
{
  auto raw_id = GINT_TO_POINTER (id);

  auto callback = (GumV8ScheduledCallback *) g_hash_table_lookup (
      self->scheduled_callbacks, raw_id);
  if (callback == NULL)
    return NULL;

  g_hash_table_remove (self->scheduled_callbacks, raw_id);

  return callback;
}

GUMJS_DEFINE_FUNCTION (gumjs_clear_timer)
{
  if (info.Length () < 1 || !info[0]->IsNumber ())
  {
    info.GetReturnValue ().Set (false);
    return;
  }

  gint id;
  if (!_gum_v8_args_parse (args, "i", &id))
    return;

  auto callback = gum_v8_core_try_steal_scheduled_callback (core, id);
  if (callback != NULL)
  {
    _gum_v8_core_pin (core);
    g_source_destroy (callback->source);
  }

  info.GetReturnValue ().Set (callback != NULL);
}

static GumV8ScheduledCallback *
gum_v8_scheduled_callback_new (guint id,
                               gboolean repeat,
                               GSource * source,
                               GumV8Core * core)
{
  auto callback = g_slice_new (GumV8ScheduledCallback);

  callback->id = id;
  callback->repeat = repeat;
  callback->source = source;

  callback->core = core;

  return callback;
}

static void
gum_v8_scheduled_callback_free (GumV8ScheduledCallback * callback)
{
  auto core = callback->core;

  {
    ScriptScope scope (core->script);

    delete callback->func;

    _gum_v8_core_unpin (core);
  }

  g_slice_free (GumV8ScheduledCallback, callback);
}

static gboolean
gum_v8_scheduled_callback_invoke (GumV8ScheduledCallback * self)
{
  auto core = self->core;

  ScriptScope scope (core->script);
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto func = Local<Function>::New (isolate, *self->func);
  auto recv = Undefined (isolate);
  auto result = func->Call (context, recv, 0, nullptr);
  _gum_v8_ignore_result (result);

  if (!self->repeat)
  {
    if (gum_v8_core_try_steal_scheduled_callback (core, self->id) != NULL)
      _gum_v8_core_pin (core);
  }

  return self->repeat;
}

GUMJS_DEFINE_FUNCTION (gumjs_send)
{
  gchar * message;
  GBytes * data;
  if (!_gum_v8_args_parse (args, "sB?", &message, &data))
    return;

  /*
   * Synchronize Interceptor state before sending the message. The application
   * might be waiting for an acknowledgement that APIs have been instrumented.
   *
   * This is very important for the RPC API.
   */
  auto interceptor = core->script->interceptor.interceptor;
  gum_interceptor_end_transaction (interceptor);
  gum_interceptor_begin_transaction (interceptor);

  core->message_emitter (core->script, message, data);

  g_bytes_unref (data);
  g_free (message);
}

GUMJS_DEFINE_FUNCTION (gumjs_set_unhandled_exception_callback)
{
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "F?", &callback))
    return;

  auto new_sink = !callback.IsEmpty ()
      ? gum_v8_exception_sink_new (callback, isolate)
      : NULL;

  auto old_sink = core->unhandled_exception_sink;
  core->unhandled_exception_sink = new_sink;

  if (old_sink != NULL)
    gum_v8_exception_sink_free (old_sink);
}

GUMJS_DEFINE_FUNCTION (gumjs_set_incoming_message_callback)
{
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "F?", &callback))
    return;

  auto new_sink = !callback.IsEmpty ()
      ? gum_v8_message_sink_new (callback, isolate)
      : NULL;

  auto old_sink = core->incoming_message_sink;
  core->incoming_message_sink = new_sink;

  if (old_sink != NULL)
    gum_v8_message_sink_free (old_sink);
}

GUMJS_DEFINE_FUNCTION (gumjs_wait_for_event)
{
  g_mutex_lock (&core->event_mutex);
  auto start_count = core->event_count;
  g_mutex_unlock (&core->event_mutex);

  gboolean event_source_available;

  core->current_scope->PerformPendingIO ();

  {
    ScriptUnlocker unlocker (core);

    auto context = gum_script_scheduler_get_js_context (core->scheduler);
    gboolean called_from_js_thread = g_main_context_is_owner (context);

    g_mutex_lock (&core->event_mutex);

    while (core->event_count == start_count && core->event_source_available)
    {
      if (called_from_js_thread)
      {
        g_mutex_unlock (&core->event_mutex);
        g_main_loop_run (core->event_loop);
        g_mutex_lock (&core->event_mutex);
      }
      else
      {
        g_cond_wait (&core->event_cond, &core->event_mutex);
      }
    }

    event_source_available = core->event_source_available;

    g_mutex_unlock (&core->event_mutex);
  }

  if (!event_source_available)
    _gum_v8_throw_ascii_literal (isolate, "script is unloading");
}

static void
gumjs_global_get (Local<Name> property,
                  const PropertyCallbackInfo<Value> & info)
{
  auto self = (GumV8Core *) info.Data ().As<External> ()->Value ();

  if (self->on_global_get == nullptr)
    return;

  auto isolate = info.GetIsolate ();
  auto context = isolate->GetCurrentContext ();

  auto get (Local<Function>::New (isolate, *self->on_global_get));
  auto recv (Local<Object>::New (isolate, *self->global_receiver));
  Local<Value> argv[] = { property };
  Local<Value> result;
  if (get->Call (context, recv, G_N_ELEMENTS (argv), argv).ToLocal (&result) &&
      !result->IsUndefined ())
  {
    info.GetReturnValue ().Set (result);
  }
}

GUMJS_DEFINE_GETTER (gumjs_frida_get_heap_size)
{
  info.GetReturnValue ().Set (gum_peek_private_memory_usage ());
}

GUMJS_DEFINE_FUNCTION (gumjs_script_evaluate)
{
  gchar * name, * source;
  if (!_gum_v8_args_parse (args, "ss", &name, &source))
    return;

  auto context = isolate->GetCurrentContext ();

  auto source_str = String::NewFromUtf8 (isolate, source).ToLocalChecked ();

  auto resource_name = String::NewFromUtf8 (isolate, name).ToLocalChecked ();
  ScriptOrigin origin (isolate, resource_name);

  Local<Script> code;
  gchar * error_description = NULL;
  int line = -1;
  {
    TryCatch trycatch (isolate);
    auto maybe_code = Script::Compile (context, source_str, &origin);
    if (!maybe_code.ToLocal (&code))
    {
      error_description =
          _gum_v8_error_get_message (isolate, trycatch.Exception ());
      line = trycatch.Message ()->GetLineNumber (context).FromMaybe (-1);
    }
  }
  if (error_description != NULL)
  {
    _gum_v8_throw (isolate,
        "could not parse '%s' line %d: %s",
        name,
        line,
        error_description);
    g_free (error_description);
  }

  if (!code.IsEmpty ())
  {
    gchar * source_map = gum_query_script_for_inline_source_map (isolate, code);
    if (source_map != NULL)
    {
      _gum_v8_script_register_source_map (core->script, name,
          (gchar *) g_steal_pointer (&source_map));
    }

    Local<Value> result;
    auto maybe_result = code->Run (context);
    if (maybe_result.ToLocal (&result))
      info.GetReturnValue ().Set (result);
  }

  g_free (source);
  g_free (name);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_load)
{
  gchar * name, * source;
  if (!_gum_v8_args_parse (args, "ss", &name, &source))
    return;

  _gum_v8_script_load_module (core->script, name, source);

  g_free (source);
  g_free (name);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_register_source_map)
{
  gchar * name, * json;
  if (!_gum_v8_args_parse (args, "ss", &name, &json))
    return;

  _gum_v8_script_register_source_map (core->script, name,
      (gchar *) g_steal_pointer (&json));

  g_free (name);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_find_source_map)
{
  gchar * name;
  if (!_gum_v8_args_parse (args, "s", &name))
    return;

  const gchar * json = NULL;
  gchar * json_malloc_data = NULL;

  GumESProgram * program = core->script->program;
  if (program->es_assets != NULL)
  {
    gchar * map_name = g_strconcat (name, ".map", NULL);

    auto map_asset =
        (GumESAsset *) g_hash_table_lookup (program->es_assets, map_name);
    if (map_asset != NULL)
    {
      json = (const gchar *) map_asset->data;
    }

    g_free (map_name);
  }

  if (json == NULL && g_strcmp0 (name, program->global_filename) == 0)
  {
    json_malloc_data = gum_query_script_for_inline_source_map (isolate,
        Local<Script>::New (isolate, *program->global_code));
    json = json_malloc_data;
  }

  if (json != NULL)
  {
    Local<Object> map;
    if (gumjs_source_map_new (json, core).ToLocal (&map))
      info.GetReturnValue ().Set (map);
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }

  g_free (json_malloc_data);
  g_free (name);
}

static gchar *
gum_query_script_for_inline_source_map (Isolate * isolate,
                                        Local<Script> script)
{
  auto url_value = script->GetUnboundScript ()->GetSourceMappingURL ();
  if (!url_value->IsString ())
    return NULL;

  String::Utf8Value url_utf8 (isolate, url_value);
  auto url = *url_utf8;

  if (!g_str_has_prefix (url, "data:application/json;"))
    return NULL;

  auto base64_start = strstr (url, "base64,");
  if (base64_start == NULL)
    return NULL;
  base64_start += 7;

  gchar * result;
  gsize size;
  auto data = (gchar *) g_base64_decode (base64_start, &size);
  if (data != NULL && g_utf8_validate (data, size, NULL))
    result = g_strndup (data, size);
  else
    result = NULL;
  g_free (data);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_next_tick)
{
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "F", &callback))
    return;

  core->current_scope->AddTickCallback (callback);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_pin)
{
  _gum_v8_core_pin (core);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_unpin)
{
  _gum_v8_core_unpin (core);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_bind_weak)
{
  Local<Value> target;
  Local<Function> callback;
  if (!_gum_v8_args_parse (args, "VF", &target, &callback))
    return;

  if (target->IsNullOrUndefined ())
  {
    _gum_v8_throw_ascii_literal (isolate, "expected a heap value");
    return;
  }

  auto id = ++core->last_weak_ref_id;

  auto ref = gum_v8_weak_ref_new (id, target, callback, core);
  g_hash_table_insert (core->weak_refs, GUINT_TO_POINTER (id), ref);

  info.GetReturnValue ().Set (id);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_unbind_weak)
{
  guint id;
  if (!_gum_v8_args_parse (args, "u", &id))
    return;

  bool removed = !!g_hash_table_remove (core->weak_refs, GUINT_TO_POINTER (id));
  info.GetReturnValue ().Set (removed);
}

static GumV8WeakRef *
gum_v8_weak_ref_new (guint id,
                     Local<Value> target,
                     Local<Function> callback,
                     GumV8Core * core)
{
  auto ref = g_slice_new (GumV8WeakRef);

  ref->id = id;
  ref->target = new Global<Value> (core->isolate, target);
  ref->target->SetWeak (ref, gum_v8_weak_ref_on_weak_notify,
      WeakCallbackType::kParameter);
  ref->callback = new Global<Function> (core->isolate, callback);

  ref->core = core;

  return ref;
}

static void
gum_v8_weak_ref_clear (GumV8WeakRef * ref)
{
  delete ref->target;
  ref->target = nullptr;
}

static void
gum_v8_weak_ref_free (GumV8WeakRef * ref)
{
  auto core = ref->core;

  gboolean in_teardown = ref->target == nullptr;

  gum_v8_weak_ref_clear (ref);

  g_queue_push_tail (&core->pending_weak_callbacks, ref->callback);
  if (!in_teardown && core->pending_weak_source == NULL)
  {
    auto source = g_idle_source_new ();
    g_source_set_callback (source,
        (GSourceFunc) gum_v8_core_invoke_pending_weak_callbacks_in_idle, core,
        NULL);
    g_source_attach (source,
        gum_script_scheduler_get_js_context (core->scheduler));
    g_source_unref (source);

    _gum_v8_core_pin (core);

    core->pending_weak_source = source;
  }

  g_slice_free (GumV8WeakRef, ref);
}

static void
gum_v8_weak_ref_on_weak_notify (const WeakCallbackInfo<GumV8WeakRef> & info)
{
  auto self = info.GetParameter ();

  g_hash_table_remove (self->core->weak_refs, GUINT_TO_POINTER (self->id));
}

static gboolean
gum_v8_core_invoke_pending_weak_callbacks_in_idle (GumV8Core * self)
{
  ScriptScope scope (self->script);

  self->pending_weak_source = NULL;

  gum_v8_core_invoke_pending_weak_callbacks (self, &scope);

  _gum_v8_core_unpin (self);

  return FALSE;
}

static void
gum_v8_core_invoke_pending_weak_callbacks (GumV8Core * self,
                                           ScriptScope * scope)
{
  auto isolate = self->isolate;
  auto context = isolate->GetCurrentContext ();

  auto recv = Undefined (isolate);

  Global<Function> * weak_callback;
  while ((weak_callback = (Global<Function> *)
      g_queue_pop_head (&self->pending_weak_callbacks)) != nullptr)
  {
    auto callback = Local<Function>::New (isolate, *weak_callback);

    auto result = callback->Call (context, recv, 0, nullptr);
    if (result.IsEmpty ())
      scope->ProcessAnyPendingException ();

    delete weak_callback;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_script_set_global_access_handler)
{
  Local<Function> on_get;
  Local<Object> callbacks;
  gboolean has_callbacks = !(info.Length () > 0 && info[0]->IsNull ());
  if (has_callbacks)
  {
    if (!_gum_v8_args_parse (args, "F{get}", &on_get))
      return;
    callbacks = info[0].As<Object> ();
  }

  delete core->on_global_get;
  delete core->global_receiver;
  core->on_global_get = nullptr;
  core->global_receiver = nullptr;

  if (has_callbacks)
  {
    core->on_global_get =
        new Global<Function> (isolate, on_get.As<Function> ());
    core->global_receiver = new Global<Object> (isolate, callbacks);
  }
}

void
_gum_v8_core_push_job (GumV8Core * self,
                       GumScriptJobFunc job_func,
                       gpointer data,
                       GDestroyNotify data_destroy)
{
  gum_script_scheduler_push_job_on_thread_pool (self->scheduler, job_func,
      data, data_destroy);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_int64_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate, "use `new Int64()` to create a new "
        "instance, or use the shorthand: `int64()`");
    return;
  }

  gint64 value;
  if (!_gum_v8_args_parse (args, "q~", &value))
    return;

  _gum_v8_int64_set_value (wrapper, value, isolate);
}

#define GUM_DEFINE_INT64_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_int64_##name) \
    { \
      gint64 lhs = _gum_v8_int64_get_value (info.Holder ()); \
      \
      gint64 rhs; \
      if (!_gum_v8_args_parse (args, "q~", &rhs)) \
        return; \
      \
      gint64 result = lhs op rhs; \
      \
      info.GetReturnValue ().Set (_gum_v8_int64_new (result, core)); \
    }

GUM_DEFINE_INT64_OP_IMPL (add, +)
GUM_DEFINE_INT64_OP_IMPL (sub, -)
GUM_DEFINE_INT64_OP_IMPL (and, &)
GUM_DEFINE_INT64_OP_IMPL (or,  |)
GUM_DEFINE_INT64_OP_IMPL (xor, ^)
GUM_DEFINE_INT64_OP_IMPL (shr, >>)
GUM_DEFINE_INT64_OP_IMPL (shl, <<)

#define GUM_DEFINE_INT64_UNARY_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_int64_##name) \
    { \
      gint64 value = _gum_v8_int64_get_value (info.Holder ()); \
      \
      gint64 result = op value; \
      \
      info.GetReturnValue ().Set (_gum_v8_int64_new (result, core)); \
    }

GUM_DEFINE_INT64_UNARY_OP_IMPL (not, ~)

GUMJS_DEFINE_FUNCTION (gumjs_int64_compare)
{
  gint64 lhs = _gum_v8_int64_get_value (info.Holder ());

  gint64 rhs;
  if (!_gum_v8_args_parse (args, "q~", &rhs))
    return;

  int32_t result = (lhs == rhs) ? 0 : ((lhs < rhs) ? -1 : 1);

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_to_number)
{
  info.GetReturnValue ().Set (
      (double) _gum_v8_int64_get_value (info.Holder ()));
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_to_string)
{
  gint radix = 10;
  if (!_gum_v8_args_parse (args, "|u", &radix))
    return;
  if (radix != 10 && radix != 16)
  {
    _gum_v8_throw_ascii_literal (isolate, "unsupported radix");
    return;
  }

  auto value = _gum_v8_int64_get_value (info.Holder ());

  gchar str[32];
  if (radix == 10)
    g_sprintf (str, "%" G_GINT64_FORMAT, value);
  else if (value >= 0)
    g_sprintf (str, "%" G_GINT64_MODIFIER "x", value);
  else
    g_sprintf (str, "-%" G_GINT64_MODIFIER "x", -value);

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, str));
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_to_json)
{
  gchar str[32];
  g_sprintf (str, "%" G_GINT64_FORMAT,
      _gum_v8_int64_get_value (info.Holder ()));

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, str));
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_value_of)
{
  info.GetReturnValue ().Set (
      (double) _gum_v8_int64_get_value (info.Holder ()));
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_uint64_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate, "use `new UInt64()` to create a new "
        "instance, or use the shorthand: `uint64()`");
    return;
  }

  guint64 value;
  if (!_gum_v8_args_parse (args, "Q~", &value))
    return;

  _gum_v8_uint64_set_value (wrapper, value, isolate);
}

#define GUM_DEFINE_UINT64_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_uint64_##name) \
    { \
      guint64 lhs = _gum_v8_uint64_get_value (info.Holder ()); \
      \
      guint64 rhs; \
      if (!_gum_v8_args_parse (args, "Q~", &rhs)) \
        return; \
      \
      guint64 result = lhs op rhs; \
      \
      info.GetReturnValue ().Set (_gum_v8_uint64_new (result, core)); \
    }

GUM_DEFINE_UINT64_OP_IMPL (add, +)
GUM_DEFINE_UINT64_OP_IMPL (sub, -)
GUM_DEFINE_UINT64_OP_IMPL (and, &)
GUM_DEFINE_UINT64_OP_IMPL (or,  |)
GUM_DEFINE_UINT64_OP_IMPL (xor, ^)
GUM_DEFINE_UINT64_OP_IMPL (shr, >>)
GUM_DEFINE_UINT64_OP_IMPL (shl, <<)

#define GUM_DEFINE_UINT64_UNARY_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_uint64_##name) \
    { \
      guint64 value = _gum_v8_uint64_get_value (info.Holder ()); \
      \
      guint64 result = op value; \
      \
      info.GetReturnValue ().Set (_gum_v8_uint64_new (result, core)); \
    }

GUM_DEFINE_UINT64_UNARY_OP_IMPL (not, ~)

GUMJS_DEFINE_FUNCTION (gumjs_uint64_compare)
{
  guint64 lhs = _gum_v8_uint64_get_value (info.Holder ());

  guint64 rhs;
  if (!_gum_v8_args_parse (args, "Q~", &rhs))
    return;

  int32_t result = (lhs == rhs) ? 0 : ((lhs < rhs) ? -1 : 1);

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_to_number)
{
  info.GetReturnValue ().Set (
      (double) _gum_v8_uint64_get_value (info.Holder ()));
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_to_string)
{
  gint radix = 10;
  if (!_gum_v8_args_parse (args, "|u", &radix))
    return;
  if (radix != 10 && radix != 16)
  {
    _gum_v8_throw_ascii_literal (isolate, "unsupported radix");
    return;
  }

  auto value = _gum_v8_uint64_get_value (info.Holder ());

  gchar str[32];
  if (radix == 10)
    g_sprintf (str, "%" G_GUINT64_FORMAT, value);
  else
    g_sprintf (str, "%" G_GINT64_MODIFIER "x", value);

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, str));
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_to_json)
{
  gchar str[32];
  g_sprintf (str, "%" G_GUINT64_FORMAT,
      _gum_v8_uint64_get_value (info.Holder ()));

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, str));
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_value_of)
{
  info.GetReturnValue ().Set (
      (double) _gum_v8_uint64_get_value (info.Holder ()));
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_pointer_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate, "use `new NativePointer()` to "
        "create a new instance, or use one of the two shorthands: "
        "`ptr()` and `NULL`");
    return;
  }

  gpointer ptr;
  if (!_gum_v8_args_parse (args, "p~", &ptr))
    return;

  wrapper->SetInternalField (0,
      BigInt::NewFromUnsigned (isolate, GPOINTER_TO_SIZE (ptr)));
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_is_null)
{
  info.GetReturnValue ().Set (GUMJS_NATIVE_POINTER_VALUE (info.Holder ()) == 0);
}

#define GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_##name) \
    { \
      gpointer lhs_ptr = GUMJS_NATIVE_POINTER_VALUE (info.Holder ()); \
      \
      gpointer rhs_ptr; \
      if (!_gum_v8_args_parse (args, "p~", &rhs_ptr)) \
        return; \
      \
      gsize lhs = GPOINTER_TO_SIZE (lhs_ptr); \
      gsize rhs = GPOINTER_TO_SIZE (rhs_ptr); \
      \
      gpointer result = GSIZE_TO_POINTER (lhs op rhs); \
      \
      info.GetReturnValue ().Set (_gum_v8_native_pointer_new (result, core)); \
    }

GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL (add, +)
GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL (sub, -)
GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL (and, &)
GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL (or,  |)
GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL (xor, ^)
GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL (shr, >>)
GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL (shl, <<)

#define GUM_DEFINE_NATIVE_POINTER_UNARY_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_##name) \
    { \
      gsize v = \
          GPOINTER_TO_SIZE (GUMJS_NATIVE_POINTER_VALUE (info.Holder ())); \
      \
      gpointer result = GSIZE_TO_POINTER (op v); \
      \
      info.GetReturnValue ().Set (_gum_v8_native_pointer_new (result, core)); \
    }

GUM_DEFINE_NATIVE_POINTER_UNARY_OP_IMPL (not, ~)

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_sign)
{
#ifdef HAVE_PTRAUTH
  gpointer value = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());

  gchar * key = NULL;
  gpointer data = NULL;
  if (!_gum_v8_args_parse (args, "|sp~", &key, &data))
    return;

  bool valid = true;
  if (key == NULL || strcmp (key, "ia") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asia, data);
  else if (strcmp (key, "ib") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asib, data);
  else if (strcmp (key, "da") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asda, data);
  else if (strcmp (key, "db") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asdb, data);
  else
    valid = false;

  g_free (key);

  if (!valid)
  {
    _gum_v8_throw (isolate, "invalid key");
    return;
  }

  info.GetReturnValue ().Set (_gum_v8_native_pointer_new (value, core));
#else
  info.GetReturnValue ().Set (info.This ());
#endif
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_strip)
{
#ifdef HAVE_PTRAUTH
  gpointer value = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());

  gchar * key = NULL;
  if (!_gum_v8_args_parse (args, "|s", &key))
    return;

  bool valid = true;
  if (key == NULL || strcmp (key, "ia") == 0)
    value = ptrauth_strip (value, ptrauth_key_asia);
  else if (strcmp (key, "ib") == 0)
    value = ptrauth_strip (value, ptrauth_key_asib);
  else if (strcmp (key, "da") == 0)
    value = ptrauth_strip (value, ptrauth_key_asda);
  else if (strcmp (key, "db") == 0)
    value = ptrauth_strip (value, ptrauth_key_asdb);
  else
    valid = false;

  g_free (key);

  if (!valid)
  {
    _gum_v8_throw (isolate, "invalid key");
    return;
  }

  info.GetReturnValue ().Set (_gum_v8_native_pointer_new (value, core));
#elif defined (HAVE_ANDROID) && defined (HAVE_ARM64)
  gpointer value = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());

  /* https://source.android.com/devices/tech/debug/tagged-pointers */
  gpointer value_without_top_byte = GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (value) & G_GUINT64_CONSTANT (0x00ffffffffffffff));

  if (value_without_top_byte == value)
  {
    info.GetReturnValue ().Set (info.This ());
    return;
  }

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (value_without_top_byte, core));
#else
  info.GetReturnValue ().Set (info.This ());
#endif
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_blend)
{
#ifdef HAVE_PTRAUTH
  gpointer value = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());

  guint small_integer;
  if (!_gum_v8_args_parse (args, "u", &small_integer))
    return;

  value = GSIZE_TO_POINTER (ptrauth_blend_discriminator (value, small_integer));

  info.GetReturnValue ().Set (_gum_v8_native_pointer_new (value, core));
#else
  info.GetReturnValue ().Set (info.This ());
#endif
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_compare)
{
  gpointer lhs_ptr = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());

  gpointer rhs_ptr;
  if (!_gum_v8_args_parse (args, "p~", &rhs_ptr))
    return;

  gsize lhs = GPOINTER_TO_SIZE (lhs_ptr);
  gsize rhs = GPOINTER_TO_SIZE (rhs_ptr);

  int32_t result = (lhs == rhs) ? 0 : ((lhs < rhs) ? -1 : 1);

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_int32)
{
  info.GetReturnValue ().Set ((int32_t) GPOINTER_TO_SIZE (
      GUMJS_NATIVE_POINTER_VALUE (info.Holder ())));
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_uint32)
{
  info.GetReturnValue ().Set ((uint32_t) GPOINTER_TO_SIZE (
      GUMJS_NATIVE_POINTER_VALUE (info.Holder ())));
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_string)
{
  gint radix = 0;
  if (!_gum_v8_args_parse (args, "|u", &radix))
    return;
  gboolean radix_specified = radix != 0;
  if (!radix_specified)
  {
    radix = 16;
  }
  else if (radix != 10 && radix != 16)
  {
    _gum_v8_throw_ascii_literal (isolate, "unsupported radix");
    return;
  }

  gsize ptr = GPOINTER_TO_SIZE (GUMJS_NATIVE_POINTER_VALUE (info.Holder ()));

  gchar str[32];
  if (radix == 10)
  {
    g_sprintf (str, "%" G_GSIZE_MODIFIER "u", ptr);
  }
  else
  {
    if (radix_specified)
      g_sprintf (str, "%" G_GSIZE_MODIFIER "x", ptr);
    else
      g_sprintf (str, "0x%" G_GSIZE_MODIFIER "x", ptr);
  }

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, str));
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_json)
{
  gsize ptr = GPOINTER_TO_SIZE (GUMJS_NATIVE_POINTER_VALUE (info.Holder ()));

  gchar str[32];
  g_sprintf (str, "0x%" G_GSIZE_MODIFIER "x", ptr);

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, str));
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_match_pattern)
{
  gchar result[24];
  gint src, dst;
  const gint num_bits = GLIB_SIZEOF_VOID_P * 8;
  gsize ptr = GPOINTER_TO_SIZE (GUMJS_NATIVE_POINTER_VALUE (info.Holder ()));
  const gchar nibble_to_char[] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      'a', 'b', 'c', 'd', 'e', 'f'
  };

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  for (src = 0, dst = 0; src != num_bits; src += 8)
#else
  for (src = num_bits - 8, dst = 0; src >= 0; src -= 8)
#endif
  {
    if (dst != 0)
      result[dst++] = ' ';
    result[dst++] = nibble_to_char[(ptr >> (src + 4)) & 0xf];
    result[dst++] = nibble_to_char[(ptr >> (src + 0)) & 0xf];
  }
  result[dst] = '\0';

  info.GetReturnValue ().Set (_gum_v8_string_new_ascii (isolate, result));
}

static void
gumjs_native_pointer_handle_read (const FunctionCallbackInfo<Value> & info,
                                  GumMemoryValueType type,
                                  const GumV8Args * args)
{
  auto core = args->core;
  auto isolate = core->isolate;
  auto exceptor = core->exceptor;
  gpointer address = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());
  gssize length = -1;
  GumExceptorScope scope;
  Local<Value> result;
  std::shared_ptr<BackingStore> store;

  switch (type)
  {
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
      if (!_gum_v8_args_parse (args, "Z", &length))
        return;
      break;
    case GUM_MEMORY_VALUE_C_STRING:
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
    case GUM_MEMORY_VALUE_ANSI_STRING:
      if (!_gum_v8_args_parse (args, "|z", &length))
        return;
      break;
    default:
      break;
  }

  if (gum_exceptor_try (exceptor, &scope))
  {
    switch (type)
    {
      case GUM_MEMORY_VALUE_POINTER:
        result = _gum_v8_native_pointer_new (*((gpointer *) address), core);
        break;
      case GUM_MEMORY_VALUE_S8:
        result = Integer::New (isolate, *((gint8 *) address));
        break;
      case GUM_MEMORY_VALUE_U8:
        result = Integer::NewFromUnsigned (isolate, *((guint8 *) address));
        break;
      case GUM_MEMORY_VALUE_S16:
        result = Integer::New (isolate, *((gint16 *) address));
        break;
      case GUM_MEMORY_VALUE_U16:
        result = Integer::NewFromUnsigned (isolate, *((guint16 *) address));
        break;
      case GUM_MEMORY_VALUE_S32:
        result = Integer::New (isolate, *((gint32 *) address));
        break;
      case GUM_MEMORY_VALUE_U32:
        result = Integer::NewFromUnsigned (isolate, *((guint32 *) address));
        break;
      case GUM_MEMORY_VALUE_S64:
        result = _gum_v8_int64_new (*((gint64 *) address), core);
        break;
      case GUM_MEMORY_VALUE_U64:
        result = _gum_v8_uint64_new (*((guint64 *) address), core);
        break;
      case GUM_MEMORY_VALUE_LONG:
        result = _gum_v8_int64_new (*((glong *) address), core);
        break;
      case GUM_MEMORY_VALUE_ULONG:
        result = _gum_v8_uint64_new (*((gulong *) address), core);
        break;
      case GUM_MEMORY_VALUE_FLOAT:
        result = Number::New (isolate, *((gfloat *) address));
        break;
      case GUM_MEMORY_VALUE_DOUBLE:
        result = Number::New (isolate, *((gdouble *) address));
        break;
      case GUM_MEMORY_VALUE_BYTE_ARRAY:
      {
        auto data = (guint8 *) address;
        if (data == NULL)
        {
          result = Null (isolate);
          break;
        }

        if (length > 0)
        {
          result = ArrayBuffer::New (isolate, length);
          store = result.As<ArrayBuffer> ()->GetBackingStore ();
          memcpy (store->Data (), data, length);
        }
        else
        {
          result = ArrayBuffer::New (isolate, 0);
        }

        break;
      }
      case GUM_MEMORY_VALUE_C_STRING:
      {
        auto data = (gchar *) address;
        if (data == NULL)
        {
          result = Null (isolate);
          break;
        }

        if (length != 0)
        {
          guint8 dummy_to_trap_bad_pointer_early;
          memcpy (&dummy_to_trap_bad_pointer_early, data, sizeof (guint8));

          gchar * str = g_utf8_make_valid (data, length);
          result = String::NewFromUtf8 (isolate, str).ToLocalChecked ();
          g_free (str);
        }
        else
        {
          result = String::Empty (isolate);
        }

        break;
      }
      case GUM_MEMORY_VALUE_UTF8_STRING:
      {
        auto data = (gchar *) address;
        if (data == NULL)
        {
          result = Null (isolate);
          break;
        }

        if (length != 0)
        {
          guint8 dummy_to_trap_bad_pointer_early;
          memcpy (&dummy_to_trap_bad_pointer_early, data, sizeof (guint8));

          const gchar * end;
          if (!g_utf8_validate (data, length, &end))
          {
            _gum_v8_throw_ascii (isolate,
                "can't decode byte 0x%02x in position %u",
                (guint8) *end, (guint) (end - data));
            break;
          }

          result = String::NewFromUtf8 (isolate, data, NewStringType::kNormal,
              length).ToLocalChecked ();
        }
        else
        {
          result = String::Empty (isolate);
        }

        break;
      }
      case GUM_MEMORY_VALUE_UTF16_STRING:
      {
        auto str_utf16 = (gunichar2 *) address;
        if (str_utf16 == NULL)
        {
          result = Null (isolate);
          break;
        }

        if (length != 0)
        {
          guint8 dummy_to_trap_bad_pointer_early;
          memcpy (&dummy_to_trap_bad_pointer_early, str_utf16, sizeof (guint8));
        }

        glong size;
        auto str_utf8 = g_utf16_to_utf8 (str_utf16, length, NULL, &size, NULL);
        if (str_utf8 == NULL)
        {
          _gum_v8_throw_ascii_literal (isolate, "invalid string");
          break;
        }

        if (size != 0)
        {
          result = String::NewFromUtf8 (isolate, str_utf8,
              NewStringType::kNormal, size).ToLocalChecked ();
        }
        else
        {
          result = String::Empty (isolate);
        }

        g_free (str_utf8);

        break;
      }
      case GUM_MEMORY_VALUE_ANSI_STRING:
      {
#ifdef HAVE_WINDOWS
        auto str_ansi = (gchar *) address;
        if (str_ansi == NULL)
        {
          result = Null (isolate);
          break;
        }

        if (length != 0)
        {
          guint8 dummy_to_trap_bad_pointer_early;
          memcpy (&dummy_to_trap_bad_pointer_early, str_ansi, sizeof (guint8));

          auto str_utf8 = _gum_ansi_string_to_utf8 (str_ansi, length);
          auto size = g_utf8_offset_to_pointer (str_utf8,
              g_utf8_strlen (str_utf8, -1)) - str_utf8;
          result = String::NewFromUtf8 (isolate, str_utf8,
              NewStringType::kNormal, size).ToLocalChecked ();
          g_free (str_utf8);
        }
        else
        {
          result = String::Empty (isolate);
        }
#else
        _gum_v8_throw_ascii_literal (isolate,
            "ANSI API is only applicable on Windows");
#endif

        break;
      }
      default:
        g_assert_not_reached ();
    }
  }

  if (gum_exceptor_catch (exceptor, &scope))
  {
    _gum_v8_throw_native (&scope.exception, core);
  }
  else
  {
    if (!result.IsEmpty ())
      info.GetReturnValue ().Set (result);
  }
}

static void
gumjs_native_pointer_handle_write (const FunctionCallbackInfo<Value> & info,
                                   GumMemoryValueType type,
                                   const GumV8Args * args)
{
  gpointer address = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());
  gpointer pointer = NULL;
  gssize s = 0;
  gsize u = 0;
  gint64 s64 = 0;
  guint64 u64 = 0;
  gdouble number = 0;
  GBytes * bytes = NULL;
  gchar * str = NULL;
  gsize str_length = 0;
  gunichar2 * str_utf16 = NULL;
#ifdef HAVE_WINDOWS
  gchar * str_ansi = NULL;
#endif
  auto core = args->core;
  auto exceptor = core->exceptor;
  GumExceptorScope scope;

  switch (type)
  {
    case GUM_MEMORY_VALUE_POINTER:
      if (!_gum_v8_args_parse (args, "p", &pointer))
        return;
      break;
    case GUM_MEMORY_VALUE_S8:
    case GUM_MEMORY_VALUE_S16:
    case GUM_MEMORY_VALUE_S32:
      if (!_gum_v8_args_parse (args, "z", &s))
        return;
      break;
    case GUM_MEMORY_VALUE_U8:
    case GUM_MEMORY_VALUE_U16:
    case GUM_MEMORY_VALUE_U32:
      if (!_gum_v8_args_parse (args, "Z", &u))
        return;
      break;
    case GUM_MEMORY_VALUE_S64:
    case GUM_MEMORY_VALUE_LONG:
      if (!_gum_v8_args_parse (args, "q", &s64))
        return;
      break;
    case GUM_MEMORY_VALUE_U64:
    case GUM_MEMORY_VALUE_ULONG:
      if (!_gum_v8_args_parse (args, "Q", &u64))
        return;
      break;
    case GUM_MEMORY_VALUE_FLOAT:
    case GUM_MEMORY_VALUE_DOUBLE:
      if (!_gum_v8_args_parse (args, "n", &number))
        return;
      break;
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
      if (!_gum_v8_args_parse (args, "B", &bytes))
        return;
      break;
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
    case GUM_MEMORY_VALUE_ANSI_STRING:
      if (!_gum_v8_args_parse (args, "s", &str))
        return;

      str_length = g_utf8_strlen (str, -1);
      if (type == GUM_MEMORY_VALUE_UTF16_STRING)
        str_utf16 = g_utf8_to_utf16 (str, -1, NULL, NULL, NULL);
#ifdef HAVE_WINDOWS
      else if (type == GUM_MEMORY_VALUE_ANSI_STRING)
        str_ansi = _gum_ansi_string_from_utf8 (str);
#endif
      break;
    default:
      g_assert_not_reached ();
  }

  if (gum_exceptor_try (exceptor, &scope))
  {
    switch (type)
    {
      case GUM_MEMORY_VALUE_POINTER:
        *((gpointer *) address) = pointer;
        break;
      case GUM_MEMORY_VALUE_S8:
        *((gint8 *) address) = (gint8) s;
        break;
      case GUM_MEMORY_VALUE_U8:
        *((guint8 *) address) = (guint8) u;
        break;
      case GUM_MEMORY_VALUE_S16:
        *((gint16 *) address) = (gint16) s;
        break;
      case GUM_MEMORY_VALUE_U16:
        *((guint16 *) address) = (guint16) u;
        break;
      case GUM_MEMORY_VALUE_S32:
        *((gint32 *) address) = (gint32) s;
        break;
      case GUM_MEMORY_VALUE_U32:
        *((guint32 *) address) = (guint32) u;
        break;
      case GUM_MEMORY_VALUE_S64:
        *((gint64 *) address) = s64;
        break;
      case GUM_MEMORY_VALUE_U64:
        *((guint64 *) address) = u64;
        break;
      case GUM_MEMORY_VALUE_LONG:
        *((glong *) address) = s64;
        break;
      case GUM_MEMORY_VALUE_ULONG:
        *((gulong *) address) = u64;
        break;
      case GUM_MEMORY_VALUE_FLOAT:
        *((gfloat *) address) = number;
        break;
      case GUM_MEMORY_VALUE_DOUBLE:
        *((gdouble *) address) = number;
        break;
      case GUM_MEMORY_VALUE_BYTE_ARRAY:
      {
        gsize size;
        auto data = g_bytes_get_data (bytes, &size);
        memcpy (address, data, size);
        break;
      }
      case GUM_MEMORY_VALUE_UTF8_STRING:
      {
        gsize size = g_utf8_offset_to_pointer (str, str_length) - str + 1;
        memcpy (address, str, size);
        break;
      }
      case GUM_MEMORY_VALUE_UTF16_STRING:
      {
        gsize size = (str_length + 1) * sizeof (gunichar2);
        memcpy (address, str_utf16, size);
        break;
      }
      case GUM_MEMORY_VALUE_ANSI_STRING:
      {
#ifdef HAVE_WINDOWS
        strcpy ((char *) address, str_ansi);
#else
        _gum_v8_throw_ascii_literal (core->isolate,
            "ANSI API is only applicable on Windows");
#endif
        break;
      }
      default:
        g_assert_not_reached ();
    }
  }

  if (gum_exceptor_catch (exceptor, &scope))
  {
    _gum_v8_throw_native (&scope.exception, core);
  }

  g_bytes_unref (bytes);
  g_free (str);
  g_free (str_utf16);
#ifdef HAVE_WINDOWS
  g_free (str_ansi);
#endif

  info.GetReturnValue ().Set (info.This ());
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_read_volatile)
{
  gpointer address = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());

  gsize length;
  if (!_gum_v8_args_parse (args, "z", &length))
    return;

  gsize n_bytes_read;
  guint8 * data = gum_memory_read (address, length, &n_bytes_read);
  if (data == NULL)
  {
    _gum_v8_throw_ascii_literal (isolate, "memory read failed");
    return;
  }

  Local<Value> result = ArrayBuffer::New (isolate, n_bytes_read);
  memcpy (result.As<ArrayBuffer> ()->GetBackingStore ()->Data (), data, length);
  info.GetReturnValue ().Set (result);

  g_free (data);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_write_volatile)
{
  gpointer address = GUMJS_NATIVE_POINTER_VALUE (info.Holder ());

  GBytes * bytes;
  if (!_gum_v8_args_parse (args, "B", &bytes))
    return;

  gsize size;
  auto data = g_bytes_get_data (bytes, &size);

  if (!gum_memory_write (address, (const guint8 *) data, size))
    _gum_v8_throw_ascii_literal (isolate, "memory write failed");
}

GUMJS_DEFINE_FUNCTION (gumjs_array_buffer_wrap)
{
  Local<Value> result;

  gpointer address;
  gsize size;
  if (!_gum_v8_args_parse (args, "pZ", &address, &size))
    return;

  if (address != NULL && size > 0)
  {
    result = ArrayBuffer::New (isolate, ArrayBuffer::NewBackingStore (address,
        size, BackingStore::EmptyDeleter, nullptr));
  }
  else
  {
    result = ArrayBuffer::New (isolate, 0);
  }

  info.GetReturnValue ().Set (result);
}

GUMJS_DEFINE_FUNCTION (gumjs_array_buffer_unwrap)
{
  auto receiver = info.This ();
  if (!receiver->IsArrayBuffer ())
  {
    _gum_v8_throw_ascii_literal (isolate, "receiver must be an ArrayBuffer");
    return;
  }

  auto store = receiver.As<ArrayBuffer> ()->GetBackingStore ();
  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (store->Data (), core));
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_function_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new NativeFunction()` to create a new instance");
    return;
  }

  GumV8NativeFunctionParams params;
  if (!gum_v8_native_function_params_init (&params, GUM_V8_RETURN_PLAIN, args))
    return;

  gumjs_native_function_init (wrapper, &params, core);
}

static void
gumjs_native_function_invoke (const FunctionCallbackInfo<Value> & info)
{
  auto self = (GumV8NativeFunction *)
      info.Holder ()->GetAlignedPointerFromInternalField (1);

  gum_v8_native_function_invoke (self, self->implementation, info, 0, nullptr);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_function_call)
{
  auto num_args = info.Length ();

  Local<Object> receiver;
  if (num_args >= 1)
  {
    Local<Value> receiver_value = info[0];
    if (!receiver_value->IsNullOrUndefined ())
    {
      if (receiver_value->IsObject ())
      {
        receiver = receiver_value.As<Object> ();
      }
      else
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid receiver");
        return;
      }
    }
  }

  GumV8NativeFunction * func;
  GCallback implementation;
  if (!gumjs_native_function_get (info, receiver, core, &func, &implementation))
    return;

  uint32_t argc = MAX ((int) num_args - 1, 0);

  Local<Value> * argv;
  if (argc > 0)
  {
    argv = g_newa (Local<Value>, argc);
    for (uint32_t i = 0; i != argc; i++)
    {
      new (&argv[i]) Local<Value> ();
      argv[i] = info[1 + i];
    }
  }
  else
  {
    argv = g_newa (Local<Value>, 1);
  }

  gum_v8_native_function_invoke (func, implementation, info, argc, argv);

  for (uint32_t i = 0; i != argc; i++)
    argv[i].~Local<Value> ();
}

GUMJS_DEFINE_FUNCTION (gumjs_native_function_apply)
{
  auto num_args = info.Length ();
  if (num_args < 1)
  {
    _gum_v8_throw_ascii_literal (isolate, "missing argument");
    return;
  }

  Local<Object> receiver;
  Local<Value> receiver_value = info[0];
  if (!receiver_value->IsNullOrUndefined ())
  {
    if (receiver_value->IsObject ())
    {
      receiver = receiver_value.As<Object> ();
    }
    else
    {
      _gum_v8_throw_ascii_literal (isolate, "invalid receiver");
      return;
    }
  }

  Local<Array> argv_array;
  if (num_args >= 2)
  {
    Local<Value> value = info[1];
    if (!value->IsNullOrUndefined ())
    {
      if (!value->IsArray ())
      {
        _gum_v8_throw_ascii_literal (isolate, "expected an array");
        return;
      }
      argv_array = value.As<Array> ();
    }
  }

  GumV8NativeFunction * func;
  GCallback implementation;
  if (!gumjs_native_function_get (info, receiver, core, &func, &implementation))
    return;

  uint32_t argc = (!argv_array.IsEmpty ()) ? argv_array->Length () : 0;

  Local<Value> * argv;
  if (argc > 0)
  {
    auto context = isolate->GetCurrentContext ();

    argv = g_newa (Local<Value>, argc);
    for (uint32_t i = 0; i != argc; i++)
    {
      new (&argv[i]) Local<Value> ();
      if (!argv_array->Get (context, i).ToLocal (&argv[i]))
      {
        for (uint32_t j = 0; j <= i; j++)
          argv[j].~Local<Value> ();
        return;
      }
    }
  }
  else
  {
    argv = g_newa (Local<Value>, 1);
  }

  gum_v8_native_function_invoke (func, implementation, info, argc, argv);

  for (uint32_t i = 0; i != argc; i++)
    argv[i].~Local<Value> ();
}

static gboolean
gumjs_native_function_get (const FunctionCallbackInfo<Value> & info,
                           Local<Object> receiver,
                           GumV8Core * core,
                           GumV8NativeFunction ** func,
                           GCallback * implementation)
{
  auto isolate = core->isolate;

  auto native_function = Local<FunctionTemplate>::New (isolate,
      *core->native_function);
  auto holder = info.Holder ();
  if (native_function->HasInstance (holder))
  {
    auto f =
        (GumV8NativeFunction *) holder->GetAlignedPointerFromInternalField (1);

    *func = f;

    if (!receiver.IsEmpty ())
    {
      if (!_gum_v8_native_pointer_get (receiver, (gpointer *) implementation,
          core))
        return FALSE;
    }
    else
    {
      *implementation = f->implementation;
    }
  }
  else
  {
    if (receiver.IsEmpty () || !native_function->HasInstance (receiver))
    {
      _gum_v8_throw_ascii_literal (isolate, "expected a NativeFunction");
      return FALSE;
    }

    auto f = (GumV8NativeFunction *)
        receiver->GetAlignedPointerFromInternalField (1);
    *func = f;
    *implementation = f->implementation;
  }

  return TRUE;
}

static GumV8NativeFunction *
gumjs_native_function_init (Local<Object> wrapper,
                            const GumV8NativeFunctionParams * params,
                            GumV8Core * core)
{
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();
  GumV8NativeFunction * func;
  ffi_type * rtype;
  uint32_t nargs_fixed, nargs_total, i;
  gboolean is_variadic;
  ffi_abi abi;

  func = g_slice_new0 (GumV8NativeFunction);
  func->implementation = params->implementation;
  func->scheduling = params->scheduling;
  func->exceptions = params->exceptions;
  func->traps = params->traps;
  func->return_shape = params->return_shape;
  func->core = core;

  if (!gum_v8_ffi_type_get (core, params->return_type, &rtype, &func->data))
    goto error;

  nargs_fixed = nargs_total = params->argument_types->Length ();
  is_variadic = FALSE;
  func->atypes = g_new (ffi_type *, nargs_total);
  for (i = 0; i != nargs_total; i++)
  {
    Local<Value> type;
    if (!params->argument_types->Get (context, i).ToLocal (&type))
      goto error;

    String::Utf8Value type_utf8 (isolate, type);
    if (strcmp (*type_utf8, "...") == 0)
    {
      if (i == 0 || is_variadic)
      {
        _gum_v8_throw_ascii_literal (isolate,
            "only one variadic marker may be specified, and can "
            "not be the first argument");
        goto error;
      }

      nargs_fixed = i;
      is_variadic = TRUE;
    }
    else
    {
      auto atype = &func->atypes[is_variadic ? i - 1 : i];

      if (!gum_v8_ffi_type_get (core, type, atype, &func->data))
        goto error;

      if (is_variadic)
        *atype = gum_ffi_maybe_promote_variadic (*atype);
    }
  }
  if (is_variadic)
    nargs_total--;

  abi = GUM_DEFAULT_FFI_ABI;
  if (!params->abi.IsEmpty ())
  {
    if (!gum_v8_ffi_abi_get (core, params->abi, &abi))
      goto error;
  }

  if (is_variadic)
  {
    if (ffi_prep_cif_var (&func->cif, abi, nargs_fixed, nargs_total, rtype,
        func->atypes) != FFI_OK)
    {
      _gum_v8_throw_ascii_literal (isolate,
          "failed to compile function call interface");
      goto error;
    }
  }
  else
  {
    if (ffi_prep_cif (&func->cif, abi, nargs_total, rtype,
        func->atypes) != FFI_OK)
    {
      _gum_v8_throw_ascii_literal (isolate,
          "failed to compile function call interface");
      goto error;
    }
  }

  func->is_variadic = nargs_fixed < nargs_total;
  func->nargs_fixed = nargs_fixed;
  func->abi = abi;

  for (i = 0; i != nargs_total; i++)
  {
    ffi_type * t = func->atypes[i];

    func->arglist_size = GUM_ALIGN_SIZE (func->arglist_size, t->alignment);
    func->arglist_size += t->size;
  }

  wrapper->SetInternalField (0, BigInt::NewFromUnsigned (isolate,
        GPOINTER_TO_SIZE (func->implementation)));
  wrapper->SetAlignedPointerInInternalField (1, func);

  func->wrapper = new Global<Object> (isolate, wrapper);
  func->wrapper->SetWeak (func, gum_v8_native_function_on_weak_notify,
      WeakCallbackType::kParameter);

  g_hash_table_add (core->native_functions, func);

  return func;

error:
  gum_v8_native_function_free (func);
  return NULL;
}

static void
gum_v8_native_function_free (GumV8NativeFunction * self)
{
  delete self->wrapper;

  while (self->data != NULL)
  {
    auto head = self->data;
    g_free (head->data);
    self->data = g_slist_delete_link (self->data, head);
  }
  g_free (self->atypes);

  g_slice_free (GumV8NativeFunction, self);
}

#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4611)
#endif

static void
gum_v8_native_function_invoke (GumV8NativeFunction * self,
                               GCallback implementation,
                               const FunctionCallbackInfo<Value> & info,
                               uint32_t argc,
                               Local<Value> * argv)
{
  auto core = (GumV8Core *) info.Data ().As<External> ()->Value ();
  auto script_scope = core->current_scope;
  auto isolate = core->isolate;
  auto cif = &self->cif;
  gsize num_args_declared = cif->nargs;
  gsize num_args_provided = (argv != nullptr) ? argc : info.Length ();
  gsize num_args_fixed = self->nargs_fixed;
  gboolean is_variadic = self->is_variadic;

  if ((is_variadic && num_args_provided < num_args_fixed) ||
      (!is_variadic && num_args_provided != num_args_declared))
  {
    _gum_v8_throw_ascii_literal (isolate, "bad argument count");
    return;
  }

  auto rtype = cif->rtype;
  auto atypes = cif->arg_types;
  gsize rsize = MAX (rtype->size, sizeof (gsize));
  gsize ralign = MAX (rtype->alignment, sizeof (gsize));
  auto rvalue = (GumFFIRet *) g_alloca (rsize + ralign - 1);
  rvalue = GUM_ALIGN_POINTER (GumFFIRet *, rvalue, ralign);

  void ** avalue;
  guint8 * avalues;
  ffi_cif tmp_cif;
  GumFFIArg tmp_value = { 0, };

  if (num_args_provided > 0)
  {
    gsize avalue_count = MAX (num_args_declared, num_args_provided);
    avalue = g_newa (void *, avalue_count);

    gsize arglist_size = self->arglist_size;
    if (is_variadic && num_args_provided > num_args_declared)
    {
      atypes = g_newa (ffi_type *, num_args_provided);

      memcpy (atypes, cif->arg_types, num_args_declared * sizeof (void *));
      for (gsize i = num_args_declared, type_idx = num_args_fixed;
          i != num_args_provided; i++)
      {
        ffi_type * t = cif->arg_types[type_idx];

        atypes[i] = t;
        arglist_size = GUM_ALIGN_SIZE (arglist_size, t->alignment);
        arglist_size += t->size;

        if (++type_idx >= num_args_declared)
          type_idx = num_args_fixed;
      }

      cif = &tmp_cif;
      if (ffi_prep_cif_var (cif, self->abi, num_args_fixed, num_args_provided,
          rtype, atypes) != FFI_OK)
      {
        _gum_v8_throw_ascii_literal (isolate,
            "failed to compile function call interface");
      }
    }

    gsize arglist_alignment = atypes[0]->alignment;
    avalues = (guint8 *) g_alloca (arglist_size + arglist_alignment - 1);
    avalues = GUM_ALIGN_POINTER (guint8 *, avalues, arglist_alignment);

    /* Prefill with zero to clear high bits of values smaller than a pointer. */
    memset (avalues, 0, arglist_size);

    gsize offset = 0;
    for (gsize i = 0; i != num_args_provided; i++)
    {
      auto t = atypes[i];

      offset = GUM_ALIGN_SIZE (offset, t->alignment);

      auto v = (GumFFIArg *) (avalues + offset);

      if (!gum_v8_value_to_ffi_type (core,
          (argv != nullptr) ? argv[i] : info[i], v, t))
        return;
      avalue[i] = v;

      offset += t->size;
    }

    for (gsize i = num_args_provided; i < num_args_declared; i++)
      avalue[i] = &tmp_value;
  }
  else
  {
    avalue = NULL;
  }

  auto scheduling = self->scheduling;
  auto exceptions = self->exceptions;
  auto traps = self->traps;
  auto return_shape = self->return_shape;
  GumExceptorScope exceptor_scope;
  GumInvocationState invocation_state;
  gint system_error = -1;

  {
    auto unlocker = g_newa (ScriptUnlocker, 1);
    auto interceptor = core->script->interceptor.interceptor;
    gboolean interceptor_was_ignoring_us = FALSE;
    GumStalker * stalker = NULL;

    if (exceptions == GUM_V8_EXCEPTIONS_PROPAGATE ||
        gum_exceptor_try (core->exceptor, &exceptor_scope))
    {
      if (exceptions == GUM_V8_EXCEPTIONS_STEAL)
        gum_interceptor_save (&invocation_state);

      if (scheduling == GUM_V8_SCHEDULING_COOPERATIVE)
      {
        new (unlocker) ScriptUnlocker (core);

        if (traps != GUM_V8_CODE_TRAPS_NONE)
        {
          interceptor_was_ignoring_us =
              gum_interceptor_maybe_unignore_current_thread (interceptor);
        }
      }

      if (traps == GUM_V8_CODE_TRAPS_ALL)
      {
        auto stalker_module = &core->script->stalker;

        _gum_v8_stalker_process_pending (stalker_module,
            &script_scope->stalker_scope);

        stalker = _gum_v8_stalker_get (stalker_module);
        gum_stalker_activate (stalker, GUM_FUNCPTR_TO_POINTER (implementation));
      }
      else if (traps == GUM_V8_CODE_TRAPS_NONE)
      {
        gum_interceptor_ignore_current_thread (interceptor);
      }

      ffi_call (cif, FFI_FN (implementation), rvalue, avalue);

      g_clear_pointer (&stalker, gum_stalker_deactivate);

      if (return_shape == GUM_V8_RETURN_DETAILED)
        system_error = gum_thread_get_system_error ();
    }

    g_clear_pointer (&stalker, gum_stalker_deactivate);

    if (traps == GUM_V8_CODE_TRAPS_NONE)
      gum_interceptor_unignore_current_thread (interceptor);

    if (scheduling == GUM_V8_SCHEDULING_COOPERATIVE)
    {
      if (traps != GUM_V8_CODE_TRAPS_NONE && interceptor_was_ignoring_us)
        gum_interceptor_ignore_current_thread (interceptor);

      unlocker->~ScriptUnlocker ();
    }
  }

  if (exceptions == GUM_V8_EXCEPTIONS_STEAL &&
      gum_exceptor_catch (core->exceptor, &exceptor_scope))
  {
    gum_interceptor_restore (&invocation_state);

    _gum_v8_throw_native (&exceptor_scope.exception, core);
    return;
  }

  Local<Value> result;
  if (!gum_v8_value_from_ffi_type (core, &result, rvalue, rtype))
    return;

  if (return_shape == GUM_V8_RETURN_DETAILED)
  {
    auto context = isolate->GetCurrentContext ();

    auto template_return_value =
        Local<Object>::New (isolate, *core->native_return_value);
    auto return_value = template_return_value->Clone ();
    return_value->Set (context,
        Local<String>::New (isolate, *core->value_key),
        result).Check ();
    return_value->Set (context,
        Local<String>::New (isolate, *core->system_error_key),
        Integer::New (isolate, system_error)).Check ();
    info.GetReturnValue ().Set (return_value);
  }
  else
  {
    info.GetReturnValue ().Set (result);
  }
}

#ifdef _MSC_VER
# pragma warning (pop)
#endif

static void
gum_v8_native_function_on_weak_notify (
    const WeakCallbackInfo<GumV8NativeFunction> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->core->native_functions, self);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_system_function_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new SystemFunction()` to create a new instance");
    return;
  }

  GumV8NativeFunctionParams params;
  if (!gum_v8_native_function_params_init (&params, GUM_V8_RETURN_DETAILED,
      args))
    return;

  gumjs_native_function_init (wrapper, &params, core);
}

static gboolean
gum_v8_native_function_params_init (GumV8NativeFunctionParams * params,
                                    GumV8ReturnValueShape return_shape,
                                    const GumV8Args * args)
{
  auto core = args->core;
  auto isolate = core->isolate;

  Local<Value> abi_or_options;
  if (!_gum_v8_args_parse (args, "pVA|V", &params->implementation,
      &params->return_type, &params->argument_types, &abi_or_options))
    return FALSE;
  params->scheduling = GUM_V8_SCHEDULING_COOPERATIVE;
  params->exceptions = GUM_V8_EXCEPTIONS_STEAL;
  params->traps = GUM_V8_CODE_TRAPS_DEFAULT;
  params->return_shape = return_shape;

  if (!abi_or_options.IsEmpty ())
  {
    if (abi_or_options->IsString ())
    {
      params->abi = abi_or_options;
    }
    else if (abi_or_options->IsObject () && !abi_or_options->IsNull ())
    {
      Local<Object> options = abi_or_options.As<Object> ();

      auto context = isolate->GetCurrentContext ();
      Local<Value> v;

      if (!options->Get (context, Local<String>::New (isolate, *core->abi_key))
          .ToLocal (&v))
        return FALSE;
      if (!v->IsUndefined ())
        params->abi = v;

      if (!options->Get (context, Local<String>::New (isolate,
          *core->scheduling_key)).ToLocal (&v))
        return FALSE;
      if (!v->IsUndefined ())
      {
        if (!gum_v8_scheduling_behavior_parse (v, &params->scheduling, isolate))
          return FALSE;
      }

      if (!options->Get (context, Local<String>::New (isolate,
          *core->exceptions_key)).ToLocal (&v))
        return FALSE;
      if (!v->IsUndefined ())
      {
        if (!gum_v8_exceptions_behavior_parse (v, &params->exceptions, isolate))
          return FALSE;
      }

      if (!options->Get (context, Local<String>::New (isolate,
          *core->traps_key)).ToLocal (&v))
        return FALSE;
      if (!v->IsUndefined ())
      {
        if (!gum_v8_code_traps_parse (v, &params->traps, isolate))
          return FALSE;
      }
    }
    else
    {
      _gum_v8_throw_ascii_literal (isolate,
          "expected string or object containing options");
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
gum_v8_scheduling_behavior_parse (Local<Value> value,
                                  GumV8SchedulingBehavior * behavior,
                                  Isolate * isolate)
{
  if (value->IsString ())
  {
    String::Utf8Value str_value (isolate, value);
    auto str = *str_value;

    if (strcmp (str, "cooperative") == 0)
    {
      *behavior = GUM_V8_SCHEDULING_COOPERATIVE;
      return TRUE;
    }

    if (strcmp (str, "exclusive") == 0)
    {
      *behavior = GUM_V8_SCHEDULING_EXCLUSIVE;
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "invalid scheduling behavior value");
  return FALSE;
}

static gboolean
gum_v8_exceptions_behavior_parse (Local<Value> value,
                                  GumV8ExceptionsBehavior * behavior,
                                  Isolate * isolate)
{
  if (value->IsString ())
  {
    String::Utf8Value str_value (isolate, value);
    auto str = *str_value;

    if (strcmp (str, "steal") == 0)
    {
      *behavior = GUM_V8_EXCEPTIONS_STEAL;
      return TRUE;
    }

    if (strcmp (str, "propagate") == 0)
    {
      *behavior = GUM_V8_EXCEPTIONS_PROPAGATE;
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "invalid exceptions behavior value");
  return FALSE;
}

static gboolean
gum_v8_code_traps_parse (Local<Value> value,
                         GumV8CodeTraps * traps,
                         Isolate * isolate)
{
  if (value->IsString ())
  {
    String::Utf8Value str_value (isolate, value);
    auto str = *str_value;

    if (strcmp (str, "default") == 0)
    {
      *traps = GUM_V8_CODE_TRAPS_DEFAULT;
      return TRUE;
    }

    if (strcmp (str, "none") == 0)
    {
      *traps = GUM_V8_CODE_TRAPS_NONE;
      return TRUE;
    }

    if (strcmp (str, "all") == 0)
    {
      *traps = GUM_V8_CODE_TRAPS_ALL;
      return TRUE;
    }
  }

  _gum_v8_throw_ascii_literal (isolate, "invalid code traps value");
  return FALSE;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_callback_construct)
{
  auto context = isolate->GetCurrentContext ();
  Local<Function> func_value;
  Local<Value> rtype_value;
  Local<Array> atypes_array;
  Local<Value> abi_value;
  GumV8NativeCallback * callback;
  ffi_type * rtype;
  uint32_t nargs, i;
  ffi_abi abi;
  gpointer func = NULL;

  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new NativeCallback()` to create a new instance");
    return;
  }

  if (!_gum_v8_args_parse (args, "FVA|V", &func_value, &rtype_value,
      &atypes_array, &abi_value))
    return;

  callback = g_slice_new0 (GumV8NativeCallback);
  callback->ref_count = 1;
  callback->func = new Global<Function> (isolate, func_value);
  callback->core = core;

  if (!gum_v8_ffi_type_get (core, rtype_value, &rtype, &callback->data))
    goto error;

  nargs = atypes_array->Length ();
  callback->atypes = g_new (ffi_type *, nargs);
  for (i = 0; i != nargs; i++)
  {
    Local<Value> v;
    if (!atypes_array->Get (context, i).ToLocal (&v))
      goto error;

    if (!gum_v8_ffi_type_get (core, v, &callback->atypes[i], &callback->data))
      goto error;
  }

  abi = GUM_DEFAULT_FFI_ABI;
  if (!abi_value.IsEmpty ())
  {
    if (!gum_v8_ffi_abi_get (core, abi_value, &abi))
      goto error;
  }

  callback->closure =
      (ffi_closure *) ffi_closure_alloc (sizeof (ffi_closure), &func);
  if (callback->closure == NULL)
  {
    _gum_v8_throw_ascii_literal (isolate, "failed to allocate closure");
    goto error;
  }

  if (ffi_prep_cif (&callback->cif, abi, nargs, rtype,
      callback->atypes) != FFI_OK)
  {
    _gum_v8_throw_ascii_literal (isolate,
        "failed to compile function call interface");
    goto error;
  }

  if (ffi_prep_closure_loc (callback->closure, &callback->cif,
      gum_v8_native_callback_invoke, callback, func) != FFI_OK)
  {
    _gum_v8_throw_ascii_literal (isolate, "failed to prepare closure");
    goto error;
  }

  wrapper->SetInternalField (0,
      BigInt::NewFromUnsigned (isolate, GPOINTER_TO_SIZE (func)));
  wrapper->SetInternalField (1, External::New (isolate, callback));

  callback->wrapper = new Global<Object> (isolate, wrapper);
  callback->wrapper->SetWeak (callback,
      gum_v8_native_callback_on_weak_notify, WeakCallbackType::kParameter);
  callback->ptr_value = func;

  g_hash_table_add (core->native_callbacks, callback);

  return;

error:
  gum_v8_native_callback_unref (callback);
}

static void
gum_v8_native_callback_on_weak_notify (
    const WeakCallbackInfo<GumV8NativeCallback> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->core->native_callbacks, self);
}

static GumV8NativeCallback *
gum_v8_native_callback_ref (GumV8NativeCallback * callback)
{
  g_atomic_int_inc (&callback->ref_count);

  return callback;
}

static void
gum_v8_native_callback_unref (GumV8NativeCallback * callback)
{
  if (!g_atomic_int_dec_and_test (&callback->ref_count))
    return;

  gum_v8_native_callback_clear (callback);

  g_clear_pointer (&callback->closure, ffi_closure_free);

  while (callback->data != NULL)
  {
    auto head = callback->data;
    g_free (head->data);
    callback->data = g_slist_delete_link (callback->data, head);
  }
  g_free (callback->atypes);

  g_slice_free (GumV8NativeCallback, callback);
}

static void
gum_v8_native_callback_clear (GumV8NativeCallback * self)
{
  delete self->wrapper;
  delete self->func;
  self->wrapper = nullptr;
  self->func = nullptr;
}

static void
gum_v8_native_callback_invoke (ffi_cif * cif,
                               void * return_value,
                               void ** args,
                               void * user_data)
{
  GumV8SystemErrorPreservationScope error_scope;
  guintptr return_address = 0;
  guintptr stack_pointer = 0;
  guintptr frame_pointer = 0;
#if defined (_MSC_VER)
  return_address = GPOINTER_TO_SIZE (_ReturnAddress ());
  stack_pointer = GPOINTER_TO_SIZE (_AddressOfReturnAddress ());
  frame_pointer = *((guintptr *) stack_pointer - 1);
#elif defined (HAVE_I386)
# if GLIB_SIZEOF_VOID_P == 4
  asm ("mov %%esp, %0" : "=m" (stack_pointer));
  asm ("mov %%ebp, %0" : "=m" (frame_pointer));
# else
  asm ("movq %%rsp, %0" : "=m" (stack_pointer));
  asm ("movq %%rbp, %0" : "=m" (frame_pointer));
# endif
#elif defined (HAVE_ARM)
  asm ("mov %0, lr" : "=r" (return_address));
  asm ("mov %0, sp" : "=r" (stack_pointer));
  asm ("mov %0, r7" : "=r" (frame_pointer));
#elif defined (HAVE_ARM64)
  asm ("mov %0, lr" : "=r" (return_address));
  asm ("mov %0, sp" : "=r" (stack_pointer));
  asm ("mov %0, x29" : "=r" (frame_pointer));

# ifdef HAVE_DARWIN
  return_address &= G_GUINT64_CONSTANT (0x7fffffffff);
# endif
#elif defined (HAVE_MIPS)
  asm ("move %0, $ra" : "=r" (return_address));
  asm ("move %0, $sp" : "=r" (stack_pointer));
  asm ("move %0, $fp" : "=r" (frame_pointer));
#endif

  auto self = (GumV8NativeCallback *) user_data;
  ScriptScope scope (self->core->script);
  auto isolate = self->core->isolate;
  auto context = isolate->GetCurrentContext ();

  gum_v8_native_callback_ref (self);

  auto rtype = cif->rtype;
  auto tmp_value = (GumFFIArg *) g_alloca (rtype->size);
  auto retval = (GumFFIRet *) return_value;
  if (rtype != &ffi_type_void)
    memset (retval, 0, rtype->size);

  auto argv = g_newa (Local<Value>, cif->nargs);
  for (guint i = 0; i != cif->nargs; i++)
  {
    new (&argv[i]) Local<Value> ();
    if (!gum_v8_value_from_ffi_type (self->core, &argv[i],
        (GumFFIRet *) args[i], cif->arg_types[i]))
    {
      for (guint j = 0; j <= i; j++)
        argv[j].~Local<Value> ();
      return;
    }
  }

  auto func (Local<Function>::New (isolate, *self->func));

  Local<Value> recv;
  auto interceptor = &self->core->script->interceptor;
  GumV8InvocationContext * jic = NULL;
  GumV8CallbackContext * jcc = NULL;
  GumCpuContext cpu_context = { 0, };
  auto ic = gum_interceptor_get_live_replacement_invocation (self->ptr_value);
  if (ic != NULL)
  {
    jic = _gum_v8_interceptor_obtain_invocation_context (interceptor);
    _gum_v8_invocation_context_reset (jic, ic);
    recv = Local<Object>::New (isolate, *jic->object);
  }
  else
  {
#if defined (HAVE_I386)
    GUM_CPU_CONTEXT_XSP (&cpu_context) = stack_pointer;
    GUM_CPU_CONTEXT_XBP (&cpu_context) = frame_pointer;
#elif defined (HAVE_ARM)
    cpu_context.lr = return_address;
    cpu_context.sp = stack_pointer;
    cpu_context.r[7] = frame_pointer;
#elif defined (HAVE_ARM64)
    cpu_context.lr = return_address;
    cpu_context.sp = stack_pointer;
    cpu_context.fp = frame_pointer;
#endif

    jcc = gum_v8_callback_context_new_persistent (self->core, &cpu_context,
        &error_scope.saved_error, return_address);
    recv = Local<Object>::New (isolate, *jcc->wrapper);
  }

  Local<Value> result;
  bool have_result = func->Call (context, recv, cif->nargs, argv)
      .ToLocal (&result);

  if (jic != NULL)
  {
    _gum_v8_invocation_context_reset (jic, NULL);
    _gum_v8_interceptor_release_invocation_context (interceptor, jic);
  }

  if (jcc != NULL)
  {
    _gum_v8_cpu_context_free_later (jcc->cpu_context, self->core);
    delete jcc->cpu_context;
    gum_v8_callback_context_free (jcc);
  }

  if (cif->rtype != &ffi_type_void && have_result)
  {
    if (gum_v8_value_to_ffi_type (self->core, result, tmp_value, cif->rtype))
      gum_ffi_arg_to_ret (cif->rtype, tmp_value, retval);
  }

  for (guint i = 0; i != cif->nargs; i++)
    argv[i].~Local<Value> ();

  gum_v8_native_callback_unref (self);
}

static GumV8CallbackContext *
gum_v8_callback_context_new_persistent (GumV8Core * core,
                                        GumCpuContext * cpu_context,
                                        gint * system_error,
                                        GumAddress raw_return_address)
{
  auto isolate = core->isolate;

  auto jcc = g_slice_new (GumV8CallbackContext);

  auto callback_context_value = Local<Object>::New (isolate,
      *core->callback_context_value);
  auto wrapper = callback_context_value->Clone ();
  wrapper->SetAlignedPointerInInternalField (0, jcc);

  jcc->wrapper = new Global<Object> (isolate, wrapper);
  jcc->cpu_context = new Global<Object> (isolate,
      _gum_v8_cpu_context_new_immutable (cpu_context, core));
  jcc->system_error = system_error;
  jcc->return_address = 0;
  jcc->raw_return_address = raw_return_address;

  return jcc;
}

static void
gum_v8_callback_context_free (GumV8CallbackContext * self)
{
  delete self->wrapper;

  g_slice_free (GumV8CallbackContext, self);
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_callback_context_get_return_address,
                           GumV8CallbackContext)
{
  if (self->return_address == 0)
  {
    auto instance (Local<Object>::New (isolate, *self->cpu_context));
    auto cpu_context =
        (GumCpuContext *) instance->GetAlignedPointerFromInternalField (0);

    auto backtracer = gum_backtracer_make_accurate ();

    if (backtracer == NULL)
    {
      self->return_address = self->raw_return_address;
    }
    else
    {
      GumReturnAddressArray ret_addrs;

      gum_backtracer_generate_with_limit (backtracer, cpu_context,
          &ret_addrs, 1);
      self->return_address = GPOINTER_TO_SIZE (ret_addrs.items[0]);
    }

    g_clear_object (&backtracer);
  }

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (GSIZE_TO_POINTER (self->return_address),
        core));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_callback_context_get_cpu_context,
                           GumV8CallbackContext)
{
  auto context = self->cpu_context;
  if (context == nullptr)
  {
    _gum_v8_throw (isolate, "invalid operation");
    return;
  }

  info.GetReturnValue ().Set (Local<Object>::New (isolate, *context));
}

GUMJS_DEFINE_CLASS_GETTER (gumjs_callback_context_get_system_error,
                           GumV8CallbackContext)
{
  info.GetReturnValue ().Set (*self->system_error);
}

GUMJS_DEFINE_CLASS_SETTER (gumjs_callback_context_set_system_error,
                           GumV8CallbackContext)
{
  gint system_error;
  if (!_gum_v8_int_get (value, &system_error, core))
    return;

  *self->system_error = system_error;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_cpu_context_construct)
{
  GumCpuContext * cpu_context = NULL;
  gboolean is_mutable = FALSE;
  if (!_gum_v8_args_parse (args, "|Xt", &cpu_context, &is_mutable))
    return;

  wrapper->SetAlignedPointerInInternalField (0, cpu_context);
  wrapper->SetInternalField (1, Boolean::New (isolate, !!is_mutable));
  wrapper->SetAlignedPointerInInternalField (2, core);
}

static void
gumjs_cpu_context_get_gpr (Local<Name> property,
                           const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.Holder ();
  auto core = (GumV8Core *) wrapper->GetAlignedPointerFromInternalField (2);
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  info.GetReturnValue ().Set (
      _gum_v8_native_pointer_new (*(gpointer *) (cpu_context + offset), core));
}

static void
gumjs_cpu_context_set_gpr (Local<Name> property,
                           Local<Value> value,
                           const PropertyCallbackInfo<void> & info)
{
  auto isolate = info.GetIsolate ();
  auto wrapper = info.Holder ();
  auto core = (GumV8Core *) wrapper->GetAlignedPointerFromInternalField (2);
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  bool is_mutable = wrapper->GetInternalField (1).As<Boolean> ()->Value ();
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  if (!is_mutable)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return;
  }

  _gum_v8_native_pointer_parse (value, (gpointer *) (cpu_context + offset),
      core);
}

#ifdef _MSC_VER
# pragma warning (push)
# pragma warning (disable: 4505)
#endif

static void
gumjs_cpu_context_get_vector (Local<Name> property,
                              const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.Holder ();
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  gsize spec = info.Data ().As<Integer> ()->Value ();
  const gsize offset = spec >> 8;
  const gsize size = spec & 0xff;

  auto result = ArrayBuffer::New (info.GetIsolate (), size);
  auto store = result.As<ArrayBuffer> ()->GetBackingStore ();
  memcpy (store->Data (), cpu_context + offset, size);

  info.GetReturnValue ().Set (result);
}

static void
gumjs_cpu_context_set_vector (Local<Name> property,
                              Local<Value> value,
                              const PropertyCallbackInfo<void> & info)
{
  auto isolate = info.GetIsolate ();
  auto wrapper = info.Holder ();
  auto core = (GumV8Core *) wrapper->GetAlignedPointerFromInternalField (2);
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  bool is_mutable = wrapper->GetInternalField (1).As<Boolean> ()->Value ();
  gsize spec = info.Data ().As<Integer> ()->Value ();
  const gsize offset = spec >> 8;
  const gsize size = spec & 0xff;

  if (!is_mutable)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return;
  }

  GBytes * new_bytes = _gum_v8_bytes_get (value, core);
  if (new_bytes == NULL)
    return;

  gsize new_size;
  gconstpointer new_data = g_bytes_get_data (new_bytes, &new_size);
  if (new_size != size)
  {
    g_bytes_unref (new_bytes);
    _gum_v8_throw_ascii_literal (isolate, "incorrect vector size");
    return;
  }

  memcpy (cpu_context + offset, new_data, new_size);

  g_bytes_unref (new_bytes);
}

#ifdef HAVE_I386

static void
gumjs_cpu_context_get_xmm (Local<Name> property,
                           const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.Holder ();
  auto cpu_context =
      (GumCpuContext *) wrapper->GetAlignedPointerFromInternalField (0);
  guint index = info.Data ().As<Integer> ()->Value ();

  if (cpu_context->xmm == NULL)
  {
    info.GetReturnValue ().SetNull ();
    return;
  }

  GumX86VectorReg * reg = &cpu_context->xmm[index];

  auto result = ArrayBuffer::New (info.GetIsolate (), sizeof (reg->q));
  auto store = result.As<ArrayBuffer> ()->GetBackingStore ();
  memcpy (store->Data (), reg->q, sizeof (reg->q));

  info.GetReturnValue ().Set (result);
}

static void
gumjs_cpu_context_set_xmm (Local<Name> property,
                           Local<Value> value,
                           const PropertyCallbackInfo<void> & info)
{
  auto isolate = info.GetIsolate ();
  auto wrapper = info.Holder ();
  auto core = (GumV8Core *) wrapper->GetAlignedPointerFromInternalField (2);
  auto cpu_context =
      (GumCpuContext *) wrapper->GetAlignedPointerFromInternalField (0);
  bool is_mutable = wrapper->GetInternalField (1).As<Boolean> ()->Value ();
  guint index = info.Data ().As<Integer> ()->Value ();

  if (!is_mutable)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return;
  }

  if (cpu_context->xmm == NULL)
  {
    _gum_v8_throw_ascii_literal (isolate,
        "vector registers are not available");
    return;
  }

  GumX86VectorReg * reg = &cpu_context->xmm[index];

  GBytes * new_bytes = _gum_v8_bytes_get (value, core);
  if (new_bytes == NULL)
    return;

  gsize new_size;
  gconstpointer new_data = g_bytes_get_data (new_bytes, &new_size);
  if (new_size != sizeof (reg->q))
  {
    g_bytes_unref (new_bytes);
    _gum_v8_throw_ascii_literal (isolate, "incorrect vector size");
    return;
  }

  memcpy (reg->q, new_data, new_size);

  g_bytes_unref (new_bytes);
}

#endif

static void
gumjs_cpu_context_get_double (Local<Name> property,
                              const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.Holder ();
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  info.GetReturnValue ().Set (
      Number::New (info.GetIsolate (), *(gdouble *) (cpu_context + offset)));
}

static void
gumjs_cpu_context_set_double (Local<Name> property,
                              Local<Value> value,
                              const PropertyCallbackInfo<void> & info)
{
  auto isolate = info.GetIsolate ();
  auto wrapper = info.Holder ();
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  bool is_mutable = wrapper->GetInternalField (1).As<Boolean> ()->Value ();
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  if (!is_mutable)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return;
  }

  if (!value->IsNumber ())
  {
    _gum_v8_throw_ascii_literal (isolate, "expected a number");
    return;
  }
  gdouble d = value.As<Number> ()->Value ();

  *(gdouble *) (cpu_context + offset) = d;
}

static void
gumjs_cpu_context_get_float (Local<Name> property,
                             const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.Holder ();
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  info.GetReturnValue ().Set (
      Number::New (info.GetIsolate (), *(gfloat *) (cpu_context + offset)));
}

static void
gumjs_cpu_context_set_float (Local<Name> property,
                             Local<Value> value,
                             const PropertyCallbackInfo<void> & info)
{
  auto isolate = info.GetIsolate ();
  auto wrapper = info.Holder ();
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  bool is_mutable = wrapper->GetInternalField (1).As<Boolean> ()->Value ();
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  if (!is_mutable)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return;
  }

  if (!value->IsNumber ())
  {
    _gum_v8_throw_ascii_literal (isolate, "expected a number");
    return;
  }
  gdouble d = value.As<Number> ()->Value ();

  *(gfloat *) (cpu_context + offset) = (gfloat) d;
}

static void
gumjs_cpu_context_get_flags (Local<Name> property,
                             const PropertyCallbackInfo<Value> & info)
{
  auto wrapper = info.Holder ();
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  info.GetReturnValue ().Set (
      Integer::NewFromUnsigned (info.GetIsolate (),
        *(gsize *) (cpu_context + offset)));
}

static void
gumjs_cpu_context_set_flags (Local<Name> property,
                             Local<Value> value,
                             const PropertyCallbackInfo<void> & info)
{
  auto isolate = info.GetIsolate ();
  auto wrapper = info.Holder ();
  auto cpu_context = (guint8 *) wrapper->GetAlignedPointerFromInternalField (0);
  bool is_mutable = wrapper->GetInternalField (1).As<Boolean> ()->Value ();
  auto core = (GumV8Core *) wrapper->GetAlignedPointerFromInternalField (2);
  const gsize offset = info.Data ().As<Integer> ()->Value ();

  if (!is_mutable)
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid operation");
    return;
  }

  gsize f;
  if (!_gum_v8_size_get (value, &f, core))
    return;

  *(gsize *) (cpu_context + offset) = f;
}

#ifdef _MSC_VER
# pragma warning (pop)
#endif

GUMJS_DEFINE_CONSTRUCTOR (gumjs_match_pattern_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new MatchPattern()` to create a new instance");
    return;
  }

  gchar * pattern_str;
  if (!_gum_v8_args_parse (args, "s", &pattern_str))
    return;

  auto pattern = gum_match_pattern_new_from_string (pattern_str);

  g_free (pattern_str);

  if (pattern == NULL)
  {
    _gum_v8_throw_literal (isolate, "invalid match pattern");
    return;
  }

  wrapper->SetInternalField (0, External::New (isolate, pattern));
  gum_v8_match_pattern_new (wrapper, pattern, module);
}

static GumV8MatchPattern *
gum_v8_match_pattern_new (Local<Object> wrapper,
                          GumMatchPattern * handle,
                          GumV8Core * core)
{
  auto pattern = g_slice_new (GumV8MatchPattern);

  pattern->wrapper = new Global<Object> (core->isolate, wrapper);
  pattern->handle = handle;

  g_hash_table_add (core->match_patterns, pattern);

  return pattern;
}

static void
gum_v8_match_pattern_free (GumV8MatchPattern * self)
{
  delete self->wrapper;

  gum_match_pattern_unref (self->handle);

  g_slice_free (GumV8MatchPattern, self);
}

static MaybeLocal<Object>
gumjs_source_map_new (const gchar * json,
                      GumV8Core * core)
{
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  auto ctor = Local<FunctionTemplate>::New (isolate, *core->source_map);

  Local<Value> args[] = {
    String::NewFromUtf8 (isolate, json).ToLocalChecked ()
  };

  return ctor->GetFunction (context).ToLocalChecked ()
      ->NewInstance (context, G_N_ELEMENTS (args), args);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_source_map_construct)
{
  if (!info.IsConstructCall ())
  {
    _gum_v8_throw_ascii_literal (isolate,
        "use `new SourceMap()` to create a new instance");
    return;
  }

  gchar * json;
  if (!_gum_v8_args_parse (args, "s", &json))
    return;

  auto handle = gum_source_map_new (json);

  g_free (json);

  if (handle == NULL)
  {
    _gum_v8_throw (isolate, "invalid source map");
    return;
  }

  auto map = gum_v8_source_map_new (wrapper, handle, module);
  wrapper->SetAlignedPointerInInternalField (0, map);
}

GUMJS_DEFINE_CLASS_METHOD (gumjs_source_map_resolve, GumV8SourceMap)
{
  guint line, column;

  if (args->info->Length () == 1)
  {
    if (!_gum_v8_args_parse (args, "u", &line))
      return;
    column = G_MAXUINT;
  }
  else
  {
    if (!_gum_v8_args_parse (args, "uu", &line, &column))
      return;
  }

  const gchar * source, * name;
  if (gum_source_map_resolve (self->handle, &line, &column, &source, &name))
  {
    auto result = Array::New (isolate, 4);

    auto context = isolate->GetCurrentContext ();
    result->Set (context, 0,
        String::NewFromUtf8 (isolate, source).ToLocalChecked ()).Check ();
    result->Set (context, 1, Integer::NewFromUnsigned (isolate, line)).Check ();
    result->Set (context, 2, Integer::NewFromUnsigned (isolate, column))
        .Check ();
    if (name != NULL)
    {
      result->Set (context, 3,
          String::NewFromUtf8 (isolate, name).ToLocalChecked ()).Check ();
    }
    else
    {
      result->Set (context, 3, Null (isolate)).Check ();
    }

    info.GetReturnValue ().Set (result);
  }
  else
  {
    info.GetReturnValue ().SetNull ();
  }
}

static GumV8SourceMap *
gum_v8_source_map_new (Local<Object> wrapper,
                       GumSourceMap * handle,
                       GumV8Core * core)
{
  auto map = g_slice_new (GumV8SourceMap);
  map->wrapper = new Global<Object> (core->isolate, wrapper);
  map->wrapper->SetWeak (map, gum_v8_source_map_on_weak_notify,
      WeakCallbackType::kParameter);
  map->handle = handle;

  map->core = core;

  g_hash_table_add (core->source_maps, map);

  return map;
}

static void
gum_v8_source_map_free (GumV8SourceMap * self)
{
  g_object_unref (self->handle);

  delete self->wrapper;

  g_slice_free (GumV8SourceMap, self);
}

static void
gum_v8_source_map_on_weak_notify (const WeakCallbackInfo<GumV8SourceMap> & info)
{
  HandleScope handle_scope (info.GetIsolate ());
  auto self = info.GetParameter ();
  g_hash_table_remove (self->core->source_maps, self);
}

static GumV8ExceptionSink *
gum_v8_exception_sink_new (Local<Function> callback,
                           Isolate * isolate)
{
  auto sink = g_slice_new (GumV8ExceptionSink);
  sink->callback = new Global<Function> (isolate, callback);
  sink->isolate = isolate;
  return sink;
}

static void
gum_v8_exception_sink_free (GumV8ExceptionSink * sink)
{
  delete sink->callback;

  g_slice_free (GumV8ExceptionSink, sink);
}

static void
gum_v8_exception_sink_handle_exception (GumV8ExceptionSink * self,
                                        Local<Value> exception)
{
  auto isolate = self->isolate;
  auto context = isolate->GetCurrentContext ();

  auto callback (Local<Function>::New (isolate, *self->callback));
  auto recv = Undefined (isolate);
  Local<Value> argv[] = { exception };
  auto result = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
  _gum_v8_ignore_result (result);
}

static GumV8MessageSink *
gum_v8_message_sink_new (Local<Function> callback,
                         Isolate * isolate)
{
  auto sink = g_slice_new (GumV8MessageSink);
  sink->callback = new Global<Function> (isolate, callback);
  sink->isolate = isolate;
  return sink;
}

static void
gum_v8_message_sink_free (GumV8MessageSink * sink)
{
  delete sink->callback;

  g_slice_free (GumV8MessageSink, sink);
}

static void
gum_v8_message_sink_post (GumV8MessageSink * self,
                          const gchar * message,
                          GBytes * data)
{
  auto isolate = self->isolate;
  auto context = isolate->GetCurrentContext ();

  Local<Value> data_value;
  if (data != NULL)
  {
    gpointer base;
    gsize size;

    base = (gpointer) g_bytes_get_data (data, &size);

    data_value = ArrayBuffer::New (isolate, ArrayBuffer::NewBackingStore (
        base, size, gum_delete_bytes_reference, data));
  }
  else
  {
    data_value = Null (isolate);
  }

  auto callback (Local<Function>::New (isolate, *self->callback));
  auto recv = Undefined (isolate);
  Local<Value> argv[] = {
    String::NewFromUtf8 (isolate, message).ToLocalChecked (),
    data_value
  };
  auto result = callback->Call (context, recv, G_N_ELEMENTS (argv), argv);
  _gum_v8_ignore_result (result);
}

static void
gum_delete_bytes_reference (void * data,
                            size_t length,
                            void * deleter_data)
{
  g_bytes_unref ((GBytes *) deleter_data);
}

static gboolean
gum_v8_ffi_type_get (GumV8Core * core,
                     Local<Value> name,
                     ffi_type ** type,
                     GSList ** data)
{
  auto isolate = core->isolate;

  if (name->IsString ())
  {
    String::Utf8Value str_value (isolate, name);
    if (gum_ffi_try_get_type_by_name (*str_value, type))
      return TRUE;
  }
  else if (name->IsArray ())
  {
    auto fields_value = name.As<Array> ();
    gsize length = fields_value->Length ();

    auto fields = g_new (ffi_type *, length + 1);
    *data = g_slist_prepend (*data, fields);

    auto context = isolate->GetCurrentContext ();
    for (gsize i = 0; i != length; i++)
    {
      Local<Value> field_value;
      if (fields_value->Get (context, i).ToLocal (&field_value))
      {
        if (!gum_v8_ffi_type_get (core, field_value, &fields[i], data))
          return FALSE;
      }
      else
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid field type specified");
        return FALSE;
      }
    }

    fields[length] = NULL;

    auto struct_type = g_new0 (ffi_type, 1);
    struct_type->type = FFI_TYPE_STRUCT;
    struct_type->elements = fields;
    *data = g_slist_prepend (*data, struct_type);

    *type = struct_type;
    return TRUE;
  }

  _gum_v8_throw_ascii_literal (isolate, "invalid type specified");
  return FALSE;
}

static gboolean
gum_v8_ffi_abi_get (GumV8Core * core,
                    Local<Value> name,
                    ffi_abi * abi)
{
  auto isolate = core->isolate;

  if (!name->IsString ())
  {
    _gum_v8_throw_ascii_literal (isolate, "invalid abi specified");
    return FALSE;
  }

  String::Utf8Value str_value (isolate, name);
  if (gum_ffi_try_get_abi_by_name (*str_value, abi))
    return TRUE;

  _gum_v8_throw_ascii_literal (isolate, "invalid abi specified");
  return FALSE;
}

static gboolean
gum_v8_value_to_ffi_type (GumV8Core * core,
                          const Local<Value> svalue,
                          GumFFIArg * value,
                          const ffi_type * type)
{
  auto isolate = core->isolate;
  auto context = isolate->GetCurrentContext ();

  if (type == &ffi_type_void)
  {
    value->v_pointer = NULL;
  }
  else if (type == &ffi_type_pointer)
  {
    if (!_gum_v8_native_pointer_get (svalue, &value->v_pointer, core))
      return FALSE;
  }
  else if (type == &ffi_type_sint8)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_sint8 = (gint8) svalue->Int32Value (context).ToChecked ();
  }
  else if (type == &ffi_type_uint8)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_uint8 = (guint8) svalue->Uint32Value (context).ToChecked ();
  }
  else if (type == &ffi_type_sint16)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_sint16 = (gint16) svalue->Int32Value (context).ToChecked ();
  }
  else if (type == &ffi_type_uint16)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_uint16 = (guint16) svalue->Uint32Value (context).ToChecked ();
  }
  else if (type == &ffi_type_sint32)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_sint32 = (gint32) svalue->Int32Value (context).ToChecked ();
  }
  else if (type == &ffi_type_uint32)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_uint32 = (guint32) svalue->Uint32Value (context).ToChecked ();
  }
  else if (type == &ffi_type_sint64)
  {
    if (!_gum_v8_int64_get (svalue, &value->v_sint64, core))
      return FALSE;
  }
  else if (type == &ffi_type_uint64)
  {
    if (!_gum_v8_uint64_get (svalue, &value->v_uint64, core))
      return FALSE;
  }
  else if (type == &gum_ffi_type_size_t)
  {
    guint64 u64;
    if (!_gum_v8_uint64_get (svalue, &u64, core))
      return FALSE;

    switch (type->size)
    {
      case 8:
        value->v_uint64 = u64;
        break;
      case 4:
        value->v_uint32 = u64;
        break;
      case 2:
        value->v_uint16 = u64;
        break;
      default:
        g_assert_not_reached ();
    }
  }
  else if (type == &gum_ffi_type_ssize_t)
  {
    gint64 i64;
    if (!_gum_v8_int64_get (svalue, &i64, core))
      return FALSE;

    switch (type->size)
    {
      case 8:
        value->v_sint64 = i64;
        break;
      case 4:
        value->v_sint32 = i64;
        break;
      case 2:
        value->v_sint16 = i64;
        break;
      default:
        g_assert_not_reached ();
    }
  }
  else if (type == &ffi_type_float)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_float = svalue->NumberValue (context).ToChecked ();
  }
  else if (type == &ffi_type_double)
  {
    if (!svalue->IsNumber ())
      goto error_expected_number;
    value->v_double = svalue->NumberValue (context).ToChecked ();
  }
  else if (type->type == FFI_TYPE_STRUCT)
  {
    if (!svalue->IsArray ())
    {
      _gum_v8_throw_ascii_literal (isolate, "expected array with fields");
      return FALSE;
    }
    auto field_svalues = svalue.As<Array> ();

    auto field_types = type->elements;
    gsize provided_length = field_svalues->Length ();
    gsize length = 0;
    for (auto t = field_types; *t != NULL; t++)
      length++;
    if (provided_length != length)
    {
      _gum_v8_throw_ascii_literal (isolate,
          "provided array length does not match number of fields");
      return FALSE;
    }

    auto field_values = (guint8 *) value;
    gsize offset = 0;
    for (gsize i = 0; i != length; i++)
    {
      auto field_type = field_types[i];

      offset = GUM_ALIGN_SIZE (offset, field_type->alignment);

      auto field_value = (GumFFIArg *) (field_values + offset);
      Local<Value> field_svalue;
      if (field_svalues->Get (context, i).ToLocal (&field_svalue))
      {
        if (!gum_v8_value_to_ffi_type (core, field_svalue, field_value,
            field_type))
        {
          return FALSE;
        }
      }
      else
      {
        _gum_v8_throw_ascii_literal (isolate, "invalid field value specified");
        return FALSE;
      }

      offset += field_type->size;
    }
  }
  else
  {
    goto error_unsupported_type;
  }

  return TRUE;

error_expected_number:
  {
    _gum_v8_throw_ascii_literal (isolate, "expected number");
    return FALSE;
  }
error_unsupported_type:
  {
    _gum_v8_throw_ascii_literal (isolate, "unsupported type");
    return FALSE;
  }
}

static gboolean
gum_v8_value_from_ffi_type (GumV8Core * core,
                            Local<Value> * svalue,
                            const GumFFIRet * value,
                            const ffi_type * type)
{
  auto isolate = core->isolate;

  if (type == &ffi_type_void)
  {
    *svalue = Undefined (isolate);
  }
  else if (type == &ffi_type_pointer)
  {
    *svalue = _gum_v8_native_pointer_new (value->v_pointer, core);
  }
  else if (type == &ffi_type_sint8)
  {
    *svalue = Integer::New (isolate, value->v_sint8);
  }
  else if (type == &ffi_type_uint8)
  {
    *svalue = Integer::NewFromUnsigned (isolate, value->v_uint8);
  }
  else if (type == &ffi_type_sint16)
  {
    *svalue = Integer::New (isolate, value->v_sint16);
  }
  else if (type == &ffi_type_uint16)
  {
    *svalue = Integer::NewFromUnsigned (isolate, value->v_uint16);
  }
  else if (type == &ffi_type_sint32)
  {
    *svalue = Integer::New (isolate, value->v_sint32);
  }
  else if (type == &ffi_type_uint32)
  {
    *svalue = Integer::NewFromUnsigned (isolate, value->v_uint32);
  }
  else if (type == &ffi_type_sint64)
  {
    *svalue = _gum_v8_int64_new (value->v_sint64, core);
  }
  else if (type == &ffi_type_uint64)
  {
    *svalue = _gum_v8_uint64_new (value->v_uint64, core);
  }
  else if (type == &gum_ffi_type_size_t)
  {
    guint64 u64;

    switch (type->size)
    {
      case 8:
        u64 = value->v_uint64;
        break;
      case 4:
        u64 = value->v_uint32;
        break;
      case 2:
        u64 = value->v_uint16;
        break;
      default:
        u64 = 0;
        g_assert_not_reached ();
    }

    *svalue = _gum_v8_uint64_new (u64, core);
  }
  else if (type == &gum_ffi_type_ssize_t)
  {
    gint64 i64;

    switch (type->size)
    {
      case 8:
        i64 = value->v_sint64;
        break;
      case 4:
        i64 = value->v_sint32;
        break;
      case 2:
        i64 = value->v_sint16;
        break;
      default:
        i64 = 0;
        g_assert_not_reached ();
    }

    *svalue = _gum_v8_int64_new (i64, core);
  }
  else if (type == &ffi_type_float)
  {
    *svalue = Number::New (isolate, value->v_float);
  }
  else if (type == &ffi_type_double)
  {
    *svalue = Number::New (isolate, value->v_double);
  }
  else if (type->type == FFI_TYPE_STRUCT)
  {
    auto context = isolate->GetCurrentContext ();
    auto field_types = type->elements;

    gsize length = 0;
    for (auto t = field_types; *t != NULL; t++)
      length++;

    auto field_svalues = Array::New (isolate, length);
    auto field_values = (const guint8 *) value;
    gsize offset = 0;
    for (gsize i = 0; i != length; i++)
    {
      auto field_type = field_types[i];

      offset = GUM_ALIGN_SIZE (offset, field_type->alignment);

      auto field_value = (const GumFFIRet *) (field_values + offset);
      Local<Value> field_svalue;
      if (gum_v8_value_from_ffi_type (core, &field_svalue, field_value,
          field_type))
      {
        field_svalues->Set (context, i, field_svalue).Check ();
      }
      else
      {
        return FALSE;
      }

      offset += field_type->size;
    }
    *svalue = field_svalues;
  }
  else
  {
    _gum_v8_throw_ascii_literal (isolate, "unsupported type");
    return FALSE;
  }

  return TRUE;
}
