/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2024 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2026 Thanos Petsas <thanpetsas@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_SCRIPT_PRIV_H__
#define __GUM_V8_SCRIPT_PRIV_H__

#include "gumv8api.h"
#include "gumv8apiresolver.h"
#include "gumv8checksum.h"
#include "gumv8cloak.h"
#include "gumv8cmodule.h"
#include "gumv8coderelocator.h"
#include "gumv8codewriter.h"
#include "gumv8controlflowgraph.h"
#include "gumv8core.h"
#include "gumv8file.h"
#include "gumv8instruction.h"
#include "gumv8interceptor.h"
#include "gumv8kernel.h"
#include "gumv8memory.h"
#include "gumv8module.h"
#include "gumv8platform.h"
#include "gumv8process.h"
#include "gumv8profiler.h"
#include "gumv8sampler.h"
#include "gumv8scope.h"
#include "gumv8script.h"
#include "gumv8scriptbackend.h"
#include "gumv8socket.h"
#include "gumv8stalker.h"
#include "gumv8stream.h"
#include "gumv8symbol.h"
#include "gumv8thread.h"
#ifdef HAVE_SQLITE
# include "gumv8database.h"
#endif

#include <v8-inspector.h>

enum GumScriptState
{
  GUM_SCRIPT_STATE_CREATED,
  GUM_SCRIPT_STATE_LOADING,
  GUM_SCRIPT_STATE_LOADED,
  GUM_SCRIPT_STATE_UNLOADING,
  GUM_SCRIPT_STATE_UNLOADED
};

enum GumInterruptState
{
  GUM_INTERRUPT_NONE,
  GUM_INTERRUPT_ONCE,
  GUM_INTERRUPT_FOREVER
};

enum GumV8InspectorState
{
  GUM_V8_RUNNING,
  GUM_V8_DEBUGGING,
  GUM_V8_PAUSED
};

struct GumESProgram;
struct GumESAsset;

class GumInspectorClient;
class GumInspectorChannel;

typedef std::map<guint, std::unique_ptr<GumInspectorChannel>>
    GumInspectorChannelMap;

struct _GumV8Script
{
  GObject parent;

  gchar * name;
  gchar * source;
  GBytes * snapshot;
  v8::StartupData * snapshot_blob;
  v8::StartupData snapshot_blob_storage;
  GMainContext * main_context;
  GumV8ScriptBackend * backend;

  GumScriptState state;
  GRecMutex interrupt_mutex;
  GumInterruptState interrupt;
  gboolean executing;
  GSList * on_unload;
  v8::Isolate * isolate;
  GumV8Core core;
  GumV8Kernel kernel;
  GumV8Memory memory;
  GumV8Module module;
  GumV8Thread thread;
  GumV8Process process;
  GumV8File file;
  GumV8Checksum checksum;
#ifndef G_OS_NONE
  GumV8Stream stream;
  GumV8Socket socket;
#endif
#ifdef HAVE_SQLITE
  GumV8Database database;
#endif
  GumV8Interceptor interceptor;
  GumV8ApiResolver api_resolver;
  GumV8Symbol symbol;
  GumV8CModule cmodule;
  GumV8Instruction instruction;
  GumV8ControlFlowGraph control_flow_graph;
  GumV8CodeWriter code_writer;
  GumV8CodeRelocator code_relocator;
  GumV8Stalker stalker;
  GumV8Cloak cloak;
  GumV8Sampler sampler;
  GumV8Profiler profiler;
  GumV8Api api;

  v8::Global<v8::Context> * context;
  GumESProgram * program;

  GumScriptMessageHandler message_handler;
  gpointer message_handler_data;
  GDestroyNotify message_handler_data_destroy;

  GMutex inspector_mutex;
  GCond inspector_cond;
  volatile GumV8InspectorState inspector_state;
  int context_group_id;

  GumScriptDebugMessageHandler debug_handler;
  gpointer debug_handler_data;
  GDestroyNotify debug_handler_data_destroy;
  GMainContext * debug_handler_context;
  GQueue debug_messages;
  volatile bool flush_scheduled;

  v8_inspector::V8Inspector * inspector;
  GumInspectorClient * inspector_client;
  GumInspectorChannelMap * channels;
};

struct GumESProgram
{
  GPtrArray * entrypoints;

  GumESAsset * runtime;
  GHashTable * es_assets;
  GHashTable * es_modules;

  gchar * global_filename;
  v8::Global<v8::Script> * global_code;
};

struct GumESAsset
{
  gint ref_count;

  const gchar * name;

  gconstpointer data;
  gsize data_size;
  GDestroyNotify data_destroy;

  v8::Global<v8::Module> * module;
};

G_GNUC_INTERNAL v8::MaybeLocal<v8::Module> _gum_v8_script_load_module (
    GumV8Script * self, const gchar * name, const gchar * source);
G_GNUC_INTERNAL void _gum_v8_script_register_source_map (GumV8Script * self,
    const gchar * name, gchar * source_map);
G_GNUC_INTERNAL void _gum_v8_script_handle_termination (GumV8Script * self);

#endif
