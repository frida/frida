/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2017 Antonio Ken Iannillo <ak.iannillo@gmail.com>
 * Copyright (C) 2019 John Coates <john@johncoates.dev>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2024 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024 Alex Soler <asoler@nowsecure.com>
 * Copyright (C) 2024 Sai Cao <1665673333@qq.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "gumarm64reader.h"
#include "gumarm64relocator.h"
#include "gumarm64writer.h"
#include "gumexceptor.h"
#include "gummemory.h"
#include "gummetalhash.h"
#include "gumspinlock.h"
#include "gumstalker-priv.h"
#include "gumunwindbroker.h"

#ifdef HAVE_PTRAUTH
# include <ptrauth.h>
#endif
#ifdef HAVE_LINUX
# include "gum-init.h"
# include "guminterceptor.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LINUX
# include <unwind.h>
# include <sys/syscall.h>
#endif

#if defined (HAVE_DARWIN) || defined (HAVE_LINUX)
# define GUM_HAVE_UNWIND_PC_TRANSLATION 1
#endif

#define GUM_CODE_SLAB_SIZE_INITIAL  (128 * 1024)
#define GUM_CODE_SLAB_SIZE_DYNAMIC  (4 * 1024 * 1024)
#define GUM_SLOW_SLAB_SIZE_INITIAL  (128 * 1024)
#define GUM_SLOW_SLAB_SIZE_DYNAMIC  (4 * 1024 * 1024)
#define GUM_DATA_SLAB_SIZE_INITIAL  (GUM_CODE_SLAB_SIZE_INITIAL / 5)
#define GUM_DATA_SLAB_SIZE_DYNAMIC  (GUM_CODE_SLAB_SIZE_DYNAMIC / 5)
#define GUM_SCRATCH_SLAB_SIZE       16384
#define GUM_EXEC_BLOCK_MIN_CAPACITY 2048
#define GUM_DATA_BLOCK_MIN_CAPACITY (sizeof (GumExecBlock) + 1024)

#define GUM_STACK_ALIGNMENT                16
#define GUM_INVALIDATE_TRAMPOLINE_MAX_SIZE 40
#define GUM_RESTORATION_PROLOG_SIZE        4
#define GUM_EXCLUSIVE_ACCESS_MAX_DEPTH     8

#if defined (__LP64__) || defined (_WIN64)
# define GUM_IC_MAGIC_EMPTY         G_GUINT64_CONSTANT (0xbaadd00ddeadface)
#else
# define GUM_IC_MAGIC_EMPTY         0xbaadd00dU
#endif

#define GUM_INSTRUCTION_OFFSET_NONE (-1)

#define GUM_STALKER_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_STALKER_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

typedef struct _GumInfectContext GumInfectContext;
typedef struct _GumDisinfectContext GumDisinfectContext;
typedef struct _GumActivation GumActivation;
typedef struct _GumInvalidateContext GumInvalidateContext;
typedef struct _GumCallProbe GumCallProbe;

typedef struct _GumExecCtx GumExecCtx;
typedef void (* GumExecHelperWriteFunc) (GumExecCtx * ctx, GumArm64Writer * cw);
typedef struct _GumExecBlock GumExecBlock;
typedef guint GumExecBlockFlags;
typedef gpointer (* GumExecCtxReplaceCurrentBlockFunc) (
    GumExecBlock * block, gpointer start_address, gpointer from_insn);

typedef struct _GumCodeSlab GumCodeSlab;
typedef struct _GumSlowSlab GumSlowSlab;
typedef struct _GumDataSlab GumDataSlab;
typedef struct _GumSlab GumSlab;

typedef guint GumPrologType;
typedef guint GumCodeContext;
typedef struct _GumGeneratorContext GumGeneratorContext;
typedef struct _GumCalloutEntry GumCalloutEntry;
typedef struct _GumInstruction GumInstruction;
typedef struct _GumBranchTarget GumBranchTarget;
typedef struct _GumIcEntry GumIcEntry;

typedef guint GumVirtualizationRequirements;
typedef guint GumBackpatchType;

typedef struct _GumBackpatchCall GumBackpatchCall;
typedef struct _GumBackpatchJmp GumBackpatchJmp;
typedef struct _GumBackpatchInlineCache GumBackpatchInlineCache;
typedef struct _GumBackpatchExcludedCall GumBackpatchExcludedCall;

#ifdef HAVE_LINUX
typedef struct _Unwind_Exception _Unwind_Exception;
typedef struct _Unwind_Context _Unwind_Context;
struct dwarf_eh_bases;
#endif

enum
{
  PROP_0,
  PROP_IC_ENTRIES,
};

struct _GumStalker
{
  GObject parent;

  guint ic_entries;

  gsize ctx_size;
  gsize ctx_header_size;

  goffset thunks_offset;
  gsize thunks_size;

  goffset code_slab_offset;
  gsize code_slab_size_initial;
  gsize code_slab_size_dynamic;

  /*
   * The instrumented code which Stalker generates is split into two parts.
   * There is the part which is always run (the fast path) and the part which
   * is run only when attempting to find the next block and call the backpatcher
   * (the slow path). Backpatching is applied to the fast path so that
   * subsequent executions no longer need to transit the slow path.
   *
   * By separating the code in this way, we can improve the locality of the code
   * executing in the fast path. This has a performance benefit as well as
   * making the backpatched code much easier to read when working in the
   * debugger.
   *
   * The slow path makes use of its own slab and its own code writer.
   */
  goffset slow_slab_offset;
  gsize slow_slab_size_initial;
  gsize slow_slab_size_dynamic;

  goffset data_slab_offset;
  gsize data_slab_size_initial;
  gsize data_slab_size_dynamic;

  goffset scratch_slab_offset;
  gsize scratch_slab_size;

  gsize page_size;
  GumCpuFeatures cpu_features;
  gboolean is_rwx_supported;

  GMutex mutex;
  GSList * contexts;

  GArray * exclusions;
  gint trust_threshold;
  volatile gboolean any_probes_attached;
  volatile gint last_probe_id;
  GumSpinlock probe_lock;
  GHashTable * probe_target_by_id;
  GHashTable * probe_array_by_address;

  GumExceptor * exceptor;

  GumUnwindBroker * unwind_broker;
};

struct _GumInfectContext
{
  GumStalker * stalker;
  GumStalkerTransformer * transformer;
  GumEventSink * sink;
};

struct _GumDisinfectContext
{
  GumExecCtx * exec_ctx;
  gboolean success;
};

struct _GumActivation
{
  GumExecCtx * ctx;
  gboolean pending;
  gconstpointer target;
};

struct _GumInvalidateContext
{
  GumExecBlock * block;
  gboolean is_executing_target_block;
};

struct _GumCallProbe
{
  gint ref_count;
  GumProbeId id;
  GumCallProbeCallback callback;
  gpointer user_data;
  GDestroyNotify user_notify;
};

struct _GumExecCtx
{
  volatile gint state;
  gint64 destroy_pending_since;

  GumStalker * stalker;
  GumThreadId thread_id;

  GumArm64Writer code_writer;
  GumArm64Writer slow_writer;
  GumArm64Relocator relocator;

  GumStalkerTransformer * transformer;
  void (* transform_block_impl) (GumStalkerTransformer * self,
      GumStalkerIterator * iterator, GumStalkerOutput * output);
  GumEventSink * sink;
  gboolean sink_started;
  GumEventType sink_mask;
  void (* sink_process_impl) (GumEventSink * self, const GumEvent * event,
      GumCpuContext * cpu_context);
  GumStalkerObserver * observer;

  gboolean unfollow_called_while_still_following;
  GumExecBlock * current_block;
  gpointer pending_return_location;
  gsize pending_calls;

  gpointer resume_at;
  gpointer return_at;
  gconstpointer activation_target;

  gpointer thunks;
  gpointer infect_thunk;
  GumAddress infect_body;

  GumSpinlock code_lock;
  GumCodeSlab * code_slab;
  GumSlowSlab * slow_slab;
  GumDataSlab * data_slab;
  GumCodeSlab * scratch_slab;
  GumMetalHashTable * mappings;

  gpointer last_prolog_minimal;
  gpointer last_epilog_minimal;
  gpointer last_prolog_full;
  gpointer last_epilog_full;
  gpointer last_invalidator;

  /*
   * GumExecBlocks are attached to a singly linked list when they are generated,
   * this allows us to store other data in the data slab (rather than relying on
   * them being found in there in sequential order).
   */
  GumExecBlock * block_list;

  /*
   * Stalker for AArch64 no longer makes use of a shadow stack for handling
   * CALL/RET instructions, so we instead keep a count of the depth of the stack
   * here when GUM_CALL or GUM_RET events are enabled.
   */
  gsize depth;

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  GumMetalHashTable * excluded_calls;
#endif
};

enum _GumExecCtxState
{
  GUM_EXEC_CTX_ACTIVE,
  GUM_EXEC_CTX_UNFOLLOW_PENDING,
  GUM_EXEC_CTX_DESTROY_PENDING
};

struct _GumExecBlock
{
  /*
   * GumExecBlock instances are held in a singly linked list to allow them to be
   * disposed. This is necessary since other data may also be stored in the data
   * slab (e.g. inline caches) and hence we cannot simply rely on them being
   * contiguous.
   */
  GumExecBlock * next;

  GumExecCtx * ctx;
  GumCodeSlab * code_slab;
  GumSlowSlab * slow_slab;
  GumExecBlock * storage_block;

  guint8 * real_start;
  guint8 * code_start;
  guint8 * slow_start;
  guint real_size;
  guint code_size;
  guint slow_size;
  guint capacity;
  guint last_callout_offset;

  GumExecBlockFlags flags;
  gint recycle_count;

  GumIcEntry * ic_entries;
};

enum _GumExecBlockFlags
{
  GUM_EXEC_BLOCK_ACTIVATION_TARGET     = 1 << 0,
  GUM_EXEC_BLOCK_HAS_EXCLUSIVE_LOAD    = 1 << 1,
  GUM_EXEC_BLOCK_HAS_EXCLUSIVE_STORE   = 1 << 2,
  GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS = 1 << 3,
};

struct _GumSlab
{
  guint8 * data;
  guint offset;
  guint size;
  guint memory_size;
  GumSlab * next;
};

struct _GumCodeSlab
{
  GumSlab slab;

  gpointer invalidator;
  gpointer prolog_minimal;
  gpointer epilog_minimal;
  gpointer prolog_full;
  gpointer epilog_full;
};

struct _GumSlowSlab
{
  GumSlab slab;

  gpointer invalidator;
};

struct _GumDataSlab
{
  GumSlab slab;
};

enum _GumPrologType
{
  GUM_PROLOG_NONE,
  GUM_PROLOG_MINIMAL,
  GUM_PROLOG_FULL
};

enum _GumCodeContext
{
  GUM_CODE_INTERRUPTIBLE,
  GUM_CODE_UNINTERRUPTIBLE
};

struct _GumGeneratorContext
{
  GumInstruction * instruction;
  GumArm64Relocator * relocator;
  GumArm64Writer * code_writer;
  GumArm64Writer * slow_writer;
  gpointer continuation_real_address;
  GumPrologType opened_prolog;
};

struct _GumInstruction
{
  const cs_insn * ci;
  guint8 * start;
  guint8 * end;
};

struct _GumStalkerIterator
{
  GumExecCtx * exec_context;
  GumExecBlock * exec_block;
  GumGeneratorContext * generator_context;

  GumInstruction instruction;
  GumVirtualizationRequirements requirements;
};

struct _GumCalloutEntry
{
  GumStalkerCallout callout;
  gpointer data;
  GDestroyNotify data_destroy;

  gpointer pc;

  GumExecCtx * exec_context;

  GumCalloutEntry * next;
};

struct _GumBranchTarget
{
  gpointer origin_ip;

  gpointer absolute_address;
  gssize relative_offset;

  arm64_reg reg;
};

struct _GumIcEntry
{
  gpointer real_start;
  gpointer code_start;
};

enum _GumVirtualizationRequirements
{
  GUM_REQUIRE_NOTHING          = 0,
  GUM_REQUIRE_RELOCATION       = 1 << 0,
};

enum _GumBackpatchType
{
  GUM_BACKPATCH_CALL,
  GUM_BACKPATCH_JMP,
  GUM_BACKPATCH_INLINE_CACHE,
  /*
   * On AArch64, immediate branches have limited range, and therefore indirect
   * branches are common. We therefore need to check dynamically whether these
   * are to excluded ranges to avoid stalking large amounts of code
   * unnecessarily.
   *
   * However, calling gum_stalker_is_excluding() repeatedly whenever an indirect
   * call is encountered would be expensive since it would be necessary to open
   * and close a prolog to preserve the register state. We therefore backpatch
   * any excluded calls into the same inline cache used for translating real
   * addresses into their instrumented blocks. We do this by setting the real
   * and instrumented addresses the same.
   *
   * However, since all instructions in AArch64 are 32-bits in length and 32-bit
   * aligned, we use the low bit of the instrumented address as a marker that
   * the call is to an excluded range, and we can therefore handle it
   * accordingly.
   *
   * Note, however, that unlike when we do something similar to handle returns
   * into the slab, we are dealing with real rather than instrumented addresses
   * for our excluded calls. Since the forkserver and it's child both share the
   * same address space, we can be certain that these real addresses will be the
   * same. Therefore unlike returns into the slab, these can also be prefetched.
   */
  GUM_BACKPATCH_EXCLUDED_CALL,
};

struct _GumBackpatchCall
{
  gsize code_offset;
  GumPrologType opened_prolog;
  gpointer ret_real_address;
};

struct _GumBackpatchJmp
{
  gsize code_offset;
  GumPrologType opened_prolog;
};

struct _GumBackpatchInlineCache
{
  guint8 dummy;
};

struct _GumBackpatchExcludedCall
{
  guint8 dummy;
};

struct _GumBackpatch
{
  GumBackpatchType type;
  gpointer to;
  gpointer from;
  gpointer from_insn;

  union
  {
    GumBackpatchCall call;
    GumBackpatchJmp jmp;
    GumBackpatchInlineCache inline_cache;
    GumBackpatchExcludedCall excluded_call;
  };
};

static void gum_stalker_unwind_translator_iface_init (gpointer iface,
    gpointer iface_data);
static void gum_stalker_dispose (GObject * object);
static void gum_stalker_finalize (GObject * object);
static void gum_stalker_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gum_stalker_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);

G_GNUC_INTERNAL gpointer _gum_stalker_do_follow_me (GumStalker * self,
    GumStalkerTransformer * transformer, GumEventSink * sink,
    gpointer ret_addr);
static void gum_stalker_infect (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
static void gum_stalker_disinfect (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
G_GNUC_INTERNAL gpointer _gum_stalker_do_activate (GumStalker * self,
    gconstpointer target, gpointer ret_addr);
G_GNUC_INTERNAL gpointer _gum_stalker_do_deactivate (GumStalker * self,
    gpointer ret_addr);
static gboolean gum_stalker_do_invalidate (GumExecCtx * ctx,
    gconstpointer address, GumActivation * activation);
static void gum_stalker_try_invalidate_block_owned_by_thread (
    GumThreadId thread_id, GumCpuContext * cpu_context, gpointer user_data);

static GumCallProbe * gum_call_probe_ref (GumCallProbe * probe);
static void gum_call_probe_unref (GumCallProbe * probe);

static GumExecCtx * gum_stalker_create_exec_ctx (GumStalker * self,
    GumThreadId thread_id, GumStalkerTransformer * transformer,
    GumEventSink * sink);
static void gum_stalker_destroy_exec_ctx (GumStalker * self, GumExecCtx * ctx);
static GumExecCtx * gum_stalker_get_exec_ctx (void);
static GumExecCtx * gum_stalker_find_exec_ctx_by_thread_id (GumStalker * self,
    GumThreadId thread_id);

static gsize gum_stalker_snapshot_space_needed_for (GumStalker * self,
    gsize real_size);
static gsize gum_stalker_get_ic_entry_size (GumStalker * self);

static void gum_stalker_thaw (GumStalker * self, gpointer code, gsize size);
static void gum_stalker_freeze (GumStalker * self, gpointer code, gsize size);

static GumAddress gum_stalker_translate_unwind_pc (
    GumUnwindPcTranslator * translator, GumAddress code_address);
static gboolean gum_stalker_install_unwind_resume_context (
    GumUnwindPcTranslator * translator, gpointer unwind_context,
    GumAddress real_resume_ip);
static gboolean gum_stalker_on_exception (GumExceptionDetails * details,
    gpointer user_data);

static GumExecCtx * gum_exec_ctx_new (GumStalker * self, GumThreadId thread_id,
    GumStalkerTransformer * transformer, GumEventSink * sink);
static void gum_exec_ctx_free (GumExecCtx * ctx);
static void gum_exec_ctx_dispose (GumExecCtx * ctx);
static GumCodeSlab * gum_exec_ctx_add_code_slab (GumExecCtx * ctx,
    GumCodeSlab * code_slab);
static GumSlowSlab * gum_exec_ctx_add_slow_slab (GumExecCtx * ctx,
    GumSlowSlab * code_slab);
static GumDataSlab * gum_exec_ctx_add_data_slab (GumExecCtx * ctx,
    GumDataSlab * data_slab);
static gboolean gum_exec_ctx_maybe_unfollow (GumExecCtx * ctx,
    gpointer resume_at);
static void gum_exec_ctx_unfollow (GumExecCtx * ctx, gpointer resume_at);
static gboolean gum_exec_ctx_has_executed (GumExecCtx * ctx);
static gboolean gum_exec_ctx_contains (GumExecCtx * ctx, gconstpointer address);
static gpointer gum_exec_ctx_switch_block (GumExecCtx * ctx,
    GumExecBlock * block, gpointer start_address, gpointer from_insn);
static void gum_exec_ctx_query_block_switch_callback (GumExecCtx * ctx,
    GumExecBlock * block, gpointer start_address, gpointer from_insn,
    gpointer * target);

static GumExecBlock * gum_exec_ctx_obtain_block_for (GumExecCtx * ctx,
    gpointer real_address, gpointer * code_address);
static void gum_exec_ctx_recompile_block (GumExecCtx * ctx,
    GumExecBlock * block);
static void gum_exec_ctx_write_scratch_slab (GumExecCtx * ctx,
    GumExecBlock * block, guint * input_size, guint * output_size,
    guint * slow_size);
static void gum_exec_ctx_compile_block (GumExecCtx * ctx, GumExecBlock * block,
    gconstpointer input_code, gpointer output_code, GumAddress output_pc,
    guint * input_size, guint * output_size, guint * slow_size);
static void gum_exec_ctx_maybe_emit_compile_event (GumExecCtx * ctx,
    GumExecBlock * block);

static gboolean gum_stalker_iterator_is_out_of_space (
    GumStalkerIterator * self);

static void gum_stalker_invoke_callout (GumCalloutEntry * entry,
    GumCpuContext * cpu_context);

static void gum_exec_ctx_write_prolog (GumExecCtx * ctx, GumPrologType type,
    GumArm64Writer * cw);
static void gum_exec_ctx_write_epilog_with_helper (gpointer helper,
    GumArm64Writer * cw);
static void gum_exec_ctx_write_epilog (GumExecCtx * ctx, GumPrologType type,
    GumArm64Writer * cw);

static void gum_exec_ctx_ensure_inline_helpers_reachable (GumExecCtx * ctx);
static void gum_exec_ctx_write_minimal_prolog_helper (GumExecCtx * ctx,
    GumArm64Writer * cw);
static void gum_exec_ctx_write_minimal_epilog_helper (GumExecCtx * ctx,
    GumArm64Writer * cw);
static void gum_exec_ctx_write_full_prolog_helper (GumExecCtx * ctx,
    GumArm64Writer * cw);
static void gum_exec_ctx_write_full_epilog_helper (GumExecCtx * ctx,
    GumArm64Writer * cw);
static void gum_exec_ctx_write_prolog_helper (GumExecCtx * ctx,
    GumPrologType type, GumArm64Writer * cw);
static void gum_exec_ctx_write_epilog_helper (GumExecCtx * ctx,
    GumPrologType type, GumArm64Writer * cw);
static void gum_exec_ctx_write_invalidator (GumExecCtx * ctx,
    GumArm64Writer * cw);
static void gum_exec_ctx_ensure_helper_reachable (GumExecCtx * ctx,
    GumSlab * code_slab, GumSlab * slow_slab, GumArm64Writer * cw,
    gpointer * helper_ptr, GumExecHelperWriteFunc write);
static gboolean gum_exec_ctx_is_helper_reachable (GumExecCtx * ctx,
    GumSlab * slab, GumArm64Writer * cw, gpointer * helper_ptr);

static void gum_exec_ctx_write_push_branch_target_address (GumExecCtx * ctx,
    const GumBranchTarget * target, GumGeneratorContext * gc,
    GumArm64Writer * cw);
static void gum_exec_ctx_load_real_register_into (GumExecCtx * ctx,
    arm64_reg target_register, arm64_reg source_register,
    GumGeneratorContext * gc, GumArm64Writer * cw);
static void gum_exec_ctx_load_real_register_from_minimal_frame_into (
    GumExecCtx * ctx, arm64_reg target_register, arm64_reg source_register,
    GumGeneratorContext * gc, GumArm64Writer * cw);
static void gum_exec_ctx_load_real_register_from_full_frame_into (
    GumExecCtx * ctx, arm64_reg target_register, arm64_reg source_register,
    GumGeneratorContext * gc, GumArm64Writer * cw);

static gboolean gum_exec_ctx_try_handle_exception (GumExecCtx * ctx,
    GumExceptionDetails * details);
static void gum_exec_ctx_handle_stp (GumCpuContext * cpu_context,
    arm64_reg reg1, arm64_reg reg2, gsize offset);
static void gum_exec_ctx_handle_ldp (GumCpuContext * cpu_context,
    arm64_reg reg1, arm64_reg reg2, gsize offset);
static guint64 gum_exec_ctx_read_register (GumCpuContext * cpu_context,
    arm64_reg reg);
static void gum_exec_ctx_write_register (GumCpuContext * cpu_context,
    arm64_reg reg, guint64 value);

static GumExecBlock * gum_exec_block_new (GumExecCtx * ctx);
static void gum_exec_block_maybe_create_new_code_slabs (GumExecCtx * ctx);
static void gum_exec_block_maybe_create_new_data_slab (GumExecCtx * ctx);
static void gum_exec_block_clear (GumExecBlock * block);
static gconstpointer gum_exec_block_check_address_for_exclusion (
    GumExecBlock * block, gconstpointer address);
static void gum_exec_block_commit (GumExecBlock * block);
static void gum_exec_block_invalidate (GumExecBlock * block);
static gpointer gum_exec_block_get_snapshot_start (GumExecBlock * block);
static GumCalloutEntry * gum_exec_block_get_last_callout_entry (
    const GumExecBlock * block);
static void gum_exec_block_set_last_callout_entry (GumExecBlock * block,
    GumCalloutEntry * entry);

static void gum_exec_block_write_jmp_to_block_start (GumExecBlock * block,
    gpointer block_start);

static void gum_exec_block_backpatch_call (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn, gsize code_offset,
    GumPrologType opened_prolog, gpointer ret_real_address);
static void gum_exec_block_backpatch_jmp (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn, gsize code_offset,
    GumPrologType opened_prolog);
static void gum_exec_block_backpatch_inline_cache (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn);

static GumVirtualizationRequirements gum_exec_block_virtualize_branch_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
static GumVirtualizationRequirements gum_exec_block_virtualize_ret_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
static GumVirtualizationRequirements gum_exec_block_virtualize_sysenter_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
#ifdef HAVE_LINUX
static GumVirtualizationRequirements gum_exec_block_virtualize_linux_sysenter (
    GumExecBlock * block, GumGeneratorContext * gc);
static void gum_exec_block_put_aligned_syscall (GumExecBlock * block,
    GumGeneratorContext * gc, const cs_insn * insn);
#endif

static void gum_exec_block_write_call_invoke_code (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_ctx_write_begin_call (GumExecCtx * ctx,
    GumArm64Writer * cw, gpointer ret_addr);
static void gum_exec_ctx_write_end_call (GumExecCtx * ctx, GumArm64Writer * cw);
static void gum_exec_block_backpatch_excluded_call (GumExecBlock * block,
    gpointer target, gpointer from_insn);
static void gum_exec_block_write_jmp_transfer_code (GumExecBlock * block,
    const GumBranchTarget * target, GumExecCtxReplaceCurrentBlockFunc func,
    GumGeneratorContext * gc);
static void gum_exec_block_write_ret_transfer_code (GumExecBlock * block,
    GumGeneratorContext * gc, arm64_reg ret_reg);
static void gum_exec_block_write_chaining_return_code (GumExecBlock * block,
    GumGeneratorContext * gc, arm64_reg ret_reg);
static void gum_exec_block_write_slab_transfer_code (GumArm64Writer * from,
    GumArm64Writer * to);
static void gum_exec_block_backpatch_slab (GumExecBlock * block,
    gpointer target);
static void gum_exec_block_maybe_inherit_exclusive_access_state (
    GumExecBlock * block, GumExecBlock * reference);
static void gum_exec_block_propagate_exclusive_access_state (
    GumExecBlock * block);
static void gum_exec_ctx_write_adjust_depth (GumExecCtx * ctx,
    GumArm64Writer * cw, gssize adj);
static arm64_reg gum_exec_block_write_inline_cache_code (
    GumExecBlock * block, arm64_reg target_reg, GumArm64Writer * cw,
    GumArm64Writer * cws);

static void gum_exec_block_write_call_event_code (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc,
    GumCodeContext cc);
static void gum_exec_block_write_ret_event_code (GumExecBlock * block,
    GumGeneratorContext * gc, GumCodeContext cc);
static void gum_exec_block_write_exec_event_code (GumExecBlock * block,
    GumGeneratorContext * gc, GumCodeContext cc);
static void gum_exec_block_write_block_event_code (GumExecBlock * block,
    GumGeneratorContext * gc, GumCodeContext cc);
static void gum_exec_block_write_unfollow_check_code (GumExecBlock * block,
    GumGeneratorContext * gc, GumCodeContext cc);

static void gum_exec_block_maybe_write_call_probe_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_call_probe_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_invoke_call_probes (GumExecBlock * block,
    GumCpuContext * cpu_context);

static void gum_exec_block_write_exec_generated_code (GumArm64Writer * cw,
    GumExecCtx * ctx);

static gpointer gum_exec_block_write_inline_data (GumArm64Writer * cw,
    gconstpointer data, gsize size, GumAddress * address);

static void gum_exec_block_open_prolog (GumExecBlock * block,
    GumPrologType type, GumGeneratorContext * gc, GumArm64Writer * cw);
static void gum_exec_block_close_prolog (GumExecBlock * block,
    GumGeneratorContext * gc, GumArm64Writer * cw);

static GumCodeSlab * gum_code_slab_new (GumExecCtx * ctx);
static void gum_code_slab_free (GumCodeSlab * code_slab);
static void gum_code_slab_init (GumCodeSlab * code_slab, gsize slab_size,
    gsize memory_size, gsize page_size);
static void gum_slow_slab_init (GumSlowSlab * slow_slab, gsize slab_size,
    gsize memory_size, gsize page_size);

static GumDataSlab * gum_data_slab_new (GumExecCtx * ctx);
static void gum_data_slab_free (GumDataSlab * data_slab);
static void gum_data_slab_init (GumDataSlab * data_slab, gsize slab_size,
    gsize memory_size);

static void gum_scratch_slab_init (GumCodeSlab * scratch_slab, gsize slab_size);

static void gum_slab_free (GumSlab * slab);
static void gum_slab_init (GumSlab * slab, gsize slab_size, gsize memory_size,
    gsize header_size);
static gsize gum_slab_available (GumSlab * self);
static gpointer gum_slab_start (GumSlab * self);
static gpointer gum_slab_end (GumSlab * self);
static gpointer gum_slab_cursor (GumSlab * self);
static gpointer gum_slab_reserve (GumSlab * self, gsize size);
static gpointer gum_slab_try_reserve (GumSlab * self, gsize size);

static gpointer gum_find_thread_exit_implementation (void);

G_DEFINE_TYPE_WITH_CODE (GumStalker, gum_stalker, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GUM_TYPE_UNWIND_PC_TRANSLATOR,
                             gum_stalker_unwind_translator_iface_init))

static GPrivate gum_stalker_exec_ctx_private;

static gpointer gum_unfollow_me_address;
static gpointer gum_deactivate_address;
static gpointer gum_thread_exit_address;

gboolean
gum_stalker_is_supported (void)
{
  return TRUE;
}

static void
gum_stalker_class_init (GumStalkerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_stalker_dispose;
  object_class->finalize = gum_stalker_finalize;
  object_class->get_property = gum_stalker_get_property;
  object_class->set_property = gum_stalker_set_property;

  g_object_class_install_property (object_class, PROP_IC_ENTRIES,
      g_param_spec_uint ("ic-entries", "IC Entries", "Inline Cache Entries",
      2, 32, 2, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS));

  gum_unfollow_me_address = gum_strip_code_pointer (gum_stalker_unfollow_me);
  gum_deactivate_address = gum_strip_code_pointer (gum_stalker_deactivate);
  gum_thread_exit_address = gum_find_thread_exit_implementation ();
}

static void
gum_stalker_unwind_translator_iface_init (gpointer iface,
                                          gpointer iface_data)
{
  GumUnwindPcTranslatorInterface * pc_iface = iface;

  pc_iface->translate = gum_stalker_translate_unwind_pc;
  pc_iface->install_resume_context = gum_stalker_install_unwind_resume_context;
}

static void
gum_stalker_init (GumStalker * self)
{
  gsize page_size;

  self->exclusions = g_array_new (FALSE, FALSE, sizeof (GumMemoryRange));
  self->trust_threshold = 1;

  gum_spinlock_init (&self->probe_lock);
  self->probe_target_by_id = g_hash_table_new_full (NULL, NULL, NULL, NULL);
  self->probe_array_by_address = g_hash_table_new_full (NULL, NULL, NULL,
      (GDestroyNotify) g_ptr_array_unref);

  page_size = gum_query_page_size ();

  self->thunks_size = page_size;
  self->code_slab_size_initial =
      GUM_ALIGN_SIZE (GUM_CODE_SLAB_SIZE_INITIAL, page_size);
  self->slow_slab_size_initial =
      GUM_ALIGN_SIZE (GUM_SLOW_SLAB_SIZE_INITIAL, page_size);
  self->data_slab_size_initial =
      GUM_ALIGN_SIZE (GUM_DATA_SLAB_SIZE_INITIAL, page_size);
  self->code_slab_size_dynamic =
      GUM_ALIGN_SIZE (GUM_CODE_SLAB_SIZE_DYNAMIC, page_size);
  self->slow_slab_size_dynamic =
      GUM_ALIGN_SIZE (GUM_SLOW_SLAB_SIZE_DYNAMIC, page_size);
  self->data_slab_size_dynamic =
      GUM_ALIGN_SIZE (GUM_DATA_SLAB_SIZE_DYNAMIC, page_size);
  self->scratch_slab_size = GUM_ALIGN_SIZE (GUM_SCRATCH_SLAB_SIZE, page_size);
  self->ctx_header_size = GUM_ALIGN_SIZE (sizeof (GumExecCtx), page_size);
  self->ctx_size =
      self->ctx_header_size +
      self->thunks_size +
      self->code_slab_size_initial +
      self->slow_slab_size_initial +
      self->data_slab_size_initial +
      self->scratch_slab_size +
      0;

  self->thunks_offset = self->ctx_header_size;
  self->code_slab_offset = self->thunks_offset + self->thunks_size;
  self->slow_slab_offset =
      self->code_slab_offset + self->code_slab_size_initial;
  self->data_slab_offset =
      self->slow_slab_offset + self->slow_slab_size_initial;
  self->scratch_slab_offset =
      self->data_slab_offset + self->data_slab_size_initial;

  self->page_size = page_size;
  self->cpu_features = gum_query_cpu_features ();
  self->is_rwx_supported = gum_query_rwx_support () != GUM_RWX_NONE;

  g_mutex_init (&self->mutex);
  self->contexts = NULL;

  self->exceptor = gum_exceptor_obtain ();
  gum_exceptor_add (self->exceptor, gum_stalker_on_exception, self);

  self->unwind_broker = gum_unwind_broker_obtain ();
  gum_unwind_broker_add_pc_translator (self->unwind_broker,
      GUM_UNWIND_PC_TRANSLATOR (self));
}

static void
gum_stalker_dispose (GObject * object)
{
  GumStalker * self = GUM_STALKER (object);

  if (self->unwind_broker != NULL)
  {
    gum_unwind_broker_remove_pc_translator (self->unwind_broker,
        GUM_UNWIND_PC_TRANSLATOR (self));
    g_object_unref (self->unwind_broker);
    self->unwind_broker = NULL;
  }

  if (self->exceptor != NULL)
  {
    gum_exceptor_remove (self->exceptor, gum_stalker_on_exception, self);
    g_object_unref (self->exceptor);
    self->exceptor = NULL;
  }

  G_OBJECT_CLASS (gum_stalker_parent_class)->dispose (object);
}

static void
gum_stalker_finalize (GObject * object)
{
  GumStalker * self = GUM_STALKER (object);

  g_hash_table_unref (self->probe_array_by_address);
  g_hash_table_unref (self->probe_target_by_id);

  g_array_free (self->exclusions, TRUE);

  g_assert (self->contexts == NULL);
  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_stalker_parent_class)->finalize (object);
}

static void
gum_stalker_get_property (GObject * object,
                          guint property_id,
                          GValue * value,
                          GParamSpec * pspec)
{
  GumStalker * self = GUM_STALKER (object);

  switch (property_id)
  {
    case PROP_IC_ENTRIES:
      g_value_set_uint (value, self->ic_entries);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
gum_stalker_set_property (GObject * object,
                          guint property_id,
                          const GValue * value,
                          GParamSpec * pspec)
{
  GumStalker * self = GUM_STALKER (object);

  switch (property_id)
  {
    case PROP_IC_ENTRIES:
      self->ic_entries = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

GumStalker *
gum_stalker_new (void)
{
  return g_object_new (GUM_TYPE_STALKER, NULL);
}

void
gum_stalker_exclude (GumStalker * self,
                     const GumMemoryRange * range)
{
  g_array_append_val (self->exclusions, *range);
}

static gboolean
gum_stalker_is_excluding (GumStalker * self,
                          gconstpointer address)
{
  GArray * exclusions = self->exclusions;
  guint i;

  for (i = 0; i != exclusions->len; i++)
  {
    GumMemoryRange * r = &g_array_index (exclusions, GumMemoryRange, i);

    if (GUM_MEMORY_RANGE_INCLUDES (r, GUM_ADDRESS (address)))
      return TRUE;
  }

  return FALSE;
}

gint
gum_stalker_get_trust_threshold (GumStalker * self)
{
  return self->trust_threshold;
}

void
gum_stalker_set_trust_threshold (GumStalker * self,
                                 gint trust_threshold)
{
  self->trust_threshold = trust_threshold;
}

void
gum_stalker_flush (GumStalker * self)
{
  GSList * sinks, * cur;

  GUM_STALKER_LOCK (self);

  sinks = NULL;
  for (cur = self->contexts; cur != NULL; cur = cur->next)
  {
    GumExecCtx * ctx = cur->data;

    sinks = g_slist_prepend (sinks, g_object_ref (ctx->sink));
  }

  GUM_STALKER_UNLOCK (self);

  for (cur = sinks; cur != NULL; cur = cur->next)
  {
    GumEventSink * sink = cur->data;

    gum_event_sink_flush (sink);
  }

  g_slist_free_full (sinks, g_object_unref);
}

void
gum_stalker_stop (GumStalker * self)
{
  GSList * cur;

  gum_spinlock_acquire (&self->probe_lock);
  g_hash_table_remove_all (self->probe_target_by_id);
  g_hash_table_remove_all (self->probe_array_by_address);
  self->any_probes_attached = FALSE;
  gum_spinlock_release (&self->probe_lock);

rescan:
  GUM_STALKER_LOCK (self);

  for (cur = self->contexts; cur != NULL; cur = cur->next)
  {
    GumExecCtx * ctx = cur->data;

    if (g_atomic_int_get (&ctx->state) == GUM_EXEC_CTX_ACTIVE)
    {
      GumThreadId thread_id = ctx->thread_id;

      GUM_STALKER_UNLOCK (self);

      gum_stalker_unfollow (self, thread_id);

      goto rescan;
    }
  }

  GUM_STALKER_UNLOCK (self);

  gum_stalker_garbage_collect (self);
}

gboolean
gum_stalker_garbage_collect (GumStalker * self)
{
  gboolean have_pending_garbage;
  GumThreadId current_thread_id;
  gint64 now;
  GSList * cur;

  current_thread_id = gum_process_get_current_thread_id ();
  now = g_get_monotonic_time ();

rescan:
  GUM_STALKER_LOCK (self);

  for (cur = self->contexts; cur != NULL; cur = cur->next)
  {
    GumExecCtx * ctx = cur->data;
    gboolean destroy_pending_and_thread_likely_back_in_original_code;

    destroy_pending_and_thread_likely_back_in_original_code =
        g_atomic_int_get (&ctx->state) == GUM_EXEC_CTX_DESTROY_PENDING &&
        (ctx->thread_id == current_thread_id ||
        now - ctx->destroy_pending_since > 20000);

    if (destroy_pending_and_thread_likely_back_in_original_code ||
        !gum_process_has_thread (ctx->thread_id))
    {
      GUM_STALKER_UNLOCK (self);

      gum_stalker_destroy_exec_ctx (self, ctx);

      goto rescan;
    }
  }

  have_pending_garbage = self->contexts != NULL;

  GUM_STALKER_UNLOCK (self);

  return have_pending_garbage;
}

gpointer
_gum_stalker_do_follow_me (GumStalker * self,
                           GumStalkerTransformer * transformer,
                           GumEventSink * sink,
                           gpointer ret_addr)
{
  GumExecCtx * ctx;
  gpointer code_address;

  ctx = gum_stalker_create_exec_ctx (self, gum_process_get_current_thread_id (),
      transformer, sink);
  g_private_set (&gum_stalker_exec_ctx_private, ctx);

  ctx->current_block = gum_exec_ctx_obtain_block_for (ctx, ret_addr,
      &code_address);

  if (gum_exec_ctx_maybe_unfollow (ctx, ret_addr))
  {
    gum_stalker_destroy_exec_ctx (self, ctx);
    return ret_addr;
  }

  gum_event_sink_start (ctx->sink);
  ctx->sink_started = TRUE;

  return (guint8 *) code_address + GUM_RESTORATION_PROLOG_SIZE;
}

void
gum_stalker_unfollow_me (GumStalker * self)
{
  GumExecCtx * ctx;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return;

  g_atomic_int_set (&ctx->state, GUM_EXEC_CTX_UNFOLLOW_PENDING);

  if (!gum_exec_ctx_maybe_unfollow (ctx, NULL))
    return;

  g_assert (ctx->unfollow_called_while_still_following);

  gum_stalker_destroy_exec_ctx (self, ctx);
}

gboolean
gum_stalker_is_following_me (GumStalker * self)
{
  return gum_stalker_get_exec_ctx () != NULL;
}

void
gum_stalker_follow (GumStalker * self,
                    GumThreadId thread_id,
                    GumStalkerTransformer * transformer,
                    GumEventSink * sink)
{
  if (thread_id == gum_process_get_current_thread_id ())
  {
    gum_stalker_follow_me (self, transformer, sink);
  }
  else
  {
    GumInfectContext ctx;

    ctx.stalker = self;
    ctx.transformer = transformer;
    ctx.sink = sink;

    gum_process_modify_thread (thread_id, gum_stalker_infect, &ctx,
        GUM_MODIFY_THREAD_FLAGS_NONE);
  }
}

void
gum_stalker_unfollow (GumStalker * self,
                      GumThreadId thread_id)
{
  if (thread_id == gum_process_get_current_thread_id ())
  {
    gum_stalker_unfollow_me (self);
  }
  else
  {
    GumExecCtx * ctx;

    ctx = gum_stalker_find_exec_ctx_by_thread_id (self, thread_id);
    if (ctx == NULL)
      return;

    if (!g_atomic_int_compare_and_exchange (&ctx->state, GUM_EXEC_CTX_ACTIVE,
        GUM_EXEC_CTX_UNFOLLOW_PENDING))
      return;

    if (!gum_exec_ctx_has_executed (ctx))
    {
      GumDisinfectContext dc;

      dc.exec_ctx = ctx;
      dc.success = FALSE;

      gum_process_modify_thread (thread_id, gum_stalker_disinfect, &dc,
          GUM_MODIFY_THREAD_FLAGS_NONE);

      if (dc.success)
        gum_stalker_destroy_exec_ctx (self, ctx);
    }
  }
}

static void
gum_stalker_infect (GumThreadId thread_id,
                    GumCpuContext * cpu_context,
                    gpointer user_data)
{
  GumInfectContext * infect_context = user_data;
  GumStalker * self = infect_context->stalker;
  GumExecCtx * ctx;
  guint8 * pc;
  gpointer code_address;
  GumArm64Writer * cw;
  const guint potential_svc_size = 4;

  ctx = gum_stalker_create_exec_ctx (self, thread_id,
      infect_context->transformer, infect_context->sink);

  pc = GSIZE_TO_POINTER (gum_strip_code_address (cpu_context->pc));

  ctx->current_block = gum_exec_ctx_obtain_block_for (ctx, pc, &code_address);

  if (gum_exec_ctx_maybe_unfollow (ctx, NULL))
  {
    gum_stalker_destroy_exec_ctx (self, ctx);
    return;
  }

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (self, ctx->thunks, self->thunks_size);
  cw = &ctx->code_writer;
  gum_arm64_writer_reset (cw, ctx->infect_thunk);

  /*
   * In case the thread is in a Linux system call we should allow it to be
   * restarted by bringing along the SVC instruction.
   */
  gum_arm64_writer_put_bytes (cw, pc - potential_svc_size, potential_svc_size);

  ctx->infect_body = GUM_ADDRESS (gum_arm64_writer_cur (cw));
#ifdef HAVE_PTRAUTH
  ctx->infect_body = GPOINTER_TO_SIZE (ptrauth_sign_unauthenticated (
      GSIZE_TO_POINTER (ctx->infect_body), ptrauth_key_process_independent_code,
      ptrauth_string_discriminator ("pc")));
#endif
  gum_exec_ctx_write_prolog (ctx, GUM_PROLOG_MINIMAL, cw);
  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (g_private_set), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (&gum_stalker_exec_ctx_private),
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx));
  gum_exec_ctx_write_epilog (ctx, GUM_PROLOG_MINIMAL, cw);

  gum_arm64_writer_put_b_imm (cw, GUM_ADDRESS (code_address) +
      GUM_RESTORATION_PROLOG_SIZE);

  gum_arm64_writer_flush (cw);
  gum_stalker_freeze (self, cw->base, gum_arm64_writer_offset (cw));

  gum_spinlock_release (&ctx->code_lock);

  gum_event_sink_start (ctx->sink);
  ctx->sink_started = TRUE;

  cpu_context->pc = ctx->infect_body;
}

static void
gum_stalker_disinfect (GumThreadId thread_id,
                       GumCpuContext * cpu_context,
                       gpointer user_data)
{
  GumDisinfectContext * disinfect_context = user_data;
  GumExecCtx * ctx = disinfect_context->exec_ctx;
  gboolean infection_not_active_yet;

  infection_not_active_yet = cpu_context->pc == ctx->infect_body;
  if (infection_not_active_yet)
  {
    cpu_context->pc = gum_sign_code_address (
        GPOINTER_TO_SIZE (ctx->current_block->real_start));

    disinfect_context->success = TRUE;
  }
}

gpointer
_gum_stalker_do_activate (GumStalker * self,
                          gconstpointer target,
                          gpointer ret_addr)
{
  GumExecCtx * ctx;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return ret_addr;

  ctx->unfollow_called_while_still_following = FALSE;
  ctx->activation_target = gum_strip_code_pointer ((gpointer) target);

  if (!gum_exec_ctx_contains (ctx, ret_addr))
  {
    gpointer code_address;

    ctx->current_block =
        gum_exec_ctx_obtain_block_for (ctx, ret_addr, &code_address);

    if (gum_exec_ctx_maybe_unfollow (ctx, ret_addr))
      return ret_addr;

    return (guint8 *) code_address + GUM_RESTORATION_PROLOG_SIZE;
  }

  return ret_addr;
}

gpointer
_gum_stalker_do_deactivate (GumStalker * self,
                            gpointer ret_addr)
{
  GumExecCtx * ctx;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return ret_addr;

  ctx->unfollow_called_while_still_following = TRUE;
  ctx->activation_target = NULL;

  if (gum_exec_ctx_contains (ctx, ret_addr))
  {
    ctx->pending_calls--;

    return ctx->pending_return_location;
  }

  return ret_addr;
}

static void
gum_stalker_maybe_deactivate (GumStalker * self,
                              GumActivation * activation)
{
  GumExecCtx * ctx;

  ctx = gum_stalker_get_exec_ctx ();
  activation->ctx = ctx;

  if (ctx != NULL && ctx->pending_calls == 0)
  {
    activation->pending = TRUE;
    activation->target = ctx->activation_target;

    gum_stalker_deactivate (self);
  }
  else
  {
    activation->pending = FALSE;
    activation->target = NULL;
  }
}

static void
gum_stalker_maybe_reactivate (GumStalker * self,
                              GumActivation * activation)
{
  if (activation->pending)
    gum_stalker_activate (self, activation->target);
}

void
gum_stalker_set_observer (GumStalker * self,
                          GumStalkerObserver * observer)
{
  GumExecCtx * ctx;

  ctx = gum_stalker_get_exec_ctx ();
  g_assert (ctx != NULL);

  if (observer != NULL)
    g_object_ref (observer);
  if (ctx->observer != NULL)
    g_object_unref (ctx->observer);
  ctx->observer = observer;
}

void
gum_stalker_prefetch (GumStalker * self,
                      gconstpointer address,
                      gint recycle_count)
{
  GumExecCtx * ctx;
  GumExecBlock * block;
  gpointer code_address;

  ctx = gum_stalker_get_exec_ctx ();
  g_assert (ctx != NULL);

  block = gum_exec_ctx_obtain_block_for (ctx, (gpointer) address,
      &code_address);
  block->recycle_count = recycle_count;
}

/*
 * This function is intended to be called in the forkserver parent when fuzzing
 * to apply backpatches which have been learnt by the child process (so that we
 * don't lose and have to regenerate those patches each time a new child is
 * spawned). The child will call a function of the Observer providing the opaque
 * GumBackpatch structure containing the necessary information to allow the
 * patch to be re-created in the parent. The mechanics of how these GumBackpatch
 * structures are communictated from the Observer running in the child back to
 * code running in the parent are left to the integrator.
 */
void
gum_stalker_prefetch_backpatch (GumStalker * self,
                                const GumBackpatch * backpatch)
{
  GumExecCtx * ctx;
  GumExecBlock * block_to, * block_from;
  gpointer code_address_to, code_address_from;
  gpointer from_insn = backpatch->from_insn;

  ctx = gum_stalker_get_exec_ctx ();
  g_assert (ctx != NULL);

  block_to = gum_exec_ctx_obtain_block_for (ctx, backpatch->to,
      &code_address_to);
  block_from = gum_exec_ctx_obtain_block_for (ctx, backpatch->from,
      &code_address_from);

  block_to->recycle_count = self->trust_threshold;
  block_from->recycle_count = self->trust_threshold;

  switch (backpatch->type)
  {
    case GUM_BACKPATCH_CALL:
    {
      const GumBackpatchCall * call = &backpatch->call;
      gum_exec_block_backpatch_call (block_to, block_from, from_insn,
          call->code_offset, call->opened_prolog, call->ret_real_address);
      break;
    }
    case GUM_BACKPATCH_JMP:
    {
      const GumBackpatchJmp * jmp = &backpatch->jmp;
      gum_exec_block_backpatch_jmp (block_to, block_from, from_insn,
          jmp->code_offset, jmp->opened_prolog);
      break;
    }
    case GUM_BACKPATCH_INLINE_CACHE:
    {
      gum_exec_block_backpatch_inline_cache (block_to, block_from, from_insn);
      break;
    }
    case GUM_BACKPATCH_EXCLUDED_CALL:
    {
      /*
       * Note that for excluded calls we don't have a target block as our
       * destination. We don't compile a GumExecBlock for an excluded range, but
       * rather allow the target to execute the original real code instead. Thus
       * the arguments here are little asymmetric to those above.
       */
      gum_exec_block_backpatch_excluded_call (block_from, backpatch->to,
          from_insn);
    }
    default:
      g_assert_not_reached ();
      break;
  }
}

void
gum_stalker_recompile (GumStalker * self,
                       gconstpointer address)
{
  GumExecCtx * ctx;
  GumExecBlock * block;

  ctx = gum_stalker_get_exec_ctx ();
  g_assert (ctx != NULL);

  block = gum_metal_hash_table_lookup (ctx->mappings, address);
  if (block == NULL)
    return;

  gum_exec_ctx_recompile_block (ctx, block);
}

gpointer
gum_stalker_backpatch_get_from (const GumBackpatch * backpatch)
{
  return backpatch->from;
}

gpointer
gum_stalker_backpatch_get_to (const GumBackpatch * backpatch)
{
  return backpatch->to;
}

void
gum_stalker_invalidate (GumStalker * self,
                        gconstpointer address)
{
  GumActivation activation;

  gum_stalker_maybe_deactivate (self, &activation);
  if (activation.ctx == NULL)
    return;

  gum_stalker_do_invalidate (activation.ctx, address, &activation);

  gum_stalker_maybe_reactivate (self, &activation);
}

void
gum_stalker_invalidate_for_thread (GumStalker * self,
                                   GumThreadId thread_id,
                                   gconstpointer address)
{
  GumActivation activation;
  GumExecCtx * ctx;

  gum_stalker_maybe_deactivate (self, &activation);

  ctx = gum_stalker_find_exec_ctx_by_thread_id (self, thread_id);
  if (ctx != NULL)
  {
    while (!gum_stalker_do_invalidate (ctx, address, &activation))
    {
      g_thread_yield ();
    }
  }

  gum_stalker_maybe_reactivate (self, &activation);
}

static void
gum_stalker_invalidate_for_all_threads (GumStalker * self,
                                        gconstpointer address,
                                        GumActivation * activation)
{
  GSList * contexts, * cur;

  GUM_STALKER_LOCK (self);
  contexts = g_slist_copy (self->contexts);
  GUM_STALKER_UNLOCK (self);

  cur = contexts;

  while (cur != NULL)
  {
    GumExecCtx * ctx = cur->data;
    GSList * l;

    if (!gum_stalker_do_invalidate (ctx, address, activation))
    {
      cur = g_slist_append (cur, ctx);
    }

    l = cur;
    cur = cur->next;
    g_slist_free_1 (l);
  }
}

static gboolean
gum_stalker_do_invalidate (GumExecCtx * ctx,
                           gconstpointer address,
                           GumActivation * activation)
{
  GumInvalidateContext ic;

  ic.is_executing_target_block = FALSE;

  gum_spinlock_acquire (&ctx->code_lock);

  if ((ic.block = gum_metal_hash_table_lookup (ctx->mappings, address)) != NULL)
  {
    if (ctx == activation->ctx)
    {
      gum_exec_block_invalidate (ic.block);
    }
    else
    {
      gum_process_modify_thread (ctx->thread_id,
          gum_stalker_try_invalidate_block_owned_by_thread, &ic,
          GUM_MODIFY_THREAD_FLAGS_NONE);
    }
  }

  gum_spinlock_release (&ctx->code_lock);

  return !ic.is_executing_target_block;
}

static void
gum_stalker_try_invalidate_block_owned_by_thread (GumThreadId thread_id,
                                                  GumCpuContext * cpu_context,
                                                  gpointer user_data)
{
  GumInvalidateContext * ic = user_data;
  GumExecBlock * block = ic->block;
  const guint8 * pc = GSIZE_TO_POINTER (cpu_context->pc);

  if (pc >= block->code_start &&
      pc < block->code_start + GUM_INVALIDATE_TRAMPOLINE_MAX_SIZE)
  {
    ic->is_executing_target_block = TRUE;
    return;
  }

  gum_exec_block_invalidate (block);
}

GumProbeId
gum_stalker_add_call_probe (GumStalker * self,
                            gpointer target_address,
                            GumCallProbeCallback callback,
                            gpointer data,
                            GDestroyNotify notify)
{
  GumActivation activation;
  GumCallProbe * probe;
  GPtrArray * probes;
  gboolean is_first_for_target;

  gum_stalker_maybe_deactivate (self, &activation);

  target_address = gum_strip_code_pointer (target_address);
  is_first_for_target = FALSE;

  probe = g_slice_new (GumCallProbe);
  probe->ref_count = 1;
  probe->id = g_atomic_int_add (&self->last_probe_id, 1) + 1;
  probe->callback = callback;
  probe->user_data = data;
  probe->user_notify = notify;

  gum_spinlock_acquire (&self->probe_lock);

  g_hash_table_insert (self->probe_target_by_id, GSIZE_TO_POINTER (probe->id),
      target_address);

  probes = g_hash_table_lookup (self->probe_array_by_address, target_address);
  if (probes == NULL)
  {
    probes =
        g_ptr_array_new_with_free_func ((GDestroyNotify) gum_call_probe_unref);
    g_hash_table_insert (self->probe_array_by_address, target_address, probes);

    is_first_for_target = TRUE;
  }

  g_ptr_array_add (probes, probe);

  self->any_probes_attached = TRUE;

  gum_spinlock_release (&self->probe_lock);

  if (is_first_for_target)
    gum_stalker_invalidate_for_all_threads (self, target_address, &activation);

  gum_stalker_maybe_reactivate (self, &activation);

  return probe->id;
}

void
gum_stalker_remove_call_probe (GumStalker * self,
                               GumProbeId id)
{
  GumActivation activation;
  gpointer target_address;
  gboolean is_last_for_target;

  gum_stalker_maybe_deactivate (self, &activation);

  gum_spinlock_acquire (&self->probe_lock);

  target_address =
      g_hash_table_lookup (self->probe_target_by_id, GSIZE_TO_POINTER (id));
  is_last_for_target = FALSE;

  if (target_address != NULL)
  {
    GPtrArray * probes;
    gint match_index = -1;
    guint i;

    g_hash_table_remove (self->probe_target_by_id, GSIZE_TO_POINTER (id));

    probes = g_hash_table_lookup (self->probe_array_by_address, target_address);
    g_assert (probes != NULL);

    for (i = 0; i != probes->len; i++)
    {
      GumCallProbe * probe = g_ptr_array_index (probes, i);
      if (probe->id == id)
      {
        match_index = i;
        break;
      }
    }
    g_assert (match_index != -1);

    g_ptr_array_remove_index (probes, match_index);

    if (probes->len == 0)
    {
      g_hash_table_remove (self->probe_array_by_address, target_address);

      is_last_for_target = TRUE;
    }

    self->any_probes_attached =
        g_hash_table_size (self->probe_array_by_address) != 0;
  }

  gum_spinlock_release (&self->probe_lock);

  if (is_last_for_target)
    gum_stalker_invalidate_for_all_threads (self, target_address, &activation);

  gum_stalker_maybe_reactivate (self, &activation);
}

static void
gum_call_probe_finalize (GumCallProbe * probe)
{
  if (probe->user_notify != NULL)
    probe->user_notify (probe->user_data);
}

static GumCallProbe *
gum_call_probe_ref (GumCallProbe * probe)
{
  g_atomic_int_inc (&probe->ref_count);

  return probe;
}

static void
gum_call_probe_unref (GumCallProbe * probe)
{
  if (g_atomic_int_dec_and_test (&probe->ref_count))
  {
    gum_call_probe_finalize (probe);
  }
}

void
_gum_stalker_modify_to_run_on_thread (GumStalker * self,
                                      GumThreadId thread_id,
                                      GumCpuContext * cpu_context,
                                      GumStalkerRunOnThreadFunc func,
                                      gpointer data)
{
  GumExecCtx * ctx;
  GumAddress pc;
  GumArm64Writer * cw;
  GumAddress cpu_context_copy;

  ctx = gum_stalker_create_exec_ctx (self, thread_id, NULL, NULL);

  pc = gum_strip_code_address (cpu_context->pc);

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (self, ctx->thunks, self->thunks_size);
  cw = &ctx->code_writer;
  gum_arm64_writer_reset (cw, ctx->infect_thunk);

  cpu_context_copy = GUM_ADDRESS (gum_arm64_writer_cur (cw));
  gum_arm64_writer_put_bytes (cw, (guint8 *) cpu_context,
      sizeof (GumCpuContext));

  ctx->infect_body = GUM_ADDRESS (gum_arm64_writer_cur (cw));

#ifdef HAVE_PTRAUTH
  ctx->infect_body = GPOINTER_TO_SIZE (ptrauth_sign_unauthenticated (
      GSIZE_TO_POINTER (ctx->infect_body),
      ptrauth_key_process_independent_code,
      ptrauth_string_discriminator ("pc")));
#endif
  gum_exec_ctx_write_prolog (ctx, GUM_PROLOG_MINIMAL, cw);

  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (func), 2,
      GUM_ARG_ADDRESS, cpu_context_copy,
      GUM_ARG_ADDRESS, GUM_ADDRESS (data));

  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_unfollow), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_ADDRESS, pc);

  gum_exec_ctx_write_epilog (ctx, GUM_PROLOG_MINIMAL, cw);

  /*
   * Here we spoil x17 since this is a necessity of the AARCH64 architecture
   * when performing long branches. However, the documentation states...
   *
   * "Registers r16 (IP0) and r17 (IP1) may be used by a linker as a scratch
   *  register between a routine and any subroutine it calls."
   *
   * This same approach is used elsewhere in Stalker for arm64.
   */
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X17, pc);
  gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_X17);

  gum_arm64_writer_flush (cw);
  gum_stalker_freeze (self, cw->base, gum_arm64_writer_offset (cw));

  gum_spinlock_release (&ctx->code_lock);

  cpu_context->pc = ctx->infect_body;
}

static GumExecCtx *
gum_stalker_create_exec_ctx (GumStalker * self,
                             GumThreadId thread_id,
                             GumStalkerTransformer * transformer,
                             GumEventSink * sink)
{
  GumExecCtx * ctx = gum_exec_ctx_new (self, thread_id, transformer, sink);

  GUM_STALKER_LOCK (self);
  self->contexts = g_slist_prepend (self->contexts, ctx);
  GUM_STALKER_UNLOCK (self);

  return ctx;
}

static void
gum_stalker_destroy_exec_ctx (GumStalker * self,
                              GumExecCtx * ctx)
{
  GSList * entry;

  GUM_STALKER_LOCK (self);
  entry = g_slist_find (self->contexts, ctx);
  if (entry != NULL)
    self->contexts = g_slist_delete_link (self->contexts, entry);
  GUM_STALKER_UNLOCK (self);

  /* Racy due to garbage-collection. */
  if (entry == NULL)
    return;

  gum_exec_ctx_dispose (ctx);

  if (ctx->sink_started)
  {
    gum_event_sink_stop (ctx->sink);

    ctx->sink_started = FALSE;
  }

  gum_exec_ctx_free (ctx);
}

static GumExecCtx *
gum_stalker_get_exec_ctx (void)
{
  return g_private_get (&gum_stalker_exec_ctx_private);
}

static GumExecCtx *
gum_stalker_find_exec_ctx_by_thread_id (GumStalker * self,
                                        GumThreadId thread_id)
{
  GumExecCtx * ctx = NULL;
  GSList * cur;

  GUM_STALKER_LOCK (self);

  for (cur = self->contexts; cur != NULL; cur = cur->next)
  {
    GumExecCtx * candidate = cur->data;

    if (candidate->thread_id == thread_id)
    {
      ctx = candidate;
      break;
    }
  }

  GUM_STALKER_UNLOCK (self);

  return ctx;
}

static gsize
gum_stalker_snapshot_space_needed_for (GumStalker * self,
                                       gsize real_size)
{
  return (self->trust_threshold != 0) ? real_size : 0;
}

static gsize
gum_stalker_get_ic_entry_size (GumStalker * self)
{
  return self->ic_entries * sizeof (GumIcEntry);
}

static void
gum_stalker_thaw (GumStalker * self,
                  gpointer code,
                  gsize size)
{
  if (size == 0)
    return;

  if (!self->is_rwx_supported)
    gum_mprotect (code, size, GUM_PAGE_RW);
}

static void
gum_stalker_freeze (GumStalker * self,
                    gpointer code,
                    gsize size)
{
  if (size == 0)
  {
    if (!self->is_rwx_supported)
    {
      guint page_offset = GPOINTER_TO_SIZE (code) & (self->page_size - 1);
      if (page_offset != 0)
      {
        gum_memory_mark_code ((guint8 *) code - page_offset,
            self->page_size - page_offset);
      }
    }

    return;
  }

  if (!self->is_rwx_supported)
    gum_memory_mark_code (code, size);

  gum_clear_cache (code, size);
}

static GumAddress
gum_stalker_translate_unwind_pc (GumUnwindPcTranslator * translator,
                                 GumAddress code_address)
{
#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  GumExecCtx * ctx;
  gpointer real_address;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return 0;

  real_address = gum_metal_hash_table_lookup (ctx->excluded_calls,
      GSIZE_TO_POINTER (code_address));
  if (real_address == NULL)
    return 0;

  return GUM_ADDRESS (real_address);
#else
  return 0;
#endif
}

static gboolean
gum_stalker_install_unwind_resume_context (GumUnwindPcTranslator * translator,
                                           gpointer unwind_context,
                                           GumAddress real_resume_ip)
{
#ifdef HAVE_LINUX
  GumExecCtx * ctx;
  gpointer resume_ip;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return FALSE;

  resume_ip = (guint8 *) gum_exec_ctx_switch_block (ctx, NULL,
      GSIZE_TO_POINTER (real_resume_ip), NULL) + GUM_RESTORATION_PROLOG_SIZE;
  _Unwind_SetIP ((struct _Unwind_Context *) unwind_context,
      GPOINTER_TO_SIZE (resume_ip));

  ctx->pending_calls--;

  return TRUE;
#else
  return FALSE;
#endif
}

static gboolean
gum_stalker_on_exception (GumExceptionDetails * details,
                          gpointer user_data)
{
  GumStalker * self = user_data;
  GumExecCtx * ctx;

  ctx = gum_stalker_find_exec_ctx_by_thread_id (self, details->thread_id);
  if (ctx == NULL)
    return FALSE;

  return gum_exec_ctx_try_handle_exception (ctx, details);
}

static GumExecCtx *
gum_exec_ctx_new (GumStalker * stalker,
                  GumThreadId thread_id,
                  GumStalkerTransformer * transformer,
                  GumEventSink * sink)
{
  GumExecCtx * ctx;
  guint8 * base;
  GumCodeSlab * code_slab;
  GumSlowSlab * slow_slab;
  GumDataSlab * data_slab;

  base = gum_memory_allocate (NULL, stalker->ctx_size, stalker->page_size,
      stalker->is_rwx_supported ? GUM_PAGE_RWX : GUM_PAGE_RW);

  ctx = (GumExecCtx *) base;

  ctx->state = GUM_EXEC_CTX_ACTIVE;

  ctx->stalker = g_object_ref (stalker);
  ctx->thread_id = thread_id;

  gum_arm64_writer_init (&ctx->code_writer, NULL);
  gum_arm64_writer_init (&ctx->slow_writer, NULL);
  gum_arm64_relocator_init (&ctx->relocator, NULL, &ctx->code_writer);

  if (transformer != NULL)
    ctx->transformer = g_object_ref (transformer);
  else
    ctx->transformer = gum_stalker_transformer_make_default ();
  ctx->transform_block_impl =
      GUM_STALKER_TRANSFORMER_GET_IFACE (ctx->transformer)->transform_block;

  if (sink != NULL)
    ctx->sink = g_object_ref (sink);
  else
    ctx->sink = gum_event_sink_make_default ();

  ctx->sink_mask = gum_event_sink_query_mask (ctx->sink);
  ctx->sink_process_impl = GUM_EVENT_SINK_GET_IFACE (ctx->sink)->process;

  ctx->observer = NULL;

  ctx->thunks = base + stalker->thunks_offset;
  ctx->infect_thunk = ctx->thunks;

  gum_spinlock_init (&ctx->code_lock);

  code_slab = (GumCodeSlab *) (base + stalker->code_slab_offset);
  gum_code_slab_init (code_slab, stalker->code_slab_size_initial, 0,
      stalker->page_size);
  gum_exec_ctx_add_code_slab (ctx, code_slab);

  slow_slab = (GumSlowSlab *) (base + stalker->slow_slab_offset);
  gum_slow_slab_init (slow_slab, stalker->slow_slab_size_initial, 0,
      stalker->page_size);
  gum_exec_ctx_add_slow_slab (ctx, slow_slab);

  data_slab = (GumDataSlab *) (base + stalker->data_slab_offset);
  gum_data_slab_init (data_slab, stalker->data_slab_size_initial, 0);
  gum_exec_ctx_add_data_slab (ctx, data_slab);

  ctx->scratch_slab = (GumCodeSlab *) (base + stalker->scratch_slab_offset);
  gum_scratch_slab_init (ctx->scratch_slab, stalker->scratch_slab_size);

  ctx->mappings = gum_metal_hash_table_new (NULL, NULL);

  gum_exec_ctx_ensure_inline_helpers_reachable (ctx);

  ctx->depth = 0;

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  ctx->excluded_calls = gum_metal_hash_table_new (NULL, NULL);
#endif

  return ctx;
}

static void
gum_exec_ctx_free (GumExecCtx * ctx)
{
  GumStalker * stalker = ctx->stalker;
  GumDataSlab * data_slab;
  GumCodeSlab * code_slab;

  gum_metal_hash_table_unref (ctx->mappings);

  data_slab = ctx->data_slab;
  while (data_slab != NULL)
  {
    GumDataSlab * next = (GumDataSlab *) data_slab->slab.next;
    gum_data_slab_free (data_slab);
    data_slab = next;
  }

  code_slab = ctx->code_slab;
  while (code_slab != NULL)
  {
    GumCodeSlab * next = (GumCodeSlab *) code_slab->slab.next;
    gum_code_slab_free (code_slab);
    code_slab = next;
  }

  g_object_unref (ctx->sink);
  g_object_unref (ctx->transformer);
  g_clear_object (&ctx->observer);

  gum_arm64_relocator_clear (&ctx->relocator);
  gum_arm64_writer_clear (&ctx->slow_writer);
  gum_arm64_writer_clear (&ctx->code_writer);

  g_object_unref (stalker);

  gum_memory_free (ctx, stalker->ctx_size);
}

static void
gum_exec_ctx_dispose (GumExecCtx * ctx)
{
  GumStalker * stalker = ctx->stalker;
  GumSlab * slab;
  GumExecBlock * block;

  for (slab = &ctx->code_slab->slab; slab != NULL; slab = slab->next)
  {
    gum_stalker_thaw (stalker, gum_slab_start (slab), slab->offset);
  }

  for (slab = &ctx->slow_slab->slab; slab != NULL; slab = slab->next)
  {
    gum_stalker_thaw (stalker, gum_slab_start (slab), slab->offset);
  }

  for (block = ctx->block_list; block != NULL; block = block->next)
  {
    gum_exec_block_clear (block);
  }

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  gum_metal_hash_table_unref (ctx->excluded_calls);
#endif
}

static GumCodeSlab *
gum_exec_ctx_add_code_slab (GumExecCtx * ctx,
                            GumCodeSlab * code_slab)
{
  code_slab->slab.next = &ctx->code_slab->slab;
  ctx->code_slab = code_slab;
  return code_slab;
}

static GumSlowSlab *
gum_exec_ctx_add_slow_slab (GumExecCtx * ctx,
                            GumSlowSlab * slow_slab)
{
  slow_slab->slab.next = &ctx->slow_slab->slab;
  ctx->slow_slab = slow_slab;
  return slow_slab;
}

static GumDataSlab *
gum_exec_ctx_add_data_slab (GumExecCtx * ctx,
                            GumDataSlab * data_slab)
{
  data_slab->slab.next = &ctx->data_slab->slab;
  ctx->data_slab = data_slab;
  return data_slab;
}

static gboolean
gum_exec_ctx_maybe_unfollow (GumExecCtx * ctx,
                             gpointer resume_at)
{
  if (g_atomic_int_get (&ctx->state) != GUM_EXEC_CTX_UNFOLLOW_PENDING)
    return FALSE;

  if (ctx->pending_calls > 0)
    return FALSE;

  gum_exec_ctx_unfollow (ctx, resume_at);

  return TRUE;
}

static void
gum_exec_ctx_unfollow (GumExecCtx * ctx,
                       gpointer resume_at)
{
  ctx->current_block = NULL;

  ctx->resume_at = resume_at;

  g_private_set (&gum_stalker_exec_ctx_private, NULL);

  ctx->destroy_pending_since = g_get_monotonic_time ();
  g_atomic_int_set (&ctx->state, GUM_EXEC_CTX_DESTROY_PENDING);
}

static gboolean
gum_exec_ctx_has_executed (GumExecCtx * ctx)
{
  return ctx->resume_at != NULL;
}

static gboolean
gum_exec_ctx_contains (GumExecCtx * ctx,
                       gconstpointer address)
{
  GumSlab * cur = &ctx->code_slab->slab;
  GumSlab * slow_slab = &ctx->slow_slab->slab;

  do
  {
    if ((const guint8 *) address >= cur->data &&
        (const guint8 *) address < (guint8 *) gum_slab_cursor (cur))
    {
      return TRUE;
    }

    cur = cur->next;
  }
  while (cur != NULL);

  do
  {
    if ((const guint8 *) address >= slow_slab->data &&
        (const guint8 *) address < (guint8 *) gum_slab_cursor (slow_slab))
    {
      return TRUE;
    }

    slow_slab = slow_slab->next;
  }
  while (slow_slab != NULL);

  return FALSE;
}

static gboolean
gum_exec_ctx_may_now_backpatch (GumExecCtx * ctx,
                                GumExecBlock * target_block)
{
  if (g_atomic_int_get (&ctx->state) != GUM_EXEC_CTX_ACTIVE)
    return FALSE;

  if ((target_block->flags & GUM_EXEC_BLOCK_ACTIVATION_TARGET) != 0)
    return FALSE;

  if (target_block->recycle_count < ctx->stalker->trust_threshold)
    return FALSE;

  return TRUE;
}

#define GUM_ENTRYGATE(name) \
    gum_exec_ctx_replace_current_block_from_##name
#define GUM_DEFINE_ENTRYGATE(name) \
    static gpointer \
    GUM_ENTRYGATE (name) ( \
        GumExecBlock * block, \
        gpointer start_address, \
        gpointer from_insn) \
    { \
      GumExecCtx * ctx = block->ctx; \
      \
      if (ctx->observer != NULL) \
        gum_stalker_observer_increment_##name (ctx->observer); \
      \
      return gum_exec_ctx_switch_block (ctx, block, start_address, from_insn); \
    }

GUM_DEFINE_ENTRYGATE (call_imm)
GUM_DEFINE_ENTRYGATE (call_reg)
GUM_DEFINE_ENTRYGATE (excluded_call_imm)
GUM_DEFINE_ENTRYGATE (excluded_call_reg)
GUM_DEFINE_ENTRYGATE (ret)

GUM_DEFINE_ENTRYGATE (jmp_imm)
GUM_DEFINE_ENTRYGATE (jmp_reg)

GUM_DEFINE_ENTRYGATE (jmp_cond_cc)
GUM_DEFINE_ENTRYGATE (jmp_cond_cbz)
GUM_DEFINE_ENTRYGATE (jmp_cond_cbnz)
GUM_DEFINE_ENTRYGATE (jmp_cond_tbz)
GUM_DEFINE_ENTRYGATE (jmp_cond_tbnz)

GUM_DEFINE_ENTRYGATE (jmp_continuation)

static gpointer
gum_exec_ctx_switch_block (GumExecCtx * ctx,
                           GumExecBlock * block,
                           gpointer start_address,
                           gpointer from_insn)
{
  if (ctx->observer != NULL)
    gum_stalker_observer_increment_total (ctx->observer);

  if (start_address == gum_unfollow_me_address ||
      start_address == gum_deactivate_address)
  {
    ctx->unfollow_called_while_still_following = TRUE;
    ctx->current_block = NULL;
    ctx->resume_at = start_address;
  }
  else if (start_address == gum_thread_exit_address)
  {
    gum_exec_ctx_unfollow (ctx, start_address);
  }
  else if (gum_exec_ctx_maybe_unfollow (ctx, start_address))
  {
  }
  else if (gum_exec_ctx_contains (ctx, start_address))
  {
    ctx->current_block = NULL;
    ctx->resume_at = start_address;
  }
  else
  {
    ctx->current_block = gum_exec_ctx_obtain_block_for (ctx, start_address,
        &ctx->resume_at);

    if (start_address == ctx->activation_target)
    {
      ctx->activation_target = NULL;
      ctx->current_block->flags |= GUM_EXEC_BLOCK_ACTIVATION_TARGET;
    }

    gum_exec_ctx_maybe_unfollow (ctx, start_address);
  }

  /*
   * When we fetch a block to be executed, before we make use of the
   * code_address, we first call-back to the observer to allow the user to make
   * any modifications to it. We also pass the user the instruction which was
   * executed immediately before the transition as well as the real address of
   * the target for the branch which resulted in this transition.
   *
   * The user can observe or modify the code being written to a given
   * instrumented address by making use of a transformer. This callback gives
   * the user the ability to modify control-flow rather than just the
   * instructions being executed.
   *
   * It should be noted that as well as making an instantaneous change to the
   * control flow, in the event that backpatching is enabled, this will result
   * in any backpatches being modified accordingly. It is therefore expected
   * that if the user is making use of backpatching that any callback should
   * provide a consistent result when called multiple times with the same
   * inputs.
   *
   * Stalker for AArch64, however, prefixes all blocks with:
   *
   *   ldp x16, x17, [sp], #0x90
   *
   * This is necessary since if we must reach the block with an indirect branch
   * (e.g. it is too far away for an immediate branch) then we must clobber a
   * register since AArch64 only has limited range for direct calls. If however,
   * the block can be reached with an immediate branch, then this first
   * instruction is skipped by the backpatcher.
   *
   * This peculiarity may cause issues for integrators which wish to optionally
   * skip a preamble emitted at the start of a block.
   */
  gum_exec_ctx_query_block_switch_callback (ctx, block, start_address,
      from_insn, &ctx->resume_at);

  return ctx->resume_at;
}

static void
gum_exec_ctx_query_block_switch_callback (GumExecCtx * ctx,
                                          GumExecBlock * block,
                                          gpointer start_address,
                                          gpointer from_insn,
                                          gpointer * target)
{
  gpointer from;

  if (ctx->observer == NULL)
    return;

  from = (block != NULL) ? block->real_start : NULL;

  gum_stalker_observer_switch_callback (ctx->observer, from, start_address,
      from_insn, target);
}

static void
gum_exec_ctx_recompile_and_switch_block (GumExecCtx * ctx,
                                         GumExecBlock * block)
{
  const gpointer start_address = block->real_start;

  if (gum_exec_ctx_maybe_unfollow (ctx, start_address))
    return;

  gum_exec_ctx_recompile_block (ctx, block);

  ctx->current_block = block;
  ctx->resume_at = block->code_start;

  if (start_address == ctx->activation_target)
  {
    ctx->activation_target = NULL;
    ctx->current_block->flags |= GUM_EXEC_BLOCK_ACTIVATION_TARGET;
  }

  gum_exec_ctx_maybe_unfollow (ctx, start_address);
}

static GumExecBlock *
gum_exec_ctx_obtain_block_for (GumExecCtx * ctx,
                               gpointer real_address,
                               gpointer * code_address)
{
  GumExecBlock * block;

  gum_spinlock_acquire (&ctx->code_lock);

  block = gum_metal_hash_table_lookup (ctx->mappings, real_address);
  if (block != NULL)
  {
    const gint trust_threshold = ctx->stalker->trust_threshold;
    gboolean still_up_to_date;

    still_up_to_date =
        (trust_threshold >= 0 && block->recycle_count >= trust_threshold) ||
        memcmp (block->real_start, gum_exec_block_get_snapshot_start (block),
            block->real_size) == 0;

    gum_spinlock_release (&ctx->code_lock);

    if (still_up_to_date)
    {
      if (trust_threshold > 0)
        block->recycle_count++;
    }
    else
    {
      gum_exec_ctx_recompile_block (ctx, block);
    }
  }
  else
  {
    block = gum_exec_block_new (ctx);
    block->real_start = real_address;
    gum_exec_block_maybe_inherit_exclusive_access_state (block, block->next);
    gum_exec_ctx_compile_block (ctx, block, real_address, block->code_start,
        GUM_ADDRESS (block->code_start), &block->real_size, &block->code_size,
        &block->slow_size);
    gum_exec_block_commit (block);
    gum_exec_block_propagate_exclusive_access_state (block);

    gum_metal_hash_table_insert (ctx->mappings, real_address, block);

    gum_spinlock_release (&ctx->code_lock);

    gum_exec_ctx_maybe_emit_compile_event (ctx, block);
  }

  *code_address = block->code_start;

  return block;
}

static void
gum_exec_ctx_recompile_block (GumExecCtx * ctx,
                              GumExecBlock * block)
{
  GumStalker * stalker = ctx->stalker;
  guint8 * internal_code = block->code_start;
  guint8 * scratch_base = ctx->scratch_slab->slab.data;
  guint input_size, output_size, slow_size;
  gsize new_block_size, new_snapshot_size;

  gum_spinlock_acquire (&ctx->code_lock);

  gum_exec_ctx_write_scratch_slab (ctx, block, &input_size, &output_size,
      &slow_size);

  new_snapshot_size =
      gum_stalker_snapshot_space_needed_for (stalker, input_size);

  new_block_size = output_size + new_snapshot_size;

  gum_stalker_thaw (stalker, internal_code, block->capacity);

  if (new_block_size <= block->capacity)
  {
    block->real_size = input_size;
    block->code_size = output_size;

    memcpy (internal_code, scratch_base, output_size);
    memcpy (gum_exec_block_get_snapshot_start (block), block->real_start,
        new_snapshot_size);

    gum_stalker_freeze (stalker, internal_code, new_block_size);
  }
  else
  {
    GumExecBlock * storage_block;
    GumArm64Writer * cw = &ctx->code_writer;
    GumAddress external_code_address;

    storage_block = gum_exec_block_new (ctx);
    storage_block->real_start = block->real_start;
    gum_exec_ctx_compile_block (ctx, storage_block, block->real_start,
        storage_block->code_start, GUM_ADDRESS (storage_block->code_start),
        &storage_block->real_size, &storage_block->code_size,
        &storage_block->slow_size);
    gum_exec_block_commit (storage_block);
    block->storage_block = storage_block;

    gum_stalker_thaw (stalker, internal_code, block->capacity);
    gum_arm64_writer_reset (cw, internal_code);

    external_code_address = GUM_ADDRESS (storage_block->code_start);
    if (gum_arm64_writer_can_branch_directly_between (cw,
        GUM_ADDRESS (internal_code), external_code_address))
    {
      gum_arm64_writer_put_b_imm (cw, external_code_address);
      gum_arm64_writer_put_b_imm (cw, external_code_address + sizeof (guint32));
    }
    else
    {
      gconstpointer already_saved = cw->code + 1;

      gum_arm64_writer_put_b_label (cw, already_saved);
      gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16,
          ARM64_REG_X17, ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE),
          GUM_INDEX_PRE_ADJUST);
      gum_arm64_writer_put_label (cw, already_saved);
      gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
          external_code_address);
      gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_X16);
    }

    gum_arm64_writer_flush (cw);
    gum_stalker_freeze (stalker, internal_code, block->capacity);
  }

  gum_spinlock_release (&ctx->code_lock);

  gum_exec_ctx_maybe_emit_compile_event (ctx, block);
}

static void
gum_exec_ctx_write_scratch_slab (GumExecCtx * ctx,
                                 GumExecBlock * block,
                                 guint * input_size,
                                 guint * output_size,
                                 guint * slow_size)
{
  GumStalker * stalker = ctx->stalker;
  guint8 * internal_code = block->code_start;
  GumSlowSlab * slow_slab;
  gsize slow_available;
  gpointer slow_start;
  GumCodeSlab * prev_code_slab;
  GumSlowSlab * prev_slow_slab;
  guint8 * scratch_base;

  gum_exec_block_maybe_create_new_code_slabs (ctx);
  gum_exec_block_maybe_create_new_data_slab (ctx);

  slow_slab = ctx->slow_slab;

  slow_available = gum_slab_available (&slow_slab->slab);

  gum_scratch_slab_init (ctx->scratch_slab, GUM_SCRATCH_SLAB_SIZE);

  slow_start = gum_slab_cursor (&slow_slab->slab);
  slow_available = gum_slab_available (&slow_slab->slab);
  gum_stalker_thaw (stalker, slow_start, slow_available);

  if (block->storage_block != NULL)
    gum_exec_block_clear (block->storage_block);
  gum_exec_block_clear (block);

  prev_code_slab = block->code_slab;
  prev_slow_slab = block->slow_slab;

  block->code_slab = ctx->scratch_slab;
  block->slow_slab = ctx->slow_slab;
  block->slow_start = gum_slab_cursor (&slow_slab->slab);
  scratch_base = ctx->scratch_slab->slab.data;
  ctx->scratch_slab->invalidator = prev_code_slab->invalidator;

  gum_exec_ctx_compile_block (ctx, block, block->real_start, scratch_base,
      GUM_ADDRESS (internal_code), input_size, output_size, slow_size);
  gum_slab_reserve (&slow_slab->slab, *slow_size);
  gum_stalker_freeze (stalker, slow_start, *slow_size);

  block->code_slab = prev_code_slab;
  block->slow_slab = prev_slow_slab;
}

static void
gum_exec_ctx_compile_block (GumExecCtx * ctx,
                            GumExecBlock * block,
                            gconstpointer input_code,
                            gpointer output_code,
                            GumAddress output_pc,
                            guint * input_size,
                            guint * output_size,
                            guint * slow_size)
{
  GumArm64Writer * cw = &ctx->code_writer;
  GumArm64Writer * cws = &ctx->slow_writer;
  GumArm64Relocator * rl = &ctx->relocator;
  GumGeneratorContext gc;
  GumStalkerIterator iterator;
  GumStalkerOutput output;
  gboolean all_labels_resolved;
  gboolean all_slow_labels_resolved;

  gum_arm64_writer_reset (cw, output_code);
  cw->pc = output_pc;

  gum_arm64_writer_reset (cws, block->slow_start);
  cws->pc = GUM_ADDRESS (block->slow_start);

  gum_arm64_relocator_reset (rl, input_code, cw);

  gum_ensure_code_readable (input_code, ctx->stalker->page_size);

  gc.instruction = NULL;
  gc.relocator = rl;
  gc.code_writer = cw;
  gc.slow_writer = cws;
  gc.continuation_real_address = NULL;
  gc.opened_prolog = GUM_PROLOG_NONE;

  iterator.exec_context = ctx;
  iterator.exec_block = block;
  iterator.generator_context = &gc;

  iterator.instruction.ci = NULL;
  iterator.instruction.start = NULL;
  iterator.instruction.end = NULL;
  iterator.requirements = GUM_REQUIRE_NOTHING;

  output.writer.arm64 = cw;
  output.encoding = GUM_INSTRUCTION_DEFAULT;

  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X16, ARM64_REG_X17,
      ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE, GUM_INDEX_POST_ADJUST);

  gum_exec_block_maybe_write_call_probe_code (block, &gc);

  ctx->pending_calls++;
  ctx->transform_block_impl (ctx->transformer, &iterator, &output);
  ctx->pending_calls--;

  if (gc.continuation_real_address != NULL)
  {
    GumBranchTarget continue_target = { 0, };

    continue_target.absolute_address = gc.continuation_real_address;
    continue_target.reg = ARM64_REG_INVALID;
    gum_exec_block_write_jmp_transfer_code (block, &continue_target,
        GUM_ENTRYGATE (jmp_continuation), &gc);
  }

  gum_arm64_writer_put_brk_imm (cw, 14);

  all_labels_resolved = gum_arm64_writer_flush (cw);
  if (!all_labels_resolved)
    gum_panic ("Failed to resolve labels");

  all_slow_labels_resolved = gum_arm64_writer_flush (cws);
  if (!all_slow_labels_resolved)
    gum_panic ("Failed to resolve slow labels");

  *input_size = rl->input_cur - rl->input_start;
  *output_size = gum_arm64_writer_offset (cw);
  *slow_size = gum_arm64_writer_offset (cws);
}

static void
gum_exec_ctx_maybe_emit_compile_event (GumExecCtx * ctx,
                                       GumExecBlock * block)
{
  if ((ctx->sink_mask & GUM_COMPILE) != 0)
  {
    GumEvent ev;

    ev.type = GUM_COMPILE;
    ev.compile.start = block->real_start;
    ev.compile.end = block->real_start + block->real_size;

    ctx->sink_process_impl (ctx->sink, &ev, NULL);
  }
}

gboolean
gum_stalker_iterator_next (GumStalkerIterator * self,
                           const cs_insn ** insn)
{
  GumGeneratorContext * gc = self->generator_context;
  GumArm64Relocator * rl = gc->relocator;
  GumInstruction * instruction;
  gboolean is_first_instruction;
  guint n_read;

  instruction = self->generator_context->instruction;
  is_first_instruction = instruction == NULL;

  if (instruction != NULL)
  {
    gboolean skip_implicitly_requested;

    skip_implicitly_requested = rl->outpos != rl->inpos;
    if (skip_implicitly_requested)
    {
      gum_arm64_relocator_skip_one (rl);
    }

    if (gum_stalker_iterator_is_out_of_space (self))
    {
      gc->continuation_real_address = instruction->end;
      return FALSE;
    }

    if (!skip_implicitly_requested && gum_arm64_relocator_eob (rl))
      return FALSE;
  }

  instruction = &self->instruction;

  n_read = gum_arm64_relocator_read_one (rl, &instruction->ci);
  if (n_read == 0)
    return FALSE;

  instruction->start = GSIZE_TO_POINTER (instruction->ci->address);
  instruction->end = instruction->start + instruction->ci->size;

  self->generator_context->instruction = instruction;

  if (is_first_instruction &&
     (self->exec_context->sink_mask & GUM_BLOCK) != 0 &&
     (self->exec_block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) == 0)
  {
    gum_exec_block_write_block_event_code (self->exec_block, gc,
        GUM_CODE_INTERRUPTIBLE);
  }

  if (insn != NULL)
    *insn = instruction->ci;

  return TRUE;
}

static gboolean
gum_stalker_iterator_is_out_of_space (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumSlab * slab = &block->code_slab->slab;
  GumArm64Writer * cw = self->generator_context->code_writer;
  gsize capacity, snapshot_size, pending_literals_size;

  capacity = (guint8 *) gum_slab_end (slab) -
      (guint8 *) gum_arm64_writer_cur (cw);

  snapshot_size = gum_stalker_snapshot_space_needed_for (
      self->exec_context->stalker,
      self->generator_context->instruction->end - block->real_start);

  pending_literals_size = (cw->literal_refs.data != NULL)
      ? cw->literal_refs.length * sizeof (guint64)
      : 0;

  return capacity < GUM_EXEC_BLOCK_MIN_CAPACITY + snapshot_size +
      pending_literals_size;
}

void
gum_stalker_iterator_keep (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  GumArm64Relocator * rl = gc->relocator;
  const cs_insn * insn = gc->instruction->ci;
  GumVirtualizationRequirements requirements;

  requirements = GUM_REQUIRE_NOTHING;

  switch (insn->id)
  {
    case ARM64_INS_LDAXR:
    case ARM64_INS_LDAXP:
    case ARM64_INS_LDAXRB:
    case ARM64_INS_LDAXRH:
    case ARM64_INS_LDXR:
    case ARM64_INS_LDXP:
    case ARM64_INS_LDXRB:
    case ARM64_INS_LDXRH:
      block->flags |= GUM_EXEC_BLOCK_HAS_EXCLUSIVE_LOAD;
      break;
    case ARM64_INS_STXR:
    case ARM64_INS_STXP:
    case ARM64_INS_STXRB:
    case ARM64_INS_STXRH:
    case ARM64_INS_STLXR:
    case ARM64_INS_STLXP:
    case ARM64_INS_STLXRB:
    case ARM64_INS_STLXRH:
      block->flags |= GUM_EXEC_BLOCK_HAS_EXCLUSIVE_STORE;
      break;
    default:
      break;
  }

  if ((self->exec_context->sink_mask & GUM_EXEC) != 0 &&
      (block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) == 0)
  {
    gum_exec_block_write_exec_event_code (block, gc, GUM_CODE_INTERRUPTIBLE);
  }

  switch (insn->id)
  {
    case ARM64_INS_B:
    case ARM64_INS_BR:
    case ARM64_INS_BRAA:
    case ARM64_INS_BRAAZ:
    case ARM64_INS_BRAB:
    case ARM64_INS_BRABZ:
    case ARM64_INS_BL:
    case ARM64_INS_BLR:
    case ARM64_INS_BLRAA:
    case ARM64_INS_BLRAAZ:
    case ARM64_INS_BLRAB:
    case ARM64_INS_BLRABZ:
    case ARM64_INS_CBZ:
    case ARM64_INS_CBNZ:
    case ARM64_INS_TBZ:
    case ARM64_INS_TBNZ:
      requirements = gum_exec_block_virtualize_branch_insn (block, gc);
      break;
    case ARM64_INS_RET:
    case ARM64_INS_RETAA:
    case ARM64_INS_RETAB:
      requirements = gum_exec_block_virtualize_ret_insn (block, gc);
      break;
    case ARM64_INS_SVC:
      requirements = gum_exec_block_virtualize_sysenter_insn (block, gc);
      break;
    case ARM64_INS_SMC:
    case ARM64_INS_HVC:
      g_assert_not_reached ();
      break;
    default:
      requirements = GUM_REQUIRE_RELOCATION;
  }

  gum_exec_block_close_prolog (block, gc, gc->code_writer);

  if ((requirements & GUM_REQUIRE_RELOCATION) != 0)
    gum_arm64_relocator_write_one (rl);

  self->requirements = requirements;
}

GumMemoryAccess
gum_stalker_iterator_get_memory_access (GumStalkerIterator * self)
{
  return ((self->exec_block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) != 0)
      ? GUM_MEMORY_ACCESS_EXCLUSIVE
      : GUM_MEMORY_ACCESS_OPEN;
}

static void
gum_exec_ctx_emit_call_event (GumExecCtx * ctx,
                              gpointer location,
                              gpointer target,
                              GumCpuContext * cpu_context)
{
  GumEvent ev;
  GumCallEvent * call = &ev.call;

  ev.type = GUM_CALL;

  call->location = location;
  call->target = target;
  call->depth = ctx->depth;

  cpu_context->pc = GPOINTER_TO_SIZE (location);

  ctx->sink_process_impl (ctx->sink, &ev, cpu_context);
}

static void
gum_exec_ctx_emit_ret_event (GumExecCtx * ctx,
                             gpointer location,
                             gpointer target,
                             GumCpuContext * cpu_context)
{
  GumEvent ev;
  GumRetEvent * ret = &ev.ret;

  ev.type = GUM_RET;

  ret->location = location;
  ret->target = target;
  ret->depth = ctx->depth;

  cpu_context->pc = GPOINTER_TO_SIZE (location);

  ctx->sink_process_impl (ctx->sink, &ev, cpu_context);
}

static void
gum_exec_ctx_emit_exec_event (GumExecCtx * ctx,
                              gpointer location,
                              GumCpuContext * cpu_context)
{
  GumEvent ev;
  GumExecEvent * exec = &ev.exec;

  ev.type = GUM_EXEC;

  exec->location = location;

  cpu_context->pc = GPOINTER_TO_SIZE (location);

  ctx->sink_process_impl (ctx->sink, &ev, cpu_context);
}

static void
gum_exec_ctx_emit_block_event (GumExecCtx * ctx,
                               const GumExecBlock * block,
                               GumCpuContext * cpu_context)
{
  GumEvent ev;
  GumBlockEvent * bev = &ev.block;

  ev.type = GUM_BLOCK;

  bev->start = block->real_start;
  bev->end = block->real_start + block->real_size;

  cpu_context->pc = GPOINTER_TO_SIZE (block->real_start);

  ctx->sink_process_impl (ctx->sink, &ev, cpu_context);
}

void
gum_stalker_iterator_put_callout (GumStalkerIterator * self,
                                  GumStalkerCallout callout,
                                  gpointer data,
                                  GDestroyNotify data_destroy)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  GumArm64Writer * cw = gc->code_writer;
  GumCalloutEntry entry;
  GumAddress entry_address;

  entry.callout = callout;
  entry.data = data;
  entry.data_destroy = data_destroy;
  entry.pc = gc->instruction->start;
  entry.exec_context = self->exec_context;
  entry.next = gum_exec_block_get_last_callout_entry (block);
  gum_exec_block_write_inline_data (cw, &entry, sizeof (entry), &entry_address);

  gum_exec_block_set_last_callout_entry (block,
      GSIZE_TO_POINTER (entry_address));

  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);
  gum_arm64_writer_put_call_address_with_arguments (gc->code_writer,
      GUM_ADDRESS (gum_stalker_invoke_callout), 2,
      GUM_ARG_ADDRESS, entry_address,
      GUM_ARG_REGISTER, ARM64_REG_X20);
  gum_exec_block_close_prolog (block, gc, gc->code_writer);
}

static void
gum_stalker_invoke_callout (GumCalloutEntry * entry,
                            GumCpuContext * cpu_context)
{
  GumExecCtx * ec = entry->exec_context;

  cpu_context->pc = GPOINTER_TO_SIZE (entry->pc);

  ec->pending_calls++;
  entry->callout (cpu_context, entry->data);
  ec->pending_calls--;
}

void
gum_stalker_iterator_put_chaining_return (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;

  if ((block->ctx->sink_mask & GUM_RET) != 0)
    gum_exec_block_write_ret_event_code (block, gc, GUM_CODE_INTERRUPTIBLE);

  gum_exec_block_write_chaining_return_code (block, gc, ARM64_REG_X30);
}

csh
gum_stalker_iterator_get_capstone (GumStalkerIterator * self)
{
  return self->exec_context->relocator.capstone;
}

/*
 * Note that as well as providing a GumArm64Writer to the functions for writing
 * a prolog or epilog, we must also provide a parameter indicating whether it is
 * being written to the code (fast) or slow slabs. This is necessary since we
 * have a separate copy of these inline helpers in each slab to mitigate the
 * issue of AArch64 not being able to make immediate branches larger than a
 * 28-bit signed offset. Note that we cannot provide the GeneratorContext here
 * since not all places where such a prolog or epilog is written is provided
 * with one.
 */
static void
gum_exec_ctx_write_prolog (GumExecCtx * ctx,
                           GumPrologType type,
                           GumArm64Writer * cw)
{
  gpointer helper;

  helper = (type == GUM_PROLOG_MINIMAL)
      ? ctx->last_prolog_minimal
      : ctx->last_prolog_full;

  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X19,
      ARM64_REG_LR, ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE),
      GUM_INDEX_PRE_ADJUST);
  gum_arm64_writer_put_bl_imm (cw, GUM_ADDRESS (helper));
}

static void
gum_exec_ctx_write_epilog (GumExecCtx * ctx,
                           GumPrologType type,
                           GumArm64Writer * cw)
{
  gpointer helper;

  helper = (type == GUM_PROLOG_MINIMAL)
      ? ctx->last_epilog_minimal
      : ctx->last_epilog_full;

  gum_exec_ctx_write_epilog_with_helper (helper, cw);
}

static void
gum_exec_ctx_write_epilog_with_helper (gpointer helper,
                                       GumArm64Writer * cw)
{
  gum_arm64_writer_put_bl_imm (cw, GUM_ADDRESS (helper));
  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X19,
      ARM64_REG_X20, ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE,
      GUM_INDEX_POST_ADJUST);
}

static void
gum_exec_ctx_ensure_inline_helpers_reachable (GumExecCtx * ctx)
{
  GumSlab * code_slab = &ctx->code_slab->slab;
  GumSlab * slow_slab = &ctx->slow_slab->slab;
  GumArm64Writer * cw = &ctx->code_writer;

  gum_exec_ctx_ensure_helper_reachable (ctx, code_slab, slow_slab, cw,
      &ctx->last_prolog_minimal, gum_exec_ctx_write_minimal_prolog_helper);
  gum_exec_ctx_ensure_helper_reachable (ctx, code_slab, slow_slab, cw,
      &ctx->last_epilog_minimal, gum_exec_ctx_write_minimal_epilog_helper);

  gum_exec_ctx_ensure_helper_reachable (ctx, code_slab, slow_slab, cw,
      &ctx->last_prolog_full, gum_exec_ctx_write_full_prolog_helper);
  gum_exec_ctx_ensure_helper_reachable (ctx, code_slab, slow_slab, cw,
      &ctx->last_epilog_full, gum_exec_ctx_write_full_epilog_helper);

  gum_exec_ctx_ensure_helper_reachable (ctx, code_slab, slow_slab, cw,
      &ctx->last_invalidator, gum_exec_ctx_write_invalidator);
  ctx->code_slab->invalidator = ctx->last_invalidator;
  ctx->slow_slab->invalidator = ctx->last_invalidator;

  /*
   * Remember the helpers reachable from this code slab, so that any future
   * backpatching of a call/jump emitted into it (which may happen long after
   * ctx->last_* has moved on to a slab too far away for a direct branch) can
   * still reach a prolog/epilog helper using an immediate branch.
   */
  ctx->code_slab->prolog_minimal = ctx->last_prolog_minimal;
  ctx->code_slab->epilog_minimal = ctx->last_epilog_minimal;
  ctx->code_slab->prolog_full = ctx->last_prolog_full;
  ctx->code_slab->epilog_full = ctx->last_epilog_full;
}

static void
gum_exec_ctx_write_minimal_prolog_helper (GumExecCtx * ctx,
                                          GumArm64Writer * cw)
{
  gum_exec_ctx_write_prolog_helper (ctx, GUM_PROLOG_MINIMAL, cw);
}

static void
gum_exec_ctx_write_minimal_epilog_helper (GumExecCtx * ctx,
                                          GumArm64Writer * cw)
{
  gum_exec_ctx_write_epilog_helper (ctx, GUM_PROLOG_MINIMAL, cw);
}

static void
gum_exec_ctx_write_full_prolog_helper (GumExecCtx * ctx,
                                       GumArm64Writer * cw)
{
  gum_exec_ctx_write_prolog_helper (ctx, GUM_PROLOG_FULL, cw);
}

static void
gum_exec_ctx_write_full_epilog_helper (GumExecCtx * ctx,
                                       GumArm64Writer * cw)
{
  gum_exec_ctx_write_epilog_helper (ctx, GUM_PROLOG_FULL, cw);
}

static void
gum_exec_ctx_write_prolog_helper (GumExecCtx * ctx,
                                  GumPrologType type,
                                  GumArm64Writer * cw)
{
  gint i;

  /* X19 and LR have been pushed by our caller */

  /*
   * Our prolog and epilog code makes extensive use of the stack to store and
   * restore registers. However, on AArch64, the stack pointer must be aligned
   * to a 16-byte boundary when it is used to access memory. One anti-Frida
   * technique observed in the wild has been to deliberately misalign the stack
   * pointer to violate this assumption and cause Stalker to attempt to access
   * data on a misaligned stack.
   *
   * In order to mitigate this, we use another register as a proxy for the stack
   * pointer and use this to perform our loads and stores. Since the other
   * registers have no alignment requirements this avoids the issue. We still
   * have the issue that this proxy stack register needs to be saved somewhere
   * so that it can be restored when control returns to the target. We therefore
   * accept that this initial store must be carried out using the stack pointer
   * and will therefore incur an exception.
   *
   * Accordingly, we install an exception handler to cope with these exceptions
   * and this exception handler simply emulates the instruction in question.
   * Since we have minimized the amount of misaligned stack usage, we only have
   * a handful of instructions which we need to emulate and these can therefore
   * be whitelisted.
   *
   * As part of the prolog code, the stack is correctly aligned once the prolog
   * is opened (such that if we call any C code from within the Stalker engine
   * itself, it will take place with the stack aligned and hence we won't need
   * to emulate additional, compiler dependent, instructions). The stack pointer
   * is restored to its original (possibly misaligned) value once the epilogue
   * is executed.
   *
   * Note that in order to simplify this code, we also ensure that both the FULL
   * and MINIMAL prologs both store the register state in the GumCpuContext
   * format, although in the case of the MINIMAL context, it is not necessary to
   * save a number of the registers and these can simply be skipped by adjusting
   * the proxy stack pointer.
   */
  gum_arm64_writer_put_mov_reg_reg (cw, ARM64_REG_X19, ARM64_REG_SP);

  if (type == GUM_PROLOG_MINIMAL)
  {
#ifndef G_OS_NONE
    /* GumCpuContext.v[0:8] plus padding for v[8:32] */
    for (i = 6; i != -2; i -= 2)
    {
      gssize size = 2 * sizeof (GumArm64VectorReg);

      if (i == 6)
        size += (32 - 8) * sizeof (GumArm64VectorReg);

      gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
          ARM64_REG_Q0 + i, ARM64_REG_Q1 + i,
          ARM64_REG_X19, -size, GUM_INDEX_PRE_ADJUST);
    }
#endif

    /* GumCpuContext.{fp,lr}, LR being a placeholder updated below */
    gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
        ARM64_REG_FP, ARM64_REG_XZR,
        ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);

    /* GumCpuContext.x[19:29]: skipped as X19-X28 are callee-saved registers */
    gum_arm64_writer_put_sub_reg_reg_imm (cw, ARM64_REG_X19, ARM64_REG_X19,
        (29 - 19) * 8);

    /* GumCpuContext.x[1:19] */
    for (i = 17; i != -1; i -= 2)
    {
      gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
          ARM64_REG_X0 + i, ARM64_REG_X1 + i,
          ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);
    }

    /* GumCpuContext.{nzcv,x0} */
    gum_arm64_writer_put_mov_reg_nzcv (cw, ARM64_REG_X1);
    gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
        ARM64_REG_X1, ARM64_REG_X0,
        ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);

    /* GumCpuContext.{pc,sp} */
    gum_arm64_writer_put_sub_reg_reg_imm (cw, ARM64_REG_X19, ARM64_REG_X19, 16);
  }
  else if (type == GUM_PROLOG_FULL)
  {
    guint distance_to_top = 0;

#ifndef G_OS_NONE
    /* GumCpuContext.v[32] */
    for (i = 30; i != -2; i -= 2)
    {
      const gssize vector_pair_size = 2 * sizeof (GumArm64VectorReg);

      gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
          ARM64_REG_Q0 + i, ARM64_REG_Q1 + i,
          ARM64_REG_X19, -vector_pair_size,
          GUM_INDEX_PRE_ADJUST);

      distance_to_top += vector_pair_size;
    }
#endif

    /* GumCpuContext.{fp,lr}, LR being a placeholder updated below */
    gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
        ARM64_REG_FP, ARM64_REG_XZR,
        ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);
    distance_to_top += 16;

    /* GumCpuContext.x[1:29] */
    for (i = 27; i != -1; i -= 2)
    {
      if (i == 19)
      {
        /*
         * X19 has been stored above our CpuContext by the prologue code, we
         * reach up and grab it here and copy it to the right place in the
         * context. Here we use X28 as scratch since it has already been saved
         * in the context above.
         */
        gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X28,
            ARM64_REG_X19, distance_to_top);
        gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
            ARM64_REG_X28, ARM64_REG_X20,
            ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);
        distance_to_top += 16;
        continue;
      }

      gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
          ARM64_REG_X0 + i, ARM64_REG_X1 + i,
          ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);
      distance_to_top += 16;
    }

    /* GumCpuContext.{nzcv,x0} */
    gum_arm64_writer_put_mov_reg_nzcv (cw, ARM64_REG_X1);
    gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
        ARM64_REG_X1, ARM64_REG_X0,
        ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);
    distance_to_top += 16;

    /* GumCpuContext.{pc,sp} */
    gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X0,
        ARM64_REG_X19, distance_to_top + 16 + GUM_RED_ZONE_SIZE);
    gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
        ARM64_REG_XZR, ARM64_REG_X0,
        ARM64_REG_X19, -16, GUM_INDEX_PRE_ADJUST);
    distance_to_top += 16;
  }

  /*
   * Read the value of the LR stored by the prologue code above the CpuContext
   * and copy it to its correct place in the CpuContext structure.
   */
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X0, ARM64_REG_X19,
      sizeof (GumCpuContext) + 8);
  gum_arm64_writer_put_str_reg_reg_offset (cw, ARM64_REG_X0, ARM64_REG_X19,
      G_STRUCT_OFFSET (GumCpuContext, lr));

  /*
   * Store the value of X20 in its place above the GumCpuContext. We have to
   * add 8 bytes beyond the context to reach the value of LR pushed in the
   * prolog code.
   */
  gum_arm64_writer_put_str_reg_reg_offset (cw, ARM64_REG_X20, ARM64_REG_X19,
      sizeof (GumCpuContext) + 8);

  /* Align our stack pointer */
  gum_arm64_writer_put_and_reg_reg_imm (cw, ARM64_REG_SP, ARM64_REG_X19, ~0xf);

  /* Set X20 as our context pointer */
  gum_arm64_writer_put_mov_reg_reg (cw, ARM64_REG_X20, ARM64_REG_X19);

  gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_LR);
}

static void
gum_exec_ctx_write_epilog_helper (GumExecCtx * ctx,
                                  GumPrologType type,
                                  GumArm64Writer * cw)
{
  gint i;

  /* X19 and X20 have been pushed by our caller */

  if (type == GUM_PROLOG_FULL)
  {
    gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw,
        ARM64_REG_X0, ARM64_REG_X1,
        ARM64_REG_X20, G_STRUCT_OFFSET (GumCpuContext, x[19]),
        GUM_INDEX_SIGNED_OFFSET);
    gum_arm64_writer_put_stp_reg_reg_reg_offset (cw,
        ARM64_REG_X0, ARM64_REG_X1,
        ARM64_REG_X20, sizeof (GumCpuContext),
        GUM_INDEX_SIGNED_OFFSET);
  }

  if (type == GUM_PROLOG_MINIMAL)
  {
    /* GumCpuContext.{pc,sp}: skipped */
    gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X20, ARM64_REG_X20, 16);

    /* GumCpuContext.{nzcv,x[0]} */
    gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X1, ARM64_REG_X0,
        ARM64_REG_X20, 16, GUM_INDEX_POST_ADJUST);

    /* Restore status */
    gum_arm64_writer_put_mov_nzcv_reg (cw, ARM64_REG_X1);

    /* GumCpuContext.x[1:19] */
    for (i = 1; i != 19; i += 2)
    {
      gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw,
          ARM64_REG_X0 + i, ARM64_REG_X1 + i,
          ARM64_REG_X20, 16, GUM_INDEX_POST_ADJUST);
    }

    /* GumCpuContext.x[19:29]: skipped as X19-X28 are callee-saved registers */
    gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X20, ARM64_REG_X20,
        (29 - 19) * 8);

    /* Last chance to grab LR so we can return from this thunk */
    gum_arm64_writer_put_mov_reg_reg (cw, ARM64_REG_X19, ARM64_REG_LR);

    /* GumCpuContext.{fp,lr} */
    gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw,
        ARM64_REG_FP, ARM64_REG_LR,
        ARM64_REG_X20, 16, GUM_INDEX_POST_ADJUST);

#ifndef G_OS_NONE
    /* GumCpuContext.v[0:8] plus padding for v[8:32] */
    for (i = 0; i != 8; i += 2)
    {
      gssize size = 2 * sizeof (GumArm64VectorReg);

      if (i == 6)
        size += (32 - 8) * sizeof (GumArm64VectorReg);

      gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw,
          ARM64_REG_Q0 + i, ARM64_REG_Q1 + i,
          ARM64_REG_X20, size, GUM_INDEX_POST_ADJUST);
    }
#endif
  }
  else if (type == GUM_PROLOG_FULL)
  {
    /* GumCpuContext.{pc,sp}: skipped */
    gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X20, ARM64_REG_X20, 16);

    /* GumCpuContext.{nzcv,x[0]} */
    gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X1, ARM64_REG_X0,
        ARM64_REG_X20, 16, GUM_INDEX_POST_ADJUST);

    /* Restore status */
    gum_arm64_writer_put_mov_nzcv_reg (cw, ARM64_REG_X1);

    /* GumCpuContext.x[1:29] */
    for (i = 1; i != 29; i += 2)
    {
      if (i == 19)
      {
        /* We already dealt with X19 and X20 above */
        gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X20,
            ARM64_REG_X20, 16);
        continue;
      }

      gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw,
          ARM64_REG_X0 + i, ARM64_REG_X1 + i,
          ARM64_REG_X20, 16, GUM_INDEX_POST_ADJUST);
    }

    /* Last chance to grab LR so we can return from this thunk */
    gum_arm64_writer_put_mov_reg_reg (cw, ARM64_REG_X19, ARM64_REG_LR);

    /* GumCpuContext.{fp,lr} */
    gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw,
        ARM64_REG_FP, ARM64_REG_LR,
        ARM64_REG_X20, 16, GUM_INDEX_POST_ADJUST);

#ifndef G_OS_NONE
    /* GumCpuContext.v[0:32] */
    for (i = 0; i != 32; i += 2)
    {
      gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw,
          ARM64_REG_Q0 + i, ARM64_REG_Q1 + i,
          ARM64_REG_X20, 2 * sizeof (GumArm64VectorReg), GUM_INDEX_POST_ADJUST);
    }
#endif
  }

  gum_arm64_writer_put_mov_reg_reg (cw, ARM64_REG_SP, ARM64_REG_X20);

  gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_X19);
}

static void
gum_exec_ctx_write_invalidator (GumExecCtx * ctx,
                                GumArm64Writer * cw)
{
  gum_exec_ctx_write_prolog (ctx, GUM_PROLOG_MINIMAL, cw);

  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_recompile_and_switch_block), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_REGISTER, ARM64_REG_X17);

  gum_exec_ctx_write_epilog (ctx, GUM_PROLOG_MINIMAL, cw);
  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X16, ARM64_REG_X17,
      ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE, GUM_INDEX_POST_ADJUST);

  gum_exec_block_write_exec_generated_code (cw, ctx);
}

static void
gum_exec_ctx_ensure_helper_reachable (GumExecCtx * ctx,
                                      GumSlab * code_slab,
                                      GumSlab * slow_slab,
                                      GumArm64Writer * cw,
                                      gpointer * helper_ptr,
                                      GumExecHelperWriteFunc write)
{
  gboolean code_reachable, slow_reachable;
  gpointer start;

  code_reachable = gum_exec_ctx_is_helper_reachable (ctx, code_slab, cw,
      helper_ptr);
  slow_reachable = gum_exec_ctx_is_helper_reachable (ctx, slow_slab, cw,
      helper_ptr);
  if (code_reachable && slow_reachable)
    return;

  start = gum_slab_cursor (code_slab);
  gum_stalker_thaw (ctx->stalker, start, gum_slab_available (code_slab));
  gum_arm64_writer_reset (cw, start);
  *helper_ptr = gum_arm64_writer_cur (cw);

  write (ctx, cw);

  gum_arm64_writer_flush (cw);
  gum_stalker_freeze (ctx->stalker, cw->base, gum_arm64_writer_offset (cw));

  gum_slab_reserve (code_slab, gum_arm64_writer_offset (cw));
}

static gboolean
gum_exec_ctx_is_helper_reachable (GumExecCtx * ctx,
                                  GumSlab * slab,
                                  GumArm64Writer * cw,
                                  gpointer * helper_ptr)
{
  GumAddress helper, start, end;

  helper = GUM_ADDRESS (*helper_ptr);
  if (helper == 0)
    return FALSE;

  start = GUM_ADDRESS (gum_slab_start (slab));
  end = GUM_ADDRESS (gum_slab_end (slab));

  if (!gum_arm64_writer_can_branch_directly_between (cw, start, helper))
    return FALSE;

  return gum_arm64_writer_can_branch_directly_between (cw, end, helper);
}

static void
gum_exec_ctx_write_push_branch_target_address (GumExecCtx * ctx,
                                               const GumBranchTarget * target,
                                               GumGeneratorContext * gc,
                                               GumArm64Writer * cw)
{
  if (target->reg == ARM64_REG_INVALID)
  {
    gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X15,
        GUM_ADDRESS (target->absolute_address));
    gum_arm64_writer_put_push_reg_reg (cw, ARM64_REG_X15, ARM64_REG_X15);
  }
  else
  {
    gum_exec_ctx_load_real_register_into (ctx, ARM64_REG_X15, target->reg, gc,
        cw);
    if ((ctx->stalker->cpu_features & GUM_CPU_PTRAUTH) != 0)
      gum_arm64_writer_put_xpaci_reg (cw, ARM64_REG_X15);
    gum_arm64_writer_put_push_reg_reg (cw, ARM64_REG_X15, ARM64_REG_X15);
  }
}

static void
gum_exec_ctx_load_real_register_into (GumExecCtx * ctx,
                                      arm64_reg target_register,
                                      arm64_reg source_register,
                                      GumGeneratorContext * gc,
                                      GumArm64Writer * cw)
{
  if (gc->opened_prolog == GUM_PROLOG_MINIMAL)
  {
    gum_exec_ctx_load_real_register_from_minimal_frame_into (ctx,
        target_register, source_register, gc, cw);
    return;
  }
  else if (gc->opened_prolog == GUM_PROLOG_FULL)
  {
    gum_exec_ctx_load_real_register_from_full_frame_into (ctx, target_register,
        source_register, gc, cw);
    return;
  }

  g_assert_not_reached ();
}

/*
 * The layout of the MINIMAL context is actually the same as the FULL context,
 * except that the callee saved registers are not stored into the GumCpuContext.
 * Instead we must retrieve these directly from the register itself. With the
 * exception of X19 and X20 which are used in the prolog/epilog itself, we
 * deliberately therefore avoid using these callee saved registers since they
 * are not restored from the MINIMAL context. Since they are callee saved, any
 * C functions which are called from Stalker will be guaranteed not to clobber
 * them either.
 */
static void
gum_exec_ctx_load_real_register_from_minimal_frame_into (
    GumExecCtx * ctx,
    arm64_reg target_register,
    arm64_reg source_register,
    GumGeneratorContext * gc,
    GumArm64Writer * cw)
{
  if (source_register >= ARM64_REG_X0 && source_register <= ARM64_REG_X18)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, target_register, ARM64_REG_X20,
        G_STRUCT_OFFSET (GumCpuContext, x) +
        ((source_register - ARM64_REG_X0) * 8));
  }
  else if (source_register == ARM64_REG_X19 || source_register == ARM64_REG_X20)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, target_register, ARM64_REG_X20,
        sizeof (GumCpuContext) + ((source_register - ARM64_REG_X19) * 8));
  }
  else if (source_register == ARM64_REG_X29)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, target_register, ARM64_REG_X20,
        G_STRUCT_OFFSET (GumCpuContext, fp));
  }
  else if (source_register == ARM64_REG_X30)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, target_register, ARM64_REG_X20,
        G_STRUCT_OFFSET (GumCpuContext, lr));
  }
  else
  {
    gum_arm64_writer_put_mov_reg_reg (cw, target_register, source_register);
  }
}

static void
gum_exec_ctx_load_real_register_from_full_frame_into (GumExecCtx * ctx,
                                                      arm64_reg target_register,
                                                      arm64_reg source_register,
                                                      GumGeneratorContext * gc,
                                                      GumArm64Writer * cw)
{
  if (source_register >= ARM64_REG_X0 && source_register <= ARM64_REG_X28)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, target_register, ARM64_REG_X20,
        G_STRUCT_OFFSET (GumCpuContext, x) +
        ((source_register - ARM64_REG_X0) * 8));
  }
  else if (source_register == ARM64_REG_X29)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, target_register, ARM64_REG_X20,
        G_STRUCT_OFFSET (GumCpuContext, fp));
  }
  else if (source_register == ARM64_REG_X30)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, target_register, ARM64_REG_X20,
        G_STRUCT_OFFSET (GumCpuContext, lr));
  }
  else
  {
    gum_arm64_writer_put_mov_reg_reg (cw, target_register, source_register);
  }
}

/*
 * This exception handler deals with exceptions caused by attempts to access the
 * stack when it isn't 16-byte aligned. Anti-Frida techniques have been observed
 * in the wild where the stack is deliberately misaligned to cause Stalker to
 * crash when it executes. Since an exception is only thrown when an attempt is
 * made to load or store from a misaligned stack pointer, this anti-Frida code
 * can misalign the stack and cause a branch without accessing stack data and
 * hence therefore force FRIDA to deal with a misaligned stack without
 * incurring any exceptions itself.
 *
 * We cope with this scenario by making use of a register to act as a proxy for
 * the stack pointer during the prolog and epilogue (where extensive use of the
 * stack is made) and having the prolog ensure the stack is aligned once it has
 * finished executing. Since all C code called from within Stalker must be
 * called from within a prolog, we can therefore ensure that no such
 * misalignment errors occur during its execution.
 *
 * This still leaves the matter of the initial instructions at the start of the
 * prolog and end of the epilogue which must save/restore the proxy register, we
 * emulate these instructions in this exception handler and advance the
 * instruction pointer. There is also code on the fast path which executes
 * outside a prolog, this code has been rationalised to make use of a minimum
 * selection of instructions which operate on the stack and therefore we only
 * need to emulate a handful of individual instructions below.
 */
static gboolean
gum_exec_ctx_try_handle_exception (GumExecCtx * ctx,
                                   GumExceptionDetails * details)
{
  GumCpuContext * cpu_context = &details->context;
  const guint32 * insn;

  insn = GSIZE_TO_POINTER (cpu_context->pc);

  if (!gum_exec_ctx_contains (ctx, insn))
    return FALSE;

  if (cpu_context->sp % GUM_STACK_ALIGNMENT == 0)
    return FALSE;

  switch (GUINT32_FROM_LE (*insn))
  {
    /* STP */
    case 0xa9bf07e0: /* stp x0, x1, [sp, #-0x10]! */
      gum_exec_ctx_handle_stp (cpu_context, ARM64_REG_X0, ARM64_REG_X1, 16);
      return TRUE;

    case 0xa9b747f0: /* stp x16, x17, [sp, #-(16 + GUM_RED_ZONE_SIZE)]! */
      gum_exec_ctx_handle_stp (cpu_context, ARM64_REG_X16, ARM64_REG_X17,
          16 + GUM_RED_ZONE_SIZE);
      return TRUE;

    case 0xa9b77bf3: /* stp x19, x30, [sp, #-(16 + GUM_RED_ZONE_SIZE)]! */
      gum_exec_ctx_handle_stp (cpu_context, ARM64_REG_X19, ARM64_REG_X30,
          16 + GUM_RED_ZONE_SIZE);
      return TRUE;

    /* LDP */
    case 0xa8c107e0: /* ldp x0, x1, [sp], #0x10 */
      gum_exec_ctx_handle_ldp (cpu_context, ARM64_REG_X0, ARM64_REG_X1, 16);
      return TRUE;

    case 0xa8c947f0: /* ldp x16, x17, [sp], #(16 + GUM_RED_ZONE_SIZE) */
      gum_exec_ctx_handle_ldp (cpu_context, ARM64_REG_X16, ARM64_REG_X17,
          16 + GUM_RED_ZONE_SIZE);
      return TRUE;

    case 0xa8c953f3: /* ldp x19, x20, [sp], #(16 + GUM_RED_ZONE_SIZE) */
      gum_exec_ctx_handle_ldp (cpu_context, ARM64_REG_X19, ARM64_REG_X20,
          16 + GUM_RED_ZONE_SIZE);
      return TRUE;

    default:
      break;
  }

  return FALSE;
}

static void
gum_exec_ctx_handle_stp (GumCpuContext * cpu_context,
                         arm64_reg reg1,
                         arm64_reg reg2,
                         gsize offset)
{
  guint64 * sp;

  cpu_context->sp -= offset;

  sp = GSIZE_TO_POINTER (cpu_context->sp);
  sp[0] = gum_exec_ctx_read_register (cpu_context, reg1);
  sp[1] = gum_exec_ctx_read_register (cpu_context, reg2);

  cpu_context->pc += 4;
}

static void
gum_exec_ctx_handle_ldp (GumCpuContext * cpu_context,
                         arm64_reg reg1,
                         arm64_reg reg2,
                         gsize offset)
{
  guint64 * sp = GSIZE_TO_POINTER (cpu_context->sp);

  gum_exec_ctx_write_register (cpu_context, reg1, sp[0]);
  gum_exec_ctx_write_register (cpu_context, reg2, sp[1]);

  cpu_context->sp += offset;
  cpu_context->pc += 4;
}

static guint64
gum_exec_ctx_read_register (GumCpuContext * cpu_context,
                            arm64_reg reg)
{
  if (reg >= ARM64_REG_X0 && reg <= ARM64_REG_X28)
    return cpu_context->x[reg - ARM64_REG_X0];

  switch (reg)
  {
    case ARM64_REG_X29:
      return cpu_context->fp;
    case ARM64_REG_X30:
      return cpu_context->lr;
    default:
      g_assert_not_reached ();
  }
}

static void
gum_exec_ctx_write_register (GumCpuContext * cpu_context,
                             arm64_reg reg,
                             guint64 value)
{
  if (reg >= ARM64_REG_X0 && reg <= ARM64_REG_X28)
  {
    cpu_context->x[reg - ARM64_REG_X0] = value;
    return;
  }

  switch (reg)
  {
    case ARM64_REG_X29:
      cpu_context->fp = value;
      break;
    case ARM64_REG_X30:
      cpu_context->lr = value;
      break;
    default:
      g_assert_not_reached ();
  }
}

static GumExecBlock *
gum_exec_block_new (GumExecCtx * ctx)
{
  GumStalker * stalker = ctx->stalker;
  GumCodeSlab * code_slab;
  GumSlowSlab * slow_slab;
  GumDataSlab * data_slab;
  gsize code_available, slow_available;
  GumExecBlock * block;

  gum_exec_block_maybe_create_new_code_slabs (ctx);
  gum_exec_block_maybe_create_new_data_slab (ctx);

  code_slab = ctx->code_slab;
  slow_slab = ctx->slow_slab;
  data_slab = ctx->data_slab;

  code_available = gum_slab_available (&code_slab->slab);
  slow_available = gum_slab_available (&slow_slab->slab);

  block = gum_slab_reserve (&data_slab->slab, sizeof (GumExecBlock));

  block->next = ctx->block_list;
  ctx->block_list = block;

  block->ctx = ctx;
  block->code_slab = code_slab;
  block->slow_slab = slow_slab;

  block->code_start = gum_slab_cursor (&code_slab->slab);
  block->slow_start = gum_slab_cursor (&slow_slab->slab);

  gum_stalker_thaw (stalker, block->code_start, code_available);
  gum_stalker_thaw (stalker, block->slow_start, slow_available);

  return block;
}

static void
gum_exec_block_maybe_create_new_code_slabs (GumExecCtx * ctx)
{
  gsize code_available, slow_available;
  gboolean enough_code, enough_slow;

  code_available = gum_slab_available (&ctx->code_slab->slab);
  slow_available = gum_slab_available (&ctx->slow_slab->slab);

  /*
   * Whilst we don't write the inline cache entry into the code slab any more,
   * we do write an unrolled loop which walks the table looking for the right
   * entry, so we need to ensure we have some extra space for that anyway.
   */
  enough_code = code_available >= GUM_EXEC_BLOCK_MIN_CAPACITY +
      gum_stalker_get_ic_entry_size (ctx->stalker);
  enough_slow = slow_available >= GUM_EXEC_BLOCK_MIN_CAPACITY;
  if (enough_code && enough_slow)
    return;

  gum_exec_ctx_add_code_slab (ctx, gum_code_slab_new (ctx));
  gum_exec_ctx_add_slow_slab (ctx, gum_slab_end (&ctx->code_slab->slab));

  gum_exec_ctx_ensure_inline_helpers_reachable (ctx);
}

static void
gum_exec_block_maybe_create_new_data_slab (GumExecCtx * ctx)
{
  GumDataSlab * data_slab = ctx->data_slab;
  gsize data_available;

  data_available = gum_slab_available (&data_slab->slab);

  if (data_available >= GUM_DATA_BLOCK_MIN_CAPACITY +
      gum_stalker_get_ic_entry_size (ctx->stalker))
    return;

  data_slab = gum_exec_ctx_add_data_slab (ctx, gum_data_slab_new (ctx));
}

static void
gum_exec_block_clear (GumExecBlock * block)
{
  GumCalloutEntry * entry;

  for (entry = gum_exec_block_get_last_callout_entry (block);
      entry != NULL;
      entry = entry->next)
  {
    if (entry->data_destroy != NULL)
      entry->data_destroy (entry->data);
  }
  block->last_callout_offset = 0;

  block->storage_block = NULL;
}

static gconstpointer
gum_exec_block_check_address_for_exclusion (GumExecBlock * block,
                                            gconstpointer address)
{
  GumExecCtx * ctx = block->ctx;

  if (ctx->activation_target != NULL)
    return address;

  if (gum_stalker_is_excluding (ctx->stalker, address))
    return NULL;

  return address;
}

static void
gum_exec_block_commit (GumExecBlock * block)
{
  GumStalker * stalker = block->ctx->stalker;
  gsize snapshot_size;

  snapshot_size =
      gum_stalker_snapshot_space_needed_for (stalker, block->real_size);
  memcpy (gum_exec_block_get_snapshot_start (block), block->real_start,
      snapshot_size);

  block->capacity = block->code_size + snapshot_size;

  gum_slab_reserve (&block->code_slab->slab, block->capacity);
  gum_stalker_freeze (stalker, block->code_start, block->code_size);

  gum_slab_reserve (&block->slow_slab->slab, block->slow_size);
  gum_stalker_freeze (stalker, block->slow_start, block->slow_size);
}

static void
gum_exec_block_invalidate (GumExecBlock * block)
{
  GumExecCtx * ctx = block->ctx;
  GumStalker * stalker = ctx->stalker;
  GumArm64Writer * cw = &ctx->code_writer;
  const gsize max_size = GUM_INVALIDATE_TRAMPOLINE_MAX_SIZE;
  gconstpointer already_saved = cw->code + 1;

  g_assert (block->code_size >= GUM_INVALIDATE_TRAMPOLINE_MAX_SIZE);

  gum_stalker_thaw (stalker, block->code_start, max_size);
  gum_arm64_writer_reset (cw, block->code_start);

  gum_arm64_writer_put_b_label (cw, already_saved);
  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16, ARM64_REG_X17,
      ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE), GUM_INDEX_PRE_ADJUST);
  gum_arm64_writer_put_label (cw, already_saved);
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X17, GUM_ADDRESS (block));
  gum_arm64_writer_put_b_imm (cw, GUM_ADDRESS (block->code_slab->invalidator));

  gum_arm64_writer_flush (cw);
  g_assert (gum_arm64_writer_offset (cw) <= max_size);
  gum_stalker_freeze (stalker, block->code_start, max_size);
}

static gpointer
gum_exec_block_get_snapshot_start (GumExecBlock * block)
{
  return block->code_start + block->code_size;
}

static GumCalloutEntry *
gum_exec_block_get_last_callout_entry (const GumExecBlock * block)
{
  const guint last_callout_offset = block->last_callout_offset;

  if (last_callout_offset == 0)
    return NULL;

  return (GumCalloutEntry *) (block->code_start + last_callout_offset);
}

static void
gum_exec_block_set_last_callout_entry (GumExecBlock * block,
                                       GumCalloutEntry * entry)
{
  block->last_callout_offset = (guint8 *) entry - block->code_start;
}

static void
gum_exec_block_backpatch_call (GumExecBlock * block,
                               GumExecBlock * from,
                               gpointer from_insn,
                               gsize code_offset,
                               GumPrologType opened_prolog,
                               gpointer ret_real_address)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  gpointer target;
  guint8 * code_start = from->code_start + code_offset;
  const gsize code_max_size = from->code_size - code_offset;
  GumArm64Writer * cw;

  just_unfollowed = block == NULL;
  if (just_unfollowed)
    return;

  ctx = block->ctx;
  if (!gum_exec_ctx_may_now_backpatch (ctx, block))
    return;

  target = block->code_start;
  gum_exec_ctx_query_block_switch_callback (ctx, block, block->real_start,
      from_insn, &target);

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (ctx->stalker, code_start, code_max_size);

  cw = &ctx->code_writer;
  gum_arm64_writer_reset (cw, code_start);

  if (opened_prolog != GUM_PROLOG_NONE)
  {
    gpointer epilog = (opened_prolog == GUM_PROLOG_MINIMAL)
        ? from->code_slab->epilog_minimal
        : from->code_slab->epilog_full;
    gum_exec_ctx_write_epilog_with_helper (epilog, cw);
  }

  gum_exec_ctx_write_adjust_depth (ctx, cw, 1);

  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_LR,
      GUM_ADDRESS (ret_real_address));
  gum_exec_block_write_jmp_to_block_start (block, target);

  gum_arm64_writer_flush (cw);
  g_assert (gum_arm64_writer_offset (cw) <= code_max_size);
  gum_stalker_freeze (ctx->stalker, code_start, code_max_size);

  gum_spinlock_release (&ctx->code_lock);

  if (ctx->observer != NULL)
  {
    GumBackpatch p;

    p.type = GUM_BACKPATCH_CALL;
    p.to = block->real_start;
    p.from = from->real_start;
    p.from_insn = from_insn;
    p.call.code_offset = code_offset;
    p.call.opened_prolog = opened_prolog;
    p.call.ret_real_address = ret_real_address;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

static void
gum_exec_block_backpatch_jmp (GumExecBlock * block,
                              GumExecBlock * from,
                              gpointer from_insn,
                              gsize code_offset,
                              GumPrologType opened_prolog)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  gpointer target;
  guint8 * code_start = from->code_start + code_offset;
  const gsize code_max_size = from->code_size - code_offset;
  GumArm64Writer * cw;

  just_unfollowed = block == NULL;
  if (just_unfollowed)
    return;

  ctx = block->ctx;
  if (!gum_exec_ctx_may_now_backpatch (ctx, block))
    return;

  target = block->code_start;
  gum_exec_ctx_query_block_switch_callback (ctx, block, block->real_start,
      from_insn, &target);

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (ctx->stalker, code_start, code_max_size);

  cw = &ctx->code_writer;
  gum_arm64_writer_reset (cw, code_start);

  if (opened_prolog != GUM_PROLOG_NONE)
  {
    gpointer epilog = (opened_prolog == GUM_PROLOG_MINIMAL)
        ? from->code_slab->epilog_minimal
        : from->code_slab->epilog_full;
    gum_exec_ctx_write_epilog_with_helper (epilog, cw);
  }

  gum_exec_block_write_jmp_to_block_start (block, target);

  gum_arm64_writer_flush (cw);
  g_assert (gum_arm64_writer_offset (cw) <= code_max_size);
  gum_stalker_freeze (ctx->stalker, code_start, code_max_size);

  gum_spinlock_release (&ctx->code_lock);

  if (ctx->observer != NULL)
  {
    GumBackpatch p;

    p.type = GUM_BACKPATCH_JMP;
    p.to = block->real_start;
    p.from = from->real_start;
    p.from_insn = from_insn;
    p.jmp.code_offset = code_offset;
    p.jmp.opened_prolog = opened_prolog;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

/*
 * In AArch64, we are limited to 28-bit signed offsets for immediate branches.
 * If we need to branch a larger distance, then we must clobber a register to
 * hold the destination address. If this is the case, we first push those
 * registers beyond the red-zone and then perform the branch.
 *
 * Each GumExecBlock is initialized to start with a:
 *
 *   ldp x16, x17, [sp], #0x90
 *
 * Therefore if we are able to branch directly to a block, we skip this first
 * instruction. Since this is emitted before any code generated by the
 * transformer, this anomaly goes largely unnoticed by the user. However, care
 * must be taken when using the switch-block callback to take this into account.
 */
static void
gum_exec_block_write_jmp_to_block_start (GumExecBlock * block,
                                         gpointer block_start)
{
  GumArm64Writer * cw = &block->ctx->code_writer;
  const GumAddress address = GUM_ADDRESS (block_start);
  const GumAddress body_address = address + GUM_RESTORATION_PROLOG_SIZE;

  if (gum_arm64_writer_can_branch_directly_between (cw, cw->pc, body_address))
  {
    gum_arm64_writer_put_b_imm (cw, body_address);
  }
  else
  {
    gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16,
        ARM64_REG_X17, ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE),
        GUM_INDEX_PRE_ADJUST);
    gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16, address);
    gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_X16);
  }
}

static void
gum_exec_block_backpatch_inline_cache (GumExecBlock * block,
                                       GumExecBlock * from,
                                       gpointer from_insn)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  gpointer target;
  GumIcEntry * ic_entries;
  guint num_ic_entries, i;

  just_unfollowed = block == NULL;
  if (just_unfollowed)
    return;

  ctx = block->ctx;
  if (!gum_exec_ctx_may_now_backpatch (ctx, block))
    return;

  target = block->code_start;
  gum_exec_ctx_query_block_switch_callback (ctx, block, block->real_start,
      from_insn, &target);

  ic_entries = from->ic_entries;
  if (ic_entries == NULL)
    return;

  num_ic_entries = ctx->stalker->ic_entries;

  for (i = 0; i != num_ic_entries; i++)
  {
    if (ic_entries[i].real_start == NULL)
      break;
    if (ic_entries[i].real_start == block->real_start)
      return;
  }

  gum_spinlock_acquire (&ctx->code_lock);

  /*
   * Shift all of the entries in the inline cache down one space and insert
   * our new entry at the beginning. If the inline cache is full, then the last
   * entry in the list is effectively removed.
   */
  memmove (&ic_entries[1], &ic_entries[0],
      (num_ic_entries - 1) * sizeof (GumIcEntry));

  ic_entries[0].real_start = block->real_start;
  ic_entries[0].code_start = target;

  gum_spinlock_release (&ctx->code_lock);

  if (ctx->observer != NULL)
  {
    GumBackpatch p;

    p.type = GUM_BACKPATCH_INLINE_CACHE;
    p.to = block->real_start;
    p.from = from->real_start;
    p.from_insn = from_insn;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

static GumVirtualizationRequirements
gum_exec_block_virtualize_branch_insn (GumExecBlock * block,
                                       GumGeneratorContext * gc)
{
  GumExecCtx * ctx = block->ctx;
  GumInstruction * insn = gc->instruction;
  const guint id = insn->ci->id;
  GumArm64Writer * cw = gc->code_writer;
  cs_arm64 * arm64 = &insn->ci->detail->arm64;
  cs_arm64_op * op = &arm64->operands[0];
  cs_arm64_op * op2 = NULL;
  cs_arm64_op * op3 = NULL;
  arm64_cc cc = arm64->cc;
  gboolean is_conditional;
  GumBranchTarget target = { 0, };

  g_assert (arm64->op_count != 0);

  is_conditional = (id == ARM64_INS_B && cc != ARM64_CC_INVALID) ||
      (id == ARM64_INS_CBZ) || (id == ARM64_INS_CBNZ) ||
      (id == ARM64_INS_TBZ) || (id == ARM64_INS_TBNZ);

  target.origin_ip = insn->end;

  switch (id)
  {
    case ARM64_INS_B:
    case ARM64_INS_BL:
      g_assert (op->type == ARM64_OP_IMM);

      target.absolute_address = GSIZE_TO_POINTER (op->imm);
      target.reg = ARM64_REG_INVALID;

      break;
    case ARM64_INS_BR:
    case ARM64_INS_BRAA:
    case ARM64_INS_BRAAZ:
    case ARM64_INS_BRAB:
    case ARM64_INS_BRABZ:
    case ARM64_INS_BLR:
    case ARM64_INS_BLRAA:
    case ARM64_INS_BLRAAZ:
    case ARM64_INS_BLRAB:
    case ARM64_INS_BLRABZ:
      g_assert (op->type == ARM64_OP_REG);

      target.reg = op->reg;

      break;
    case ARM64_INS_CBZ:
    case ARM64_INS_CBNZ:
      op2 = &arm64->operands[1];

      g_assert (op->type == ARM64_OP_REG);
      g_assert (op2->type == ARM64_OP_IMM);

      target.absolute_address = GSIZE_TO_POINTER (op2->imm);
      target.reg = ARM64_REG_INVALID;

      break;
    case ARM64_INS_TBZ:
    case ARM64_INS_TBNZ:
      op2 = &arm64->operands[1];
      op3 = &arm64->operands[2];

      g_assert (op->type == ARM64_OP_REG);
      g_assert (op2->type == ARM64_OP_IMM);
      g_assert (op3->type == ARM64_OP_IMM);

      target.absolute_address = GSIZE_TO_POINTER (op3->imm);
      target.reg = ARM64_REG_INVALID;

      break;
    default:
      g_assert_not_reached ();
  }

  switch (id)
  {
    case ARM64_INS_B:
    case ARM64_INS_BR:
    case ARM64_INS_BRAA:
    case ARM64_INS_BRAAZ:
    case ARM64_INS_BRAB:
    case ARM64_INS_BRABZ:
    case ARM64_INS_CBZ:
    case ARM64_INS_CBNZ:
    case ARM64_INS_TBZ:
    case ARM64_INS_TBNZ:
    {
      gpointer is_false;
      GumExecCtxReplaceCurrentBlockFunc regular_entry_func, cond_entry_func;

      gum_arm64_relocator_skip_one (gc->relocator);

      is_false =
          GUINT_TO_POINTER ((GPOINTER_TO_UINT (insn->start) << 16) | 0xbeef);

      if (is_conditional)
      {
        gum_exec_block_close_prolog (block, gc, gc->code_writer);

        regular_entry_func = NULL;

        /* jump to is_false if is_false */
        switch (id)
        {
          case ARM64_INS_B:
          {
            arm64_cc not_cc;

            g_assert (cc != ARM64_CC_INVALID);
            g_assert (cc > ARM64_CC_INVALID);
            g_assert (cc <= ARM64_CC_NV);

            not_cc = cc + 2 * (cc % 2) - 1;
            gum_arm64_writer_put_b_cond_label (cw, not_cc, is_false);

            cond_entry_func = GUM_ENTRYGATE (jmp_cond_cc);

            break;
          }
          case ARM64_INS_CBZ:
            gum_arm64_writer_put_cbnz_reg_label (cw, op->reg, is_false);
            cond_entry_func = GUM_ENTRYGATE (jmp_cond_cbz);
            break;
          case ARM64_INS_CBNZ:
            gum_arm64_writer_put_cbz_reg_label (cw, op->reg, is_false);
            cond_entry_func = GUM_ENTRYGATE (jmp_cond_cbnz);
            break;
          case ARM64_INS_TBZ:
            gum_arm64_writer_put_tbnz_reg_imm_label (cw, op->reg, op2->imm,
                is_false);
            cond_entry_func = GUM_ENTRYGATE (jmp_cond_tbz);
            break;
          case ARM64_INS_TBNZ:
            gum_arm64_writer_put_tbz_reg_imm_label (cw, op->reg, op2->imm,
                is_false);
            cond_entry_func = GUM_ENTRYGATE (jmp_cond_tbnz);
            break;
          default:
            cond_entry_func = NULL;
            g_assert_not_reached ();
        }
      }
      else
      {
        if (target.reg != ARM64_REG_INVALID)
          regular_entry_func = GUM_ENTRYGATE (jmp_reg);
        else
          regular_entry_func = GUM_ENTRYGATE (jmp_imm);
        cond_entry_func = NULL;
      }

      gum_exec_block_write_jmp_transfer_code (block, &target,
          is_conditional ? cond_entry_func : regular_entry_func, gc);

      if (is_conditional)
      {
        GumBranchTarget cond_target = { 0, };

        cond_target.absolute_address = insn->end;
        cond_target.reg = ARM64_REG_INVALID;

        gum_arm64_writer_put_label (cw, is_false);

        gum_exec_block_write_jmp_transfer_code (block, &cond_target,
            cond_entry_func, gc);
      }

      break;
    }
    case ARM64_INS_BL:
    case ARM64_INS_BLR:
    case ARM64_INS_BLRAA:
    case ARM64_INS_BLRAAZ:
    case ARM64_INS_BLRAB:
    case ARM64_INS_BLRABZ:
    {
      gboolean target_is_excluded = FALSE;

      if ((ctx->sink_mask & GUM_CALL) != 0)
      {
        gum_exec_block_write_call_event_code (block, &target, gc,
            GUM_CODE_INTERRUPTIBLE);
      }

      if (target.reg == ARM64_REG_INVALID &&
          ctx->activation_target == NULL)
      {
        target_is_excluded =
            gum_stalker_is_excluding (ctx->stalker, target.absolute_address);
      }

      if (target_is_excluded)
      {
        GumBranchTarget next_instruction = { 0, };

        gum_exec_block_close_prolog (block, gc, gc->code_writer);

        /*
         * Since write_begin_call() and write_end_call() are implemented as
         * generated code, we can make the necessary updates to the ExecCtx
         * without the overhead of opening and closing a prolog.
         */
        gum_exec_ctx_write_begin_call (ctx, cw, insn->end);
        gum_arm64_relocator_write_one (gc->relocator);
#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
        gum_metal_hash_table_insert (ctx->excluded_calls, cw->code, insn->end);
#endif
        gum_exec_ctx_write_end_call (ctx, cw);

        next_instruction.absolute_address = insn->end;
        next_instruction.reg = ARM64_REG_INVALID;
        gum_exec_block_write_jmp_transfer_code (block, &next_instruction,
            GUM_ENTRYGATE (excluded_call_imm), gc);

        return GUM_REQUIRE_NOTHING;
      }

      gum_arm64_relocator_skip_one (gc->relocator);
      gum_exec_block_write_call_invoke_code (block, &target, gc);

      break;
    }
    default:
      g_assert_not_reached ();
  }

  return GUM_REQUIRE_NOTHING;
}

static GumVirtualizationRequirements
gum_exec_block_virtualize_ret_insn (GumExecBlock * block,
                                    GumGeneratorContext * gc)
{
  GumInstruction * insn;
  cs_arm64 * arm64;
  cs_arm64_op * op;
  arm64_reg ret_reg;

  if ((block->ctx->sink_mask & GUM_RET) != 0)
    gum_exec_block_write_ret_event_code (block, gc, GUM_CODE_INTERRUPTIBLE);

  insn = gc->instruction;
  arm64 = &insn->ci->detail->arm64;

  if (arm64->op_count == 0)
  {
    ret_reg = ARM64_REG_X30;
  }
  else
  {
    g_assert (arm64->op_count == 1);

    op = &arm64->operands[0];
    g_assert (op->type == ARM64_OP_REG);

    ret_reg = op->reg;
  }
  gum_arm64_relocator_skip_one (gc->relocator);
  gum_exec_block_write_ret_transfer_code (block, gc, ret_reg);

  return GUM_REQUIRE_NOTHING;
}

static GumVirtualizationRequirements
gum_exec_block_virtualize_sysenter_insn (GumExecBlock * block,
                                         GumGeneratorContext * gc)
{
#ifdef HAVE_LINUX
  return gum_exec_block_virtualize_linux_sysenter (block, gc);
#else
  return GUM_REQUIRE_RELOCATION;
#endif
}

#ifdef HAVE_LINUX

static GumVirtualizationRequirements
gum_exec_block_virtualize_linux_sysenter (GumExecBlock * block,
                                          GumGeneratorContext * gc)
{
  GumArm64Writer * cw = gc->code_writer;
  const cs_insn * insn = gc->instruction->ci;
  gconstpointer perform_clone_syscall = cw->code + 1;
  gconstpointer perform_regular_syscall = cw->code + 2;
  gconstpointer perform_next_instruction = cw->code + 3;

  gum_arm64_relocator_skip_one (gc->relocator);

  if (gc->opened_prolog != GUM_PROLOG_NONE)
    gum_exec_block_close_prolog (block, gc, cw);

  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X15, ARM64_REG_X17,
      ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE), GUM_INDEX_PRE_ADJUST);
  gum_arm64_writer_put_mov_reg_nzcv (cw, ARM64_REG_X15);

  gum_arm64_writer_put_sub_reg_reg_imm (cw, ARM64_REG_X17,
      ARM64_REG_X8, __NR_clone);
  gum_arm64_writer_put_cbz_reg_label (cw, ARM64_REG_X17,
      perform_clone_syscall);
  gum_arm64_writer_put_b_label (cw, perform_regular_syscall);

  gum_arm64_writer_put_label (cw, perform_clone_syscall);
  gum_arm64_writer_put_mov_nzcv_reg (cw, ARM64_REG_X15);
  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X15,
      ARM64_REG_X17, ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE,
      GUM_INDEX_POST_ADJUST);
  gum_exec_block_put_aligned_syscall (block, gc, insn);
  gum_arm64_writer_put_b_label (cw, perform_next_instruction);

  gum_arm64_writer_put_label (cw, perform_regular_syscall);
  gum_arm64_writer_put_mov_nzcv_reg (cw, ARM64_REG_X15);
  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X15,
      ARM64_REG_X17, ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE,
      GUM_INDEX_POST_ADJUST);
  gum_arm64_writer_put_bytes (cw, insn->bytes, 4);

  gum_arm64_writer_put_label (cw, perform_next_instruction);

  return GUM_REQUIRE_NOTHING;
}

static void
gum_exec_block_put_aligned_syscall (GumExecBlock * block,
                                    GumGeneratorContext * gc,
                                    const cs_insn * insn)
{
  GumArm64Writer * cw = gc->code_writer;
  gsize page_size, page_mask;
  guint page_offset_start, pad_start;
  guint page_offset_end, pad_end;
  guint i;
  gconstpointer start = cw->code + 1;
  gconstpointer not_child = cw->code + 2;
  gconstpointer end = cw->code + 3;

  /*
   * If we have reached this point, then we know that the syscall being
   * performed was a clone. This means that both the calling thread and the
   * newly spawned thread will begin execution from the point immediately after
   * the SVC instruction. However, this causes a potential race condition, if
   * the calling thread attempts to either compile a new block, or backpatch
   * an existing one in the same page. During patching the block may be thawed
   * leading to the target thread (which may be stalled at the mercy of the
   * scheduler) attempting to execute a non-executable page.
   */

  page_size = gum_query_page_size ();
  page_mask = page_size - 1;

  page_offset_start = GPOINTER_TO_SIZE (cw->code) & page_mask;
  g_assert ((page_offset_start % 4) == 0);
  pad_start = (page_size - page_offset_start) / 4;

  if (pad_start != 0)
  {
    gum_arm64_writer_put_b_label (cw, start);

    for (i = 0; i != pad_start; i++)
      gum_arm64_writer_put_brk_imm (cw, 15);

    gum_arm64_writer_put_label (cw, start);
  }

  gum_arm64_writer_put_bytes (cw, insn->bytes, insn->size);
  gum_arm64_writer_put_cbnz_reg_label (cw, ARM64_REG_X0, not_child);

  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X17,
      GUM_ADDRESS (gc->instruction->start + GUM_RESTORATION_PROLOG_SIZE));
  gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_X17);

  gum_arm64_writer_put_label (cw, not_child);

  page_offset_end = GPOINTER_TO_SIZE (cw->code) & page_mask;
  g_assert ((page_offset_end % 4) == 0);
  pad_end = (page_size - page_offset_end) / 4;

  if (pad_end != 0)
  {
    gum_arm64_writer_put_b_label (cw, end);

    for (i = 0; i != pad_end; i++)
      gum_arm64_writer_put_brk_imm (cw, 16);

    gum_arm64_writer_put_label (cw, end);
  }
}

#endif

static void
gum_exec_block_write_call_invoke_code (GumExecBlock * block,
                                       const GumBranchTarget * target,
                                       GumGeneratorContext * gc)
{
  GumExecCtx * ctx = block->ctx;
  GumStalker * stalker = ctx->stalker;
  const gint trust_threshold = stalker->trust_threshold;
  GumArm64Writer * cw = gc->code_writer;
  GumArm64Writer * cws = gc->slow_writer;
  const GumAddress call_code_start = cw->pc;
  const GumPrologType opened_prolog = gc->opened_prolog;
  gboolean can_backpatch_statically;
  GumAddress ret_real_address = GUM_ADDRESS (gc->instruction->end);
  GumPrologType second_prolog;
  GumExecCtxReplaceCurrentBlockFunc entry_func;
  gconstpointer is_excluded = cw->code + 1;
  gconstpointer keep_this_blr = cw->code + 2;

  can_backpatch_statically =
      trust_threshold >= 0 &&
      target->reg == ARM64_REG_INVALID;

  gum_exec_ctx_write_adjust_depth (ctx, cw, 1);

  if (trust_threshold >= 0 && !can_backpatch_statically)
  {
    arm64_reg result_reg;

    gum_exec_block_close_prolog (block, gc, cw);

    /*
     * The call invoke code will transfer control to the slow slab in the event
     * of a cache miss. Otherwise, it will return control to the code (fast)
     * slab with the values of X16/X17 still pushed above the red-zone.
     *
     * If the low bit of the target address is set, then this denotes an
     * excluded call and we therefore branch further down the fast slab to
     * handle it.
     */
    result_reg = gum_exec_block_write_inline_cache_code (block, target->reg,
        cw, cws);

    gum_arm64_writer_put_tbnz_reg_imm_label (cw, result_reg, 0, is_excluded);

    gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_LR, ret_real_address);
    gum_arm64_writer_put_br_reg_no_auth (cw, result_reg);

    /* Handle excluded call */
    gum_arm64_writer_put_label (cw, is_excluded);
  }
  else
  {
    guint i;

    /*
     * If we don't have an inline cache (the branch is immediate or the code
     * isn't trusted), then we jump directly to the slow slab. If an indirect
     * branch is backpatched, then this will be overwritten along with the
     * subsequent padding with code to carry out a direct branch to the relevant
     * instrumented block.
     */
    gum_exec_block_write_slab_transfer_code (cw, cws);

    /*
     * We need some padding so the backpatching doesn't overwrite the return
     * handling logic below.
     */
    for (i = 0; i != 10; i++)
      gum_arm64_writer_put_nop (cw);
  }

  /* Slow Path */

  gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc, cws);
  second_prolog = gc->opened_prolog;

  if (target->reg == ARM64_REG_INVALID)
  {
    entry_func = GUM_ENTRYGATE (call_imm);
  }
  else
  {
    /*
     * Check if the call target is excluded, and branch further down the slow
     * slab to perform any necessary backpatching before it is called.
     */
    gum_exec_ctx_write_push_branch_target_address (ctx, target, gc, cws);
    gum_arm64_writer_put_pop_reg_reg (cws, ARM64_REG_X0, ARM64_REG_X1);

    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_check_address_for_exclusion), 2,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_REGISTER, ARM64_REG_X1);

    gum_arm64_writer_put_cbz_reg_label (cws, ARM64_REG_X0, keep_this_blr);

    entry_func = GUM_ENTRYGATE (call_reg);
  }

  /*
   * Fetch the relevant block using the entry gate.
   */
  gum_exec_ctx_write_push_branch_target_address (ctx, target, gc, cws);
  gum_arm64_writer_put_pop_reg_reg (cws, ARM64_REG_X0, ARM64_REG_X1);

  gum_arm64_writer_put_call_address_with_arguments (cws,
      GUM_ADDRESS (entry_func), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM64_REG_X1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  /* Perform any relevant backpatching */
  if (trust_threshold >= 0)
  {
    gum_arm64_writer_put_ldr_reg_address (cws, ARM64_REG_X0,
        GUM_ADDRESS (&ctx->current_block));
    gum_arm64_writer_put_ldr_reg_reg_offset (cws, ARM64_REG_X0,
        ARM64_REG_X0, 0);
  }

  if (can_backpatch_statically)
  {
    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_backpatch_call), 6,
        GUM_ARG_REGISTER, ARM64_REG_X0,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
        GUM_ARG_ADDRESS, call_code_start - GUM_ADDRESS (block->code_start),
        GUM_ARG_ADDRESS, GUM_ADDRESS (opened_prolog),
        GUM_ARG_ADDRESS, ret_real_address);
  }
  else if (trust_threshold >= 0)
  {
    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_backpatch_inline_cache), 3,
        GUM_ARG_REGISTER, ARM64_REG_X0,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  }

  /* Branch to the target block */
  gum_exec_block_close_prolog (block, gc, cws);
  gum_arm64_writer_put_ldr_reg_address (cws, ARM64_REG_LR, ret_real_address);
  gum_exec_block_write_exec_generated_code (cws, ctx);

  if (target->reg != ARM64_REG_INVALID)
  {
    GumInstruction * insn = gc->instruction;
    GumBranchTarget next_insn_as_target = { 0, };

    next_insn_as_target.absolute_address = insn->end;
    next_insn_as_target.reg = ARM64_REG_INVALID;

    /* Handle excluded calls */
    gum_arm64_writer_put_label (cws, keep_this_blr);

    if (target->reg != ARM64_REG_X1)
      gum_arm64_writer_put_mov_reg_reg (cws, ARM64_REG_X1, target->reg);

    /*
     * Backpatch the excluded call into the same inline cache used for
     * non-excluded calls above. We set the low bit of the code_address to
     * denote that this should be handled as an excluded call.
     */
    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_backpatch_excluded_call), 3,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_REGISTER, ARM64_REG_X1,
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

    gc->opened_prolog = second_prolog;

    gum_exec_block_close_prolog (block, gc, cws);

    /* Branch back to the fast slab to actually execute the target function */
    gum_exec_block_write_slab_transfer_code (cws, cw);

    /* Fast Path */
    gum_exec_ctx_write_begin_call (ctx, cw, gc->instruction->end);

    /*
     * We use the original target register as the target for the branch and
     * therefore don't have to strip the low bit from the target address
     * returned from the inline cache code.
     */
    gum_arm64_writer_put_bytes (cw, insn->start, insn->ci->size);

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
    gum_metal_hash_table_insert (ctx->excluded_calls, cw->code, insn->end);
#endif

    gum_exec_ctx_write_end_call (ctx, cw);

    /*
     * Write the standard jmp transfer code to vector to the instruction
     * immediately following the call.
     */
    gum_exec_block_write_jmp_transfer_code (block, &next_insn_as_target,
        GUM_ENTRYGATE (excluded_call_reg), gc);
  }
}

static void
gum_exec_ctx_write_begin_call (GumExecCtx * ctx,
                               GumArm64Writer * cw,
                               gpointer ret_addr)
{
  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE),
      GUM_INDEX_PRE_ADJUST);

  /* ctx->pending_return_location = ret_addr; */
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
      GUM_ADDRESS (&ctx->pending_return_location));
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X17,
      GUM_ADDRESS (ret_addr));
  gum_arm64_writer_put_str_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);

  /* ctx->pending_calls++ */
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
      GUM_ADDRESS (&ctx->pending_calls));
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);
  gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X17, ARM64_REG_X17, 1);
  gum_arm64_writer_put_str_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);

  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE,
      GUM_INDEX_POST_ADJUST);
}

static void
gum_exec_ctx_write_end_call (GumExecCtx * ctx,
                             GumArm64Writer * cw)
{
  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE),
      GUM_INDEX_PRE_ADJUST);

  /* ctx->pending_calls-- */
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
      GUM_ADDRESS (&ctx->pending_calls));
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);
  gum_arm64_writer_put_sub_reg_reg_imm (cw, ARM64_REG_X17, ARM64_REG_X17, 1);
  gum_arm64_writer_put_str_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);

  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE,
      GUM_INDEX_POST_ADJUST);
}

static void
gum_exec_block_backpatch_excluded_call (GumExecBlock * block,
                                        gpointer target,
                                        gpointer from_insn)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  GumIcEntry * ic_entries;
  guint num_ic_entries, i;

  just_unfollowed = block == NULL;
  if (just_unfollowed)
    return;

  ctx = block->ctx;
  if (!gum_exec_ctx_may_now_backpatch (ctx, block))
    return;
  if (!gum_exec_ctx_contains (ctx, target))
    return;

  ic_entries = block->ic_entries;
  g_assert (ic_entries != NULL);
  num_ic_entries = ctx->stalker->ic_entries;

  for (i = 0; i != num_ic_entries; i++)
  {
    if (ic_entries[i].real_start == target)
      return;
  }

  gum_spinlock_acquire (&ctx->code_lock);

  memmove (&ic_entries[1], &ic_entries[0],
      (num_ic_entries - 1) * sizeof (GumIcEntry));

  ic_entries[0].real_start = target;
  ic_entries[0].code_start = (guint8 *) target + 1;

  gum_spinlock_release (&ctx->code_lock);

  /*
   * We can prefetch backpatches to excluded calls since we are dealing with
   * real rather than instrumented addresses. Whilst blocks may not necessarily
   * be instrumented in the same location in the forkserver and its child
   * (block may not be compiled in the same order for example, or allocators may
   * be non-deterministic), since the address space is the same for each, the
   * real addresses which we are dealing with here will be the same. Note that
   * this is contrary to backpatches for returns into the slab.
   */
  if (ctx->observer != NULL)
  {
    GumBackpatch p;

    p.type = GUM_BACKPATCH_EXCLUDED_CALL;
    p.to = target;
    p.from = block->real_start;
    p.from_insn = from_insn;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

static void
gum_exec_block_write_jmp_transfer_code (GumExecBlock * block,
                                        const GumBranchTarget * target,
                                        GumExecCtxReplaceCurrentBlockFunc func,
                                        GumGeneratorContext * gc)
{
  GumStalker * stalker = block->ctx->stalker;
  const gint trust_threshold = stalker->trust_threshold;
  GumArm64Writer * cw = gc->code_writer;
  GumArm64Writer * cws = gc->slow_writer;
  const GumAddress code_start = cw->pc;
  const GumPrologType opened_prolog = gc->opened_prolog;
  gboolean can_backpatch_statically;

  can_backpatch_statically =
      trust_threshold >= 0 &&
      target->reg == ARM64_REG_INVALID;

  if (trust_threshold >= 0 && !can_backpatch_statically)
  {
    arm64_reg result_reg;

    gum_exec_block_close_prolog (block, gc, cw);

    /*
     * The call invoke code will transfer control to the slow slab in the event
     * of a cache miss. Otherwise, it will return control to the code (fast)
     * slab with the values of X16/X17 still pushed above the red-zone.
     */
    result_reg = gum_exec_block_write_inline_cache_code (block, target->reg,
        cw, cws);
    gum_arm64_writer_put_br_reg_no_auth (cw, result_reg);
  }
  else
  {
    guint i;

    /*
     * If we don't have an inline cache (the branch is immediate or the code
     * isn't trusted), then we jump directly to the slow slab. If an indirect
     * branch is backpatched, then this will be overwritten along with the
     * subsequent padding with code to carry out a direct branch to the relevant
     * instrumented block.
     */
    gum_exec_block_write_slab_transfer_code (cw, cws);

    for (i = 0; i != 10; i++)
      gum_arm64_writer_put_nop (cw);
  }

  gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc, cws);

  gum_exec_ctx_write_push_branch_target_address (block->ctx, target, gc, cws);
  gum_arm64_writer_put_pop_reg_reg (cws, ARM64_REG_X0, ARM64_REG_X1);

  gum_arm64_writer_put_call_address_with_arguments (cws,
      GUM_ADDRESS (func), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM64_REG_X1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  if (trust_threshold >= 0)
  {
    gum_arm64_writer_put_ldr_reg_address (cws, ARM64_REG_X0,
        GUM_ADDRESS (&block->ctx->current_block));
    gum_arm64_writer_put_ldr_reg_reg_offset (cws, ARM64_REG_X0,
        ARM64_REG_X0, 0);
  }

  if (can_backpatch_statically)
  {
    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_backpatch_jmp), 5,
        GUM_ARG_REGISTER, ARM64_REG_X0,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
        GUM_ARG_ADDRESS, code_start - GUM_ADDRESS (block->code_start),
        GUM_ARG_ADDRESS, GUM_ADDRESS (opened_prolog));
  }
  else if (trust_threshold >= 0)
  {
    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_backpatch_inline_cache), 3,
        GUM_ARG_REGISTER, ARM64_REG_X0,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  }

  gum_exec_block_close_prolog (block, gc, cws);
  gum_exec_block_write_exec_generated_code (cws, block->ctx);
}

static void
gum_exec_block_write_ret_transfer_code (GumExecBlock * block,
                                        GumGeneratorContext * gc,
                                        arm64_reg ret_reg)
{
  gum_exec_block_write_chaining_return_code (block, gc, ret_reg);
}

static void
gum_exec_block_write_chaining_return_code (GumExecBlock * block,
                                           GumGeneratorContext  * gc,
                                           arm64_reg ret_reg)
{
  GumArm64Writer * cw = gc->code_writer;
  GumArm64Writer * cws = gc->slow_writer;
  const gint trust_threshold = block->ctx->stalker->trust_threshold;
  GumExecCtx * ctx = block->ctx;
  arm64_reg result_reg;

  gum_exec_ctx_write_adjust_depth (ctx, cw, -1);

  gum_exec_block_close_prolog (block, gc, cw);

  if (trust_threshold >= 0)
  {
    /*
     * The call invoke code will transfer control to the slow slab in the event
     * of a cache miss. Otherwise, it will return control to the code (fast)
     * slab with the values of X16/X17 still pushed above the red-zone.
     */
    result_reg = gum_exec_block_write_inline_cache_code (block, ret_reg,
        cw, cws);
    gum_arm64_writer_put_br_reg_no_auth (cw, result_reg);
  }
  else
  {
    /*
     * If we don't have an inline cache, or we have a cache miss, we branch to
     * to the slow slab, never to return.
     */
    gum_exec_block_write_slab_transfer_code (cw, cws);
  }

  /* Slow Path */

  gum_arm64_writer_put_stp_reg_reg_reg_offset (cws, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE),
      GUM_INDEX_PRE_ADJUST);

  if (ret_reg != ARM64_REG_X16)
    gum_arm64_writer_put_mov_reg_reg (cws, ARM64_REG_X16, ret_reg);

  if ((ctx->stalker->cpu_features & GUM_CPU_PTRAUTH) != 0)
    gum_arm64_writer_put_xpaci_reg (cws, ARM64_REG_X16);

  gum_arm64_writer_put_ldr_reg_address (cws, ARM64_REG_X17,
      GUM_ADDRESS (&ctx->return_at));
  gum_arm64_writer_put_str_reg_reg_offset (cws, ARM64_REG_X16,
      ARM64_REG_X17, 0);

   gum_arm64_writer_put_ldp_reg_reg_reg_offset (cws, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE,
      GUM_INDEX_POST_ADJUST);

  gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc, cws);

  gum_arm64_writer_put_ldr_reg_address (cws, ARM64_REG_X0,
      GUM_ADDRESS (&ctx->return_at));
  gum_arm64_writer_put_ldr_reg_reg_offset (cws, ARM64_REG_X1, ARM64_REG_X0, 0);

  /* Fetch the target block */
  gum_arm64_writer_put_call_address_with_arguments (cws,
      GUM_ADDRESS (GUM_ENTRYGATE (ret)), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM64_REG_X1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  if (trust_threshold >= 0)
  {
    gum_arm64_writer_put_ldr_reg_address (cws, ARM64_REG_X0,
        GUM_ADDRESS (&ctx->current_block));
    gum_arm64_writer_put_ldr_reg_reg_offset (cws, ARM64_REG_X0,
        ARM64_REG_X0, 0);

    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_backpatch_inline_cache), 3,
        GUM_ARG_REGISTER, ARM64_REG_X0,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

    /*
     * If the user emits a BL instruction from within their transformer, then
     * this will result in control flow returning back to the code slab when
     * that function returns. The target address for this RET is therefore not
     * an instrumented block (e.g. a real address within the application which
     * has been instrumented), but actually a code address within an
     * instrumented block itself. This therefore needs to be treated as a
     * special case.
     *
     * Also since we cannot guarantee that code addresses between a Stalker
     * instance and an observer are identical (hence prefetched backpatches are
     * communicated in terms of their real address), whilst these can be
     * backpatched by adding them to the inline cache, they cannot be
     * prefetched.
     *
     * This block handles the backpatching of the entry into the inline cache,
     * but the block is still fetched by the call to `ret_slow_path` above, but
     * since ctx->current_block is not set and therefore the block is not
     * backpatched by backpatch_inline_cache() in the traditional way.
     */
    gum_exec_ctx_load_real_register_into (ctx, ARM64_REG_X1, ret_reg, gc, cws);
    gum_arm64_writer_put_call_address_with_arguments (cws,
        GUM_ADDRESS (gum_exec_block_backpatch_slab), 2,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_REGISTER, ARM64_REG_X1);
  }

  gum_exec_block_close_prolog (block, gc, cws);
  gum_exec_block_write_exec_generated_code (cws, ctx);
}

static void
gum_exec_block_write_slab_transfer_code (GumArm64Writer * from,
                                         GumArm64Writer * to)
{
  /*
   * We ensure that our code (fast) and slow slabs are allocated so that we can
   * perform an immediate branch between them. Otherwise Arm64Writer is forced
   * to perform an indirect branch and clobber a register (which is obviously
   * undesirable).
   */
  g_assert (gum_arm64_writer_can_branch_directly_between (from, from->pc,
      GUM_ADDRESS (to->code)));
  gum_arm64_writer_put_branch_address (from, GUM_ADDRESS (to->code));
}

/*
 * This function is responsible for backpatching code_slab addresses into the
 * inline cache. This may be encountered, for example when control flow returns
 * following execution of a CALL instruction emitted by a transformer. Note that
 * these cannot be prefetched, since there is no guarantee that the address of
 * instrumented blocks will be the same between the forkserver and its child.
 * This may be because, for example, blocks are compiled in a different order,
 * or the allocators are non-deterministic.
 */
static void
gum_exec_block_backpatch_slab (GumExecBlock * block,
                               gpointer target)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  GumIcEntry * ic_entries;
  guint num_ic_entries, i;

  just_unfollowed = block == NULL;
  if (just_unfollowed)
    return;

  ctx = block->ctx;
  if (!gum_exec_ctx_may_now_backpatch (ctx, block))
    return;
  if (!gum_exec_ctx_contains (ctx, target))
    return;

  ic_entries = block->ic_entries;
  g_assert (ic_entries != NULL);
  num_ic_entries = ctx->stalker->ic_entries;

  for (i = 0; i != num_ic_entries; i++)
  {
    if (ic_entries[i].real_start == target)
      return;
  }

  gum_spinlock_acquire (&ctx->code_lock);

  memmove (&ic_entries[1], &ic_entries[0],
      (num_ic_entries - 1) * sizeof (GumIcEntry));

  ic_entries[0].real_start = target;
  ic_entries[0].code_start = target;

  gum_spinlock_release (&ctx->code_lock);
}

static void
gum_exec_block_maybe_inherit_exclusive_access_state (GumExecBlock * block,
                                                     GumExecBlock * reference)
{
  const guint8 * real_address = block->real_start;
  GumExecBlock * cur;

  for (cur = reference; cur != NULL; cur = cur->next)
  {
    if ((cur->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) == 0)
      return;

    if (real_address >= cur->real_start &&
        real_address < cur->real_start + cur->real_size)
    {
      block->flags |= GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS;
      return;
    }
  }
}

static void
gum_exec_block_propagate_exclusive_access_state (GumExecBlock * block)
{
  GumExecBlock * block_containing_load, * cur;
  guint i;

  if ((block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) != 0)
    return;

  if ((block->flags & GUM_EXEC_BLOCK_HAS_EXCLUSIVE_STORE) == 0)
    return;

  block_containing_load = NULL;
  for (cur = block, i = 0;
      cur != NULL && i != GUM_EXCLUSIVE_ACCESS_MAX_DEPTH;
      cur = cur->next, i++)
  {
    if ((cur->flags & GUM_EXEC_BLOCK_HAS_EXCLUSIVE_LOAD) != 0)
    {
      block_containing_load = cur;
      break;
    }
  }
  if (block_containing_load == NULL)
    return;

  for (cur = block; TRUE; cur = cur->next)
  {
    cur->flags |= GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS;
    gum_exec_block_invalidate (cur);

    if (cur == block_containing_load)
      break;
  }
}

static void
gum_exec_ctx_write_adjust_depth (GumExecCtx * ctx,
                                 GumArm64Writer * cw,
                                 gssize adj)
{
  /* ctx->depth += adj */
  if ((ctx->sink_mask & (GUM_CALL | GUM_RET)) == 0)
    return;

  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16, ARM64_REG_X17,
      ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE), GUM_INDEX_PRE_ADJUST);

  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
      GUM_ADDRESS (&ctx->depth));
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);
  if (adj > 0)
  {
    gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X17,
        ARM64_REG_X17, adj);
  }
  else
  {
    gum_arm64_writer_put_sub_reg_reg_imm (cw, ARM64_REG_X17,
        ARM64_REG_X17, -adj);
  }
  gum_arm64_writer_put_str_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);

  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X16, ARM64_REG_X17,
      ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE, GUM_INDEX_POST_ADJUST);
}

/*
 * This function generates code required to search the inline cache stored in
 * the data slab in search of the target address of a branch. If successful, the
 * code (instrumented) address from the inline cache is returned in the register
 * returned by the function (fixed at X17). In this case, the values of X16 and
 * X17 stored beyond the red-zone are not restored (this facilitates the
 * indirect branching to the target block if necessary).
 *
 * If there is a cache miss, however, control passes to the slow slab, and the
 * values of X16 and X17 are restored from the stack.
 */
static arm64_reg
gum_exec_block_write_inline_cache_code (GumExecBlock * block,
                                        arm64_reg target_reg,
                                        GumArm64Writer * cw,
                                        GumArm64Writer * cws)
{
  GumSlab * data_slab = &block->ctx->data_slab->slab;
  GumStalker * stalker = block->ctx->stalker;
  guint i;
  const gsize empty_val = GUM_IC_MAGIC_EMPTY;
  gconstpointer match, mismatch;

  block->ic_entries = gum_slab_reserve (data_slab,
      gum_stalker_get_ic_entry_size (stalker));

  for (i = 0; i != stalker->ic_entries; i++)
  {
    block->ic_entries[i].real_start = NULL;
    block->ic_entries[i].code_start = GSIZE_TO_POINTER (empty_val);
  }

  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE),
      GUM_INDEX_PRE_ADJUST);

  gum_arm64_writer_put_push_reg_reg (cw, ARM64_REG_X0, ARM64_REG_X1);

  if (target_reg != ARM64_REG_X16)
    gum_arm64_writer_put_mov_reg_reg (cw, ARM64_REG_X16, target_reg);

  if ((stalker->cpu_features & GUM_CPU_PTRAUTH) != 0)
    gum_arm64_writer_put_xpaci_reg (cw, ARM64_REG_X16);

  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X17,
      GUM_ADDRESS (block->ic_entries));

  match = gum_arm64_writer_cur (cw);
  for (i = 0; i != stalker->ic_entries; i++)
  {
    gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X0, ARM64_REG_X17,
        G_STRUCT_OFFSET (GumIcEntry, real_start));
    gum_arm64_writer_put_sub_reg_reg_reg (cw, ARM64_REG_X0, ARM64_REG_X0,
        ARM64_REG_X16);

    mismatch = gum_arm64_writer_cur (cw);
    gum_arm64_writer_put_cbnz_reg_label (cw, ARM64_REG_X0, mismatch);
    gum_arm64_writer_put_b_label (cw, match);

    gum_arm64_writer_put_label (cw, mismatch);
    gum_arm64_writer_put_add_reg_reg_imm (cw, ARM64_REG_X17, ARM64_REG_X17,
        sizeof (GumIcEntry));
  }

  gum_exec_block_write_slab_transfer_code (cw, cws);
  gum_arm64_writer_put_pop_reg_reg (cws, ARM64_REG_X0, ARM64_REG_X1);
  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cws, ARM64_REG_X16,
      ARM64_REG_X17, ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE,
      GUM_INDEX_POST_ADJUST);

  gum_arm64_writer_put_label (cw, match);
  gum_arm64_writer_put_pop_reg_reg (cw, ARM64_REG_X0, ARM64_REG_X1);
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X17,
      G_STRUCT_OFFSET (GumIcEntry, code_start));

  return ARM64_REG_X17;
}

static void
gum_exec_block_write_exec_generated_code (GumArm64Writer * cw,
                                          GumExecCtx * ctx)
{
  gconstpointer dont_pop_now = cw->code + 1;

  gum_arm64_writer_put_stp_reg_reg_reg_offset (cw, ARM64_REG_X16, ARM64_REG_X17,
      ARM64_REG_SP, -(16 + GUM_RED_ZONE_SIZE), GUM_INDEX_PRE_ADJUST);

  /*
   * If there is an instrumented target block (ctx->current_block), then we
   * perform an indirect branch to (ctx->resume_at) leaving the values of X16
   * and X17 on the stack. Otherwise, our branch target is not an instrumented
   * block and we must therefore pop these values from the stack before we carry
   * out the branch. In this case, we don't care about the fact we clobber X16
   * below as the AArch64 documentation denotes X16 and X17 as IP0 and IP1 and
   * states:
   *
   * "Registers r16 (IP0) and r17 (IP1) may be used by a linker as a scratch
   * register between a routine and any subroutine it calls."
   */
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
      GUM_ADDRESS (&ctx->current_block));
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);
  gum_arm64_writer_put_cbnz_reg_label (cw, ARM64_REG_X17, dont_pop_now);
  gum_arm64_writer_put_ldp_reg_reg_reg_offset (cw, ARM64_REG_X16, ARM64_REG_X17,
      ARM64_REG_SP, 16 + GUM_RED_ZONE_SIZE, GUM_INDEX_POST_ADJUST);

  gum_arm64_writer_put_label (cw, dont_pop_now);
  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
      GUM_ADDRESS (&ctx->resume_at));
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16, 0);
  gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_X17);
}

static void
gum_exec_block_write_call_event_code (GumExecBlock * block,
                                      const GumBranchTarget * target,
                                      GumGeneratorContext * gc,
                                      GumCodeContext cc)
{
  GumArm64Writer * cw = gc->code_writer;

  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, cw);

  gum_exec_ctx_write_push_branch_target_address (block->ctx, target, gc,
      gc->code_writer);
  gum_arm64_writer_put_pop_reg_reg (cw, ARM64_REG_X2, ARM64_REG_XZR);

  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_emit_call_event), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, ARM64_REG_X2,
      GUM_ARG_REGISTER, ARM64_REG_X20);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
}

static void
gum_exec_block_write_ret_event_code (GumExecBlock * block,
                                     GumGeneratorContext * gc,
                                     GumCodeContext cc)
{
  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_exec_ctx_load_real_register_into (block->ctx, ARM64_REG_X2, ARM64_REG_LR,
      gc, gc->code_writer);

  gum_arm64_writer_put_call_address_with_arguments (gc->code_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_ret_event), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, ARM64_REG_X2,
      GUM_ARG_REGISTER, ARM64_REG_X20);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
}

static void
gum_exec_block_write_exec_event_code (GumExecBlock * block,
                                      GumGeneratorContext * gc,
                                      GumCodeContext cc)
{
  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_arm64_writer_put_call_address_with_arguments (gc->code_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_exec_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, ARM64_REG_X20);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
}

static void
gum_exec_block_write_block_event_code (GumExecBlock * block,
                                       GumGeneratorContext * gc,
                                       GumCodeContext cc)
{
  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_arm64_writer_put_call_address_with_arguments (gc->code_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_block_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM64_REG_X20);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
}

static void
gum_exec_block_write_unfollow_check_code (GumExecBlock * block,
                                          GumGeneratorContext * gc,
                                          GumCodeContext cc)
{
  GumExecCtx * ctx = block->ctx;
  GumArm64Writer * cw = gc->code_writer;
  gconstpointer beach = cw->code + 1;
  GumPrologType opened_prolog;

  if (cc != GUM_CODE_INTERRUPTIBLE)
    return;

  gum_arm64_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_maybe_unfollow), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  gum_arm64_writer_put_cbz_reg_label (cw, ARM64_REG_X0, beach);

  opened_prolog = gc->opened_prolog;
  gum_exec_block_close_prolog (block, gc, cw);
  gc->opened_prolog = opened_prolog;

  gum_arm64_writer_put_ldr_reg_address (cw, ARM64_REG_X16,
      GUM_ADDRESS (&ctx->resume_at));
  gum_arm64_writer_put_ldr_reg_reg_offset (cw, ARM64_REG_X17, ARM64_REG_X16,
      0);
  gum_arm64_writer_put_br_reg_no_auth (cw, ARM64_REG_X17);

  gum_arm64_writer_put_label (cw, beach);
}

static void
gum_exec_block_maybe_write_call_probe_code (GumExecBlock * block,
                                            GumGeneratorContext * gc)
{
  GumStalker * stalker = block->ctx->stalker;

  if (!stalker->any_probes_attached)
    return;

  gum_spinlock_acquire (&stalker->probe_lock);

  if (g_hash_table_contains (stalker->probe_array_by_address,
          block->real_start))
  {
    gum_exec_block_write_call_probe_code (block, gc);
  }

  gum_spinlock_release (&stalker->probe_lock);
}

static void
gum_exec_block_write_call_probe_code (GumExecBlock * block,
                                      GumGeneratorContext * gc)
{
  g_assert (gc->opened_prolog == GUM_PROLOG_NONE);
  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_arm64_writer_put_call_address_with_arguments (gc->code_writer,
      GUM_ADDRESS (gum_exec_block_invoke_call_probes), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM64_REG_X20);
}

static void
gum_exec_block_invoke_call_probes (GumExecBlock * block,
                                   GumCpuContext * cpu_context)
{
  GumStalker * stalker = block->ctx->stalker;
  const gpointer target_address = block->real_start;
  GumCallProbe ** probes_copy;
  guint num_probes, i;
  GumCallDetails d;

  probes_copy = NULL;
  num_probes = 0;
  {
    GPtrArray * probes;

    gum_spinlock_acquire (&stalker->probe_lock);

    probes =
        g_hash_table_lookup (stalker->probe_array_by_address, target_address);
    if (probes != NULL)
    {
      num_probes = probes->len;
      probes_copy = g_newa (GumCallProbe *, num_probes);
      for (i = 0; i != num_probes; i++)
      {
        probes_copy[i] = gum_call_probe_ref (g_ptr_array_index (probes, i));
      }
    }

    gum_spinlock_release (&stalker->probe_lock);
  }
  if (num_probes == 0)
    return;

  d.target_address = target_address;
  d.return_address = GSIZE_TO_POINTER (cpu_context->lr);
  d.stack_data = GSIZE_TO_POINTER (cpu_context->sp);
  d.cpu_context = cpu_context;

  cpu_context->pc = GPOINTER_TO_SIZE (target_address);

  for (i = 0; i != num_probes; i++)
  {
    GumCallProbe * probe = probes_copy[i];

    probe->callback (&d, probe->user_data);

    gum_call_probe_unref (probe);
  }
}

static gpointer
gum_exec_block_write_inline_data (GumArm64Writer * cw,
                                  gconstpointer data,
                                  gsize size,
                                  GumAddress * address)
{
  gpointer location;
  gconstpointer after_data = cw->code + 1;

  g_assert (size % 4 == 0);

  while (gum_arm64_writer_offset (cw) < GUM_INVALIDATE_TRAMPOLINE_MAX_SIZE)
  {
    gum_arm64_writer_put_nop (cw);
  }

  gum_arm64_writer_put_b_label (cw, after_data);

  location = gum_arm64_writer_cur (cw);
  if (address != NULL)
    *address = cw->pc;
  gum_arm64_writer_put_bytes (cw, data, size);

  gum_arm64_writer_put_label (cw, after_data);

  return location;
}

static void
gum_exec_block_open_prolog (GumExecBlock * block,
                            GumPrologType type,
                            GumGeneratorContext * gc,
                            GumArm64Writer * cw)
{
  if (gc->opened_prolog >= type)
    return;

  /* We don't want to handle this case for performance reasons */
  g_assert (gc->opened_prolog == GUM_PROLOG_NONE);

  gc->opened_prolog = type;

  gum_exec_ctx_write_prolog (block->ctx, type, cw);
}

static void
gum_exec_block_close_prolog (GumExecBlock * block,
                             GumGeneratorContext * gc,
                             GumArm64Writer * cw)
{
  if (gc->opened_prolog == GUM_PROLOG_NONE)
    return;

  gum_exec_ctx_write_epilog (block->ctx, gc->opened_prolog, cw);

  gc->opened_prolog = GUM_PROLOG_NONE;
}

static GumCodeSlab *
gum_code_slab_new (GumExecCtx * ctx)
{
  GumStalker * stalker = ctx->stalker;
  gsize total_size;
  GumCodeSlab * code_slab;
  GumSlowSlab * slow_slab;

  total_size = stalker->code_slab_size_dynamic +
      stalker->slow_slab_size_dynamic;

  code_slab = gum_memory_allocate (NULL, total_size, stalker->page_size,
      stalker->is_rwx_supported ? GUM_PAGE_RWX : GUM_PAGE_RW);

  gum_code_slab_init (code_slab, stalker->code_slab_size_dynamic, total_size,
      stalker->page_size);

  slow_slab = gum_slab_end (&code_slab->slab);
  gum_slow_slab_init (slow_slab, stalker->slow_slab_size_dynamic, 0,
      stalker->page_size);

  return code_slab;
}

static void
gum_code_slab_free (GumCodeSlab * code_slab)
{
  gum_slab_free (&code_slab->slab);
}

static void
gum_code_slab_init (GumCodeSlab * code_slab,
                    gsize slab_size,
                    gsize memory_size,
                    gsize page_size)
{
  /*
   * We don't want to thaw and freeze the header just to update the offset,
   * so we trade a little memory for speed.
   */
  const gsize header_size = GUM_ALIGN_SIZE (sizeof (GumCodeSlab), page_size);

  gum_slab_init (&code_slab->slab, slab_size, memory_size, header_size);

  code_slab->invalidator = NULL;
  code_slab->prolog_minimal = NULL;
  code_slab->epilog_minimal = NULL;
  code_slab->prolog_full = NULL;
  code_slab->epilog_full = NULL;
}

static void
gum_slow_slab_init (GumSlowSlab * slow_slab,
                    gsize slab_size,
                    gsize memory_size,
                    gsize page_size)
{
  /*
   * We don't want to thaw and freeze the header just to update the offset,
   * so we trade a little memory for speed.
   */
  const gsize header_size = GUM_ALIGN_SIZE (sizeof (GumCodeSlab), page_size);

  gum_slab_init (&slow_slab->slab, slab_size, memory_size, header_size);

  slow_slab->invalidator = NULL;
}

static GumDataSlab *
gum_data_slab_new (GumExecCtx * ctx)
{
  GumDataSlab * slab;
  GumStalker * stalker = ctx->stalker;
  const gsize slab_size = stalker->data_slab_size_dynamic;

  slab = gum_memory_allocate (NULL, slab_size, stalker->page_size,
      GUM_PAGE_RW);

  gum_data_slab_init (slab, slab_size, slab_size);

  return slab;
}

static void
gum_data_slab_free (GumDataSlab * data_slab)
{
  gum_slab_free (&data_slab->slab);
}

static void
gum_data_slab_init (GumDataSlab * data_slab,
                    gsize slab_size,
                    gsize memory_size)
{
  GumSlab * slab = &data_slab->slab;
  const gsize header_size = sizeof (GumDataSlab);

  gum_slab_init (slab, slab_size, memory_size, header_size);
}

static void
gum_scratch_slab_init (GumCodeSlab * scratch_slab,
                       gsize slab_size)
{
  const gsize header_size = sizeof (GumCodeSlab);

  gum_slab_init (&scratch_slab->slab, slab_size, slab_size, header_size);

  scratch_slab->invalidator = NULL;
}

static void
gum_slab_free (GumSlab * slab)
{
  if (slab->memory_size != 0)
    gum_memory_free (slab, slab->memory_size);
}

static void
gum_slab_init (GumSlab * slab,
               gsize slab_size,
               gsize memory_size,
               gsize header_size)
{
  slab->data = (guint8 *) slab + header_size;
  slab->offset = 0;
  slab->size = slab_size - header_size;
  slab->memory_size = memory_size;
  slab->next = NULL;
}

static gsize
gum_slab_available (GumSlab * self)
{
  return self->size - self->offset;
}

static gpointer
gum_slab_start (GumSlab * self)
{
  return self->data;
}

static gpointer
gum_slab_end (GumSlab * self)
{
  return self->data + self->size;
}

static gpointer
gum_slab_cursor (GumSlab * self)
{
  return self->data + self->offset;
}

static gpointer
gum_slab_reserve (GumSlab * self,
                  gsize size)
{
  gpointer cursor;

  cursor = gum_slab_try_reserve (self, size);
  g_assert (cursor != NULL);

  return cursor;
}

static gpointer
gum_slab_try_reserve (GumSlab * self,
                      gsize size)
{
  gpointer cursor;

  if (gum_slab_available (self) < size)
    return NULL;

  cursor = gum_slab_cursor (self);
  self->offset += size;

  return cursor;
}

static gpointer
gum_find_thread_exit_implementation (void)
{
#if defined (HAVE_DARWIN)
  return gum_arm64_reader_find_next_bl_target (GSIZE_TO_POINTER (
        gum_strip_code_address (gum_module_find_export_by_name (
            gum_process_get_libc_module (), "pthread_exit"))));
#elif defined (HAVE_GLIBC)
  return GSIZE_TO_POINTER (gum_module_find_export_by_name (
        gum_process_get_libc_module (),
        "__call_tls_dtors"));
#elif defined (HAVE_ANDROID)
  return GSIZE_TO_POINTER (gum_module_find_export_by_name (
        gum_process_get_libc_module (),
        "pthread_exit"));
#elif defined (HAVE_FREEBSD)
  GumAddress result;
  GumModule * libthr;

  libthr = gum_process_find_module_by_name ("/lib/libthr.so.3");
  g_assert (libthr != NULL);
  result = gum_module_find_export_by_name (libthr, "_pthread_exit");
  g_object_unref (libthr);

  return GSIZE_TO_POINTER (result);
#elif defined (HAVE_WINDOWS)
  GumAddress result;
  GumModule * ntdll;

  ntdll = gum_process_find_module_by_name ("ntdll.dll");
  g_assert (ntdll != NULL);
  result = gum_module_find_export_by_name (ntdll, "RtlExitUserThread");
  g_object_unref (ntdll);

  return GSIZE_TO_POINTER (result);
#else
  return NULL;
#endif
}
