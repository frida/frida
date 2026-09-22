/*
 * Copyright (C) 2020-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020-2021 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_QUICK_CORE_H__
#define __GUM_QUICK_CORE_H__

#include "gumquickscript.h"
#include "gumquickscriptbackend-priv.h"

#include <ffi.h>
#include <gum/gumexceptor.h>

#define GUM_QUICK_CORE_ATOM(core, name) \
    core->G_PASTE (atom_for_, name)

#define GUM_QUICK_SCOPE_INIT(core) { core, NULL, }

#ifdef HAVE_WINDOWS
# define GUMJS_SYSTEM_ERROR_FIELD "lastError"
#else
# define GUMJS_SYSTEM_ERROR_FIELD "errno"
#endif

G_BEGIN_DECLS

typedef struct _GumQuickCore GumQuickCore;
typedef struct _GumQuickInterceptor GumQuickInterceptor;
typedef struct _GumQuickStalker GumQuickStalker;
typedef struct _GumQuickScope GumQuickScope;
typedef struct _GumQuickWeakRef GumQuickWeakRef;

typedef struct _GumQuickScheduledCallback GumQuickScheduledCallback;
typedef struct _GumQuickExceptionSink GumQuickExceptionSink;
typedef struct _GumQuickMessageSink GumQuickMessageSink;

typedef struct _GumQuickInt64 GumQuickInt64;
typedef struct _GumQuickUInt64 GumQuickUInt64;
typedef struct _GumQuickNativePointer GumQuickNativePointer;
typedef struct _GumQuickCpuContext GumQuickCpuContext;
typedef guint GumQuickCpuContextAccess;
typedef struct _GumQuickNativeResource GumQuickNativeResource;
typedef struct _GumQuickKernelResource GumQuickKernelResource;
typedef struct _GumQuickNativeCallback GumQuickNativeCallback;

typedef void (* GumQuickWeakNotify) (gpointer data);
typedef void (* GumQuickFlushNotify) (gpointer data);
typedef void (* GumQuickMessageEmitter) (const gchar * message, GBytes * data,
    gpointer user_data);
typedef void (* GumQuickKernelDestroyNotify) (GumAddress data);

struct _GumQuickCore
{
  GumQuickScript * script;
  GumQuickScriptBackend * backend;
  GumESProgram * program;
  GumQuickInterceptor * interceptor;
  GumQuickStalker * stalker;
  GumQuickMessageEmitter message_emitter;
  gpointer message_emitter_data;
  GumScriptScheduler * scheduler;
  GumExceptor * exceptor;
  JSRuntime * rt;
  JSContext * ctx;
  GHashTable * module_data;
  GumQuickScope * current_scope;
  GumThreadId current_owner;
  GumThreadId transaction_released_owner;

  GRecMutex * mutex;
  guint usage_count;
  guint mutex_depth;
  GumQuickFlushNotify flush_notify;
  gpointer flush_data;
  GDestroyNotify flush_data_destroy;

  GMainLoop * event_loop;
  GMutex event_mutex;
  GCond event_cond;
  guint event_count;
  gboolean event_source_available;

  GumQuickExceptionSink * unhandled_exception_sink;
  GumQuickMessageSink * incoming_message_sink;

  JSValue on_global_get;
  JSValue global_receiver;

  GHashTable * weak_callbacks;
  guint next_weak_callback_id;
  JSValue weak_objects;
  JSValue weak_map_ctor;
  JSValue weak_map_get_method;
  JSValue weak_map_set_method;
  JSValue weak_map_delete_method;
  GQueue pending_weak_refs;
  GSource * pending_weak_source;

  GHashTable * scheduled_callbacks;
  guint next_callback_id;

  GHashTable * workers;

  GHashTable * subclasses;

  JSClassID weak_ref_class;
  JSClassID int64_class;
  JSClassID uint64_class;
  JSClassID native_pointer_class;
  JSValue native_pointer_proto;
  JSClassID native_resource_class;
  JSClassID kernel_resource_class;
  JSClassID native_function_class;
  JSClassID system_function_class;
  JSClassID native_callback_class;
  JSClassID callback_context_class;
  JSClassID cpu_context_class;
  JSClassID match_pattern_class;
  JSClassID source_map_class;
  JSValue source_map_ctor;
  JSClassID worker_class;

#define GUM_DECLARE_ATOM(id) \
    JSAtom G_PASTE (atom_for_, id)

  GUM_DECLARE_ATOM (abi);
  GUM_DECLARE_ATOM (access);
  GUM_DECLARE_ATOM (address);
  GUM_DECLARE_ATOM (autoClose);
  GUM_DECLARE_ATOM (base);
  GUM_DECLARE_ATOM (cachedInput);
  GUM_DECLARE_ATOM (cachedOutput);
  GUM_DECLARE_ATOM (context);
  GUM_DECLARE_ATOM (entrypoint);
  GUM_DECLARE_ATOM (exceptions);
  GUM_DECLARE_ATOM (file);
  GUM_DECLARE_ATOM (handle);
  GUM_DECLARE_ATOM (id);
  GUM_DECLARE_ATOM (ip);
  GUM_DECLARE_ATOM (isGlobal);
  GUM_DECLARE_ATOM (length);
  GUM_DECLARE_ATOM (memory);
  GUM_DECLARE_ATOM (message);
  GUM_DECLARE_ATOM (module);
  GUM_DECLARE_ATOM (name);
  GUM_DECLARE_ATOM (nativeContext);
  GUM_DECLARE_ATOM (offset);
  GUM_DECLARE_ATOM (operation);
  GUM_DECLARE_ATOM (parameter);
  GUM_DECLARE_ATOM (path);
  GUM_DECLARE_ATOM (pc);
  GUM_DECLARE_ATOM (port);
  GUM_DECLARE_ATOM (protection);
  GUM_DECLARE_ATOM (prototype);
  GUM_DECLARE_ATOM (read);
  GUM_DECLARE_ATOM (resource);
  GUM_DECLARE_ATOM (routine);
  GUM_DECLARE_ATOM (scheduling);
  GUM_DECLARE_ATOM (section);
  GUM_DECLARE_ATOM (size);
  GUM_DECLARE_ATOM (slot);
  GUM_DECLARE_ATOM (state);
  GUM_DECLARE_ATOM (system_error);
  GUM_DECLARE_ATOM (toolchain);
  GUM_DECLARE_ATOM (traps);
  GUM_DECLARE_ATOM (type);
  GUM_DECLARE_ATOM (value);
  GUM_DECLARE_ATOM (written);

#if defined (HAVE_I386)
  GUM_DECLARE_ATOM (disp);
  GUM_DECLARE_ATOM (index);
  GUM_DECLARE_ATOM (scale);
  GUM_DECLARE_ATOM (segment);
#elif defined (HAVE_ARM)
  GUM_DECLARE_ATOM (disp);
  GUM_DECLARE_ATOM (index);
  GUM_DECLARE_ATOM (scale);
  GUM_DECLARE_ATOM (shift);
  GUM_DECLARE_ATOM (subtracted);
  GUM_DECLARE_ATOM (vectorIndex);
#elif defined (HAVE_ARM64)
  GUM_DECLARE_ATOM (disp);
  GUM_DECLARE_ATOM (ext);
  GUM_DECLARE_ATOM (index);
  GUM_DECLARE_ATOM (shift);
  GUM_DECLARE_ATOM (vas);
  GUM_DECLARE_ATOM (vectorIndex);
#elif defined (HAVE_MIPS)
  GUM_DECLARE_ATOM (disp);
#endif

#undef GUM_DECLARE_ATOM
};

struct _GumQuickScope
{
  GumQuickCore * core;
  GumQuickScope * previous_scope;
  GumThreadId previous_owner;
  guint previous_mutex_depth;
  JSRuntimeThreadState thread_state;

  GQueue tick_callbacks;
  GQueue scheduled_sources;

  gint pending_stalker_level;
  GumStalkerTransformer * pending_stalker_transformer;
  GumEventSink * pending_stalker_sink;
};

struct _GumQuickInt64
{
  gint64 value;
};

struct _GumQuickUInt64
{
  guint64 value;
};

struct _GumQuickNativePointer
{
  gpointer value;
};

struct _GumQuickCpuContext
{
  JSValue wrapper;
  GumCpuContext * handle;
  GumQuickCpuContextAccess access;
  GumCpuContext storage;

  GumQuickCore * core;
};

enum _GumQuickCpuContextAccess
{
  GUM_CPU_CONTEXT_READONLY = 1,
  GUM_CPU_CONTEXT_READWRITE
};

struct _GumQuickNativeResource
{
  GumQuickNativePointer native_pointer;

  gsize size;
  gboolean owns_pages;
  GDestroyNotify notify;
};

struct _GumQuickKernelResource
{
  GumQuickUInt64 u64;

  GumQuickKernelDestroyNotify notify;
};

struct _GumQuickNativeCallback
{
  GumQuickNativePointer native_pointer;

  JSValue wrapper;
  JSValue func;
  ffi_closure * closure;
  ffi_cif cif;
  ffi_type ** atypes;
  GSList * data;

  GumQuickCore * core;
};

G_GNUC_INTERNAL void _gum_quick_core_init (GumQuickCore * self,
    GumQuickScript * script, JSContext * ctx, JSValue ns, GRecMutex * mutex,
    GumESProgram * program, GumQuickInterceptor * interceptor,
    GumQuickStalker * stalker, GumQuickMessageEmitter message_emitter,
    gpointer message_emitter_data, GumScriptScheduler * scheduler);
G_GNUC_INTERNAL gboolean _gum_quick_core_flush (GumQuickCore * self,
    GumQuickFlushNotify flush_notify, gpointer flush_data,
    GDestroyNotify flush_data_destroy);
G_GNUC_INTERNAL void _gum_quick_core_dispose (GumQuickCore * self);
G_GNUC_INTERNAL void _gum_quick_core_finalize (GumQuickCore * self);

G_GNUC_INTERNAL void _gum_quick_core_pin (GumQuickCore * self);
G_GNUC_INTERNAL void _gum_quick_core_unpin (GumQuickCore * self);

G_GNUC_INTERNAL void _gum_quick_core_on_unhandled_exception (
    GumQuickCore * self, JSValue exception);

G_GNUC_INTERNAL void _gum_quick_core_post (GumQuickCore * self,
    const gchar * message, GBytes * data);

G_GNUC_INTERNAL void _gum_quick_core_push_job (GumQuickCore * self,
    GumScriptJobFunc job_func, gpointer data, GDestroyNotify data_destroy);

G_GNUC_INTERNAL void _gum_quick_core_store_module_data (GumQuickCore * self,
    const gchar * key, gpointer value);
G_GNUC_INTERNAL gpointer _gum_quick_core_load_module_data (GumQuickCore * self,
    const gchar * key);

G_GNUC_INTERNAL void _gum_quick_scope_enter (GumQuickScope * self,
    GumQuickCore * core);
G_GNUC_INTERNAL void _gum_quick_scope_suspend (GumQuickScope * self);
G_GNUC_INTERNAL void _gum_quick_scope_resume (GumQuickScope * self);
G_GNUC_INTERNAL JSValue _gum_quick_scope_call (GumQuickScope * self,
    JSValueConst func_obj, JSValueConst this_obj, int argc,
    JSValueConst * argv);
G_GNUC_INTERNAL gboolean _gum_quick_scope_call_void (GumQuickScope * self,
    JSValueConst func_obj, JSValueConst this_obj, int argc,
    JSValueConst * argv);
G_GNUC_INTERNAL void _gum_quick_scope_catch_and_emit (GumQuickScope * self);
G_GNUC_INTERNAL void _gum_quick_scope_perform_pending_io (GumQuickScope * self);
G_GNUC_INTERNAL void _gum_quick_scope_leave (GumQuickScope * self);

G_END_DECLS

#endif
