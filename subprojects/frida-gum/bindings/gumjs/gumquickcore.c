/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020-2022 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2020 Marcus Mengs <mame8282@googlemail.com>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 * Copyright (C) 2024 Simon Zuckerbraun <Simon_Zuckerbraun@trendmicro.com>
 * Copyright (C) 2026 Thanos Petsas <thanpetsas@gmail.com>
 * Copyright (C) 2026 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumquickcore.h"

#include "gumansi.h"
#include "gumffi.h"
#include "gumquickinterceptor.h"
#include "gumquickmacros.h"
#include "gumquickscript-priv.h"
#include "gumquickstalker.h"
#include "gumsourcemap.h"

#include <string.h>
#include <glib/gprintf.h>
#ifdef _MSC_VER
# include <intrin.h>
#endif
#ifdef HAVE_PTRAUTH
# include <ptrauth.h>
#endif
#ifdef HAVE_TINY_STACK
# include <gum/gumbarebone.h>
#endif

#define GUM_QUICK_FFI_FUNCTION_PARAMS_EMPTY { NULL, }

typedef struct _GumQuickWeakCallback GumQuickWeakCallback;
typedef struct _GumQuickFlushCallback GumQuickFlushCallback;
typedef struct _GumQuickModuleInitOperation GumQuickModuleInitOperation;
typedef guint GumMemoryValueType;
typedef struct _GumQuickFFIFunctionParams GumQuickFFIFunctionParams;
typedef guint8 GumQuickSchedulingBehavior;
typedef guint8 GumQuickExceptionsBehavior;
typedef guint8 GumQuickCodeTraps;
typedef guint8 GumQuickReturnValueShape;
typedef struct _GumQuickFFIFunction GumQuickFFIFunction;
typedef struct _GumQuickCallbackContext GumQuickCallbackContext;

struct _GumQuickFlushCallback
{
  GumQuickFlushNotify func;
  gpointer data;
  GDestroyNotify data_destroy;
};

struct _GumQuickModuleInitOperation
{
  JSValue module;
  JSValue perform_init;

  GumQuickCore * core;
};

struct _GumQuickWeakRef
{
  JSValue target;
  GArray * callbacks;
};

struct _GumQuickWeakCallback
{
  guint id;
  JSValue callback;
};

struct _GumQuickScheduledCallback
{
  gint id;
  gboolean repeat;
  JSValue func;
  GSource * source;

  GumQuickCore * core;
};

struct _GumQuickExceptionSink
{
  JSValue callback;
  GumQuickCore * core;
};

struct _GumQuickMessageSink
{
  JSValue callback;
  GumQuickCore * core;
};

enum _GumMemoryValueType
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

struct _GumQuickFFIFunctionParams
{
  GCallback implementation;
  JSValueConst return_type;
  JSValueConst argument_types;
  const gchar * abi_name;
  GumQuickSchedulingBehavior scheduling;
  GumQuickExceptionsBehavior exceptions;
  GumQuickCodeTraps traps;
  GumQuickReturnValueShape return_shape;

  JSContext * ctx;
};

enum _GumQuickSchedulingBehavior
{
  GUM_QUICK_SCHEDULING_COOPERATIVE,
  GUM_QUICK_SCHEDULING_EXCLUSIVE
};

enum _GumQuickExceptionsBehavior
{
  GUM_QUICK_EXCEPTIONS_STEAL,
  GUM_QUICK_EXCEPTIONS_PROPAGATE
};

enum _GumQuickCodeTraps
{
  GUM_QUICK_CODE_TRAPS_DEFAULT,
  GUM_QUICK_CODE_TRAPS_NONE,
  GUM_QUICK_CODE_TRAPS_ALL
};

enum _GumQuickReturnValueShape
{
  GUM_QUICK_RETURN_PLAIN,
  GUM_QUICK_RETURN_DETAILED
};

struct _GumQuickFFIFunction
{
  GumQuickNativePointer native_pointer;

  GCallback implementation;
  GumQuickSchedulingBehavior scheduling;
  GumQuickExceptionsBehavior exceptions;
  GumQuickCodeTraps traps;
  GumQuickReturnValueShape return_shape;
  ffi_cif cif;
  ffi_type ** atypes;
  gsize arglist_size;
  gboolean is_variadic;
  guint nargs_fixed;
  ffi_abi abi;
  GSList * data;
};

struct _GumQuickCallbackContext
{
  JSValue wrapper;
  GumQuickCpuContext * cpu_context;
  gint * system_error;
  GumAddress return_address;
  GumAddress raw_return_address;
  int initial_property_count;
};

static gboolean gum_quick_core_handle_crashed_js (GumExceptionDetails * details,
    gpointer user_data);
static gboolean gum_quick_exception_is_interrupt (JSContext * ctx,
    JSValueConst exception);

static void gum_quick_flush_callback_free (GumQuickFlushCallback * self);
static gboolean gum_quick_flush_callback_notify (GumQuickFlushCallback * self);

GUMJS_DECLARE_FUNCTION (gumjs_set_timeout)
GUMJS_DECLARE_FUNCTION (gumjs_set_interval)
GUMJS_DECLARE_FUNCTION (gumjs_clear_timer)
GUMJS_DECLARE_FUNCTION (gumjs_gc)
GUMJS_DECLARE_FUNCTION (gumjs_send)
GUMJS_DECLARE_FUNCTION (gumjs_set_unhandled_exception_callback)
GUMJS_DECLARE_FUNCTION (gumjs_set_incoming_message_callback)
GUMJS_DECLARE_FUNCTION (gumjs_wait_for_event)

GUMJS_DECLARE_GETTER (gumjs_frida_get_heap_size)

GUMJS_DECLARE_FUNCTION (gumjs_script_evaluate)
GUMJS_DECLARE_FUNCTION (gumjs_script_load)
static gboolean gum_quick_core_init_module (GumQuickModuleInitOperation * op);
GUMJS_DECLARE_FUNCTION (gumjs_script_register_source_map)
GUMJS_DECLARE_FUNCTION (gumjs_script_find_source_map)
GUMJS_DECLARE_FUNCTION (gumjs_script_next_tick)
GUMJS_DECLARE_FUNCTION (gumjs_script_pin)
GUMJS_DECLARE_FUNCTION (gumjs_script_unpin)
GUMJS_DECLARE_FUNCTION (gumjs_script_bind_weak)
GUMJS_DECLARE_FUNCTION (gumjs_script_unbind_weak)
GUMJS_DECLARE_FUNCTION (gumjs_script_deref_weak)

GUMJS_DECLARE_FINALIZER (gumjs_weak_ref_finalize)
static gboolean gum_quick_core_invoke_pending_weak_callbacks_in_idle (
    GumQuickCore * self);

GUMJS_DECLARE_FUNCTION (gumjs_script_set_global_access_handler)
static JSValue gum_quick_core_on_global_get (JSContext * ctx, JSAtom name,
    void * opaque);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_int64_construct)
GUMJS_DECLARE_FINALIZER (gumjs_int64_finalize)
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

GUMJS_DECLARE_CONSTRUCTOR (gumjs_uint64_construct)
GUMJS_DECLARE_FINALIZER (gumjs_uint64_finalize)
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

GUMJS_DECLARE_CONSTRUCTOR (gumjs_native_pointer_construct)
GUMJS_DECLARE_FINALIZER (gumjs_native_pointer_finalize)
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

static JSValue gumjs_native_pointer_handle_read (JSContext * ctx,
    JSValueConst this_val, GumMemoryValueType type, GumQuickArgs * args,
    GumQuickCore * core);
static JSValue gumjs_native_pointer_handle_write (JSContext * ctx,
    JSValueConst this_val, GumMemoryValueType type, GumQuickArgs * args,
    GumQuickCore * core);

#define GUMJS_DEFINE_NATIVE_POINTER_READ(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_read_##T) \
    { \
      return gumjs_native_pointer_handle_read (ctx, this_val, \
          GUM_MEMORY_VALUE_##T, args, core); \
    }
#define GUMJS_DEFINE_MEMORY_WRITE(T) \
    GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_write_##T) \
    { \
      return gumjs_native_pointer_handle_write (ctx, this_val, \
          GUM_MEMORY_VALUE_##T, args, core); \
    }
#define GUMJS_DEFINE_NATIVE_POINTER_READ_WRITE(T) \
    GUMJS_DEFINE_NATIVE_POINTER_READ (T); \
    GUMJS_DEFINE_MEMORY_WRITE (T)

#define GUMJS_EXPORT_NATIVE_POINTER_READ(N, T) \
    JS_CFUNC_DEF ("read" N, 0, gumjs_native_pointer_read_##T)
#define GUMJS_EXPORT_MEMORY_WRITE(N, T) \
    JS_CFUNC_DEF ("write" N, 0, gumjs_native_pointer_write_##T)
#define GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE(N, T) \
    GUMJS_EXPORT_NATIVE_POINTER_READ (N, T), \
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

GUMJS_DECLARE_FINALIZER (gumjs_native_resource_finalize)

GUMJS_DECLARE_FINALIZER (gumjs_kernel_resource_finalize)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_native_function_construct)
GUMJS_DECLARE_FINALIZER (gumjs_native_function_finalize)
GUMJS_DECLARE_CALL_HANDLER (gumjs_native_function_invoke)
GUMJS_DECLARE_FUNCTION (gumjs_native_function_call)
GUMJS_DECLARE_FUNCTION (gumjs_native_function_apply)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_system_function_construct)
GUMJS_DECLARE_FINALIZER (gumjs_system_function_finalize)
GUMJS_DECLARE_CALL_HANDLER (gumjs_system_function_invoke)
GUMJS_DECLARE_FUNCTION (gumjs_system_function_call)
GUMJS_DECLARE_FUNCTION (gumjs_system_function_apply)

static GumQuickFFIFunction * gumjs_ffi_function_new (JSContext * ctx,
    const GumQuickFFIFunctionParams * params, GumQuickCore * core);
static void gum_quick_ffi_function_finalize (GumQuickFFIFunction * func);
static JSValue gum_quick_ffi_function_invoke (GumQuickFFIFunction * self,
    JSContext * ctx, GCallback implementation, guint argc, JSValueConst * argv,
    GumQuickCore * core);
static JSValue gumjs_ffi_function_invoke (JSContext * ctx,
    JSValueConst func_obj, JSClassID klass, GumQuickArgs * args,
    GumQuickCore * core);
static JSValue gumjs_ffi_function_call (JSContext * ctx, JSValueConst func_obj,
    JSClassID klass, GumQuickArgs * args, GumQuickCore * core);
static JSValue gumjs_ffi_function_apply (JSContext * ctx, JSValueConst func_obj,
    JSClassID klass, GumQuickArgs * args, GumQuickCore * core);
static gboolean gumjs_ffi_function_get (JSContext * ctx, JSValueConst func_obj,
    JSValueConst receiver, JSClassID klass, GumQuickCore * core,
    GumQuickFFIFunction ** func, GCallback * implementation);

static gboolean gum_quick_ffi_function_params_init (
    GumQuickFFIFunctionParams * params, GumQuickReturnValueShape return_shape,
    GumQuickArgs * args);
static void gum_quick_ffi_function_params_destroy (
    GumQuickFFIFunctionParams * params);

static gboolean gum_quick_scheduling_behavior_get (JSContext * ctx,
    JSValueConst val, GumQuickSchedulingBehavior * behavior);
static gboolean gum_quick_exceptions_behavior_get (JSContext * ctx,
    JSValueConst val, GumQuickExceptionsBehavior * behavior);
static gboolean gum_quick_code_traps_get (JSContext * ctx, JSValueConst val,
    GumQuickCodeTraps * traps);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_native_callback_construct)
GUMJS_DECLARE_FINALIZER (gumjs_native_callback_finalize)
static void gum_quick_native_callback_finalize (GumQuickNativeCallback * func);
static void gum_quick_native_callback_invoke (ffi_cif * cif,
    void * return_value, void ** args, void * user_data);

GUMJS_DECLARE_FINALIZER (gumjs_callback_context_finalize)
GUMJS_DECLARE_GETTER (gumjs_callback_context_get_return_address)
GUMJS_DECLARE_GETTER (gumjs_callback_context_get_cpu_context)
GUMJS_DECLARE_GETTER (gumjs_callback_context_get_system_error)
GUMJS_DECLARE_SETTER (gumjs_callback_context_set_system_error)
static JSValue gum_quick_callback_context_new (GumQuickCore * core,
    GumCpuContext * cpu_context, gint * system_error,
    GumAddress raw_return_address, GumQuickCallbackContext ** context);
static gboolean gum_quick_callback_context_get (JSContext * ctx,
    JSValueConst val, GumQuickCore * core, GumQuickCallbackContext ** ic);

GUMJS_DECLARE_FINALIZER (gumjs_cpu_context_finalize)
GUMJS_DECLARE_FUNCTION (gumjs_cpu_context_to_json)
static JSValue gumjs_cpu_context_set_gpr (GumQuickCpuContext * self,
    JSContext * ctx, JSValueConst val, gpointer * reg);
G_GNUC_UNUSED static JSValue gumjs_cpu_context_set_vector (
    GumQuickCpuContext * self, JSContext * ctx, JSValueConst val,
    guint8 * bytes, gsize size);
G_GNUC_UNUSED static JSValue gumjs_cpu_context_set_double (
    GumQuickCpuContext * self, JSContext * ctx, JSValueConst val, gdouble * d);
G_GNUC_UNUSED static JSValue gumjs_cpu_context_set_float (
    GumQuickCpuContext * self, JSContext * ctx, JSValueConst val, gfloat * f);
G_GNUC_UNUSED static JSValue gumjs_cpu_context_set_flags (
    GumQuickCpuContext * self, JSContext * ctx, JSValueConst val, gsize * f);

GUMJS_DECLARE_CONSTRUCTOR (gumjs_match_pattern_construct)
GUMJS_DECLARE_FINALIZER (gumjs_match_pattern_finalize)

static JSValue gumjs_source_map_new (const gchar * json, GumQuickCore * core);
GUMJS_DECLARE_CONSTRUCTOR (gumjs_source_map_construct)
GUMJS_DECLARE_FINALIZER (gumjs_source_map_finalize)
GUMJS_DECLARE_FUNCTION (gumjs_source_map_resolve)

GUMJS_DECLARE_CONSTRUCTOR (gumjs_worker_construct)
static void gum_quick_worker_destroy (GumQuickWorker * worker);
GUMJS_DECLARE_FUNCTION (gumjs_worker_terminate)
GUMJS_DECLARE_FUNCTION (gumjs_worker_post)

static JSValue gum_quick_core_schedule_callback (GumQuickCore * self,
    GumQuickArgs * args, gboolean repeat);
static GumQuickScheduledCallback * gum_quick_core_try_steal_scheduled_callback (
    GumQuickCore * self, gint id);

static GumQuickScheduledCallback * gum_scheduled_callback_new (guint id,
    JSValueConst func, gboolean repeat, GSource * source, GumQuickCore * core);
static void gum_scheduled_callback_free (GumQuickScheduledCallback * callback);
static gboolean gum_scheduled_callback_invoke (
    GumQuickScheduledCallback * self);

static GumQuickExceptionSink * gum_quick_exception_sink_new (
    JSValueConst callback, GumQuickCore * core);
static void gum_quick_exception_sink_free (GumQuickExceptionSink * sink);
static void gum_quick_exception_sink_handle_exception (
    GumQuickExceptionSink * self, JSValueConst exception);

static GumQuickMessageSink * gum_quick_message_sink_new (JSValueConst callback,
    GumQuickCore * core);
static void gum_quick_message_sink_free (GumQuickMessageSink * sink);
static void gum_quick_message_sink_post (GumQuickMessageSink * self,
    const gchar * message, GBytes * data, GumQuickScope * scope);

static gboolean gum_quick_ffi_type_get (JSContext * ctx, JSValueConst val,
    GumQuickCore * core, ffi_type ** type, GSList ** data);
static gboolean gum_quick_ffi_abi_get (JSContext * ctx, const gchar * name,
    ffi_abi * abi);
static gboolean gum_quick_value_to_ffi (JSContext * ctx, JSValueConst sval,
    const ffi_type * type, GumQuickCore * core, GumFFIArg * val);
static JSValue gum_quick_value_from_ffi (JSContext * ctx,
    const GumFFIRet * val, const ffi_type * type, GumQuickCore * core);

static void gum_quick_core_setup_atoms (GumQuickCore * self);
static void gum_quick_core_teardown_atoms (GumQuickCore * self);

static const JSCFunctionListEntry gumjs_root_entries[] =
{
  JS_CFUNC_DEF ("_setTimeout", 0, gumjs_set_timeout),
  JS_CFUNC_DEF ("_setInterval", 0, gumjs_set_interval),
  JS_CFUNC_DEF ("clearTimeout", 1, gumjs_clear_timer),
  JS_CFUNC_DEF ("clearInterval", 1, gumjs_clear_timer),
  JS_CFUNC_DEF ("gc", 0, gumjs_gc),
  JS_CFUNC_DEF ("_send", 0, gumjs_send),
  JS_CFUNC_DEF ("_setUnhandledExceptionCallback", 0,
      gumjs_set_unhandled_exception_callback),
  JS_CFUNC_DEF ("_setIncomingMessageCallback", 0,
      gumjs_set_incoming_message_callback),
  JS_CFUNC_DEF ("_waitForEvent", 0, gumjs_wait_for_event),
};

static const JSCFunctionListEntry gumjs_frida_entries[] =
{
  JS_PROP_STRING_DEF ("version", FRIDA_VERSION, JS_PROP_C_W_E),
  JS_CGETSET_DEF ("heapSize", gumjs_frida_get_heap_size, NULL),
};

static const JSCFunctionListEntry gumjs_script_entries[] =
{
  JS_PROP_STRING_DEF ("runtime", "QJS", JS_PROP_C_W_E),
  JS_CFUNC_DEF ("evaluate", 0, gumjs_script_evaluate),
  JS_CFUNC_DEF ("_load", 0, gumjs_script_load),
  JS_CFUNC_DEF ("registerSourceMap", 0, gumjs_script_register_source_map),
  JS_CFUNC_DEF ("_findSourceMap", 0, gumjs_script_find_source_map),
  JS_CFUNC_DEF ("_nextTick", 0, gumjs_script_next_tick),
  JS_CFUNC_DEF ("pin", 0, gumjs_script_pin),
  JS_CFUNC_DEF ("unpin", 0, gumjs_script_unpin),
  JS_CFUNC_DEF ("bindWeak", 0, gumjs_script_bind_weak),
  JS_CFUNC_DEF ("unbindWeak", 0, gumjs_script_unbind_weak),
  JS_CFUNC_DEF ("_derefWeak", 0, gumjs_script_deref_weak),
  JS_CFUNC_DEF ("setGlobalAccessHandler", 1,
      gumjs_script_set_global_access_handler),
};

static const JSClassDef gumjs_weak_ref_def =
{
  .class_name = "WeakRef",
  .finalizer = gumjs_weak_ref_finalize,
};

static const JSClassDef gumjs_int64_def =
{
  .class_name = "Int64",
  .finalizer = gumjs_int64_finalize,
};

static const JSCFunctionListEntry gumjs_int64_entries[] =
{
  JS_CFUNC_DEF ("add", 0, gumjs_int64_add),
  JS_CFUNC_DEF ("sub", 0, gumjs_int64_sub),
  JS_CFUNC_DEF ("and", 0, gumjs_int64_and),
  JS_CFUNC_DEF ("or", 0, gumjs_int64_or),
  JS_CFUNC_DEF ("xor", 0, gumjs_int64_xor),
  JS_CFUNC_DEF ("shr", 0, gumjs_int64_shr),
  JS_CFUNC_DEF ("shl", 0, gumjs_int64_shl),
  JS_CFUNC_DEF ("not", 0, gumjs_int64_not),
  JS_CFUNC_DEF ("compare", 0, gumjs_int64_compare),
  JS_CFUNC_DEF ("toNumber", 0, gumjs_int64_to_number),
  JS_CFUNC_DEF ("toString", 0, gumjs_int64_to_string),
  JS_CFUNC_DEF ("toJSON", 0, gumjs_int64_to_json),
  JS_CFUNC_DEF ("valueOf", 0, gumjs_int64_value_of),
};

static const JSClassDef gumjs_uint64_def =
{
  .class_name = "UInt64",
  .finalizer = gumjs_uint64_finalize,
};

static const JSCFunctionListEntry gumjs_uint64_entries[] =
{
  JS_CFUNC_DEF ("add", 0, gumjs_uint64_add),
  JS_CFUNC_DEF ("sub", 0, gumjs_uint64_sub),
  JS_CFUNC_DEF ("and", 0, gumjs_uint64_and),
  JS_CFUNC_DEF ("or", 0, gumjs_uint64_or),
  JS_CFUNC_DEF ("xor", 0, gumjs_uint64_xor),
  JS_CFUNC_DEF ("shr", 0, gumjs_uint64_shr),
  JS_CFUNC_DEF ("shl", 0, gumjs_uint64_shl),
  JS_CFUNC_DEF ("not", 0, gumjs_uint64_not),
  JS_CFUNC_DEF ("compare", 0, gumjs_uint64_compare),
  JS_CFUNC_DEF ("toNumber", 0, gumjs_uint64_to_number),
  JS_CFUNC_DEF ("toString", 0, gumjs_uint64_to_string),
  JS_CFUNC_DEF ("toJSON", 0, gumjs_uint64_to_json),
  JS_CFUNC_DEF ("valueOf", 0, gumjs_uint64_value_of),
};

static const JSClassDef gumjs_native_pointer_def =
{
  .class_name = "NativePointer",
  .finalizer = gumjs_native_pointer_finalize,
};

static const JSCFunctionListEntry gumjs_native_pointer_entries[] =
{
  JS_CFUNC_DEF ("isNull", 0, gumjs_native_pointer_is_null),
  JS_CFUNC_DEF ("add", 0, gumjs_native_pointer_add),
  JS_CFUNC_DEF ("sub", 0, gumjs_native_pointer_sub),
  JS_CFUNC_DEF ("and", 0, gumjs_native_pointer_and),
  JS_CFUNC_DEF ("or", 0, gumjs_native_pointer_or),
  JS_CFUNC_DEF ("xor", 0, gumjs_native_pointer_xor),
  JS_CFUNC_DEF ("shr", 0, gumjs_native_pointer_shr),
  JS_CFUNC_DEF ("shl", 0, gumjs_native_pointer_shl),
  JS_CFUNC_DEF ("not", 0, gumjs_native_pointer_not),
  JS_CFUNC_DEF ("sign", 0, gumjs_native_pointer_sign),
  JS_CFUNC_DEF ("strip", 0, gumjs_native_pointer_strip),
  JS_CFUNC_DEF ("blend", 0, gumjs_native_pointer_blend),
  JS_CFUNC_DEF ("compare", 0, gumjs_native_pointer_compare),
  JS_CFUNC_DEF ("toInt32", 0, gumjs_native_pointer_to_int32),
  JS_CFUNC_DEF ("toUInt32", 0, gumjs_native_pointer_to_uint32),
  JS_CFUNC_DEF ("toString", 0, gumjs_native_pointer_to_string),
  JS_CFUNC_DEF ("toJSON", 0, gumjs_native_pointer_to_json),
  JS_CFUNC_DEF ("toMatchPattern", 0,
      gumjs_native_pointer_to_match_pattern),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Pointer", POINTER),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("S8", S8),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("U8", U8),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("S16", S16),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("U16", U16),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("S32", S32),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("U32", U32),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("S64", S64),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("U64", U64),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Short", S16),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("UShort", U16),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Int", S32),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("UInt", U32),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Long", LONG),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("ULong", ULONG),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Float", FLOAT),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Double", DOUBLE),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("ByteArray", BYTE_ARRAY),
  GUMJS_EXPORT_NATIVE_POINTER_READ ("CString", C_STRING),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Utf8String", UTF8_STRING),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("Utf16String", UTF16_STRING),
  GUMJS_EXPORT_NATIVE_POINTER_READ_WRITE ("AnsiString", ANSI_STRING),
  JS_CFUNC_DEF ("readVolatile", 0, gumjs_native_pointer_read_volatile),
  JS_CFUNC_DEF ("writeVolatile", 0, gumjs_native_pointer_write_volatile),
};

static const JSCFunctionListEntry gumjs_array_buffer_class_entries[] =
{
  JS_CFUNC_DEF ("wrap", 0, gumjs_array_buffer_wrap),
};

static const JSCFunctionListEntry gumjs_array_buffer_instance_entries[] =
{
  JS_CFUNC_DEF ("unwrap", 0, gumjs_array_buffer_unwrap),
};

static const JSClassDef gumjs_native_resource_def =
{
  .class_name = "NativeResource",
  .finalizer = gumjs_native_resource_finalize,
};

static const JSClassDef gumjs_kernel_resource_def =
{
  .class_name = "KernelResource",
  .finalizer = gumjs_kernel_resource_finalize,
};

static const JSClassDef gumjs_native_function_def =
{
  .class_name = "NativeFunction",
  .finalizer = gumjs_native_function_finalize,
  .call = gumjs_native_function_invoke,
};

static const JSCFunctionListEntry gumjs_native_function_entries[] =
{
  JS_CFUNC_DEF ("call", 0, gumjs_native_function_call),
  JS_CFUNC_DEF ("apply", 2, gumjs_native_function_apply),
};

static const JSClassDef gumjs_system_function_def =
{
  .class_name = "SystemFunction",
  .finalizer = gumjs_system_function_finalize,
  .call = gumjs_system_function_invoke,
};

static const JSCFunctionListEntry gumjs_system_function_entries[] =
{
  JS_CFUNC_DEF ("call", 0, gumjs_system_function_call),
  JS_CFUNC_DEF ("apply", 2, gumjs_system_function_apply),
};

static const JSClassDef gumjs_native_callback_def =
{
  .class_name = "NativeCallback",
  .finalizer = gumjs_native_callback_finalize,
};

static const JSClassDef gumjs_callback_context_def =
{
  .class_name = "CallbackContext",
  .finalizer = gumjs_callback_context_finalize,
};

static const JSCFunctionListEntry gumjs_callback_context_entries[] =
{
  JS_CGETSET_DEF ("returnAddress", gumjs_callback_context_get_return_address,
      NULL),
  JS_CGETSET_DEF ("context", gumjs_callback_context_get_cpu_context, NULL),
  JS_CGETSET_DEF (GUMJS_SYSTEM_ERROR_FIELD,
      gumjs_callback_context_get_system_error,
      gumjs_callback_context_set_system_error),
};

static const JSClassDef gumjs_cpu_context_def =
{
  .class_name = "CpuContext",
  .finalizer = gumjs_cpu_context_finalize,
};

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED(A, R) \
    GUMJS_DEFINE_GETTER (gumjs_cpu_context_get_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return _gum_quick_native_pointer_new (ctx, \
          GSIZE_TO_POINTER (self->handle->R), core); \
    } \
    \
    GUMJS_DEFINE_SETTER (gumjs_cpu_context_set_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return gumjs_cpu_context_set_gpr (self, ctx, val, \
          (gpointer *) &self->handle->R); \
    }
#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR(R) \
    GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (R, R)

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR(A, R) \
    GUMJS_DEFINE_GETTER (gumjs_cpu_context_get_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return JS_NewArrayBufferCopy (ctx, self->handle->R, \
          sizeof (self->handle->R)); \
    } \
    \
    GUMJS_DEFINE_SETTER (gumjs_cpu_context_set_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return gumjs_cpu_context_set_vector (self, ctx, val, self->handle->R, \
          sizeof (self->handle->R)); \
    }

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM(A, INDEX) \
    GUMJS_DEFINE_GETTER (gumjs_cpu_context_get_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      if (self->handle->xmm == NULL) \
        return JS_NULL; \
      \
      return JS_NewArrayBufferCopy (ctx, self->handle->xmm[INDEX].q, \
          sizeof (self->handle->xmm[INDEX].q)); \
    } \
    \
    GUMJS_DEFINE_SETTER (gumjs_cpu_context_set_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      if (self->handle->xmm == NULL) \
        return _gum_quick_throw_literal (ctx, \
            "vector registers are not available"); \
      \
      return gumjs_cpu_context_set_vector (self, ctx, val, \
          self->handle->xmm[INDEX].q, sizeof (self->handle->xmm[INDEX].q)); \
    }

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE(A, R) \
    GUMJS_DEFINE_GETTER (gumjs_cpu_context_get_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return JS_NewFloat64 (ctx, self->handle->R); \
    } \
    \
    GUMJS_DEFINE_SETTER (gumjs_cpu_context_set_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return gumjs_cpu_context_set_double (self, ctx, val, &self->handle->R); \
    }

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT(A, R) \
    GUMJS_DEFINE_GETTER (gumjs_cpu_context_get_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return JS_NewFloat64 (ctx, self->handle->R); \
    } \
    \
    GUMJS_DEFINE_SETTER (gumjs_cpu_context_set_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return gumjs_cpu_context_set_float (self, ctx, val, &self->handle->R); \
    }

#define GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLAGS(A, R) \
    GUMJS_DEFINE_GETTER (gumjs_cpu_context_get_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return JS_NewUint32 (ctx, self->handle->R); \
    } \
    \
    GUMJS_DEFINE_SETTER (gumjs_cpu_context_set_##A) \
    { \
      GumQuickCpuContext * self; \
      \
      if (!_gum_quick_cpu_context_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      return gumjs_cpu_context_set_flags (self, ctx, val, \
          (gsize *) &self->handle->R); \
    }

#define GUM_EXPORT_CPU_CONTEXT_ACCESSOR_ALIASED(A, R) \
    JS_CGETSET_DEF (G_STRINGIFY (A), gumjs_cpu_context_get_##R, \
        gumjs_cpu_context_set_##R)
#define GUM_EXPORT_CPU_CONTEXT_ACCESSOR(R) \
    GUM_EXPORT_CPU_CONTEXT_ACCESSOR_ALIASED (R, R)

#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (eax)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ecx)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (edx)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ebx)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (esp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ebp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (esi)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (edi)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (eip)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm0, 0)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm1, 1)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm2, 2)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm3, 3)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm4, 4)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm5, 5)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm6, 6)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm7, 7)
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rax)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rcx)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rdx)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rbx)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rsp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rbp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rsi)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rdi)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r8)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r9)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r10)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r11)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r12)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r13)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r14)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r15)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (rip)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm0, 0)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm1, 1)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm2, 2)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm3, 3)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm4, 4)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm5, 5)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm6, 6)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm7, 7)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm8, 8)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm9, 9)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm10, 10)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm11, 11)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm12, 12)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm13, 13)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm14, 14)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_XMM (xmm15, 15)
#elif defined (HAVE_ARM)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (pc)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (sp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLAGS (cpsr, cpsr)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r0, r[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r1, r[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r2, r[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r3, r[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r4, r[4])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r5, r[5])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r6, r[6])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (r7, r[7])

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r8)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r9)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r10)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r11)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (r12)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (lr)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q0, v[0].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q1, v[1].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q2, v[2].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q3, v[3].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q4, v[4].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q5, v[5].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q6, v[6].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q7, v[7].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q8, v[8].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q9, v[9].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q10, v[10].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q11, v[11].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q12, v[12].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q13, v[13].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q14, v[14].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q15, v[15].q)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d0, v[0].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d1, v[0].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d2, v[1].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d3, v[1].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d4, v[2].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d5, v[2].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d6, v[3].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d7, v[3].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d8, v[4].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d9, v[4].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d10, v[5].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d11, v[5].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d12, v[6].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d13, v[6].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d14, v[7].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d15, v[7].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d16, v[8].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d17, v[8].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d18, v[9].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d19, v[9].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d20, v[10].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d21, v[10].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d22, v[11].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d23, v[11].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d24, v[12].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d25, v[12].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d26, v[13].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d27, v[13].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d28, v[14].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d29, v[14].d[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d30, v[15].d[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d31, v[15].d[1])

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s0, v[0].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s1, v[0].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s2, v[0].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s3, v[0].s[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s4, v[1].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s5, v[1].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s6, v[1].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s7, v[1].s[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s8, v[2].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s9, v[2].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s10, v[2].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s11, v[2].s[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s12, v[3].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s13, v[3].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s14, v[3].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s15, v[3].s[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s16, v[4].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s17, v[4].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s18, v[4].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s19, v[4].s[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s20, v[5].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s21, v[5].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s22, v[5].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s23, v[5].s[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s24, v[6].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s25, v[6].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s26, v[6].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s27, v[6].s[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s28, v[7].s[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s29, v[7].s[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s30, v[7].s[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s31, v[7].s[3])
#elif defined (HAVE_ARM64)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (pc)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (sp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLAGS (nzcv, nzcv)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x0, x[0])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x1, x[1])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x2, x[2])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x3, x[3])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x4, x[4])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x5, x[5])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x6, x[6])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x7, x[7])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x8, x[8])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x9, x[9])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x10, x[10])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x11, x[11])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x12, x[12])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x13, x[13])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x14, x[14])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x15, x[15])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x16, x[16])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x17, x[17])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x18, x[18])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x19, x[19])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x20, x[20])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x21, x[21])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x22, x[22])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x23, x[23])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x24, x[24])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x25, x[25])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x26, x[26])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x27, x[27])
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR_ALIASED (x28, x[28])

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (fp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (lr)

#ifndef G_OS_NONE
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q0, v[0].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q1, v[1].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q2, v[2].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q3, v[3].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q4, v[4].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q5, v[5].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q6, v[6].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q7, v[7].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q8, v[8].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q9, v[9].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q10, v[10].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q11, v[11].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q12, v[12].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q13, v[13].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q14, v[14].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q15, v[15].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q16, v[16].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q17, v[17].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q18, v[18].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q19, v[19].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q20, v[20].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q21, v[21].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q22, v[22].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q23, v[23].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q24, v[24].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q25, v[25].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q26, v[26].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q27, v[27].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q28, v[28].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q29, v[29].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q30, v[30].q)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_VECTOR (q31, v[31].q)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d0, v[0].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d1, v[1].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d2, v[2].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d3, v[3].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d4, v[4].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d5, v[5].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d6, v[6].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d7, v[7].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d8, v[8].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d9, v[9].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d10, v[10].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d11, v[11].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d12, v[12].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d13, v[13].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d14, v[14].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d15, v[15].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d16, v[16].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d17, v[17].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d18, v[18].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d19, v[19].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d20, v[20].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d21, v[21].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d22, v[22].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d23, v[23].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d24, v[24].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d25, v[25].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d26, v[26].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d27, v[27].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d28, v[28].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d29, v[29].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d30, v[30].d)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_DOUBLE (d31, v[31].d)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s0, v[0].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s1, v[1].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s2, v[2].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s3, v[3].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s4, v[4].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s5, v[5].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s6, v[6].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s7, v[7].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s8, v[8].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s9, v[9].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s10, v[10].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s11, v[11].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s12, v[12].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s13, v[13].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s14, v[14].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s15, v[15].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s16, v[16].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s17, v[17].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s18, v[18].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s19, v[19].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s20, v[20].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s21, v[21].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s22, v[22].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s23, v[23].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s24, v[24].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s25, v[25].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s26, v[26].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s27, v[27].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s28, v[28].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s29, v[29].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s30, v[30].s)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_FLOAT (s31, v[31].s)
#endif
#elif defined (HAVE_MIPS)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (pc)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (gp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (sp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (fp)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (ra)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (hi)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (lo)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (at)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (v0)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (v1)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a0)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a1)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a2)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (a3)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t0)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t1)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t2)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t3)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t4)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t5)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t6)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t7)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t8)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (t9)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s0)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s1)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s2)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s3)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s4)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s5)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s6)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (s7)

GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (k0)
GUM_DEFINE_CPU_CONTEXT_ACCESSOR_GPR (k1)
#endif

static const JSCFunctionListEntry gumjs_cpu_context_entries[] =
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR_ALIASED (pc, eip),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR_ALIASED (sp, esp),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (eax),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (ecx),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (edx),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (ebx),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (esp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (ebp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (esi),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (edi),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (eip),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm7),
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR_ALIASED (pc, rip),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR_ALIASED (sp, rsp),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rax),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rcx),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rdx),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rbx),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rsp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rbp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rsi),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rdi),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r15),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (rip),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (xmm15),
#elif defined (HAVE_ARM)
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (pc),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (sp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (cpsr),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r7),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (r12),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (lr),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q15),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d15),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d16),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d17),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d18),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d19),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d20),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d21),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d22),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d23),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d24),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d25),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d26),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d27),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d28),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d29),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d30),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d31),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s15),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s16),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s17),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s18),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s19),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s20),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s21),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s22),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s23),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s24),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s25),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s26),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s27),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s28),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s29),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s30),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s31),
#elif defined (HAVE_ARM64)
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (pc),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (sp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (nzcv),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x15),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x16),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x17),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x18),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x19),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x20),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x21),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x22),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x23),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x24),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x25),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x26),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x27),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (x28),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (fp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (lr),

#ifndef G_OS_NONE
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q15),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q16),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q17),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q18),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q19),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q20),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q21),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q22),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q23),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q24),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q25),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q26),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q27),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q28),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q29),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q30),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (q31),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d15),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d16),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d17),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d18),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d19),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d20),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d21),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d22),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d23),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d24),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d25),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d26),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d27),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d28),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d29),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d30),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (d31),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s9),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s10),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s11),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s12),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s13),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s14),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s15),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s16),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s17),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s18),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s19),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s20),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s21),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s22),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s23),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s24),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s25),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s26),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s27),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s28),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s29),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s30),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s31),
#endif
#elif defined (HAVE_MIPS)
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (pc),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (gp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (sp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (fp),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (ra),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (hi),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (lo),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (at),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (v0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (v1),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (a0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (a1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (a2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (a3),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t7),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t8),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (t9),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s1),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s2),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s3),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s4),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s5),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s6),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (s7),

  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (k0),
  GUM_EXPORT_CPU_CONTEXT_ACCESSOR (k1),
#endif

  JS_CFUNC_DEF ("toJSON", 0, gumjs_cpu_context_to_json),
};

static const JSClassDef gumjs_match_pattern_def =
{
  .class_name = "MatchPattern",
  .finalizer = gumjs_match_pattern_finalize,
};

static const JSClassDef gumjs_source_map_def =
{
  .class_name = "SourceMap",
  .finalizer = gumjs_source_map_finalize,
};

static const JSCFunctionListEntry gumjs_source_map_entries[] =
{
  JS_CFUNC_DEF ("_resolve", 0, gumjs_source_map_resolve),
};

static const JSClassDef gumjs_worker_def =
{
  .class_name = "_Worker",
};

static const JSCFunctionListEntry gumjs_worker_entries[] =
{
  JS_CFUNC_DEF ("terminate", 0, gumjs_worker_terminate),
  JS_CFUNC_DEF ("post", 0, gumjs_worker_post),
};

void
_gum_quick_core_init (GumQuickCore * self,
                      GumQuickScript * script,
                      JSContext * ctx,
                      JSValue ns,
                      GRecMutex * mutex,
                      GumESProgram * program,
                      GumQuickInterceptor * interceptor,
                      GumQuickStalker * stalker,
                      GumQuickMessageEmitter message_emitter,
                      gpointer message_emitter_data,
                      GumScriptScheduler * scheduler)
{
  JSRuntime * rt;
  JSValue global_obj, obj, proto, ctor, uint64_proto;

  rt = JS_GetRuntime (ctx);

  global_obj = JS_GetGlobalObject (ctx);

  g_object_get (script, "backend", &self->backend, NULL);
  g_object_unref (self->backend);

  self->script = script;
  self->program = program;
  self->interceptor = interceptor;
  self->stalker = stalker;
  self->message_emitter = message_emitter;
  self->message_emitter_data = message_emitter_data;
  self->scheduler = scheduler;
  self->exceptor = gum_exceptor_obtain ();
  self->rt = rt;
  self->ctx = ctx;
  self->module_data =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  self->current_scope = NULL;
  self->current_owner = GUM_THREAD_ID_INVALID;
  self->transaction_released_owner = GUM_THREAD_ID_INVALID;

  gum_quick_core_setup_atoms (self);

  self->mutex = mutex;
  self->usage_count = 0;
  self->mutex_depth = 0;
  self->flush_notify = NULL;
  self->flush_data = NULL;
  self->flush_data_destroy = NULL;

  self->event_loop = g_main_loop_new (
      gum_script_scheduler_get_js_context (scheduler), FALSE);
  g_mutex_init (&self->event_mutex);
  g_cond_init (&self->event_cond);
  self->event_count = 0;
  self->event_source_available = TRUE;

  self->on_global_get = JS_NULL;
  self->global_receiver = JS_NULL;

  self->weak_callbacks = g_hash_table_new (NULL, NULL);
  self->next_weak_callback_id = 1;
  ctor = JS_GetPropertyStr (ctx, global_obj, "WeakMap");
  proto = JS_GetProperty (ctx, ctor, GUM_QUICK_CORE_ATOM (self, prototype));
  self->weak_objects = JS_CallConstructor (ctx, ctor, 0, NULL);
  self->weak_map_ctor = ctor;
  self->weak_map_get_method = JS_GetPropertyStr (ctx, proto, "get");
  self->weak_map_set_method = JS_GetPropertyStr (ctx, proto, "set");
  self->weak_map_delete_method = JS_GetPropertyStr (ctx, proto, "delete");
  JS_FreeValue (ctx, proto);

  self->scheduled_callbacks = g_hash_table_new (NULL, NULL);
  self->next_callback_id = 1;

  self->workers = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) gum_quick_worker_destroy);

  self->subclasses = g_hash_table_new (NULL, NULL);

  JS_SetPropertyFunctionList (ctx, ns, gumjs_root_entries,
      G_N_ELEMENTS (gumjs_root_entries));

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_frida_entries,
      G_N_ELEMENTS (gumjs_frida_entries));
  JS_DefinePropertyValueStr (ctx, ns, "Frida", obj, JS_PROP_C_W_E);

  obj = JS_NewObject (ctx);
  JS_SetPropertyFunctionList (ctx, obj, gumjs_script_entries,
      G_N_ELEMENTS (gumjs_script_entries));
  JS_DefinePropertyValueStr (ctx, ns, "Script", obj, JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_weak_ref_def, self,
      &self->weak_ref_class, &proto);

  _gum_quick_create_class (ctx, &gumjs_int64_def, self, &self->int64_class,
      &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_int64_construct,
      gumjs_int64_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_int64_entries,
      G_N_ELEMENTS (gumjs_int64_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_int64_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_uint64_def, self, &self->uint64_class,
      &proto);
  uint64_proto = proto;
  ctor = JS_NewCFunction2 (ctx, gumjs_uint64_construct,
      gumjs_uint64_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_uint64_entries,
      G_N_ELEMENTS (gumjs_uint64_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_uint64_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_native_pointer_def, self,
      &self->native_pointer_class, &proto);
  self->native_pointer_proto = JS_DupValue (ctx, proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_native_pointer_construct,
      gumjs_native_pointer_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_native_pointer_entries,
      G_N_ELEMENTS (gumjs_native_pointer_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_native_pointer_def.class_name, ctor,
      JS_PROP_C_W_E);

  obj = JS_GetPropertyStr (ctx, global_obj, "ArrayBuffer");
  JS_SetPropertyFunctionList (ctx, obj, gumjs_array_buffer_class_entries,
      G_N_ELEMENTS (gumjs_array_buffer_class_entries));
  proto = JS_GetProperty (ctx, obj, GUM_QUICK_CORE_ATOM (self, prototype));
  JS_SetPropertyFunctionList (ctx, proto, gumjs_array_buffer_instance_entries,
      G_N_ELEMENTS (gumjs_array_buffer_instance_entries));
  JS_FreeValue (ctx, proto);
  JS_FreeValue (ctx, obj);

  _gum_quick_create_subclass (ctx, &gumjs_native_resource_def,
      self->native_pointer_class, self->native_pointer_proto, self,
      &self->native_resource_class, &proto);

  _gum_quick_create_subclass (ctx, &gumjs_kernel_resource_def,
      self->uint64_class, uint64_proto, self, &self->kernel_resource_class,
      &proto);

  _gum_quick_create_subclass (ctx, &gumjs_native_function_def,
      self->native_pointer_class, self->native_pointer_proto, self,
      &self->native_function_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_native_function_construct,
      gumjs_native_function_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_native_function_entries,
      G_N_ELEMENTS (gumjs_native_function_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_native_function_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_create_subclass (ctx, &gumjs_system_function_def,
      self->native_pointer_class, self->native_pointer_proto, self,
      &self->system_function_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_system_function_construct,
      gumjs_system_function_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_system_function_entries,
      G_N_ELEMENTS (gumjs_system_function_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_system_function_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_create_subclass (ctx, &gumjs_native_callback_def,
      self->native_pointer_class, self->native_pointer_proto, self,
      &self->native_callback_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_native_callback_construct,
      gumjs_native_callback_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_DefinePropertyValueStr (ctx, ns, gumjs_native_callback_def.class_name,
      ctor, JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_callback_context_def, self,
      &self->callback_context_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_callback_context_entries,
      G_N_ELEMENTS (gumjs_callback_context_entries));

  _gum_quick_create_class (ctx, &gumjs_cpu_context_def, self,
      &self->cpu_context_class, &proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_cpu_context_entries,
      G_N_ELEMENTS (gumjs_cpu_context_entries));

  _gum_quick_create_class (ctx, &gumjs_match_pattern_def, self,
      &self->match_pattern_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_match_pattern_construct,
      gumjs_match_pattern_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_DefinePropertyValueStr (ctx, ns, gumjs_match_pattern_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_source_map_def, self,
      &self->source_map_class, &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_source_map_construct,
      gumjs_source_map_def.class_name, 0, JS_CFUNC_constructor, 0);
  self->source_map_ctor = JS_DupValue (ctx, ctor);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_source_map_entries,
      G_N_ELEMENTS (gumjs_source_map_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_source_map_def.class_name, ctor,
      JS_PROP_C_W_E);

  _gum_quick_create_class (ctx, &gumjs_worker_def, self, &self->worker_class,
      &proto);
  ctor = JS_NewCFunction2 (ctx, gumjs_worker_construct,
      gumjs_worker_def.class_name, 0, JS_CFUNC_constructor, 0);
  JS_SetConstructor (ctx, ctor, proto);
  JS_SetPropertyFunctionList (ctx, proto, gumjs_worker_entries,
      G_N_ELEMENTS (gumjs_worker_entries));
  JS_DefinePropertyValueStr (ctx, ns, gumjs_worker_def.class_name, ctor,
      JS_PROP_C_W_E);

  JS_FreeValue (ctx, global_obj);

  gum_exceptor_add (self->exceptor, gum_quick_core_handle_crashed_js, self);
}

static gboolean
gum_quick_core_handle_crashed_js (GumExceptionDetails * details,
                                  gpointer user_data)
{
  GumQuickCore * self = user_data;
  GumThreadId thread_id = details->thread_id;

  if (gum_exceptor_has_scope (self->exceptor, thread_id))
    return FALSE;

  if (self->current_owner == thread_id &&
      self->transaction_released_owner != thread_id)
  {
    gum_interceptor_end_transaction (self->interceptor->interceptor);
    self->transaction_released_owner = thread_id;
    gum_quick_script_backend_mark_scope_mutex_trapped (self->backend);
  }

  return FALSE;
}

gboolean
_gum_quick_core_flush (GumQuickCore * self,
                       GumQuickFlushNotify flush_notify,
                       gpointer flush_data,
                       GDestroyNotify flush_data_destroy)
{
  JSContext * ctx = self->ctx;
  GHashTableIter iter;
  GumQuickScheduledCallback * callback;
  JSValue old_objects;
  gboolean done;

  self->flush_notify = flush_notify;
  self->flush_data = flush_data;
  self->flush_data_destroy = flush_data_destroy;

  g_mutex_lock (&self->event_mutex);
  self->event_source_available = FALSE;
  g_cond_broadcast (&self->event_cond);
  g_mutex_unlock (&self->event_mutex);
  g_main_loop_quit (self->event_loop);

  if (self->usage_count > 1)
    return FALSE;

  g_hash_table_iter_init (&iter, self->scheduled_callbacks);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &callback))
  {
    _gum_quick_core_pin (self);
    g_source_destroy (callback->source);
  }
  g_hash_table_remove_all (self->scheduled_callbacks);

  if (self->usage_count > 1)
    return FALSE;

  old_objects = self->weak_objects;
  self->weak_objects = JS_CallConstructor (ctx, self->weak_map_ctor, 0, NULL);
  JS_FreeValue (ctx, old_objects);

  done = self->usage_count == 1;
  if (done)
  {
    if (flush_data_destroy != NULL)
      flush_data_destroy (flush_data);

    self->flush_notify = NULL;
    self->flush_data = NULL;
    self->flush_data_destroy = NULL;
  }

  return done;
}

static void
gum_quick_core_notify_flushed (GumQuickCore * self,
                               GumQuickFlushNotify func,
                               gpointer data,
                               GDestroyNotify data_destroy)
{
  GumQuickFlushCallback * cb;
  GSource * source;

  cb = g_slice_new (GumQuickFlushCallback);
  cb->func = func;
  cb->data = data;
  cb->data_destroy = data_destroy;

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) gum_quick_flush_callback_notify,
      cb, (GDestroyNotify) gum_quick_flush_callback_free);
  g_source_attach (source,
      gum_script_scheduler_get_js_context (self->scheduler));
  g_source_unref (source);
}

static void
gum_quick_flush_callback_free (GumQuickFlushCallback * self)
{
  if (self->data_destroy != NULL)
    self->data_destroy (self->data);

  g_slice_free (GumQuickFlushCallback, self);
}

static gboolean
gum_quick_flush_callback_notify (GumQuickFlushCallback * self)
{
  self->func (self->data);
  return FALSE;
}

void
_gum_quick_core_dispose (GumQuickCore * self)
{
  JSContext * ctx = self->ctx;

  g_hash_table_remove_all (self->workers);

  g_assert (g_hash_table_size (self->weak_callbacks) == 0);

  JS_SetGlobalAccessFunctions (ctx, NULL);

  JS_FreeValue (ctx, self->on_global_get);
  JS_FreeValue (ctx, self->global_receiver);
  self->on_global_get = JS_NULL;
  self->global_receiver = JS_NULL;

  g_clear_pointer (&self->unhandled_exception_sink,
      gum_quick_exception_sink_free);

  g_clear_pointer (&self->incoming_message_sink, gum_quick_message_sink_free);

  gum_exceptor_remove (self->exceptor, gum_quick_core_handle_crashed_js, self);
  g_object_unref (self->exceptor);
  self->exceptor = NULL;

  JS_FreeValue (ctx, self->source_map_ctor);
  JS_FreeValue (ctx, self->native_pointer_proto);

  JS_FreeValue (ctx, self->weak_objects);
  JS_FreeValue (ctx, self->weak_map_ctor);
  JS_FreeValue (ctx, self->weak_map_get_method);
  JS_FreeValue (ctx, self->weak_map_set_method);
  JS_FreeValue (ctx, self->weak_map_delete_method);
  self->weak_objects = JS_NULL;
  self->weak_map_ctor = JS_NULL;
  self->weak_map_get_method = JS_NULL;
  self->weak_map_set_method = JS_NULL;
  self->weak_map_delete_method = JS_NULL;

  gum_quick_core_teardown_atoms (self);
}

void
_gum_quick_core_finalize (GumQuickCore * self)
{
  g_hash_table_unref (self->subclasses);
  self->subclasses = NULL;

  g_hash_table_unref (self->workers);
  self->workers = NULL;

  g_hash_table_unref (self->scheduled_callbacks);
  self->scheduled_callbacks = NULL;

  g_hash_table_unref (self->weak_callbacks);
  self->weak_callbacks = NULL;

  g_main_loop_unref (self->event_loop);
  self->event_loop = NULL;
  g_mutex_clear (&self->event_mutex);
  g_cond_clear (&self->event_cond);

  g_assert (self->current_scope == NULL);
  self->ctx = NULL;

  g_hash_table_unref (self->module_data);
  self->module_data = NULL;
}

void
_gum_quick_core_pin (GumQuickCore * self)
{
  self->usage_count++;
}

void
_gum_quick_core_unpin (GumQuickCore * self)
{
  self->usage_count--;
}

void
_gum_quick_core_on_unhandled_exception (GumQuickCore * self,
                                        JSValue exception)
{
  if (self->unhandled_exception_sink == NULL)
    return;

  gum_quick_exception_sink_handle_exception (self->unhandled_exception_sink,
      exception);
}

void
_gum_quick_core_post (GumQuickCore * self,
                      const gchar * message,
                      GBytes * data)
{
  gboolean delivered = FALSE;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, self);

  if (self->incoming_message_sink != NULL)
  {
    gum_quick_message_sink_post (self->incoming_message_sink, message, data,
        &scope);
    delivered = TRUE;
  }

  _gum_quick_scope_leave (&scope);

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

void
_gum_quick_core_push_job (GumQuickCore * self,
                          GumScriptJobFunc job_func,
                          gpointer data,
                          GDestroyNotify data_destroy)
{
  gum_script_scheduler_push_job_on_thread_pool (self->scheduler, job_func,
      data, data_destroy);
}

void
_gum_quick_core_store_module_data (GumQuickCore * self,
                                   const gchar * key,
                                   gpointer value)
{
  g_hash_table_insert (self->module_data, g_strdup (key), value);
}

gpointer
_gum_quick_core_load_module_data (GumQuickCore * self,
                                  const gchar * key)
{
  return g_hash_table_lookup (self->module_data, key);
}

void
_gum_quick_scope_enter (GumQuickScope * self,
                        GumQuickCore * core)
{
  self->core = core;

  if (core->interceptor != NULL)
    gum_interceptor_begin_transaction (core->interceptor->interceptor);

  g_rec_mutex_lock (core->mutex);

  _gum_quick_core_pin (core);
  core->mutex_depth++;

  if (core->mutex_depth == 1)
  {
    g_assert (core->current_scope == NULL);
    core->current_scope = self;
    core->current_owner = gum_process_get_current_thread_id ();

    JS_Enter (core->rt);
#ifdef HAVE_TINY_STACK
    {
      gsize available = gum_barebone_query_stack_size ();
      if (available != 0)
        JS_SetMaxStackSize (core->rt, available - available / 4);
    }
#endif

    _gum_quick_script_on_scope_entered (core);
  }

  g_queue_init (&self->tick_callbacks);
  g_queue_init (&self->scheduled_sources);

  self->pending_stalker_level = 0;
  self->pending_stalker_transformer = NULL;
  self->pending_stalker_sink = NULL;
}

void
_gum_quick_scope_suspend (GumQuickScope * self)
{
  GumQuickCore * core = self->core;
  guint i;

  JS_Suspend (core->rt, &self->thread_state);

  g_assert (core->current_scope != NULL);
  self->previous_scope = g_steal_pointer (&core->current_scope);
  self->previous_owner = core->current_owner;
  core->current_owner = GUM_THREAD_ID_INVALID;

  self->previous_mutex_depth = core->mutex_depth;
  core->mutex_depth = 0;

  for (i = 0; i != self->previous_mutex_depth; i++)
    g_rec_mutex_unlock (core->mutex);

  if (core->interceptor != NULL)
    gum_interceptor_end_transaction (core->interceptor->interceptor);
}

void
_gum_quick_scope_resume (GumQuickScope * self)
{
  GumQuickCore * core = self->core;
  guint i;

  if (core->interceptor != NULL)
    gum_interceptor_begin_transaction (core->interceptor->interceptor);

  for (i = 0; i != self->previous_mutex_depth; i++)
    g_rec_mutex_lock (core->mutex);

  g_assert (core->current_scope == NULL);
  core->current_scope = g_steal_pointer (&self->previous_scope);
  core->current_owner = self->previous_owner;

  core->mutex_depth = self->previous_mutex_depth;
  self->previous_mutex_depth = 0;

  JS_Resume (core->rt, &self->thread_state);
}

JSValue
_gum_quick_scope_call (GumQuickScope * self,
                       JSValueConst func_obj,
                       JSValueConst this_obj,
                       int argc,
                       JSValueConst * argv)
{
  JSValue result;
  GumQuickCore * core = self->core;
  JSContext * ctx = core->ctx;

  result = JS_Call (ctx, func_obj, this_obj, argc, argv);

  if (JS_IsException (result))
    _gum_quick_scope_catch_and_emit (self);

  return result;
}

gboolean
_gum_quick_scope_call_void (GumQuickScope * self,
                            JSValueConst func_obj,
                            JSValueConst this_obj,
                            int argc,
                            JSValueConst * argv)
{
  JSValue result;

  result = _gum_quick_scope_call (self, func_obj, this_obj, argc, argv);
  if (JS_IsException (result))
    return FALSE;

  JS_FreeValue (self->core->ctx, result);

  return TRUE;
}

void
_gum_quick_scope_catch_and_emit (GumQuickScope * self)
{
  GumQuickCore * core = self->core;
  JSContext * ctx = core->ctx;
  JSValue exception;

  exception = JS_GetException (ctx);
  if (JS_IsNull (exception))
    return;

  if (gum_quick_exception_is_interrupt (ctx, exception))
  {
    JS_FreeValue (ctx, exception);
    return;
  }

  _gum_quick_core_on_unhandled_exception (core, exception);

  JS_FreeValue (ctx, exception);
}

static gboolean
gum_quick_exception_is_interrupt (JSContext * ctx,
                                  JSValueConst exception)
{
  gboolean is_interrupt;
  const char * message;

  message = JS_ToCString (ctx, exception);
  if (message == NULL)
    return FALSE;
  is_interrupt = strstr (message, "InternalError: interrupted") != NULL;
  JS_FreeCString (ctx, message);

  return is_interrupt;
}

void
_gum_quick_scope_perform_pending_io (GumQuickScope * self)
{
  GumQuickCore * core = self->core;
  JSContext * ctx = core->ctx;
  gboolean io_performed;

  do
  {
    JSContext * pctx;
    JSValue * tick_callback;
    GSource * source;

    io_performed = FALSE;

    do
    {
      int res = JS_ExecutePendingJob (core->rt, &pctx);
      if (res == -1)
        _gum_quick_scope_catch_and_emit (self);
    }
    while (pctx != NULL);

    while ((tick_callback = g_queue_pop_head (&self->tick_callbacks)) != NULL)
    {
      _gum_quick_scope_call_void (self, *tick_callback, JS_UNDEFINED, 0, NULL);

      JS_FreeValue (ctx, *tick_callback);
      g_slice_free (JSValue, tick_callback);

      io_performed = TRUE;
    }

    while ((source = g_queue_pop_head (&self->scheduled_sources)) != NULL)
    {
      if (!g_source_is_destroyed (source))
      {
        g_source_attach (source,
            gum_script_scheduler_get_js_context (core->scheduler));
      }

      g_source_unref (source);

      io_performed = TRUE;
    }
  }
  while (io_performed);
}

void
_gum_quick_scope_leave (GumQuickScope * self)
{
  GumQuickCore * core = self->core;
  GumQuickFlushNotify flush_notify = NULL;
  gpointer flush_data = NULL;
  GDestroyNotify flush_data_destroy = NULL;

  _gum_quick_scope_perform_pending_io (self);

  if (core->mutex_depth == 1)
  {
    _gum_quick_script_on_scope_left (core);

    JS_Leave (core->rt);

    core->current_scope = NULL;
    core->current_owner = GUM_THREAD_ID_INVALID;
  }

  core->mutex_depth--;
  _gum_quick_core_unpin (core);

  if (core->flush_notify != NULL && core->usage_count == 0)
  {
    flush_notify = g_steal_pointer (&core->flush_notify);
    flush_data = g_steal_pointer (&core->flush_data);
    flush_data_destroy = g_steal_pointer (&core->flush_data_destroy);
  }

  g_rec_mutex_unlock (core->mutex);

  if (core->interceptor != NULL)
  {
    GumThreadId thread_id = gum_process_get_current_thread_id ();

    if (core->transaction_released_owner == thread_id)
    {
      core->transaction_released_owner = GUM_THREAD_ID_INVALID;
      gum_quick_script_backend_unmark_scope_mutex_trapped (core->backend);
    }
    else
    {
      gum_interceptor_end_transaction (core->interceptor->interceptor);
    }
  }

  if (flush_notify != NULL)
  {
    gum_quick_core_notify_flushed (core, flush_notify, flush_data,
        flush_data_destroy);
  }

  _gum_quick_stalker_process_pending (core->stalker, self);
}

GUMJS_DEFINE_GETTER (gumjs_frida_get_heap_size)
{
  return JS_NewUint32 (ctx, gum_peek_private_memory_usage ());
}

GUMJS_DEFINE_FUNCTION (gumjs_script_evaluate)
{
  const gchar * name, * source;
  JSValue func;
  gchar * source_map;

  if (!_gum_quick_args_parse (args, "ss", &name, &source))
    return JS_EXCEPTION;

  func = JS_Eval (ctx, source, strlen (source), name,
      JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY |
      JS_EVAL_FLAG_BACKTRACE_BARRIER);
  if (JS_IsException (func))
  {
    return _gum_quick_script_rethrow_parse_error_with_decorations (core->script,
        ctx, name);
  }

  source_map = gum_source_map_try_extract_inline (source);
  if (source_map != NULL)
  {
    gchar * map_name;
    GumESAsset * asset;

    map_name = g_strconcat (name, ".map", NULL);
    asset =
        gum_es_asset_new (map_name, source_map, strlen (source_map), g_free);

    g_hash_table_insert (core->program->es_assets, map_name, asset);
  }

  return JS_EvalFunction (ctx, func);
}

GUMJS_DEFINE_FUNCTION (gumjs_script_load)
{
  GHashTable * es_assets = core->program->es_assets;
  const gchar * name, * source;
  JSValue perform_init, module;
  gchar * name_copy, * source_map;
  GumQuickModuleInitOperation * op;
  GSource * gsource;

  if (!_gum_quick_args_parse (args, "ssF", &name, &source, &perform_init))
    return JS_EXCEPTION;

  if (g_hash_table_contains (es_assets, name))
    return _gum_quick_throw (ctx, "module '%s' already exists", name);

  module = JS_Eval (ctx, source, strlen (source), name,
      JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_STRICT | JS_EVAL_FLAG_COMPILE_ONLY |
      JS_EVAL_FLAG_BACKTRACE_BARRIER);
  if (JS_IsException (module))
  {
    return _gum_quick_script_rethrow_parse_error_with_decorations (core->script,
        ctx, name);
  }

  name_copy = g_strdup (name);
  g_hash_table_insert (es_assets, name_copy,
      gum_es_asset_new (name_copy, NULL, 0, NULL));

  source_map = gum_source_map_try_extract_inline (source);
  if (source_map != NULL)
  {
    gchar * map_name;
    GumESAsset * asset;

    map_name = g_strconcat (name, ".map", NULL);
    asset =
        gum_es_asset_new (map_name, source_map, strlen (source_map), g_free);

    g_hash_table_insert (es_assets, map_name, asset);
  }

  /*
   * QuickJS does not support having a synchronously evaluating module
   * dynamically define and evaluate a new module depending on itself.
   * This is only allowed if it is an asynchronously evaluating module.
   * We defer the evaluation to avoid this edge-case.
   */
  op = g_slice_new (GumQuickModuleInitOperation);
  op->module = module;
  op->perform_init = JS_DupValue (ctx, perform_init);
  op->core = core;

  gsource = g_idle_source_new ();
  g_source_set_callback (gsource, (GSourceFunc) gum_quick_core_init_module,
      op, NULL);
  g_source_attach (gsource,
      gum_script_scheduler_get_js_context (core->scheduler));
  g_source_unref (gsource);

  _gum_quick_core_pin (core);

  return JS_UNDEFINED;
}

static gboolean
gum_quick_core_init_module (GumQuickModuleInitOperation * op)
{
  GumQuickCore * self = op->core;
  JSContext * ctx = self->ctx;
  GumQuickScope scope;
  JSValue result;

  _gum_quick_scope_enter (&scope, self);

  result = JS_EvalFunction (ctx, op->module);
  _gum_quick_scope_call_void (&scope, op->perform_init, JS_UNDEFINED,
      1, &result);
  JS_FreeValue (ctx, result);

  JS_FreeValue (ctx, op->perform_init);
  g_slice_free (GumQuickModuleInitOperation, op);

  _gum_quick_core_unpin (self);

  _gum_quick_scope_leave (&scope);

  return G_SOURCE_REMOVE;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_register_source_map)
{
  const gchar * name, * json;
  gchar * map_name;
  GumESAsset * asset;

  if (!_gum_quick_args_parse (args, "ss", &name, &json))
    return JS_EXCEPTION;

  map_name = g_strconcat (name, ".map", NULL);
  asset = gum_es_asset_new (map_name, g_strdup (json), strlen (json), g_free);

  g_hash_table_insert (core->program->es_assets, map_name, asset);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_find_source_map)
{
  GumESProgram * program = core->program;
  JSValue map = JS_NULL;
  const gchar * name, * json;

  if (!_gum_quick_args_parse (args, "s", &name))
    return JS_EXCEPTION;

  json = NULL;

  if (program->es_assets != NULL)
  {
    gchar * map_name;
    GumESAsset * map_asset;

    map_name = g_strconcat (name, ".map", NULL);

    map_asset = g_hash_table_lookup (program->es_assets, map_name);
    if (map_asset != NULL)
    {
      json = map_asset->data;
    }

    g_free (map_name);
  }

  if (json == NULL)
  {
    if (g_strcmp0 (name, program->global_filename) == 0)
      json = program->global_source_map;
  }

  if (json != NULL)
    map = gumjs_source_map_new (json, core);

  return map;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_next_tick)
{
  JSValue callback;

  if (!_gum_quick_args_parse (args, "F", &callback))
    return JS_EXCEPTION;

  JS_DupValue (ctx, callback);
  g_queue_push_tail (&core->current_scope->tick_callbacks,
      g_slice_dup (JSValue, &callback));

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_pin)
{
  _gum_quick_core_pin (core);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_unpin)
{
  _gum_quick_core_unpin (core);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_bind_weak)
{
  guint id;
  JSValue target, callback;
  JSValue wrapper = JS_NULL;
  GumQuickWeakRef * ref;
  GumQuickWeakCallback entry;

  if (!_gum_quick_args_parse (args, "VF", &target, &callback))
    goto propagate_exception;

  wrapper = JS_Call (ctx, core->weak_map_get_method, core->weak_objects,
      1, &target);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  if (JS_IsUndefined (wrapper))
  {
    JSValue argv[2], val;

    wrapper = JS_NewObjectClass (ctx, core->weak_ref_class);

    ref = g_slice_new (GumQuickWeakRef);
    ref->target = target;
    ref->callbacks = g_array_new (FALSE, FALSE, sizeof (GumQuickWeakCallback));

    JS_SetOpaque (wrapper, ref);

    argv[0] = target;
    argv[1] = wrapper;
    val = JS_Call (ctx, core->weak_map_set_method, core->weak_objects,
        G_N_ELEMENTS (argv), argv);
    if (JS_IsException (val))
      goto propagate_exception;
    JS_FreeValue (ctx, val);
  }
  else
  {
    ref = JS_GetOpaque2 (ctx, wrapper, core->weak_ref_class);
  }

  id = core->next_weak_callback_id++;

  entry.id = id;
  entry.callback = JS_DupValue (ctx, callback);
  g_array_append_val (ref->callbacks, entry);

  g_hash_table_insert (core->weak_callbacks, GUINT_TO_POINTER (id), ref);

  JS_FreeValue (ctx, wrapper);

  return JS_NewInt32 (ctx, id);

propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_script_unbind_weak)
{
  guint id;
  GumQuickWeakRef * ref;
  GArray * callbacks;

  if (!_gum_quick_args_parse (args, "u", &id))
    return JS_EXCEPTION;

  ref = g_hash_table_lookup (core->weak_callbacks, GUINT_TO_POINTER (id));
  if (ref == NULL)
    return JS_FALSE;

  callbacks = ref->callbacks;

  if (callbacks->len == 1)
  {
    JS_Call (ctx, core->weak_map_delete_method, core->weak_objects,
        1, &ref->target);
  }
  else
  {
    guint i;
    JSValue cb_val = JS_NULL;

    g_hash_table_remove (core->weak_callbacks, GUINT_TO_POINTER (id));

    for (i = 0; i != callbacks->len; i++)
    {
      GumQuickWeakCallback * entry =
          &g_array_index (callbacks, GumQuickWeakCallback, i);

      if (entry->id == id)
      {
        cb_val = entry->callback;
        g_array_remove_index (callbacks, i);
        break;
      }
    }

    _gum_quick_scope_call_void (core->current_scope, cb_val, JS_UNDEFINED,
        0, NULL);

    JS_FreeValue (ctx, cb_val);
  }

  return JS_TRUE;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_deref_weak)
{
  guint id;
  GumQuickWeakRef * ref;

  if (!_gum_quick_args_parse (args, "u", &id))
    return JS_EXCEPTION;

  ref = g_hash_table_lookup (core->weak_callbacks, GUINT_TO_POINTER (id));
  if (ref == NULL)
    return JS_UNDEFINED;

  return JS_DupValue (ctx, ref->target);
}

GUMJS_DEFINE_FINALIZER (gumjs_weak_ref_finalize)
{
  GumQuickWeakRef * ref;
  GArray * callbacks;
  guint i;

  ref = JS_GetOpaque (val, core->weak_ref_class);

  ref->target = JS_UNDEFINED;

  callbacks = ref->callbacks;
  for (i = 0; i != callbacks->len; i++)
  {
    GumQuickWeakCallback * entry =
        &g_array_index (callbacks, GumQuickWeakCallback, i);
    g_hash_table_remove (core->weak_callbacks, GUINT_TO_POINTER (entry->id));
  }

  g_queue_push_tail (&core->pending_weak_refs, ref);

  if (core->pending_weak_source == NULL)
  {
    GSource * source = g_idle_source_new ();

    g_source_set_callback (source,
        (GSourceFunc) gum_quick_core_invoke_pending_weak_callbacks_in_idle,
        core, NULL);
    g_source_attach (source,
        gum_script_scheduler_get_js_context (core->scheduler));
    g_source_unref (source);

    _gum_quick_core_pin (core);

    core->pending_weak_source = source;
  }
}

static gboolean
gum_quick_core_invoke_pending_weak_callbacks_in_idle (GumQuickCore * self)
{
  GumQuickWeakRef * ref;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, self);

  self->pending_weak_source = NULL;

  while ((ref = g_queue_pop_head (&self->pending_weak_refs)) != NULL)
  {
    GArray * callbacks = ref->callbacks;
    guint i;

    for (i = 0; i != callbacks->len; i++)
    {
      GumQuickWeakCallback * entry =
          &g_array_index (callbacks, GumQuickWeakCallback, i);
      _gum_quick_scope_call_void (&scope, entry->callback, JS_UNDEFINED,
          0, NULL);
      JS_FreeValue (self->ctx, entry->callback);
    }
    g_array_free (callbacks, TRUE);

    g_slice_free (GumQuickWeakRef, ref);
  }

  _gum_quick_core_unpin (self);

  _gum_quick_scope_leave (&scope);

  return FALSE;
}

GUMJS_DEFINE_FUNCTION (gumjs_script_set_global_access_handler)
{
  JSValueConst * argv = args->elements;
  JSValue receiver, get;

  if (!JS_IsNull (argv[0]))
  {
    receiver = argv[0];
    if (!_gum_quick_args_parse (args, "F{get}", &get))
      return JS_EXCEPTION;
  }
  else
  {
    receiver = JS_NULL;
    get = JS_NULL;
  }

  if (JS_IsNull (receiver))
    JS_SetGlobalAccessFunctions (ctx, NULL);

  JS_FreeValue (ctx, core->on_global_get);
  JS_FreeValue (ctx, core->global_receiver);
  core->on_global_get = JS_NULL;
  core->global_receiver = JS_NULL;

  if (!JS_IsNull (receiver))
  {
    JSGlobalAccessFunctions funcs;

    core->on_global_get = JS_DupValue (ctx, get);
    core->global_receiver = JS_DupValue (ctx, receiver);

    funcs.get = gum_quick_core_on_global_get;
    funcs.opaque = core;
    JS_SetGlobalAccessFunctions (ctx, &funcs);
  }

  return JS_UNDEFINED;
}

static JSValue
gum_quick_core_on_global_get (JSContext * ctx,
                              JSAtom name,
                              void * opaque)
{
  GumQuickCore * self = opaque;
  JSValue result;
  JSValue name_val;

  name_val = JS_AtomToValue (ctx, name);

  result = _gum_quick_scope_call (self->current_scope, self->on_global_get,
      self->global_receiver, 1, &name_val);

  JS_FreeValue (ctx, name_val);

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_set_timeout)
{
  GumQuickCore * self = core;

  return gum_quick_core_schedule_callback (self, args, FALSE);
}

GUMJS_DEFINE_FUNCTION (gumjs_set_interval)
{
  GumQuickCore * self = core;

  return gum_quick_core_schedule_callback (self, args, TRUE);
}

GUMJS_DEFINE_FUNCTION (gumjs_clear_timer)
{
  GumQuickCore * self = core;
  gint id;
  GumQuickScheduledCallback * callback;

  if (!JS_IsNumber (args->elements[0]))
    goto invalid_handle;

  if (!_gum_quick_args_parse (args, "i", &id))
    return JS_EXCEPTION;

  callback = gum_quick_core_try_steal_scheduled_callback (self, id);
  if (callback != NULL)
  {
    _gum_quick_core_pin (self);
    g_source_destroy (callback->source);
  }

  return JS_NewBool (ctx, callback != NULL);

invalid_handle:
  {
    return JS_NewBool (ctx, FALSE);
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_gc)
{
  JS_RunGC (core->rt);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_send)
{
  GumQuickCore * self = core;
  GumInterceptor * interceptor = (self->interceptor != NULL)
      ? self->interceptor->interceptor
      : NULL;
  const char * message;
  GBytes * data;

  if (!_gum_quick_args_parse (args, "sB?", &message, &data))
    return JS_EXCEPTION;

  /*
   * Synchronize Interceptor state before sending the message. The application
   * might be waiting for an acknowledgement that APIs have been instrumented.
   *
   * This is very important for the RPC API.
   */
  if (interceptor != NULL)
  {
    gum_interceptor_end_transaction (interceptor);
    gum_interceptor_begin_transaction (interceptor);
  }

  self->message_emitter (message, data, self->message_emitter_data);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_set_unhandled_exception_callback)
{
  GumQuickCore * self = core;
  JSValue callback;
  GumQuickExceptionSink * new_sink, * old_sink;

  if (!_gum_quick_args_parse (args, "F?", &callback))
    return JS_EXCEPTION;

  new_sink = !JS_IsNull (callback)
      ? gum_quick_exception_sink_new (callback, self)
      : NULL;

  old_sink = self->unhandled_exception_sink;
  self->unhandled_exception_sink = new_sink;

  if (old_sink != NULL)
    gum_quick_exception_sink_free (old_sink);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_set_incoming_message_callback)
{
  GumQuickCore * self = core;
  JSValue callback;
  GumQuickMessageSink * new_sink, * old_sink;

  if (!_gum_quick_args_parse (args, "F?", &callback))
    return JS_EXCEPTION;

  new_sink = !JS_IsNull (callback)
      ? gum_quick_message_sink_new (callback, self)
      : NULL;

  old_sink = self->incoming_message_sink;
  self->incoming_message_sink = new_sink;

  if (old_sink != NULL)
    gum_quick_message_sink_free (old_sink);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_wait_for_event)
{
  GumQuickCore * self = core;
  guint start_count;
  GumQuickScope scope = GUM_QUICK_SCOPE_INIT (self);
  GMainContext * context;
  gboolean called_from_js_thread;
  gboolean event_source_available;

  g_mutex_lock (&self->event_mutex);
  start_count = self->event_count;
  g_mutex_unlock (&self->event_mutex);

  _gum_quick_scope_perform_pending_io (self->current_scope);

  _gum_quick_scope_suspend (&scope);

  context = gum_script_scheduler_get_js_context (self->scheduler);
  called_from_js_thread = g_main_context_is_owner (context);

  g_mutex_lock (&self->event_mutex);

  while (self->event_count == start_count && self->event_source_available)
  {
    if (called_from_js_thread)
    {
      g_mutex_unlock (&self->event_mutex);
      g_main_loop_run (self->event_loop);
      g_mutex_lock (&self->event_mutex);
    }
    else
    {
      g_cond_wait (&self->event_cond, &self->event_mutex);
    }
  }

  event_source_available = self->event_source_available;

  g_mutex_unlock (&self->event_mutex);

  _gum_quick_scope_resume (&scope);

  if (!event_source_available)
    return _gum_quick_throw_literal (ctx, "script is unloading");

  return JS_UNDEFINED;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_int64_construct)
{
  JSValue wrapper;
  gint64 value;
  JSValue proto;
  GumQuickInt64 * i64;

  if (!_gum_quick_args_parse (args, "q~", &value))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->int64_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

  i64 = g_slice_new (GumQuickInt64);
  i64->value = value;

  JS_SetOpaque (wrapper, i64);

  return wrapper;
}

GUMJS_DEFINE_FINALIZER (gumjs_int64_finalize)
{
  GumQuickInt64 * i;

  i = JS_GetOpaque (val, core->int64_class);
  if (i == NULL)
    return;

  g_slice_free (GumQuickInt64, i);
}

#define GUM_DEFINE_INT64_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_int64_##name) \
    { \
      GumQuickInt64 * self; \
      gint64 lhs, rhs, result; \
      \
      if (!_gum_quick_int64_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      lhs = self->value; \
      \
      if (!_gum_quick_args_parse (args, "q~", &rhs)) \
        return JS_EXCEPTION; \
      \
      result = lhs op rhs; \
      \
      return _gum_quick_int64_new (ctx, result, core); \
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
      GumQuickInt64 * self; \
      gint64 result; \
      \
      if (!_gum_quick_int64_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      result = op self->value; \
      \
      return _gum_quick_int64_new (ctx, result, core); \
    }

GUM_DEFINE_INT64_UNARY_OP_IMPL (not, ~)

GUMJS_DEFINE_FUNCTION (gumjs_int64_compare)
{
  GumQuickInt64 * self;
  gint64 lhs, rhs;
  gint result;

  if (!_gum_quick_int64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  lhs = self->value;

  if (!_gum_quick_args_parse (args, "q~", &rhs))
    return JS_EXCEPTION;

  result = (lhs == rhs) ? 0 : ((lhs < rhs) ? -1 : 1);

  return JS_NewInt32 (ctx, result);
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_to_number)
{
  GumQuickInt64 * self;

  if (!_gum_quick_int64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_NewInt64 (ctx, self->value);
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_to_string)
{
  GumQuickInt64 * self;
  gint64 value;
  gint radix;
  gchar str[32];

  if (!_gum_quick_int64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  value = self->value;

  radix = 10;
  if (!_gum_quick_args_parse (args, "|u", &radix))
    return JS_EXCEPTION;
  if (radix != 10 && radix != 16)
    return _gum_quick_throw_literal (ctx, "unsupported radix");

  if (radix == 10)
    g_sprintf (str, "%" G_GINT64_FORMAT, value);
  else if (value >= 0)
    g_sprintf (str, "%" G_GINT64_MODIFIER "x", value);
  else
    g_sprintf (str, "-%" G_GINT64_MODIFIER "x", -value);

  return JS_NewString (ctx, str);
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_to_json)
{
  GumQuickInt64 * self;
  gchar str[32];

  if (!_gum_quick_int64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  g_sprintf (str, "%" G_GINT64_FORMAT, self->value);

  return JS_NewString (ctx, str);
}

GUMJS_DEFINE_FUNCTION (gumjs_int64_value_of)
{
  GumQuickInt64 * self;

  if (!_gum_quick_int64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_NewInt64 (ctx, self->value);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_uint64_construct)
{
  JSValue wrapper;
  guint64 value;
  JSValue proto;
  GumQuickUInt64 * u64;

  if (!_gum_quick_args_parse (args, "Q~", &value))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->uint64_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

  u64 = g_slice_new (GumQuickUInt64);
  u64->value = value;

  JS_SetOpaque (wrapper, u64);

  return wrapper;
}

GUMJS_DEFINE_FINALIZER (gumjs_uint64_finalize)
{
  GumQuickUInt64 * u;

  u = JS_GetOpaque (val, core->uint64_class);
  if (u == NULL)
    return;

  g_slice_free (GumQuickUInt64, u);
}

#define GUM_DEFINE_UINT64_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_uint64_##name) \
    { \
      GumQuickUInt64 * self; \
      guint64 lhs, rhs, result; \
      \
      if (!_gum_quick_uint64_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      lhs = self->value; \
      \
      if (!_gum_quick_args_parse (args, "Q~", &rhs)) \
        return JS_EXCEPTION; \
      \
      result = lhs op rhs; \
      \
      return _gum_quick_uint64_new (ctx, result, core); \
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
      GumQuickUInt64 * self; \
      guint64 result; \
      \
      if (!_gum_quick_uint64_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      result = op self->value; \
      \
      return _gum_quick_uint64_new (ctx, result, core); \
    }

GUM_DEFINE_UINT64_UNARY_OP_IMPL (not, ~)

GUMJS_DEFINE_FUNCTION (gumjs_uint64_compare)
{
  GumQuickUInt64 * self;
  guint64 lhs, rhs;
  gint result;

  if (!_gum_quick_uint64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  lhs = self->value;

  if (!_gum_quick_args_parse (args, "Q~", &rhs))
    return JS_EXCEPTION;

  result = (lhs == rhs) ? 0 : ((lhs < rhs) ? -1 : 1);

  return JS_NewInt32 (ctx, result);
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_to_number)
{
  GumQuickUInt64 * self;

  if (!_gum_quick_uint64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_NewFloat64 (ctx, (double) self->value);
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_to_string)
{
  GumQuickUInt64 * self;
  guint64 value;
  gint radix;
  gchar str[32];

  if (!_gum_quick_uint64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  value = self->value;

  radix = 10;
  if (!_gum_quick_args_parse (args, "|u", &radix))
    return JS_EXCEPTION;
  if (radix != 10 && radix != 16)
    return _gum_quick_throw_literal (ctx, "unsupported radix");

  if (radix == 10)
    g_sprintf (str, "%" G_GUINT64_FORMAT, value);
  else
    g_sprintf (str, "%" G_GINT64_MODIFIER "x", value);

  return JS_NewString (ctx, str);
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_to_json)
{
  GumQuickUInt64 * self;
  gchar str[32];

  if (!_gum_quick_uint64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  g_sprintf (str, "%" G_GUINT64_FORMAT, self->value);

  return JS_NewString (ctx, str);
}

GUMJS_DEFINE_FUNCTION (gumjs_uint64_value_of)
{
  GumQuickUInt64 * self;

  if (!_gum_quick_uint64_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_NewInt64 (ctx, self->value);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_pointer_construct)
{
  JSValue wrapper;
  gpointer ptr;
  JSValue proto;
  GumQuickNativePointer * np;

  if (!_gum_quick_args_parse (args, "p~", &ptr))
    return JS_EXCEPTION;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->native_pointer_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    return JS_EXCEPTION;

  np = g_slice_new0 (GumQuickNativePointer);
  np->value = ptr;

  JS_SetOpaque (wrapper, np);

  return wrapper;
}

GUMJS_DEFINE_FINALIZER (gumjs_native_pointer_finalize)
{
  GumQuickNativePointer * p;

  p = JS_GetOpaque (val, core->native_pointer_class);
  if (p == NULL)
    return;

  g_slice_free (GumQuickNativePointer, p);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_is_null)
{
  GumQuickNativePointer * self;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_NewBool (ctx, self->value == NULL);
}

#define GUM_DEFINE_NATIVE_POINTER_BINARY_OP_IMPL(name, op) \
    GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_##name) \
    { \
      GumQuickNativePointer * self; \
      gpointer lhs_ptr, rhs_ptr; \
      gsize lhs_bits, rhs_bits; \
      gpointer result; \
      \
      if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      lhs_ptr = self->value; \
      \
      if (!_gum_quick_args_parse (args, "p~", &rhs_ptr)) \
        return JS_EXCEPTION; \
      \
      lhs_bits = GPOINTER_TO_SIZE (lhs_ptr); \
      rhs_bits = GPOINTER_TO_SIZE (rhs_ptr); \
      \
      result = GSIZE_TO_POINTER (lhs_bits op rhs_bits); \
      \
      return _gum_quick_native_pointer_new (ctx, result, core); \
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
      GumQuickNativePointer * self; \
      gpointer result; \
      \
      if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self)) \
        return JS_EXCEPTION; \
      \
      result = GSIZE_TO_POINTER (op GPOINTER_TO_SIZE (self->value)); \
      \
      return _gum_quick_native_pointer_new (ctx, result, core); \
    }

GUM_DEFINE_NATIVE_POINTER_UNARY_OP_IMPL (not, ~)

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_sign)
{
#ifdef HAVE_PTRAUTH
  GumQuickNativePointer * self;
  gpointer value;
  const gchar * key;
  gpointer data;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  value = self->value;

  key = "ia";
  data = NULL;
  if (!_gum_quick_args_parse (args, "|sp~", &key, &data))
    return JS_EXCEPTION;

  if (strcmp (key, "ia") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asia, data);
  else if (strcmp (key, "ib") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asib, data);
  else if (strcmp (key, "da") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asda, data);
  else if (strcmp (key, "db") == 0)
    value = ptrauth_sign_unauthenticated (value, ptrauth_key_asdb, data);
  else
    return _gum_quick_throw_literal (ctx, "invalid key");

  return _gum_quick_native_pointer_new (ctx, value, core);
#else
  return JS_DupValue (ctx, this_val);
#endif
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_strip)
{
#ifdef HAVE_PTRAUTH
  GumQuickNativePointer * self;
  gpointer value;
  const gchar * key;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  value = self->value;

  key = "ia";
  if (!_gum_quick_args_parse (args, "|s", &key))
    return JS_EXCEPTION;

  if (strcmp (key, "ia") == 0)
    value = ptrauth_strip (value, ptrauth_key_asia);
  else if (strcmp (key, "ib") == 0)
    value = ptrauth_strip (value, ptrauth_key_asib);
  else if (strcmp (key, "da") == 0)
    value = ptrauth_strip (value, ptrauth_key_asda);
  else if (strcmp (key, "db") == 0)
    value = ptrauth_strip (value, ptrauth_key_asdb);
  else
    return _gum_quick_throw_literal (ctx, "invalid key");

  return _gum_quick_native_pointer_new (ctx, value, core);
#elif defined (HAVE_ANDROID) && defined (HAVE_ARM64)
  GumQuickNativePointer * self;
  gpointer value_without_top_byte;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  /* https://source.android.com/devices/tech/debug/tagged-pointers */
  value_without_top_byte = GSIZE_TO_POINTER (
      GPOINTER_TO_SIZE (self->value) & G_GUINT64_CONSTANT (0x00ffffffffffffff));

  if (value_without_top_byte == self->value)
    return JS_DupValue (ctx, this_val);

  return _gum_quick_native_pointer_new (ctx, value_without_top_byte, core);
#else
  return JS_DupValue (ctx, this_val);
#endif
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_blend)
{
#ifdef HAVE_PTRAUTH
  GumQuickNativePointer * self;
  gpointer value;
  guint small_integer;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  value = self->value;

  if (!_gum_quick_args_parse (args, "u", &small_integer))
    return JS_EXCEPTION;

  value = GSIZE_TO_POINTER (ptrauth_blend_discriminator (value, small_integer));

  return _gum_quick_native_pointer_new (ctx, value, core);
#else
  return JS_DupValue (ctx, this_val);
#endif
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_compare)
{
  GumQuickNativePointer * self;
  gpointer lhs_ptr, rhs_ptr;
  gsize lhs_bits, rhs_bits;
  gint result;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  lhs_ptr = self->value;

  if (!_gum_quick_args_parse (args, "p~", &rhs_ptr))
    return JS_EXCEPTION;

  lhs_bits = GPOINTER_TO_SIZE (lhs_ptr);
  rhs_bits = GPOINTER_TO_SIZE (rhs_ptr);

  result = (lhs_bits == rhs_bits) ? 0 : ((lhs_bits < rhs_bits) ? -1 : 1);

  return JS_NewInt32 (ctx, result);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_int32)
{
  GumQuickNativePointer * self;
  gint32 result;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  result = (gint32) GPOINTER_TO_SIZE (self->value);

  return JS_NewInt32 (ctx, result);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_uint32)
{
  GumQuickNativePointer * self;
  guint32 result;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  result = (guint32) GPOINTER_TO_SIZE (self->value);

  return JS_NewUint32 (ctx, result);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_string)
{
  GumQuickNativePointer * self;
  gint radix = 0;
  gboolean radix_specified;
  gsize ptr_bits;
  gchar str[32];

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "|u", &radix))
    return JS_EXCEPTION;

  radix_specified = radix != 0;
  if (!radix_specified)
    radix = 16;
  else if (radix != 10 && radix != 16)
    return _gum_quick_throw_literal (ctx, "unsupported radix");

  ptr_bits = GPOINTER_TO_SIZE (self->value);

  if (radix == 10)
  {
    g_sprintf (str, "%" G_GSIZE_MODIFIER "u", ptr_bits);
  }
  else
  {
    if (radix_specified)
      g_sprintf (str, "%" G_GSIZE_MODIFIER "x", ptr_bits);
    else
      g_sprintf (str, "0x%" G_GSIZE_MODIFIER "x", ptr_bits);
  }

  return JS_NewString (ctx, str);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_json)
{
  GumQuickNativePointer * self;
  gchar str[32];

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  g_sprintf (str, "0x%" G_GSIZE_MODIFIER "x", GPOINTER_TO_SIZE (self->value));

  return JS_NewString (ctx, str);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_to_match_pattern)
{
  GumQuickNativePointer * self;
  gsize ptr_bits;
  gchar str[24];
  gint src, dst;
  const gint num_bits = GLIB_SIZEOF_VOID_P * 8;
  const gchar nibble_to_char[] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      'a', 'b', 'c', 'd', 'e', 'f'
  };

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  ptr_bits = GPOINTER_TO_SIZE (self->value);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  for (src = 0, dst = 0; src != num_bits; src += 8)
#else
  for (src = num_bits - 8, dst = 0; src >= 0; src -= 8)
#endif
  {
    if (dst != 0)
      str[dst++] = ' ';
    str[dst++] = nibble_to_char[(ptr_bits >> (src + 4)) & 0xf];
    str[dst++] = nibble_to_char[(ptr_bits >> (src + 0)) & 0xf];
  }
  str[dst] = '\0';

  return JS_NewString (ctx, str);
}

static JSValue
gumjs_native_pointer_handle_read (JSContext * ctx,
                                  JSValueConst this_val,
                                  GumMemoryValueType type,
                                  GumQuickArgs * args,
                                  GumQuickCore * core)
{
  JSValue result = JS_NULL;
  GumQuickNativePointer * self;
  gpointer address;
  GumExceptor * exceptor = core->exceptor;
  gssize length = -1;
  GumExceptorScope scope;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  address = self->value;

  switch (type)
  {
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
      if (!_gum_quick_args_parse (args, "Z", &length))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_C_STRING:
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
    case GUM_MEMORY_VALUE_ANSI_STRING:
      if (!_gum_quick_args_parse (args, "|z", &length))
        return JS_EXCEPTION;
      break;
    default:
      break;
  }

  if (gum_exceptor_try (exceptor, &scope))
  {
    switch (type)
    {
      case GUM_MEMORY_VALUE_POINTER:
        result =
            _gum_quick_native_pointer_new (ctx, *((gpointer *) address), core);
        break;
      case GUM_MEMORY_VALUE_S8:
        result = JS_NewInt32 (ctx, *((gint8 *) address));
        break;
      case GUM_MEMORY_VALUE_U8:
        result = JS_NewUint32 (ctx, *((guint8 *) address));
        break;
      case GUM_MEMORY_VALUE_S16:
        result = JS_NewInt32 (ctx, *((gint16 *) address));
        break;
      case GUM_MEMORY_VALUE_U16:
        result = JS_NewUint32 (ctx, *((guint16 *) address));
        break;
      case GUM_MEMORY_VALUE_S32:
        result = JS_NewInt32 (ctx, *((gint32 *) address));
        break;
      case GUM_MEMORY_VALUE_U32:
        result = JS_NewUint32 (ctx, *((guint32 *) address));
        break;
      case GUM_MEMORY_VALUE_S64:
        result = _gum_quick_int64_new (ctx, *((gint64 *) address), core);
        break;
      case GUM_MEMORY_VALUE_U64:
        result = _gum_quick_uint64_new (ctx, *((guint64 *) address), core);
        break;
      case GUM_MEMORY_VALUE_LONG:
        result = _gum_quick_int64_new (ctx, *((glong *) address), core);
        break;
      case GUM_MEMORY_VALUE_ULONG:
        result = _gum_quick_uint64_new (ctx, *((gulong *) address), core);
        break;
      case GUM_MEMORY_VALUE_FLOAT:
        result = JS_NewFloat64 (ctx, *((gfloat *) address));
        break;
      case GUM_MEMORY_VALUE_DOUBLE:
        result = JS_NewFloat64 (ctx, *((gdouble *) address));
        break;
      case GUM_MEMORY_VALUE_BYTE_ARRAY:
      {
        const guint8 * data = address;
        gpointer buffer_data;

        if (data == NULL)
        {
          result = JS_NULL;
          break;
        }

        buffer_data = g_malloc (length);
        result = JS_NewArrayBuffer (ctx, buffer_data, length,
            _gum_quick_array_buffer_free, buffer_data, FALSE);

        memcpy (buffer_data, data, length);

        break;
      }
      case GUM_MEMORY_VALUE_C_STRING:
      {
        const gchar * data = address;
        guint8 dummy_to_trap_bad_pointer_early;
        gchar * str;

        if (data == NULL)
        {
          result = JS_NULL;
          break;
        }

        if (length != 0)
          memcpy (&dummy_to_trap_bad_pointer_early, data, sizeof (guint8));

        str = g_utf8_make_valid (data, length);
        result = JS_NewString (ctx, str);
        g_free (str);

        break;
      }
      case GUM_MEMORY_VALUE_UTF8_STRING:
      {
        const gchar * data = address;
        guint8 dummy_to_trap_bad_pointer_early;
        const gchar * end;

        if (data == NULL)
        {
          result = JS_NULL;
          break;
        }

        if (length != 0)
          memcpy (&dummy_to_trap_bad_pointer_early, data, sizeof (guint8));

        if (g_utf8_validate (data, length, &end))
        {
          result = JS_NewStringLen (ctx, data, end - data);
        }
        else
        {
          result = _gum_quick_throw (ctx,
              "can't decode byte 0x%02x in position %u",
              (guint8) *end, (guint) (end - data));
        }

        break;
      }
      case GUM_MEMORY_VALUE_UTF16_STRING:
      {
        const gunichar2 * str_utf16 = address;
        gchar * str_utf8;
        guint8 dummy_to_trap_bad_pointer_early;
        glong size;

        if (str_utf16 == NULL)
        {
          result = JS_NULL;
          break;
        }

        if (length != 0)
          memcpy (&dummy_to_trap_bad_pointer_early, str_utf16, sizeof (guint8));

        str_utf8 = g_utf16_to_utf8 (str_utf16, length, NULL, &size, NULL);

        if (str_utf8 != NULL)
          result = JS_NewString (ctx, str_utf8);
        else
          result = _gum_quick_throw_literal (ctx, "invalid string");

        g_free (str_utf8);

        break;
      }
      case GUM_MEMORY_VALUE_ANSI_STRING:
      {
#ifdef HAVE_WINDOWS
        const gchar * str_ansi = address;

        if (str_ansi == NULL)
        {
          result = JS_NULL;
          break;
        }

        if (length != 0)
        {
          guint8 dummy_to_trap_bad_pointer_early;
          gchar * str_utf8;

          memcpy (&dummy_to_trap_bad_pointer_early, str_ansi, sizeof (guint8));

          str_utf8 = _gum_ansi_string_to_utf8 (str_ansi, length);
          result = JS_NewString (ctx, str_utf8);
          g_free (str_utf8);
        }
        else
        {
          result = JS_NewString (ctx, "");
        }
#else
        result = _gum_quick_throw_literal (ctx,
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
    JS_FreeValue (ctx, result);
    result = _gum_quick_throw_native (ctx, &scope.exception, core);
  }

  return result;
}

static JSValue
gumjs_native_pointer_handle_write (JSContext * ctx,
                                   JSValueConst this_val,
                                   GumMemoryValueType type,
                                   GumQuickArgs * args,
                                   GumQuickCore * core)
{
  JSValue result = JS_UNDEFINED;
  GumQuickNativePointer * self;
  gpointer address = NULL;
  GumExceptor * exceptor = core->exceptor;
  gpointer pointer = NULL;
  gssize s = 0;
  gsize u = 0;
  gint64 s64 = 0;
  guint64 u64 = 0;
  gdouble number = 0;
  GBytes * bytes = NULL;
  const gchar * str = NULL;
  gsize str_length = 0;
  gunichar2 * str_utf16 = NULL;
#ifdef HAVE_WINDOWS
  gchar * str_ansi = NULL;
#endif
  GumExceptorScope scope;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;
  address = self->value;

  switch (type)
  {
    case GUM_MEMORY_VALUE_POINTER:
      if (!_gum_quick_args_parse (args, "p", &pointer))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_S8:
    case GUM_MEMORY_VALUE_S16:
    case GUM_MEMORY_VALUE_S32:
      if (!_gum_quick_args_parse (args, "z", &s))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_U8:
    case GUM_MEMORY_VALUE_U16:
    case GUM_MEMORY_VALUE_U32:
      if (!_gum_quick_args_parse (args, "Z", &u))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_S64:
    case GUM_MEMORY_VALUE_LONG:
      if (!_gum_quick_args_parse (args, "q", &s64))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_U64:
    case GUM_MEMORY_VALUE_ULONG:
      if (!_gum_quick_args_parse (args, "Q", &u64))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_FLOAT:
    case GUM_MEMORY_VALUE_DOUBLE:
      if (!_gum_quick_args_parse (args, "n", &number))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_BYTE_ARRAY:
      if (!_gum_quick_args_parse (args, "B", &bytes))
        return JS_EXCEPTION;
      break;
    case GUM_MEMORY_VALUE_UTF8_STRING:
    case GUM_MEMORY_VALUE_UTF16_STRING:
    case GUM_MEMORY_VALUE_ANSI_STRING:
      if (!_gum_quick_args_parse (args, "s", &str))
        return JS_EXCEPTION;

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
        gconstpointer data;
        gsize size;

        data = g_bytes_get_data (bytes, &size);

        memcpy (address, data, size);
        break;
      }
      case GUM_MEMORY_VALUE_UTF8_STRING:
      {
        gsize size;

        size = g_utf8_offset_to_pointer (str, str_length) - str + 1;
        memcpy (address, str, size);
        break;
      }
      case GUM_MEMORY_VALUE_UTF16_STRING:
      {
        gsize size;

        size = (str_length + 1) * sizeof (gunichar2);
        memcpy (address, str_utf16, size);
        break;
      }
      case GUM_MEMORY_VALUE_ANSI_STRING:
      {
#ifdef HAVE_WINDOWS
        strcpy (address, str_ansi);
#else
        result = _gum_quick_throw_literal (ctx,
            "ANSI API is only applicable on Windows");
#endif

        break;
      }
      default:
        g_assert_not_reached ();
    }
  }

  if (gum_exceptor_catch (exceptor, &scope))
    result = _gum_quick_throw_native (ctx, &scope.exception, core);
  else if (JS_IsUndefined (result))
    result = JS_DupValue (ctx, this_val);

  g_free (str_utf16);
#ifdef HAVE_WINDOWS
  g_free (str_ansi);
#endif

  return result;
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_read_volatile)
{
  GumQuickNativePointer * self;
  gsize length;
  gsize n_bytes_read;
  guint8 * data;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "z", &length))
    return JS_EXCEPTION;

  data = gum_memory_read (self->value, length, &n_bytes_read);
  if (data == NULL)
    return _gum_quick_throw_literal (ctx, "memory read failed");

  return JS_NewArrayBuffer (ctx, data, n_bytes_read,
      _gum_quick_array_buffer_free, data, FALSE);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_pointer_write_volatile)
{
  JSValue result;
  GumQuickNativePointer * self;
  GBytes * bytes = NULL;
  gconstpointer data;
  gsize size;

  if (!_gum_quick_native_pointer_unwrap (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "B", &bytes))
    goto propagate_exception;

  data = g_bytes_get_data (bytes, &size);

  if (!gum_memory_write (self->value, data, size))
    goto write_failed;

  result = JS_UNDEFINED;
  goto beach;

write_failed:
  {
    _gum_quick_throw_literal (ctx, "memory write failed");
    goto propagate_exception;
  }
propagate_exception:
  {
    result = JS_EXCEPTION;
    goto beach;
  }
beach:
  {
    g_bytes_unref (bytes);

    return result;
  }
}

GUMJS_DEFINE_FUNCTION (gumjs_array_buffer_wrap)
{
  gpointer address;
  gsize size;

  if (!_gum_quick_args_parse (args, "pZ", &address, &size))
    return JS_EXCEPTION;

  return JS_NewArrayBuffer (ctx, address, size, NULL, NULL, FALSE);
}

GUMJS_DEFINE_FUNCTION (gumjs_array_buffer_unwrap)
{
  uint8_t * address;
  size_t size;

  address = JS_GetArrayBuffer (ctx, &size, this_val);
  if (address == NULL)
    return JS_EXCEPTION;

  return _gum_quick_native_pointer_new (ctx, address, core);
}

GUMJS_DEFINE_FINALIZER (gumjs_native_resource_finalize)
{
  GumQuickNativeResource * r;

  r = JS_GetOpaque (val, core->native_resource_class);
  if (r == NULL)
    return;

  if (r->owns_pages)
    gum_memory_free (r->native_pointer.value, r->size);
  else if (r->notify != NULL)
    r->notify (r->native_pointer.value);

  g_slice_free (GumQuickNativeResource, r);
}

GUMJS_DEFINE_FINALIZER (gumjs_kernel_resource_finalize)
{
  GumQuickKernelResource * r;

  r = JS_GetOpaque (val, core->kernel_resource_class);
  if (r == NULL)
    return;

  if (r->notify != NULL)
    r->notify (r->u64.value);

  g_slice_free (GumQuickKernelResource, r);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_function_construct)
{
  JSValue wrapper = JS_NULL;
  GumQuickFFIFunctionParams p = GUM_QUICK_FFI_FUNCTION_PARAMS_EMPTY;
  JSValue proto;
  GumQuickFFIFunction * func;

  if (!gum_quick_ffi_function_params_init (&p, GUM_QUICK_RETURN_PLAIN, args))
    goto propagate_exception;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->native_function_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  func = gumjs_ffi_function_new (ctx, &p, core);
  if (func == NULL)
    goto propagate_exception;

  JS_SetOpaque (wrapper, func);

  gum_quick_ffi_function_params_destroy (&p);

  return wrapper;

propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);
    gum_quick_ffi_function_params_destroy (&p);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FINALIZER (gumjs_native_function_finalize)
{
  GumQuickFFIFunction * f;

  f = JS_GetOpaque (val, core->native_function_class);
  if (f == NULL)
    return;

  gum_quick_ffi_function_finalize (f);
}

GUMJS_DEFINE_CALL_HANDLER (gumjs_native_function_invoke)
{
  return gumjs_ffi_function_invoke (ctx, func_obj, core->native_function_class,
      args, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_function_call)
{
  return gumjs_ffi_function_call (ctx, this_val, core->native_function_class,
      args, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_native_function_apply)
{
  return gumjs_ffi_function_apply (ctx, this_val, core->native_function_class,
      args, core);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_system_function_construct)
{
  JSValue wrapper = JS_NULL;
  GumQuickFFIFunctionParams p = GUM_QUICK_FFI_FUNCTION_PARAMS_EMPTY;
  JSValue proto;
  GumQuickFFIFunction * func;

  if (!gum_quick_ffi_function_params_init (&p, GUM_QUICK_RETURN_DETAILED, args))
    goto propagate_exception;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->system_function_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  func = gumjs_ffi_function_new (ctx, &p, core);
  if (func == NULL)
    goto propagate_exception;

  JS_SetOpaque (wrapper, func);

  gum_quick_ffi_function_params_destroy (&p);

  return wrapper;

propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);
    gum_quick_ffi_function_params_destroy (&p);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FINALIZER (gumjs_system_function_finalize)
{
  GumQuickFFIFunction * f;

  f = JS_GetOpaque (val, core->system_function_class);
  if (f == NULL)
    return;

  gum_quick_ffi_function_finalize (f);
}

GUMJS_DEFINE_CALL_HANDLER (gumjs_system_function_invoke)
{
  return gumjs_ffi_function_invoke (ctx, func_obj, core->system_function_class,
      args, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_system_function_call)
{
  return gumjs_ffi_function_call (ctx, this_val, core->system_function_class,
      args, core);
}

GUMJS_DEFINE_FUNCTION (gumjs_system_function_apply)
{
  return gumjs_ffi_function_apply (ctx, this_val, core->system_function_class,
      args, core);
}

static GumQuickFFIFunction *
gumjs_ffi_function_new (JSContext * ctx,
                        const GumQuickFFIFunctionParams * params,
                        GumQuickCore * core)
{
  GumQuickFFIFunction * func;
  GumQuickNativePointer * ptr;
  ffi_type * rtype;
  JSValue val = JS_UNDEFINED;
  guint nargs_fixed, nargs_total, length, i;
  gboolean is_variadic;
  ffi_abi abi;

  func = g_slice_new0 (GumQuickFFIFunction);
  ptr = &func->native_pointer;
  ptr->value = GUM_FUNCPTR_TO_POINTER (params->implementation);
  func->implementation = params->implementation;
  func->scheduling = params->scheduling;
  func->exceptions = params->exceptions;
  func->traps = params->traps;
  func->return_shape = params->return_shape;

  if (!gum_quick_ffi_type_get (ctx, params->return_type, core, &rtype,
      &func->data))
    goto invalid_return_type;

  if (!_gum_quick_array_get_length (ctx, params->argument_types, core, &length))
    goto invalid_argument_array;

  nargs_fixed = nargs_total = length;
  is_variadic = FALSE;

  func->atypes = g_new (ffi_type *, nargs_total);

  for (i = 0; i != nargs_total; i++)
  {
    gboolean is_marker;

    val = JS_GetPropertyUint32 (ctx, params->argument_types, i);
    if (JS_IsException (val))
      goto invalid_argument_array;

    if (JS_IsString (val))
    {
      const char * str = JS_ToCString (ctx, val);
      is_marker = strcmp (str, "...") == 0;
      JS_FreeCString (ctx, str);
    }
    else
    {
      is_marker = FALSE;
    }

    if (is_marker)
    {
      if (i == 0 || is_variadic)
        goto unexpected_marker;

      nargs_fixed = i;
      is_variadic = TRUE;
    }
    else
    {
      ffi_type ** atype;

      atype = &func->atypes[is_variadic ? i - 1 : i];

      if (!gum_quick_ffi_type_get (ctx, val, core, atype, &func->data))
        goto invalid_argument_type;

      if (is_variadic)
        *atype = gum_ffi_maybe_promote_variadic (*atype);
    }

    JS_FreeValue (ctx, val);
    val = JS_UNDEFINED;
  }

  if (is_variadic)
    nargs_total--;

  if (params->abi_name != NULL)
  {
    if (!gum_quick_ffi_abi_get (ctx, params->abi_name, &abi))
      goto invalid_abi;
  }
  else
  {
    abi = GUM_DEFAULT_FFI_ABI;
  }

  if (is_variadic)
  {
    if (ffi_prep_cif_var (&func->cif, abi, (guint) nargs_fixed,
        (guint) nargs_total, rtype, func->atypes) != FFI_OK)
      goto compilation_failed;
  }
  else
  {
    if (ffi_prep_cif (&func->cif, abi, (guint) nargs_total, rtype,
        func->atypes) != FFI_OK)
      goto compilation_failed;
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

  return func;

invalid_return_type:
invalid_argument_array:
invalid_argument_type:
invalid_abi:
  {
    JS_FreeValue (ctx, val);
    gum_quick_ffi_function_finalize (func);

    return NULL;
  }
unexpected_marker:
  {
    JS_FreeValue (ctx, val);
    gum_quick_ffi_function_finalize (func);

    _gum_quick_throw_literal (ctx, "only one variadic marker may be specified, "
        "and can not be the first argument");
    return NULL;
  }
compilation_failed:
  {
    gum_quick_ffi_function_finalize (func);

    _gum_quick_throw_literal (ctx, "failed to compile function call interface");
    return NULL;
  }
}

static void
gum_quick_ffi_function_finalize (GumQuickFFIFunction * func)
{
  while (func->data != NULL)
  {
    GSList * head = func->data;
    g_free (head->data);
    func->data = g_slist_delete_link (func->data, head);
  }
  g_free (func->atypes);

  g_slice_free (GumQuickFFIFunction, func);
}

static JSValue
gum_quick_ffi_function_invoke (GumQuickFFIFunction * self,
                               JSContext * ctx,
                               GCallback implementation,
                               guint argc,
                               JSValueConst * argv,
                               GumQuickCore * core)
{
  JSValue result;
  ffi_cif * cif;
  guint nargs, nargs_fixed;
  gboolean is_variadic;
  ffi_type * rtype;
  ffi_type ** atypes;
  gsize rsize, ralign;
  GumFFIRet * rvalue;
  void ** avalue;
  guint8 * avalues;
  ffi_cif tmp_cif;
  GumFFIArg tmp_value = { 0, };
  GumQuickSchedulingBehavior scheduling;
  GumQuickExceptionsBehavior exceptions;
  GumQuickCodeTraps traps;
  GumQuickReturnValueShape return_shape;
  GumExceptorScope exceptor_scope;
  GumInvocationState invocation_state;
  gint system_error;

  cif = &self->cif;
  nargs = cif->nargs;
  nargs_fixed = self->nargs_fixed;
  is_variadic = self->is_variadic;

  if ((is_variadic && argc < nargs_fixed) || (!is_variadic && argc != nargs))
    return _gum_quick_throw_literal (ctx, "bad argument count");

  rtype = cif->rtype;
  atypes = cif->arg_types;
  rsize = MAX (rtype->size, sizeof (gsize));
  ralign = MAX (rtype->alignment, sizeof (gsize));
  rvalue = g_alloca (rsize + ralign - 1);
  rvalue = GUM_ALIGN_POINTER (GumFFIRet *, rvalue, ralign);

  if (argc > 0)
  {
    gsize arglist_size, arglist_alignment, offset, i;

    avalue = g_newa (void *, MAX (nargs, argc));

    arglist_size = self->arglist_size;
    if (is_variadic && argc > nargs)
    {
      gsize type_idx;

      atypes = g_newa (ffi_type *, argc);

      memcpy (atypes, cif->arg_types, nargs * sizeof (void *));
      for (i = nargs, type_idx = nargs_fixed; i != argc; i++)
      {
        ffi_type * t = cif->arg_types[type_idx];

        atypes[i] = t;
        arglist_size = GUM_ALIGN_SIZE (arglist_size, t->alignment);
        arglist_size += t->size;

        if (++type_idx >= nargs)
          type_idx = nargs_fixed;
      }

      cif = &tmp_cif;
      if (ffi_prep_cif_var (cif, self->abi, (guint) nargs_fixed,
          (guint) argc, rtype, atypes) != FFI_OK)
      {
        return _gum_quick_throw_literal (ctx,
            "failed to compile function call interface");
      }
    }

    arglist_alignment = atypes[0]->alignment;
    avalues = g_alloca (arglist_size + arglist_alignment - 1);
    avalues = GUM_ALIGN_POINTER (guint8 *, avalues, arglist_alignment);

    /* Prefill with zero to clear high bits of values smaller than a pointer. */
    memset (avalues, 0, arglist_size);

    offset = 0;
    for (i = 0; i != argc; i++)
    {
      ffi_type * t;
      GumFFIArg * v;

      t = atypes[i];
      offset = GUM_ALIGN_SIZE (offset, t->alignment);
      v = (GumFFIArg *) (avalues + offset);

      if (!gum_quick_value_to_ffi (ctx, argv[i], t, core, v))
        return JS_EXCEPTION;
      avalue[i] = v;

      offset += t->size;
    }

    while (i < nargs)
      avalue[i++] = &tmp_value;
  }
  else
  {
    avalue = NULL;
  }

  scheduling = self->scheduling;
  exceptions = self->exceptions;
  traps = self->traps;
  return_shape = self->return_shape;
  system_error = -1;

  {
    GumQuickScope scope = GUM_QUICK_SCOPE_INIT (core);
    GumInterceptor * interceptor = (core->interceptor != NULL)
        ? core->interceptor->interceptor
        : NULL;
    gboolean interceptor_was_ignoring_us = FALSE;
    GumStalker * stalker = NULL;

    if (exceptions == GUM_QUICK_EXCEPTIONS_PROPAGATE ||
        gum_exceptor_try (core->exceptor, &exceptor_scope))
    {
      if (exceptions == GUM_QUICK_EXCEPTIONS_STEAL)
        gum_interceptor_save (&invocation_state);

      if (scheduling == GUM_QUICK_SCHEDULING_COOPERATIVE)
      {
        _gum_quick_scope_suspend (&scope);

        if (traps != GUM_QUICK_CODE_TRAPS_NONE && interceptor != NULL)
        {
          interceptor_was_ignoring_us =
              gum_interceptor_maybe_unignore_current_thread (interceptor);
        }
      }

      if (traps == GUM_QUICK_CODE_TRAPS_ALL)
      {
        _gum_quick_stalker_process_pending (core->stalker,
            scope.previous_scope);

        stalker = _gum_quick_stalker_get (core->stalker);
        gum_stalker_activate (stalker,
            GUM_FUNCPTR_TO_POINTER (implementation));
      }
      else if (traps == GUM_QUICK_CODE_TRAPS_NONE && interceptor != NULL)
      {
        gum_interceptor_ignore_current_thread (interceptor);
      }

      ffi_call (cif, implementation, rvalue, avalue);

      g_clear_pointer (&stalker, gum_stalker_deactivate);

      if (return_shape == GUM_QUICK_RETURN_DETAILED)
        system_error = gum_thread_get_system_error ();
    }

    g_clear_pointer (&stalker, gum_stalker_deactivate);

    if (traps == GUM_QUICK_CODE_TRAPS_NONE && interceptor != NULL)
      gum_interceptor_unignore_current_thread (interceptor);

    if (scheduling == GUM_QUICK_SCHEDULING_COOPERATIVE)
    {
      if (traps != GUM_QUICK_CODE_TRAPS_NONE && interceptor_was_ignoring_us)
        gum_interceptor_ignore_current_thread (interceptor);

      _gum_quick_scope_resume (&scope);
    }
  }

  if (exceptions == GUM_QUICK_EXCEPTIONS_STEAL &&
      gum_exceptor_catch (core->exceptor, &exceptor_scope))
  {
    gum_interceptor_restore (&invocation_state);

    return _gum_quick_throw_native (ctx, &exceptor_scope.exception, core);
  }

  result = gum_quick_value_from_ffi (ctx, rvalue, rtype, core);

  if (return_shape == GUM_QUICK_RETURN_DETAILED)
  {
    JSValue d = JS_NewObject (ctx);
    JS_DefinePropertyValue (ctx, d,
        GUM_QUICK_CORE_ATOM (core, value),
        result,
        JS_PROP_C_W_E);
    JS_DefinePropertyValue (ctx, d,
        GUM_QUICK_CORE_ATOM (core, system_error),
        JS_NewInt32 (ctx, system_error),
        JS_PROP_C_W_E);
    return d;
  }
  else
  {
    return result;
  }
}

static JSValue
gumjs_ffi_function_invoke (JSContext * ctx,
                           JSValueConst func_obj,
                           JSClassID klass,
                           GumQuickArgs * args,
                           GumQuickCore * core)
{
  GumQuickFFIFunction * self;

  if (!_gum_quick_unwrap (ctx, func_obj, klass, core, (gpointer *) &self))
    return JS_EXCEPTION;

  return gum_quick_ffi_function_invoke (self, ctx, self->implementation,
      args->count, args->elements, core);
}

static JSValue
gumjs_ffi_function_call (JSContext * ctx,
                         JSValueConst func_obj,
                         JSClassID klass,
                         GumQuickArgs * args,
                         GumQuickCore * core)
{
  const int argc = args->count;
  JSValueConst * argv = args->elements;
  JSValue receiver;
  GumQuickFFIFunction * func;
  GCallback impl;

  if (argc == 0 || JS_IsNull (argv[0]) || JS_IsUndefined (argv[0]))
  {
    receiver = JS_NULL;
  }
  else if (JS_IsObject (argv[0]))
  {
    receiver = argv[0];
  }
  else
  {
    return _gum_quick_throw_literal (ctx, "invalid receiver");
  }

  if (!gumjs_ffi_function_get (ctx, func_obj, receiver, klass, core, &func,
      &impl))
  {
    return JS_EXCEPTION;
  }

  return gum_quick_ffi_function_invoke (func, ctx, impl, MAX (argc - 1, 0),
      argv + 1, core);
}

static JSValue
gumjs_ffi_function_apply (JSContext * ctx,
                          JSValueConst func_obj,
                          JSClassID klass,
                          GumQuickArgs * args,
                          GumQuickCore * core)
{
  JSValueConst * argv = args->elements;
  JSValue receiver;
  GumQuickFFIFunction * func;
  GCallback impl;
  guint n, i;
  JSValue * values;

  if (JS_IsNull (argv[0]) || JS_IsUndefined (argv[0]))
  {
    receiver = JS_NULL;
  }
  else if (JS_IsObject (argv[0]))
  {
    receiver = argv[0];
  }
  else
  {
    return _gum_quick_throw_literal (ctx, "invalid receiver");
  }

  if (!gumjs_ffi_function_get (ctx, func_obj, receiver, klass, core, &func,
      &impl))
  {
    return JS_EXCEPTION;
  }

  if (JS_IsNull (argv[1]) || JS_IsUndefined (argv[1]))
  {
    return gum_quick_ffi_function_invoke (func, ctx, impl, 0, NULL, core);
  }
  else
  {
    JSValueConst elements = argv[1];
    JSValue result;

    if (!_gum_quick_array_get_length (ctx, elements, core, &n))
      return JS_EXCEPTION;

    values = g_newa (JSValue, n);

    for (i = 0; i != n; i++)
    {
      values[i] = JS_GetPropertyUint32 (ctx, elements, i);
      if (JS_IsException (values[i]))
        goto invalid_argument_value;
    }

    result = gum_quick_ffi_function_invoke (func, ctx, impl, n, values, core);

    for (i = 0; i != n; i++)
      JS_FreeValue (ctx, values[i]);

    return result;
  }

invalid_argument_value:
  {
    n = i;
    for (i = 0; i != n; i++)
      JS_FreeValue (ctx, values[i]);

    return JS_EXCEPTION;
  }
}

static gboolean
gumjs_ffi_function_get (JSContext * ctx,
                        JSValueConst func_obj,
                        JSValueConst receiver,
                        JSClassID klass,
                        GumQuickCore * core,
                        GumQuickFFIFunction ** func,
                        GCallback * implementation)
{
  GumQuickFFIFunction * f;

  if (_gum_quick_try_unwrap (func_obj, klass, core, (gpointer *) &f))
  {
    *func = f;

    if (!JS_IsNull (receiver))
    {
      gpointer impl;
      if (!_gum_quick_native_pointer_get (ctx, receiver, core, &impl))
        return FALSE;
      *implementation = GUM_POINTER_TO_FUNCPTR (GCallback, impl);
    }
    else
    {
      *implementation = f->implementation;
    }
  }
  else
  {
    if (!_gum_quick_unwrap (ctx, receiver, klass, core, (gpointer *) &f))
      return FALSE;

    *func = f;
    *implementation = f->implementation;
  }

  return TRUE;
}

static gboolean
gum_quick_ffi_function_params_init (GumQuickFFIFunctionParams * params,
                                    GumQuickReturnValueShape return_shape,
                                    GumQuickArgs * args)
{
  JSContext * ctx = args->ctx;
  JSValueConst abi_or_options;
  JSValue val;

  params->ctx = ctx;

  abi_or_options = JS_UNDEFINED;
  if (!_gum_quick_args_parse (args, "pVA|V", &params->implementation,
      &params->return_type, &params->argument_types, &abi_or_options))
  {
    return FALSE;
  }
  params->abi_name = NULL;
  params->scheduling = GUM_QUICK_SCHEDULING_COOPERATIVE;
  params->exceptions = GUM_QUICK_EXCEPTIONS_STEAL;
  params->traps = GUM_QUICK_CODE_TRAPS_DEFAULT;
  params->return_shape = return_shape;

  if (JS_IsString (abi_or_options))
  {
    JSValueConst abi = abi_or_options;

    params->abi_name = JS_ToCString (ctx, abi);
  }
  else if (JS_IsObject (abi_or_options))
  {
    JSValueConst options = abi_or_options;
    GumQuickCore * core = args->core;

    val = JS_GetProperty (ctx, options, GUM_QUICK_CORE_ATOM (core, abi));
    if (JS_IsException (val))
      goto invalid_value;
    if (!JS_IsUndefined (val))
    {
      params->abi_name = JS_ToCString (ctx, val);
      if (params->abi_name == NULL)
        goto invalid_value;
      JS_FreeValue (ctx, val);
    }

    val = JS_GetProperty (ctx, options, GUM_QUICK_CORE_ATOM (core, scheduling));
    if (JS_IsException (val))
      goto invalid_value;
    if (!JS_IsUndefined (val))
    {
      if (!gum_quick_scheduling_behavior_get (ctx, val, &params->scheduling))
        goto invalid_value;
      JS_FreeValue (ctx, val);
    }

    val = JS_GetProperty (ctx, options, GUM_QUICK_CORE_ATOM (core, exceptions));
    if (JS_IsException (val))
      goto invalid_value;
    if (!JS_IsUndefined (val))
    {
      if (!gum_quick_exceptions_behavior_get (ctx, val, &params->exceptions))
        goto invalid_value;
      JS_FreeValue (ctx, val);
    }

    val = JS_GetProperty (ctx, options, GUM_QUICK_CORE_ATOM (core, traps));
    if (JS_IsException (val))
      goto invalid_value;
    if (!JS_IsUndefined (val))
    {
      if (!gum_quick_code_traps_get (ctx, val, &params->traps))
        goto invalid_value;
      JS_FreeValue (ctx, val);
    }
  }
  else if (!JS_IsUndefined (abi_or_options))
  {
    _gum_quick_throw_literal (ctx,
        "expected string or object containing options");
    return FALSE;
  }

  return TRUE;

invalid_value:
  {
    JS_FreeValue (ctx, val);
    JS_FreeCString (ctx, params->abi_name);

    return FALSE;
  }
}

static void
gum_quick_ffi_function_params_destroy (GumQuickFFIFunctionParams * params)
{
  JSContext * ctx = params->ctx;

  JS_FreeCString (ctx, params->abi_name);
}

static gboolean
gum_quick_scheduling_behavior_get (JSContext * ctx,
                                   JSValueConst val,
                                   GumQuickSchedulingBehavior * behavior)
{
  const char * str;

  str = JS_ToCString (ctx, val);
  if (str == NULL)
    return FALSE;

  if (strcmp (str, "cooperative") == 0)
    *behavior = GUM_QUICK_SCHEDULING_COOPERATIVE;
  else if (strcmp (str, "exclusive") == 0)
    *behavior = GUM_QUICK_SCHEDULING_EXCLUSIVE;
  else
    goto invalid_value;

  JS_FreeCString (ctx, str);

  return TRUE;

invalid_value:
  {
    JS_FreeCString (ctx, str);

    _gum_quick_throw_literal (ctx, "invalid scheduling behavior value");
    return FALSE;
  }
}

static gboolean
gum_quick_exceptions_behavior_get (JSContext * ctx,
                                   JSValueConst val,
                                   GumQuickExceptionsBehavior * behavior)
{
  const char * str;

  str = JS_ToCString (ctx, val);
  if (str == NULL)
    return FALSE;

  if (strcmp (str, "steal") == 0)
    *behavior = GUM_QUICK_EXCEPTIONS_STEAL;
  else if (strcmp (str, "propagate") == 0)
    *behavior = GUM_QUICK_EXCEPTIONS_PROPAGATE;
  else
    goto invalid_value;

  JS_FreeCString (ctx, str);

  return TRUE;

invalid_value:
  {
    JS_FreeCString (ctx, str);

    _gum_quick_throw_literal (ctx, "invalid exceptions behavior value");
    return FALSE;
  }
}

static gboolean
gum_quick_code_traps_get (JSContext * ctx,
                          JSValueConst val,
                          GumQuickCodeTraps * traps)
{
  const char * str;

  str = JS_ToCString (ctx, val);
  if (str == NULL)
    return FALSE;

  if (strcmp (str, "default") == 0)
    *traps = GUM_QUICK_CODE_TRAPS_DEFAULT;
  else if (strcmp (str, "none") == 0)
    *traps = GUM_QUICK_CODE_TRAPS_NONE;
  else if (strcmp (str, "all") == 0)
    *traps = GUM_QUICK_CODE_TRAPS_ALL;
  else
    goto invalid_value;

  JS_FreeCString (ctx, str);

  return TRUE;

invalid_value:
  {
    JS_FreeCString (ctx, str);

    _gum_quick_throw_literal (ctx, "invalid code traps value");
    return FALSE;
  }
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_native_callback_construct)
{
  JSValue wrapper = JS_NULL;
  JSValue func, rtype_value, atypes_array, proto;
  gchar * abi_str = NULL;
  GumQuickNativeCallback * cb = NULL;
  GumQuickNativePointer * ptr;
  ffi_type * rtype;
  guint nargs, i;
  JSValue val = JS_NULL;
  ffi_abi abi;

  if (!_gum_quick_args_parse (args, "FVA|s", &func, &rtype_value, &atypes_array,
      &abi_str))
    goto propagate_exception;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->native_callback_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  cb = g_slice_new0 (GumQuickNativeCallback);
  ptr = &cb->native_pointer;
  cb->wrapper = wrapper;
  cb->func = func;
  cb->core = core;

  if (!gum_quick_ffi_type_get (ctx, rtype_value, core, &rtype, &cb->data))
    goto propagate_exception;

  if (!_gum_quick_array_get_length (ctx, atypes_array, core, &nargs))
    goto propagate_exception;

  cb->atypes = g_new (ffi_type *, nargs);

  for (i = 0; i != nargs; i++)
  {
    ffi_type ** atype;

    val = JS_GetPropertyUint32 (ctx, atypes_array, i);
    if (JS_IsException (val))
      goto propagate_exception;

    atype = &cb->atypes[i];

    if (!gum_quick_ffi_type_get (ctx, val, core, atype, &cb->data))
      goto propagate_exception;

    JS_FreeValue (ctx, val);
    val = JS_NULL;
  }

  if (abi_str != NULL)
  {
    if (!gum_quick_ffi_abi_get (ctx, abi_str, &abi))
      goto propagate_exception;
  }
  else
  {
    abi = GUM_DEFAULT_FFI_ABI;
  }

  cb->closure = ffi_closure_alloc (sizeof (ffi_closure), &ptr->value);
  if (cb->closure == NULL)
    goto alloc_failed;

  if (ffi_prep_cif (&cb->cif, abi, (guint) nargs, rtype, cb->atypes) != FFI_OK)
    goto compilation_failed;

  if (ffi_prep_closure_loc (cb->closure, &cb->cif,
      gum_quick_native_callback_invoke, cb, ptr->value) != FFI_OK)
    goto prepare_failed;

  JS_SetOpaque (wrapper, cb);
  JS_DefinePropertyValue (ctx, wrapper,
      GUM_QUICK_CORE_ATOM (core, resource),
      JS_DupValue (ctx, func),
      0);

  return wrapper;

alloc_failed:
  {
    _gum_quick_throw_literal (ctx, "failed to allocate closure");
    goto propagate_exception;
  }
compilation_failed:
  {
    _gum_quick_throw_literal (ctx, "failed to compile function call interface");
    goto propagate_exception;
  }
prepare_failed:
  {
    _gum_quick_throw_literal (ctx, "failed to prepare closure");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeValue (ctx, val);
    if (cb != NULL)
      gum_quick_native_callback_finalize (cb);
    JS_FreeValue (ctx, wrapper);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FINALIZER (gumjs_native_callback_finalize)
{
  GumQuickNativeCallback * c;

  c = JS_GetOpaque (val, core->native_callback_class);
  if (c == NULL)
    return;

  gum_quick_native_callback_finalize (c);
}

static void
gum_quick_native_callback_finalize (GumQuickNativeCallback * callback)
{
  g_clear_pointer (&callback->closure, ffi_closure_free);

  while (callback->data != NULL)
  {
    GSList * head = callback->data;
    g_free (head->data);
    callback->data = g_slist_delete_link (callback->data, head);
  }
  g_free (callback->atypes);

  g_slice_free (GumQuickNativeCallback, callback);
}

static void
gum_quick_native_callback_invoke (ffi_cif * cif,
                                  void * return_value,
                                  void ** args,
                                  void * user_data)
{
  GumQuickNativeCallback * self = user_data;
  GumQuickCore * core = self->core;
  gint saved_system_error;
  guintptr return_address = 0;
  guintptr stack_pointer = 0;
  guintptr frame_pointer = 0;
  GumQuickScope scope;
  JSContext * ctx = core->ctx;
  ffi_type * rtype = cif->rtype;
  GumFFIArg * tmp_value;
  GumFFIRet * retval = return_value;
  GumInvocationContext * ic;
  GumQuickInvocationContext * jic = NULL;
  JSValue this_obj;
  GumQuickCallbackContext * jcc = NULL;
  int argc, i;
  JSValue * argv;
  JSValue result;

  tmp_value = g_alloca (rtype->size);

  saved_system_error = gum_thread_get_system_error ();

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

  _gum_quick_scope_enter (&scope, core);

  JS_DupValue (ctx, self->wrapper);

  if (rtype != &ffi_type_void)
    memset (retval, 0, rtype->size);

  if (core->interceptor != NULL &&
      (ic = gum_interceptor_get_live_replacement_invocation (
        self->native_pointer.value)) != NULL)
  {
    jic = _gum_quick_interceptor_obtain_invocation_context (core->interceptor);
    _gum_quick_invocation_context_reset (jic, ic);

    this_obj = jic->wrapper;
  }
  else
  {
    GumCpuContext cpu_context = { 0, };

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

    this_obj = gum_quick_callback_context_new (core, &cpu_context,
        &saved_system_error, return_address, &jcc);
  }

  argc = cif->nargs;
  argv = g_newa (JSValue, argc);

  for (i = 0; i != argc; i++)
    argv[i] = gum_quick_value_from_ffi (ctx, args[i], cif->arg_types[i], core);

  result = _gum_quick_scope_call (&scope, self->func, this_obj, argc, argv);

  for (i = 0; i != argc; i++)
    JS_FreeValue (ctx, argv[i]);

  if (jic != NULL)
  {
    _gum_quick_invocation_context_reset (jic, NULL);
    _gum_quick_interceptor_release_invocation_context (core->interceptor, jic);
  }

  if (jcc != NULL)
  {
    jcc->system_error = NULL;
    JS_FreeValue (ctx, jcc->cpu_context->wrapper);
    jcc->cpu_context = NULL;
    JS_FreeValue (ctx, jcc->wrapper);
  }

  if (!JS_IsException (result) && cif->rtype != &ffi_type_void)
  {
    if (gum_quick_value_to_ffi (ctx, result, cif->rtype, core, tmp_value))
      gum_ffi_arg_to_ret (cif->rtype, tmp_value, retval);
    else
      _gum_quick_scope_catch_and_emit (&scope);
  }
  JS_FreeValue (ctx, result);

  JS_FreeValue (ctx, self->wrapper);

  _gum_quick_scope_leave (&scope);

  gum_thread_set_system_error (saved_system_error);
}

GUMJS_DEFINE_FINALIZER (gumjs_callback_context_finalize)
{
  GumQuickCallbackContext * c;

  c = JS_GetOpaque (val, core->callback_context_class);
  if (c == NULL)
    return;

  g_slice_free (GumQuickCallbackContext, c);
}

static JSValue
gum_quick_callback_context_new (GumQuickCore * core,
                                GumCpuContext * cpu_context,
                                gint * system_error,
                                GumAddress raw_return_address,
                                GumQuickCallbackContext ** context)
{
  JSValue wrapper;
  GumQuickCallbackContext * jcc;
  JSContext * ctx = core->ctx;

  wrapper = JS_NewObjectClass (ctx, core->callback_context_class);

  jcc = g_slice_new (GumQuickCallbackContext);
  jcc->wrapper = wrapper;
  jcc->cpu_context = NULL;
  jcc->system_error = system_error;
  jcc->return_address = 0;
  jcc->raw_return_address = raw_return_address;
  jcc->initial_property_count = JS_GetOwnPropertyCountUnchecked (wrapper);

  _gum_quick_cpu_context_new (ctx, cpu_context, GUM_CPU_CONTEXT_READONLY,
      core, &jcc->cpu_context);

  JS_SetOpaque (wrapper, jcc);

  *context = jcc;

  return wrapper;
}

GUMJS_DEFINE_GETTER (gumjs_callback_context_get_return_address)
{
  GumQuickCallbackContext * self;

  if (!gum_quick_callback_context_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (self->return_address == 0)
  {
    GumCpuContext * cpu_context = self->cpu_context->handle;
    GumBacktracer * backtracer;

    backtracer = gum_backtracer_make_accurate ();

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

  return _gum_quick_native_pointer_new (ctx,
      GSIZE_TO_POINTER (self->return_address), core);
}

GUMJS_DEFINE_GETTER (gumjs_callback_context_get_cpu_context)
{
  GumQuickCallbackContext * self;

  if (!gum_quick_callback_context_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_DupValue (ctx, self->cpu_context->wrapper);
}

GUMJS_DEFINE_GETTER (gumjs_callback_context_get_system_error)
{
  GumQuickCallbackContext * self;

  if (!gum_quick_callback_context_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  return JS_NewInt32 (ctx, *self->system_error);
}

GUMJS_DEFINE_SETTER (gumjs_callback_context_set_system_error)
{
  GumQuickCallbackContext * self;
  gint value;

  if (!gum_quick_callback_context_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_int_get (ctx, val, &value))
    return JS_EXCEPTION;

  *self->system_error = value;

  return JS_UNDEFINED;
}

static gboolean
gum_quick_callback_context_get (JSContext * ctx,
                                JSValueConst val,
                                GumQuickCore * core,
                                GumQuickCallbackContext ** cc)
{
  GumQuickCallbackContext * c;

  if (!_gum_quick_unwrap (ctx, val, core->callback_context_class, core,
        (gpointer *) &c))
    return FALSE;

  if (c->cpu_context == NULL)
  {
    _gum_quick_throw_literal (ctx, "invalid operation");
    return FALSE;
  }

  *cc = c;
  return TRUE;
}

GUMJS_DEFINE_FINALIZER (gumjs_cpu_context_finalize)
{
  GumQuickCpuContext * c;

  c = JS_GetOpaque (val, core->cpu_context_class);
  if (c == NULL)
    return;

  g_slice_free (GumQuickCpuContext, c);
}

GUMJS_DEFINE_FUNCTION (gumjs_cpu_context_to_json)
{
  JSValue result;
  guint i;

  result = JS_NewObject (ctx);

  for (i = 0; i != G_N_ELEMENTS (gumjs_cpu_context_entries); i++)
  {
    const JSCFunctionListEntry * e = &gumjs_cpu_context_entries[i];
    JSValue val;

    if (e->def_type != JS_DEF_CGETSET)
      continue;

    val = JS_GetPropertyStr (ctx, this_val, e->name);
    if (JS_IsException (val))
      goto propagate_exception;
    JS_SetPropertyStr (ctx, result, e->name, val);
  }

  return result;

propagate_exception:
  {
    JS_FreeValue (ctx, result);

    return JS_EXCEPTION;
  }
}

static JSValue
gumjs_cpu_context_set_gpr (GumQuickCpuContext * self,
                           JSContext * ctx,
                           JSValueConst val,
                           gpointer * reg)
{
  if (self->access == GUM_CPU_CONTEXT_READONLY)
    return _gum_quick_throw_literal (ctx, "invalid operation");

  return _gum_quick_native_pointer_parse (ctx, val, self->core, reg)
      ? JS_UNDEFINED
      : JS_EXCEPTION;
}

static JSValue
gumjs_cpu_context_set_vector (GumQuickCpuContext * self,
                              JSContext * ctx,
                              JSValueConst val,
                              guint8 * bytes,
                              gsize size)
{
  GBytes * new_bytes;
  gconstpointer new_data;
  gsize new_size;

  if (self->access == GUM_CPU_CONTEXT_READONLY)
    return _gum_quick_throw_literal (ctx, "invalid operation");

  if (!_gum_quick_bytes_get (ctx, val, self->core, &new_bytes))
    return JS_EXCEPTION;

  new_data = g_bytes_get_data (new_bytes, &new_size);
  if (new_size != size)
    goto incorrect_size;

  memcpy (bytes, new_data, new_size);

  g_bytes_unref (new_bytes);

  return JS_UNDEFINED;

incorrect_size:
  {
    g_bytes_unref (new_bytes);
    return _gum_quick_throw_literal (ctx, "incorrect vector size");
  }
}

static JSValue
gumjs_cpu_context_set_double (GumQuickCpuContext * self,
                              JSContext * ctx,
                              JSValueConst val,
                              gdouble * d)
{
  if (self->access == GUM_CPU_CONTEXT_READONLY)
    return _gum_quick_throw_literal (ctx, "invalid operation");

  return _gum_quick_float64_get (ctx, val, d)
      ? JS_UNDEFINED
      : JS_EXCEPTION;
}

static JSValue
gumjs_cpu_context_set_float (GumQuickCpuContext * self,
                             JSContext * ctx,
                             JSValueConst val,
                             gfloat * f)
{
  gdouble d;

  if (self->access == GUM_CPU_CONTEXT_READONLY)
    return _gum_quick_throw_literal (ctx, "invalid operation");

  if (!_gum_quick_float64_get (ctx, val, &d))
    return JS_EXCEPTION;

  *f = (gfloat) d;

  return JS_UNDEFINED;
}

static JSValue
gumjs_cpu_context_set_flags (GumQuickCpuContext * self,
                             JSContext * ctx,
                             JSValueConst val,
                             gsize * f)
{
  return _gum_quick_size_get (ctx, val, self->core, f)
      ? JS_UNDEFINED
      : JS_EXCEPTION;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_match_pattern_construct)
{
  JSValue wrapper;
  const gchar * pattern_str;
  JSValue proto;
  GumMatchPattern * pattern;

  wrapper = JS_NULL;

  if (!_gum_quick_args_parse (args, "s", &pattern_str))
    goto propagate_exception;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->match_pattern_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  pattern = gum_match_pattern_new_from_string (pattern_str);
  if (pattern == NULL)
    goto invalid_match_pattern;

  JS_SetOpaque (wrapper, pattern);

  return wrapper;

invalid_match_pattern:
  {
    _gum_quick_throw_literal (ctx, "invalid match pattern");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FINALIZER (gumjs_match_pattern_finalize)
{
  GumMatchPattern * p;

  p = JS_GetOpaque (val, core->match_pattern_class);
  if (p == NULL)
    return;

  gum_match_pattern_unref (p);
}

static gboolean
gum_quick_source_map_get (JSContext * ctx,
                          JSValueConst val,
                          GumQuickCore * core,
                          GumSourceMap ** source_map)
{
  return _gum_quick_unwrap (ctx, val, core->source_map_class, core,
      (gpointer *) source_map);
}

static JSValue
gumjs_source_map_new (const gchar * json,
                      GumQuickCore * core)
{
  JSValue result;
  JSContext * ctx = core->ctx;
  JSValue json_val;

  json_val = JS_NewString (ctx, json);

  result = JS_CallConstructor (ctx, core->source_map_ctor, 1, &json_val);

  JS_FreeValue (ctx, json_val);

  return result;
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_source_map_construct)
{
  JSValue wrapper = JS_NULL;
  const gchar * json;
  JSValue proto;
  GumSourceMap * map;

  if (!_gum_quick_args_parse (args, "s", &json))
    goto propagate_exception;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->source_map_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  map = gum_source_map_new (json);
  if (map == NULL)
    goto invalid_source_map;

  JS_SetOpaque (wrapper, map);

  return wrapper;

invalid_source_map:
  {
    _gum_quick_throw_literal (ctx, "invalid source map");
    goto propagate_exception;
  }
propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);

    return JS_EXCEPTION;
  }
}

GUMJS_DEFINE_FINALIZER (gumjs_source_map_finalize)
{
  GumSourceMap * m;

  m = JS_GetOpaque (val, core->source_map_class);
  if (m == NULL)
    return;

  g_object_unref (m);
}

GUMJS_DEFINE_FUNCTION (gumjs_source_map_resolve)
{
  GumSourceMap * self;
  guint line, column;
  const gchar * source, * name;

  if (!gum_quick_source_map_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (args->count == 1)
  {
    if (!_gum_quick_args_parse (args, "u", &line))
      return JS_EXCEPTION;
    column = G_MAXUINT;
  }
  else
  {
    if (!_gum_quick_args_parse (args, "uu", &line, &column))
      return JS_EXCEPTION;
  }

  if (gum_source_map_resolve (self, &line, &column, &source, &name))
  {
    JSValue pos;
    const int fl = JS_PROP_C_W_E;

    pos = JS_NewArray (ctx);
    JS_DefinePropertyValueUint32 (ctx, pos, 0, JS_NewString (ctx, source), fl);
    JS_DefinePropertyValueUint32 (ctx, pos, 1, JS_NewUint32 (ctx, line), fl);
    JS_DefinePropertyValueUint32 (ctx, pos, 2, JS_NewUint32 (ctx, column), fl);
    JS_DefinePropertyValueUint32 (ctx, pos, 3,
        (name != NULL) ? JS_NewString (ctx, name) : JS_NULL, fl);

    return pos;
  }
  else
  {
    return JS_NULL;
  }
}

static gboolean
gum_quick_worker_get (JSContext * ctx,
                      JSValueConst val,
                      GumQuickCore * core,
                      GumQuickWorker ** worker)
{
  return _gum_quick_unwrap (ctx, val, core->worker_class, core,
      (gpointer *) worker);
}

GUMJS_DEFINE_CONSTRUCTOR (gumjs_worker_construct)
{
  JSValue wrapper = JS_NULL;
  const gchar * url;
  JSValue on_message, proto;
  GumQuickWorker * worker;

  if (!_gum_quick_args_parse (args, "sF", &url, &on_message))
    goto propagate_exception;

  proto = JS_GetProperty (ctx, new_target,
      GUM_QUICK_CORE_ATOM (core, prototype));
  wrapper = JS_NewObjectProtoClass (ctx, proto, core->worker_class);
  JS_FreeValue (ctx, proto);
  if (JS_IsException (wrapper))
    goto propagate_exception;

  worker = _gum_quick_script_make_worker (core->script, url, on_message);
  if (worker == NULL)
    goto propagate_exception;

  JS_SetOpaque (wrapper, worker);
  JS_DefinePropertyValue (ctx, wrapper,
      GUM_QUICK_CORE_ATOM (core, resource),
      JS_DupValue (ctx, on_message),
      0);

  g_hash_table_add (core->workers, worker);

  return wrapper;

propagate_exception:
  {
    JS_FreeValue (ctx, wrapper);

    return JS_EXCEPTION;
  }
}

static void
gum_quick_worker_destroy (GumQuickWorker * worker)
{
  _gum_quick_worker_terminate (worker);
  _gum_quick_worker_unref (worker);
}

GUMJS_DEFINE_FUNCTION (gumjs_worker_terminate)
{
  GumQuickWorker * self;

  if (!gum_quick_worker_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  JS_SetOpaque (this_val, NULL);

  g_hash_table_remove (core->workers, self);

  return JS_UNDEFINED;
}

GUMJS_DEFINE_FUNCTION (gumjs_worker_post)
{
  GumQuickWorker * self;
  const char * message;
  GBytes * data;

  if (!gum_quick_worker_get (ctx, this_val, core, &self))
    return JS_EXCEPTION;

  if (!_gum_quick_args_parse (args, "sB?", &message, &data))
    return JS_EXCEPTION;

  _gum_quick_worker_post (self, message, data);

  return JS_UNDEFINED;
}

static JSValue
gum_quick_core_schedule_callback (GumQuickCore * self,
                                  GumQuickArgs * args,
                                  gboolean repeat)
{
  JSValue func;
  gsize delay;
  guint id;
  GSource * source;
  GumQuickScheduledCallback * callback;

  if (repeat)
  {
    if (!_gum_quick_args_parse (args, "FZ", &func, &delay))
      return JS_EXCEPTION;
  }
  else
  {
    delay = 0;
    if (!_gum_quick_args_parse (args, "F|Z", &func, &delay))
      return JS_EXCEPTION;
  }

  id = self->next_callback_id++;
  if (delay == 0)
    source = g_idle_source_new ();
  else
    source = g_timeout_source_new ((guint) delay);

  callback = gum_scheduled_callback_new (id, func, repeat, source, self);
  g_source_set_callback (source, (GSourceFunc) gum_scheduled_callback_invoke,
      callback, (GDestroyNotify) gum_scheduled_callback_free);

  g_hash_table_insert (self->scheduled_callbacks, GINT_TO_POINTER (id),
      callback);
  g_queue_push_tail (&self->current_scope->scheduled_sources, source);

  return JS_NewUint32 (self->ctx, id);
}

static GumQuickScheduledCallback *
gum_quick_core_try_steal_scheduled_callback (GumQuickCore * self,
                                             gint id)
{
  GumQuickScheduledCallback * callback;
  gpointer raw_id;

  raw_id = GINT_TO_POINTER (id);

  callback = g_hash_table_lookup (self->scheduled_callbacks, raw_id);
  if (callback == NULL)
    return NULL;

  g_hash_table_remove (self->scheduled_callbacks, raw_id);

  return callback;
}

static GumQuickScheduledCallback *
gum_scheduled_callback_new (guint id,
                            JSValueConst func,
                            gboolean repeat,
                            GSource * source,
                            GumQuickCore * core)
{
  GumQuickScheduledCallback * cb;

  cb = g_slice_new (GumQuickScheduledCallback);
  cb->id = id;
  cb->func = JS_DupValue (core->ctx, func);
  cb->repeat = repeat;
  cb->source = source;
  cb->core = core;

  return cb;
}

static void
gum_scheduled_callback_free (GumQuickScheduledCallback * callback)
{
  GumQuickCore * core = callback->core;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, core);
  _gum_quick_core_unpin (core);
  JS_FreeValue (core->ctx, callback->func);
  _gum_quick_scope_leave (&scope);

  g_slice_free (GumQuickScheduledCallback, callback);
}

static gboolean
gum_scheduled_callback_invoke (GumQuickScheduledCallback * self)
{
  GumQuickCore * core = self->core;
  GumQuickScope scope;

  _gum_quick_scope_enter (&scope, self->core);

  _gum_quick_scope_call_void (&scope, self->func, JS_UNDEFINED, 0, NULL);

  if (!self->repeat)
  {
    if (gum_quick_core_try_steal_scheduled_callback (core, self->id) != NULL)
      _gum_quick_core_pin (core);
  }

  _gum_quick_scope_leave (&scope);

  return self->repeat;
}

static GumQuickExceptionSink *
gum_quick_exception_sink_new (JSValueConst callback,
                              GumQuickCore * core)
{
  GumQuickExceptionSink * sink;

  sink = g_slice_new (GumQuickExceptionSink);
  sink->callback = JS_DupValue (core->ctx, callback);
  sink->core = core;

  return sink;
}

static void
gum_quick_exception_sink_free (GumQuickExceptionSink * sink)
{
  JS_FreeValue (sink->core->ctx, sink->callback);

  g_slice_free (GumQuickExceptionSink, sink);
}

static void
gum_quick_exception_sink_handle_exception (GumQuickExceptionSink * self,
                                           JSValueConst exception)
{
  JSContext * ctx = self->core->ctx;
  JSValue result;

  result = JS_Call (ctx, self->callback, JS_UNDEFINED, 1, &exception);
  if (JS_IsException (result))
    JS_FreeValue (ctx, JS_GetException (ctx));
  else
    JS_FreeValue (ctx, result);
}

static GumQuickMessageSink *
gum_quick_message_sink_new (JSValueConst callback,
                            GumQuickCore * core)
{
  GumQuickMessageSink * sink;

  sink = g_slice_new (GumQuickMessageSink);
  sink->callback = JS_DupValue (core->ctx, callback);
  sink->core = core;

  return sink;
}

static void
gum_quick_message_sink_free (GumQuickMessageSink * sink)
{
  JS_FreeValue (sink->core->ctx, sink->callback);

  g_slice_free (GumQuickMessageSink, sink);
}

static void
gum_quick_message_sink_post (GumQuickMessageSink * self,
                             const gchar * message,
                             GBytes * data,
                             GumQuickScope * scope)
{
  JSContext * ctx = self->core->ctx;
  JSValue argv[2];

  argv[0] = JS_NewString (ctx, message);

  if (data != NULL)
  {
    gpointer data_buffer;
    gsize data_size;

    data_buffer = g_bytes_unref_to_data (data, &data_size);

    argv[1] = JS_NewArrayBuffer (ctx, data_buffer, data_size,
        _gum_quick_array_buffer_free, data_buffer, FALSE);
  }
  else
  {
    argv[1] = JS_NULL;
  }

  _gum_quick_scope_call_void (scope, self->callback, JS_UNDEFINED,
      G_N_ELEMENTS (argv), argv);

  JS_FreeValue (ctx, argv[1]);
  JS_FreeValue (ctx, argv[0]);
}

static gboolean
gum_quick_ffi_type_get (JSContext * ctx,
                        JSValueConst val,
                        GumQuickCore * core,
                        ffi_type ** type,
                        GSList ** data)
{
  gboolean success = FALSE;
  JSValue field_value;

  if (JS_IsString (val))
  {
    const gchar * type_name = JS_ToCString (ctx, val);
    success = gum_ffi_try_get_type_by_name (type_name, type);
    JS_FreeCString (ctx, type_name);
  }
  else if (JS_IsArray (ctx, val))
  {
    guint length, i;
    ffi_type ** fields, * struct_type;

    if (!_gum_quick_array_get_length (ctx, val, core, &length))
      return FALSE;

    fields = g_new (ffi_type *, length + 1);
    *data = g_slist_prepend (*data, fields);

    for (i = 0; i != length; i++)
    {
      field_value = JS_GetPropertyUint32 (ctx, val, i);

      if (!gum_quick_ffi_type_get (ctx, field_value, core, &fields[i], data))
        goto invalid_field_value;

      JS_FreeValue (ctx, field_value);
    }

    fields[length] = NULL;

    struct_type = g_new0 (ffi_type, 1);
    struct_type->type = FFI_TYPE_STRUCT;
    struct_type->elements = fields;
    *data = g_slist_prepend (*data, struct_type);

    *type = struct_type;
    success = TRUE;
  }

  if (!success)
    _gum_quick_throw_literal (ctx, "invalid type specified");

  return success;

invalid_field_value:
  {
    JS_FreeValue (ctx, field_value);

    return FALSE;
  }
}

static gboolean
gum_quick_ffi_abi_get (JSContext * ctx,
                       const gchar * name,
                       ffi_abi * abi)
{
  if (gum_ffi_try_get_abi_by_name (name, abi))
    return TRUE;

  _gum_quick_throw_literal (ctx, "invalid abi specified");
  return FALSE;
}

static gboolean
gum_quick_value_to_ffi (JSContext * ctx,
                        JSValueConst sval,
                        const ffi_type * type,
                        GumQuickCore * core,
                        GumFFIArg * val)
{
  gint i;
  guint u;
  gint64 i64;
  guint64 u64;
  gdouble d;

  if (type == &ffi_type_void)
  {
    val->v_pointer = NULL;
  }
  else if (type == &ffi_type_pointer)
  {
    if (!_gum_quick_native_pointer_get (ctx, sval, core, &val->v_pointer))
      return FALSE;
  }
  else if (type == &ffi_type_sint8)
  {
    if (!_gum_quick_int_get (ctx, sval, &i))
      return FALSE;
    val->v_sint8 = i;
  }
  else if (type == &ffi_type_uint8)
  {
    if (!_gum_quick_uint_get (ctx, sval, &u))
      return FALSE;
    val->v_uint8 = u;
  }
  else if (type == &ffi_type_sint16)
  {
    if (!_gum_quick_int_get (ctx, sval, &i))
      return FALSE;
    val->v_sint16 = i;
  }
  else if (type == &ffi_type_uint16)
  {
    if (!_gum_quick_uint_get (ctx, sval, &u))
      return FALSE;
    val->v_uint16 = u;
  }
  else if (type == &ffi_type_sint32)
  {
    if (!_gum_quick_int_get (ctx, sval, &i))
      return FALSE;
    val->v_sint32 = i;
  }
  else if (type == &ffi_type_uint32)
  {
    if (!_gum_quick_uint_get (ctx, sval, &u))
      return FALSE;
    val->v_uint32 = u;
  }
  else if (type == &ffi_type_sint64)
  {
    if (!_gum_quick_int64_get (ctx, sval, core, &i64))
      return FALSE;
    val->v_sint64 = i64;
  }
  else if (type == &ffi_type_uint64)
  {
    if (!_gum_quick_uint64_get (ctx, sval, core, &u64))
      return FALSE;
    val->v_uint64 = u64;
  }
  else if (type == &gum_ffi_type_size_t)
  {
    if (!_gum_quick_uint64_get (ctx, sval, core, &u64))
      return FALSE;

    switch (type->size)
    {
      case 8:
        val->v_uint64 = u64;
        break;
      case 4:
        val->v_uint32 = u64;
        break;
      case 2:
        val->v_uint16 = u64;
        break;
      default:
        g_assert_not_reached ();
    }
  }
  else if (type == &gum_ffi_type_ssize_t)
  {
    if (!_gum_quick_int64_get (ctx, sval, core, &i64))
      return FALSE;

    switch (type->size)
    {
      case 8:
        val->v_sint64 = i64;
        break;
      case 4:
        val->v_sint32 = i64;
        break;
      case 2:
        val->v_sint16 = i64;
        break;
      default:
        g_assert_not_reached ();
    }
  }
  else if (type == &ffi_type_float)
  {
    if (!_gum_quick_float64_get (ctx, sval, &d))
      return FALSE;
    val->v_float = d;
  }
  else if (type == &ffi_type_double)
  {
    if (!_gum_quick_float64_get (ctx, sval, &d))
      return FALSE;
    val->v_double = d;
  }
  else if (type->type == FFI_TYPE_STRUCT)
  {
    ffi_type ** const field_types = type->elements, ** t;
    guint length, expected_length, field_index;
    guint8 * field_values;
    gsize offset;

    if (!_gum_quick_array_get_length (ctx, sval, core, &length))
      return FALSE;

    expected_length = 0;
    for (t = field_types; *t != NULL; t++)
      expected_length++;

    if (length != expected_length)
    {
      _gum_quick_throw_literal (ctx,
          "provided array length does not match number of fields");
      return FALSE;
    }

    field_values = (guint8 *) val;
    offset = 0;

    for (field_index = 0; field_index != length; field_index++)
    {
      const ffi_type * field_type = field_types[field_index];
      GumFFIArg * field_val;
      JSValue field_sval;
      gboolean valid;

      offset = GUM_ALIGN_SIZE (offset, field_type->alignment);

      field_val = (GumFFIArg *) (field_values + offset);

      field_sval = JS_GetPropertyUint32 (ctx, sval, field_index);
      if (JS_IsException (field_sval))
        return FALSE;

      valid =
          gum_quick_value_to_ffi (ctx, field_sval, field_type, core, field_val);

      JS_FreeValue (ctx, field_sval);

      if (!valid)
        return FALSE;

      offset += field_type->size;
    }
  }
  else
  {
    g_assert_not_reached ();
  }

  return TRUE;
}

static JSValue
gum_quick_value_from_ffi (JSContext * ctx,
                          const GumFFIRet * val,
                          const ffi_type * type,
                          GumQuickCore * core)
{
  if (type == &ffi_type_void)
  {
    return JS_UNDEFINED;
  }
  else if (type == &ffi_type_pointer)
  {
    return _gum_quick_native_pointer_new (ctx, val->v_pointer, core);
  }
  else if (type == &ffi_type_sint8)
  {
    return JS_NewInt32 (ctx, val->v_sint8);
  }
  else if (type == &ffi_type_uint8)
  {
    return JS_NewUint32 (ctx, val->v_uint8);
  }
  else if (type == &ffi_type_sint16)
  {
    return JS_NewInt32 (ctx, val->v_sint16);
  }
  else if (type == &ffi_type_uint16)
  {
    return JS_NewUint32 (ctx, val->v_uint16);
  }
  else if (type == &ffi_type_sint32)
  {
    return JS_NewInt32 (ctx, val->v_sint32);
  }
  else if (type == &ffi_type_uint32)
  {
    return JS_NewUint32 (ctx, val->v_uint32);
  }
  else if (type == &ffi_type_sint64)
  {
    return _gum_quick_int64_new (ctx, val->v_sint64, core);
  }
  else if (type == &ffi_type_uint64)
  {
    return _gum_quick_uint64_new (ctx, val->v_uint64, core);
  }
  else if (type == &gum_ffi_type_size_t)
  {
    guint64 u64;

    switch (type->size)
    {
      case 8:
        u64 = val->v_uint64;
        break;
      case 4:
        u64 = val->v_uint32;
        break;
      case 2:
        u64 = val->v_uint16;
        break;
      default:
        u64 = 0;
        g_assert_not_reached ();
    }

    return _gum_quick_uint64_new (ctx, u64, core);
  }
  else if (type == &gum_ffi_type_ssize_t)
  {
    gint64 i64;

    switch (type->size)
    {
      case 8:
        i64 = val->v_sint64;
        break;
      case 4:
        i64 = val->v_sint32;
        break;
      case 2:
        i64 = val->v_sint16;
        break;
      default:
        i64 = 0;
        g_assert_not_reached ();
    }

    return _gum_quick_int64_new (ctx, i64, core);
  }
  else if (type == &ffi_type_float)
  {
    return JS_NewFloat64 (ctx, val->v_float);
  }
  else if (type == &ffi_type_double)
  {
    return JS_NewFloat64 (ctx, val->v_double);
  }
  else if (type->type == FFI_TYPE_STRUCT)
  {
    ffi_type ** const field_types = type->elements, ** t;
    guint length, i;
    const guint8 * field_values;
    gsize offset;
    JSValue field_svalues;

    length = 0;
    for (t = field_types; *t != NULL; t++)
      length++;

    field_values = (const guint8 *) val;
    offset = 0;

    field_svalues = JS_NewArray (ctx);

    for (i = 0; i != length; i++)
    {
      const ffi_type * field_type = field_types[i];
      const GumFFIRet * field_val;
      JSValue field_sval;

      offset = GUM_ALIGN_SIZE (offset, field_type->alignment);
      field_val = (const GumFFIRet *) (field_values + offset);

      field_sval = gum_quick_value_from_ffi (ctx, field_val, field_type, core);

      JS_DefinePropertyValueUint32 (ctx, field_svalues, i, field_sval,
          JS_PROP_C_W_E);

      offset += field_type->size;
    }

    return field_svalues;
  }
  else
  {
    g_assert_not_reached ();
  }
}

static void
gum_quick_core_setup_atoms (GumQuickCore * self)
{
  JSContext * ctx = self->ctx;

#define GUM_SETUP_ATOM(id) \
    GUM_SETUP_ATOM_NAMED (id, G_STRINGIFY (id))
#define GUM_SETUP_ATOM_NAMED(id, name) \
    GUM_QUICK_CORE_ATOM (self, id) = JS_NewAtom (ctx, name)

  GUM_SETUP_ATOM (abi);
  GUM_SETUP_ATOM (access);
  GUM_SETUP_ATOM (address);
  GUM_SETUP_ATOM (autoClose);
  GUM_SETUP_ATOM (base);
  GUM_SETUP_ATOM_NAMED (cachedInput, "$i");
  GUM_SETUP_ATOM_NAMED (cachedOutput, "$o");
  GUM_SETUP_ATOM (context);
  GUM_SETUP_ATOM (entrypoint);
  GUM_SETUP_ATOM (exceptions);
  GUM_SETUP_ATOM (file);
  GUM_SETUP_ATOM (handle);
  GUM_SETUP_ATOM (id);
  GUM_SETUP_ATOM (ip);
  GUM_SETUP_ATOM (isGlobal);
  GUM_SETUP_ATOM (length);
  GUM_SETUP_ATOM (memory);
  GUM_SETUP_ATOM (message);
  GUM_SETUP_ATOM (module);
  GUM_SETUP_ATOM (name);
  GUM_SETUP_ATOM (nativeContext);
  GUM_SETUP_ATOM (offset);
  GUM_SETUP_ATOM (operation);
  GUM_SETUP_ATOM (parameter);
  GUM_SETUP_ATOM (path);
  GUM_SETUP_ATOM (pc);
  GUM_SETUP_ATOM (port);
  GUM_SETUP_ATOM (protection);
  GUM_SETUP_ATOM (prototype);
  GUM_SETUP_ATOM (read);
  GUM_SETUP_ATOM_NAMED (resource, "$r");
  GUM_SETUP_ATOM (routine);
  GUM_SETUP_ATOM (scheduling);
  GUM_SETUP_ATOM (section);
  GUM_SETUP_ATOM (size);
  GUM_SETUP_ATOM (slot);
  GUM_SETUP_ATOM (state);
  GUM_SETUP_ATOM_NAMED (system_error, GUMJS_SYSTEM_ERROR_FIELD);
  GUM_SETUP_ATOM (toolchain);
  GUM_SETUP_ATOM (traps);
  GUM_SETUP_ATOM (type);
  GUM_SETUP_ATOM (value);
  GUM_SETUP_ATOM (written);

#if defined (HAVE_I386)
  GUM_SETUP_ATOM (disp);
  GUM_SETUP_ATOM (index);
  GUM_SETUP_ATOM (scale);
  GUM_SETUP_ATOM (segment);
#elif defined (HAVE_ARM)
  GUM_SETUP_ATOM (disp);
  GUM_SETUP_ATOM (index);
  GUM_SETUP_ATOM (scale);
  GUM_SETUP_ATOM (shift);
  GUM_SETUP_ATOM (subtracted);
  GUM_SETUP_ATOM (vectorIndex);
#elif defined (HAVE_ARM64)
  GUM_SETUP_ATOM (disp);
  GUM_SETUP_ATOM (ext);
  GUM_SETUP_ATOM (index);
  GUM_SETUP_ATOM (shift);
  GUM_SETUP_ATOM (vas);
  GUM_SETUP_ATOM (vectorIndex);
#elif defined (HAVE_MIPS)
  GUM_SETUP_ATOM (disp);
#endif

#undef GUM_SETUP_ATOM
}

static void
gum_quick_core_teardown_atoms (GumQuickCore * self)
{
  JSContext * ctx = self->ctx;

#define GUM_TEARDOWN_ATOM(id) \
    JS_FreeAtom (ctx, GUM_QUICK_CORE_ATOM (self, id)); \
    GUM_QUICK_CORE_ATOM (self, id) = JS_ATOM_NULL

  GUM_TEARDOWN_ATOM (abi);
  GUM_TEARDOWN_ATOM (access);
  GUM_TEARDOWN_ATOM (address);
  GUM_TEARDOWN_ATOM (autoClose);
  GUM_TEARDOWN_ATOM (base);
  GUM_TEARDOWN_ATOM (cachedInput);
  GUM_TEARDOWN_ATOM (cachedOutput);
  GUM_TEARDOWN_ATOM (context);
  GUM_TEARDOWN_ATOM (exceptions);
  GUM_TEARDOWN_ATOM (file);
  GUM_TEARDOWN_ATOM (handle);
  GUM_TEARDOWN_ATOM (id);
  GUM_TEARDOWN_ATOM (ip);
  GUM_TEARDOWN_ATOM (isGlobal);
  GUM_TEARDOWN_ATOM (length);
  GUM_TEARDOWN_ATOM (memory);
  GUM_TEARDOWN_ATOM (message);
  GUM_TEARDOWN_ATOM (module);
  GUM_TEARDOWN_ATOM (name);
  GUM_TEARDOWN_ATOM (nativeContext);
  GUM_TEARDOWN_ATOM (offset);
  GUM_TEARDOWN_ATOM (operation);
  GUM_TEARDOWN_ATOM (path);
  GUM_TEARDOWN_ATOM (pc);
  GUM_TEARDOWN_ATOM (port);
  GUM_TEARDOWN_ATOM (protection);
  GUM_TEARDOWN_ATOM (prototype);
  GUM_TEARDOWN_ATOM (read);
  GUM_TEARDOWN_ATOM (resource);
  GUM_TEARDOWN_ATOM (scheduling);
  GUM_TEARDOWN_ATOM (section);
  GUM_TEARDOWN_ATOM (size);
  GUM_TEARDOWN_ATOM (slot);
  GUM_TEARDOWN_ATOM (state);
  GUM_TEARDOWN_ATOM (system_error);
  GUM_TEARDOWN_ATOM (toolchain);
  GUM_TEARDOWN_ATOM (traps);
  GUM_TEARDOWN_ATOM (type);
  GUM_TEARDOWN_ATOM (value);
  GUM_TEARDOWN_ATOM (written);

#if defined (HAVE_I386)
  GUM_TEARDOWN_ATOM (disp);
  GUM_TEARDOWN_ATOM (index);
  GUM_TEARDOWN_ATOM (scale);
  GUM_TEARDOWN_ATOM (segment);
#elif defined (HAVE_ARM)
  GUM_TEARDOWN_ATOM (disp);
  GUM_TEARDOWN_ATOM (index);
  GUM_TEARDOWN_ATOM (scale);
  GUM_TEARDOWN_ATOM (shift);
  GUM_TEARDOWN_ATOM (subtracted);
  GUM_TEARDOWN_ATOM (vectorIndex);
#elif defined (HAVE_ARM64)
  GUM_TEARDOWN_ATOM (disp);
  GUM_TEARDOWN_ATOM (ext);
  GUM_TEARDOWN_ATOM (index);
  GUM_TEARDOWN_ATOM (shift);
  GUM_TEARDOWN_ATOM (vas);
  GUM_TEARDOWN_ATOM (vectorIndex);
#elif defined (HAVE_MIPS)
  GUM_TEARDOWN_ATOM (disp);
#endif

#undef GUM_TEARDOWN_ATOM
}
