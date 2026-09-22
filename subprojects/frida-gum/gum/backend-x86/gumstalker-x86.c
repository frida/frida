/*
 * Copyright (C) 2009-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2010-2013 Karl Trygve Kalleberg <karltk@boblycat.org>
 * Copyright (C) 2020      Duy Phan Thanh <phanthanhduypr@gmail.com>
 * Copyright (C) 2026 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "gummetalhash.h"
#include "gumx86reader.h"
#include "gumx86writer.h"
#include "gummemory.h"
#include "gumx86relocator.h"
#include "gumspinlock.h"
#include "gumprocess-priv.h"
#include "gumstalker-priv.h"
#include "gumunwindbroker.h"
#ifdef HAVE_WINDOWS
# include "gumexceptor.h"
#endif
#ifdef HAVE_LINUX
# include "gum-init.h"
# include "gumelfmodule.h"
#endif
#ifdef HAVE_LINUX
# include "guminterceptor.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_WINDOWS
# define VC_EXTRALEAN
# include <windows.h>
# include <psapi.h>
#endif
#ifdef HAVE_LINUX
# include <sys/syscall.h>
# include <unwind.h>
# ifndef __NR_clone3
#  define __NR_clone3 435
# endif
#endif

#define GUM_CODE_SLAB_SIZE_INITIAL  (128 * 1024)
#define GUM_CODE_SLAB_SIZE_DYNAMIC  (4 * 1024 * 1024)
#define GUM_SLOW_SLAB_SIZE_INITIAL  (128 * 1024)
#define GUM_SLOW_SLAB_SIZE_DYNAMIC  (4 * 1024 * 1024)
#define GUM_DATA_SLAB_SIZE_INITIAL  (GUM_CODE_SLAB_SIZE_INITIAL / 5)
#define GUM_DATA_SLAB_SIZE_DYNAMIC  (GUM_CODE_SLAB_SIZE_DYNAMIC / 5)
#define GUM_SCRATCH_SLAB_SIZE       16384
/*
 * If we encounter the `clone` syscall, then we have to burn a page to prevent
 * issues with both threads running in the same page.
 */
#define GUM_EXEC_BLOCK_MIN_CAPACITY (1024 + 8192)
#define GUM_DATA_BLOCK_MIN_CAPACITY (sizeof (GumExecBlock) + 1024)

#if GLIB_SIZEOF_VOID_P == 4 && \
    (defined (HAVE_WINDOWS) || defined (HAVE_DARWIN) || \
     defined (HAVE_LINUX) || defined (HAVE_FREEBSD))
# define GUM_HAVE_SYSENTER_VIRTUALIZATION 1
#endif

#if GLIB_SIZEOF_VOID_P == 4
# define GUM_INVALIDATE_TRAMPOLINE_SIZE            16
# define GUM_STATE_PRESERVE_TOPMOST_REGISTER_INDEX 3
# define GUM_IC_MAGIC_EMPTY                        0xdeadface
# define GUM_IC_MAGIC_SCRATCH                      0xcafef00d
#else
# define GUM_INVALIDATE_TRAMPOLINE_SIZE            17
# define GUM_STATE_PRESERVE_TOPMOST_REGISTER_INDEX 9
# define GUM_IC_MAGIC_EMPTY                        0xbaadd00ddeadface
# define GUM_IC_MAGIC_SCRATCH                      0xbaadd00dcafef00d
#endif
#define GUM_MINIMAL_PROLOG_RETURN_OFFSET \
    ((GUM_STATE_PRESERVE_TOPMOST_REGISTER_INDEX + 2) * sizeof (gpointer))
#define GUM_FULL_PROLOG_GPR_TOP_OFFSET \
    (GUM_CPU_CONTEXT_OFFSET_XAX + sizeof (gpointer))
#define GUM_FULL_PROLOG_RETURN_OFFSET \
    (sizeof (GumCpuContext) + sizeof (gpointer))
#define GUM_X86_THUNK_ARGLIST_STACK_RESERVE 64 /* x64 ABI compatibility */

#define GUM_STALKER_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_STALKER_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

typedef struct _GumInfectContext GumInfectContext;
typedef struct _GumDisinfectContext GumDisinfectContext;
typedef struct _GumActivation GumActivation;
typedef struct _GumInvalidateContext GumInvalidateContext;
typedef struct _GumCallProbe GumCallProbe;

typedef struct _GumExecCtx GumExecCtx;
typedef guint GumExecCtxMode;
typedef void (* GumExecHelperWriteFunc) (GumExecCtx * ctx, GumX86Writer * cw);
typedef struct _GumExecBlock GumExecBlock;
typedef guint GumExecBlockFlags;
typedef gpointer (* GumExecCtxReplaceCurrentBlockFunc) (GumExecBlock * block,
    gpointer start_address, gpointer from_insn);

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
typedef guint GumBackpatchType;
typedef struct _GumBackpatchCall GumBackpatchCall;
typedef struct _GumBackpatchRet GumBackpatchRet;
typedef struct _GumBackpatchJmp GumBackpatchJmp;
typedef struct _GumBackpatchInlineCache GumBackpatchInlineCache;
typedef struct _GumIcEntry GumIcEntry;

typedef guint GumVirtualizationRequirements;

#ifdef HAVE_WINDOWS
# if GLIB_SIZEOF_VOID_P == 8
typedef DWORD64 GumNativeRegisterValue;
# else
typedef DWORD GumNativeRegisterValue;
# endif
#endif

#ifdef HAVE_LINUX
typedef struct _GumCheckElfSection GumCheckElfSection;
#endif

#ifdef HAVE_LINUX
typedef struct _Unwind_Exception _Unwind_Exception;
typedef struct _Unwind_Context _Unwind_Context;
struct dwarf_eh_bases;
#endif

enum
{
  PROP_0,
  PROP_IC_ENTRIES,
  PROP_ADJACENT_BLOCKS,
};

struct _GumStalker
{
  GObject parent;

  guint ic_entries;
  /*
   * Stalker compiles each block on demand. However, when we reach the end of a
   * given block, we may encounter an instruction (e.g. a Jcc or a CALL) which
   * means whilst we have reached the end of the block, another block of code
   * will immediately follow it. In the event of a Jcc, we know that if the
   * branch is not taken, then control flow will continue immediately to that
   * adjacent block.
   *
   * By fetching these adjacent blocks ahead of time, before they are needed to
   * be executed, we can ensure that they will also occur adjacently in their
   * instrumented form in the code_slab. Therefore when we come to backpatch
   * between such adjacent blocks, we can instead replace the usual JMP
   * statement with a NOP slide and gain a bit of performance.
   */
  guint adj_blocks;

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

#ifdef HAVE_WINDOWS
  GumExceptor * exceptor;
# if GLIB_SIZEOF_VOID_P == 4
  gpointer user32_start, user32_end;
  gpointer ki_user_callback_dispatcher_impl;
  GArray * wow_transition_impls;
# endif
#endif

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
  GumExecCtxMode mode;
  gint64 destroy_pending_since;

  GumStalker * stalker;
  GumThreadId thread_id;
#ifdef HAVE_WINDOWS
  GumNativeRegisterValue previous_pc;
  GumNativeRegisterValue previous_dr0;
  GumNativeRegisterValue previous_dr1;
  GumNativeRegisterValue previous_dr2;
  GumNativeRegisterValue previous_dr7;
#endif

  GumX86Writer code_writer;
  GumX86Writer slow_writer;
  GumX86Relocator relocator;

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
  guint pending_calls;

  gpointer resume_at;
  gpointer return_at;
  gpointer app_stack;
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
   * Stalker for x86 no longer makes use of a shadow stack for handling CALL/RET
   * instructions, so we instead keep a count of the depth of the stack here
   * when GUM_CALL or GUM_RET events are enabled.
   */
  gint depth;

#ifdef HAVE_LINUX
  gpointer last_int80;
  gpointer last_syscall;
  GumAddress syscall_end;
  GumMetalHashTable * excluded_calls;
#endif
};

enum _GumExecCtxState
{
  GUM_EXEC_CTX_ACTIVE,
  GUM_EXEC_CTX_UNFOLLOW_PENDING,
  GUM_EXEC_CTX_DESTROY_PENDING
};

enum _GumExecCtxMode
{
  GUM_EXEC_CTX_NORMAL,
  GUM_EXEC_CTX_SINGLE_STEPPING_ON_CALL,
  GUM_EXEC_CTX_SINGLE_STEPPING_THROUGH_CALL
};

struct _GumExecBlock
{
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
  GUM_EXEC_BLOCK_ACTIVATION_TARGET = 1 << 0,
};

struct _GumSlab
{
  guint8 * data;
  guint offset;
  guint size;
  GumSlab * next;
};

struct _GumCodeSlab
{
  GumSlab slab;

  gpointer invalidator;
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
  GUM_PROLOG_IC,
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
  GumX86Relocator * relocator;
  GumX86Writer * code_writer;
  GumX86Writer * slow_writer;
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

  gboolean is_indirect;
  uint8_t pfx_seg;
  x86_reg base;
  x86_reg index;
  guint8 scale;
};

enum _GumBackpatchType
{
  GUM_BACKPATCH_CALL,
  GUM_BACKPATCH_JMP,
  GUM_BACKPATCH_INLINE_CACHE,
};

struct _GumBackpatchCall
{
  gsize code_offset;
  GumPrologType opened_prolog;
  gpointer ret_real_address;
  gsize ret_code_offset;
};

struct _GumBackpatchRet
{
  gsize code_offset;
};

struct _GumBackpatchJmp
{
  guint id;
  gsize code_offset;
  GumPrologType opened_prolog;
};

struct _GumBackpatchInlineCache
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
    GumBackpatchRet ret;
    GumBackpatchJmp jmp;
    GumBackpatchInlineCache inline_cache;
  };
};

struct _GumIcEntry
{
  gpointer real_start;
  gpointer code_start;
};

enum _GumVirtualizationRequirements
{
  GUM_REQUIRE_NOTHING         = 0,

  GUM_REQUIRE_RELOCATION      = 1 << 0,
  GUM_REQUIRE_SINGLE_STEP     = 1 << 1
};

#ifdef HAVE_LINUX

struct _GumCheckElfSection
{
  gchar name[PATH_MAX];
  GumBranchTarget * target;
  gboolean found;
};

#endif

static void gum_stalker_unwind_translator_iface_init (gpointer iface,
    gpointer iface_data);
static void gum_stalker_dispose (GObject * object);
static void gum_stalker_finalize (GObject * object);
static void gum_stalker_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec);
static void gum_stalker_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec);

G_GNUC_INTERNAL void _gum_stalker_do_follow_me (GumStalker * self,
    GumStalkerTransformer * transformer, GumEventSink * sink,
    gpointer * ret_addr_ptr);
static void gum_stalker_infect (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
static void gum_stalker_disinfect (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
G_GNUC_INTERNAL void _gum_stalker_do_activate (GumStalker * self,
    gconstpointer target, gpointer * ret_addr_ptr);
G_GNUC_INTERNAL void _gum_stalker_do_deactivate (GumStalker * self,
    gpointer * ret_addr_ptr);
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

static GumAddress gum_stalker_translate_unwind_pc (
    GumUnwindPcTranslator * translator, GumAddress code_address);
static gboolean gum_stalker_install_unwind_resume_context (
    GumUnwindPcTranslator * translator, gpointer unwind_context,
    GumAddress real_resume_ip);

static gsize gum_stalker_snapshot_space_needed_for (GumStalker * self,
    gsize real_size);
static gsize gum_stalker_get_ic_entry_size (GumStalker * stalker);

static void gum_stalker_thaw (GumStalker * self, gpointer code, gsize size);
static void gum_stalker_freeze (GumStalker * self, gpointer code, gsize size);

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
static void gum_exec_ctx_compute_code_address_spec (GumExecCtx * ctx,
    gsize slab_size, GumAddressSpec * spec);
static void gum_exec_ctx_compute_data_address_spec (GumExecCtx * ctx,
    gsize slab_size, GumAddressSpec * spec);
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
static GumExecBlock * gum_exec_ctx_build_block (GumExecCtx * ctx,
    gpointer real_address);
static void gum_exec_ctx_recompile_block (GumExecCtx * ctx,
    GumExecBlock * block);
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
    GumX86Writer * cw);
static void gum_exec_ctx_write_epilog (GumExecCtx * ctx, GumPrologType type,
    GumX86Writer * cw);

static void gum_exec_ctx_ensure_inline_helpers_reachable (GumExecCtx * ctx);
static void gum_exec_ctx_write_minimal_prolog_helper (GumExecCtx * ctx,
    GumX86Writer * cw);
static void gum_exec_ctx_write_minimal_epilog_helper (GumExecCtx * ctx,
    GumX86Writer * cw);
static void gum_exec_ctx_write_full_prolog_helper (GumExecCtx * ctx,
    GumX86Writer * cw);
static void gum_exec_ctx_write_full_epilog_helper (GumExecCtx * ctx,
    GumX86Writer * cw);
static void gum_exec_ctx_write_prolog_helper (GumExecCtx * ctx,
    GumPrologType type, GumX86Writer * cw);
static void gum_exec_ctx_write_epilog_helper (GumExecCtx * ctx,
    GumPrologType type, GumX86Writer * cw);
static void gum_exec_ctx_write_invalidator (GumExecCtx * ctx,
    GumX86Writer * cw);
static void gum_exec_ctx_ensure_helper_reachable (GumExecCtx * ctx,
    gpointer * helper_ptr, GumExecHelperWriteFunc write);
static gboolean gum_exec_ctx_is_helper_reachable (GumExecCtx * ctx,
    gpointer * helper_ptr);

static void gum_exec_ctx_get_branch_target_address (GumExecCtx * ctx,
    const GumBranchTarget * target, GumGeneratorContext * gc,
    GumX86Writer * cw);
static void gum_exec_ctx_load_real_register_into (GumExecCtx * ctx,
    GumX86Reg target_register, GumX86Reg source_register,
    gpointer ip, GumGeneratorContext * gc, GumX86Writer * cw);
static void gum_exec_ctx_load_real_register_from_minimal_frame_into (
    GumExecCtx * ctx, GumX86Reg target_register, GumX86Reg source_register,
    gpointer ip, GumGeneratorContext * gc, GumX86Writer * cw);
static void gum_exec_ctx_load_real_register_from_full_frame_into (
    GumExecCtx * ctx, GumX86Reg target_register, GumX86Reg source_register,
    gpointer ip, GumGeneratorContext * gc, GumX86Writer * cw);
static void gum_exec_ctx_load_real_register_from_ic_frame_into (
    GumExecCtx * ctx, GumX86Reg target_register, GumX86Reg source_register,
    gpointer ip, GumGeneratorContext * gc, GumX86Writer * cw);

static GumExecBlock * gum_exec_block_new (GumExecCtx * ctx);
static void gum_exec_block_clear (GumExecBlock * block);
static void gum_exec_block_commit (GumExecBlock * block);
static void gum_exec_block_invalidate (GumExecBlock * block);
static gpointer gum_exec_block_get_snapshot_start (GumExecBlock * block);
static GumCalloutEntry * gum_exec_block_get_last_callout_entry (
    const GumExecBlock * block);
static void gum_exec_block_set_last_callout_entry (GumExecBlock * block,
    GumCalloutEntry * entry);

static void gum_exec_block_backpatch_call (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn, gsize code_offset,
    GumPrologType opened_prolog, gpointer ret_real_address,
    gsize ret_code_offset);
static void gum_exec_block_backpatch_jmp (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn, guint id, gsize code_offset,
    GumPrologType opened_prolog);
static gboolean gum_exec_block_get_eob (gpointer from_insn, guint id);
static void gum_exec_block_backpatch_conditional_jmp (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn, guint id, gsize jcc_code_offset,
    GumPrologType opened_prolog);
static GumExecBlock * gum_exec_block_get_adjacent (GumExecBlock * from);
static void gum_exec_block_backpatch_unconditional_jmp (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn, gboolean is_eob, gsize code_offset,
    GumPrologType opened_prolog);
static gboolean gum_exec_block_is_adjacent (gpointer target,
    GumExecBlock * from);
static void gum_exec_block_backpatch_inline_cache (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn);

static GumVirtualizationRequirements gum_exec_block_virtualize_branch_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
static gboolean gum_exec_block_is_direct_jmp_to_plt_got (GumExecBlock * block,
    GumGeneratorContext * gc, GumBranchTarget * target);
#ifdef HAVE_LINUX
static GArray * gum_exec_ctx_get_plt_got_ranges (void);
static void gum_exec_ctx_deinit_plt_got_ranges (void);
static gboolean gum_collect_plt_got_in_module (GumModule * module,
    gpointer user_data);
static gboolean gum_collect_if_plt_got_section (
    const GumSectionDetails * details, gpointer user_data);
#endif
static void gum_exec_block_handle_direct_jmp_to_plt_got (GumExecBlock * block,
    GumGeneratorContext * gc, GumBranchTarget * target);
static GumVirtualizationRequirements gum_exec_block_virtualize_ret_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
static void gum_exec_block_write_adjust_depth (GumExecBlock * block,
    GumX86Writer * cw, gssize adj);
static GumVirtualizationRequirements gum_exec_block_virtualize_sysenter_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
static GumVirtualizationRequirements gum_exec_block_virtualize_syscall_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
static GumVirtualizationRequirements gum_exec_block_virtualize_int_insn (
    GumExecBlock * block, GumGeneratorContext * gc);
#ifdef HAVE_LINUX
static GumVirtualizationRequirements gum_exec_block_virtualize_linux_syscall (
    GumExecBlock * block, GumGeneratorContext * gc);
static void gum_exec_ctx_write_int80_helper (GumExecCtx * ctx,
    GumX86Writer * cw);
static void gum_exec_ctx_write_syscall_helper (GumExecCtx * ctx,
    GumX86Writer * cw);
static void gum_exec_ctx_write_aligned_syscall (GumExecCtx * ctx,
    GumX86Writer * cw, const guint8 * syscall_insn, gsize syscall_size);
#endif
#if GLIB_SIZEOF_VOID_P == 4 && defined (HAVE_WINDOWS)
static GumVirtualizationRequirements
    gum_exec_block_virtualize_wow64_transition (GumExecBlock * block,
    GumGeneratorContext * gc, gpointer impl);
#endif

static void gum_exec_block_write_call_invoke_code (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_jmp_transfer_code (GumExecBlock * block,
    const GumBranchTarget * target, GumExecCtxReplaceCurrentBlockFunc func,
    GumGeneratorContext * gc, guint id, GumAddress jcc_address);
static void gum_exec_block_write_ret_transfer_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_chaining_return_code (GumExecBlock * block,
    GumGeneratorContext * gc, guint16 npop);
static gpointer * gum_exec_block_write_inline_cache_code (GumExecBlock * block,
    GumGeneratorContext * gc, GumX86Writer * cw, GumX86Writer * cws);
static void gum_exec_block_backpatch_slab (GumExecBlock * block,
    gpointer target);
static void gum_exec_block_write_single_step_transfer_code (
    GumExecBlock * block, GumGeneratorContext * gc);
#ifdef GUM_HAVE_SYSENTER_VIRTUALIZATION
static void gum_exec_block_write_sysenter_continuation_code (
    GumExecBlock * block, GumGeneratorContext * gc, gpointer saved_ret_addr);
#endif

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

static gpointer gum_exec_block_write_inline_data (GumX86Writer * cw,
    gconstpointer data, gsize size, GumAddress * address);

static void gum_exec_block_open_prolog (GumExecBlock * block,
    GumPrologType type, GumGeneratorContext * gc, GumX86Writer * cw);
static void gum_exec_block_close_prolog (GumExecBlock * block,
    GumGeneratorContext * gc, GumX86Writer * cw);

static GumCodeSlab * gum_code_slab_new (GumExecCtx * ctx);
static GumSlowSlab * gum_slow_slab_new (GumExecCtx * ctx);
static void gum_code_slab_free (GumCodeSlab * code_slab);
static void gum_code_slab_init (GumCodeSlab * code_slab, gsize slab_size,
    gsize page_size);
static void gum_slow_slab_init (GumSlowSlab * slow_slab, gsize slab_size,
    gsize page_size);
static void gum_slow_slab_free (GumSlowSlab * slow_slab);

static GumDataSlab * gum_data_slab_new (GumExecCtx * ctx);
static void gum_data_slab_free (GumDataSlab * data_slab);
static void gum_data_slab_init (GumDataSlab * data_slab, gsize slab_size);

static void gum_scratch_slab_init (GumCodeSlab * scratch_slab, gsize slab_size);

static void gum_slab_free (GumSlab * slab);
static void gum_slab_init (GumSlab * slab, gsize slab_size, gsize header_size);
static gsize gum_slab_available (GumSlab * self);
static gpointer gum_slab_start (GumSlab * self);
static gpointer gum_slab_end (GumSlab * self);
static gpointer gum_slab_cursor (GumSlab * self);
static gpointer gum_slab_reserve (GumSlab * self, gsize size);
static gpointer gum_slab_try_reserve (GumSlab * self, gsize size);

static void gum_write_segment_prefix (uint8_t segment, GumX86Writer * cw);

static GumX86Reg gum_x86_meta_reg_from_real_reg (GumX86Reg reg);
static GumX86Reg gum_x86_reg_from_capstone (x86_reg reg);

#ifdef HAVE_WINDOWS
static gboolean gum_stalker_on_exception (GumExceptionDetails * details,
    gpointer user_data);
static void gum_enable_hardware_breakpoint (GumNativeRegisterValue * dr7_reg,
    guint index);
# if GLIB_SIZEOF_VOID_P == 4
static void gum_collect_export (GArray * impls, const WCHAR * module_name,
    const gchar * export_name);
static void gum_collect_export_by_handle (GArray * impls,
    HMODULE module_handle, const gchar * export_name);
static gpointer gum_find_system_call_above_us (GumStalker * stalker,
    gpointer * start_esp);
# endif
#endif

static gpointer gum_find_thread_exit_implementation (void);
#ifdef HAVE_DARWIN
static gboolean gum_store_thread_exit_match (GumAddress address, gsize size,
    gpointer user_data);
#endif

G_DEFINE_TYPE_WITH_CODE (GumStalker, gum_stalker, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GUM_TYPE_UNWIND_PC_TRANSLATOR,
                             gum_stalker_unwind_translator_iface_init))

static GPrivate gum_stalker_exec_ctx_private;

static gpointer _gum_thread_exit_impl;

#ifdef HAVE_LINUX
static const guint8 gum_int80_code[] = { 0xcd, 0x80 };
static const guint8 gum_syscall_code[] = { 0x0f, 0x05 };
#endif

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
      2, 32, 2,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_ADJACENT_BLOCKS,
      g_param_spec_uint ("adjacent-blocks", "Adjacent Blocks",
      "Prefetch Adjacent Blocks", 0, 32, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  _gum_thread_exit_impl = gum_find_thread_exit_implementation ();
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

#ifdef HAVE_WINDOWS
  self->exceptor = gum_exceptor_obtain ();
  gum_exceptor_add (self->exceptor, gum_stalker_on_exception, self);

# if GLIB_SIZEOF_VOID_P == 4
  {
    HMODULE ntmod, usermod;
    MODULEINFO mi;
    BOOL success G_GNUC_UNUSED;
    gboolean found_user32_code G_GNUC_UNUSED;
    guint8 * p;
    GArray * impls;

    ntmod = GetModuleHandleW (L"ntdll.dll");
    usermod = GetModuleHandleW (L"user32.dll");
    g_assert (ntmod != NULL && usermod != NULL);

    success = GetModuleInformation (GetCurrentProcess (), usermod,
        &mi, sizeof (mi));
    g_assert (success);
    self->user32_start = mi.lpBaseOfDll;
    self->user32_end = (guint8 *) mi.lpBaseOfDll + mi.SizeOfImage;

    found_user32_code = FALSE;
    for (p = self->user32_start; p < (guint8 *) self->user32_end;)
    {
      MEMORY_BASIC_INFORMATION mbi;

      success = VirtualQuery (p, &mbi, sizeof (mbi)) == sizeof (mbi);
      g_assert (success);

      if (mbi.Protect == PAGE_EXECUTE_READ ||
          mbi.Protect == PAGE_EXECUTE_READWRITE ||
          mbi.Protect == PAGE_EXECUTE_WRITECOPY)
      {
        self->user32_start = mbi.BaseAddress;
        self->user32_end = (guint8 *) mbi.BaseAddress + mbi.RegionSize;

        found_user32_code = TRUE;
      }

      p = (guint8 *) mbi.BaseAddress + mbi.RegionSize;
    }
    g_assert (found_user32_code);

    self->ki_user_callback_dispatcher_impl = GUM_FUNCPTR_TO_POINTER (
        GetProcAddress (ntmod, "KiUserCallbackDispatcher"));
    g_assert (self->ki_user_callback_dispatcher_impl != NULL);

    impls = g_array_sized_new (FALSE, FALSE, sizeof (gpointer), 5);
    self->wow_transition_impls = impls;
    gum_collect_export_by_handle (impls, ntmod, "Wow64Transition");
    gum_collect_export_by_handle (impls, usermod, "Wow64Transition");
    gum_collect_export (impls, L"kernel32.dll", "Wow64Transition");
    gum_collect_export (impls, L"kernelbase.dll", "Wow64Transition");
    gum_collect_export (impls, L"win32u.dll", "Wow64Transition");
  }
# endif
#endif

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

#ifdef HAVE_WINDOWS
  {
    GumExceptor * exceptor;

    exceptor = g_steal_pointer (&self->exceptor);
    if (exceptor != NULL)
    {
      gum_exceptor_remove (exceptor, gum_stalker_on_exception, self);

      g_object_unref (exceptor);
    }
  }
#endif

  G_OBJECT_CLASS (gum_stalker_parent_class)->dispose (object);
}

static void
gum_stalker_finalize (GObject * object)
{
  GumStalker * self = GUM_STALKER (object);

#if defined (HAVE_WINDOWS) && GLIB_SIZEOF_VOID_P == 4
  g_array_unref (self->wow_transition_impls);
#endif

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
    case PROP_ADJACENT_BLOCKS:
      g_value_set_uint (value, self->adj_blocks);
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
    case PROP_ADJACENT_BLOCKS:
      self->adj_blocks = g_value_get_uint (value);
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

#ifdef _MSC_VER

#define RETURN_ADDRESS_POINTER_FROM_FIRST_ARGUMENT(arg)   \
    ((gpointer *) ((volatile guint8 *) &arg - sizeof (gpointer)))

GUM_NOINLINE void
gum_stalker_follow_me (GumStalker * self,
                       GumStalkerTransformer * transformer,
                       GumEventSink * sink)
{
  gpointer * ret_addr_ptr;

  ret_addr_ptr = RETURN_ADDRESS_POINTER_FROM_FIRST_ARGUMENT (self);

  _gum_stalker_do_follow_me (self, transformer, sink, ret_addr_ptr);
}

#endif

void
_gum_stalker_do_follow_me (GumStalker * self,
                           GumStalkerTransformer * transformer,
                           GumEventSink * sink,
                           gpointer * ret_addr_ptr)
{
  GumExecCtx * ctx;
  gpointer code_address;

  ctx = gum_stalker_create_exec_ctx (self, gum_process_get_current_thread_id (),
      transformer, sink);
  g_private_set (&gum_stalker_exec_ctx_private, ctx);

  ctx->current_block = gum_exec_ctx_obtain_block_for (ctx, *ret_addr_ptr,
      &code_address);

  if (gum_exec_ctx_maybe_unfollow (ctx, *ret_addr_ptr))
  {
    gum_stalker_destroy_exec_ctx (self, ctx);
    return;
  }

  gum_event_sink_start (ctx->sink);
  ctx->sink_started = TRUE;

  *ret_addr_ptr = code_address;
}

GUM_NOINLINE void
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
  GumInfectContext * infect_context = (GumInfectContext *) user_data;
  GumStalker * self = infect_context->stalker;
  GumExecCtx * ctx;
  guint8 * pc;
  const guint max_syscall_size = 2;
  gpointer code_address;
  GumX86Writer * cw;

  ctx = gum_stalker_create_exec_ctx (self, thread_id,
      infect_context->transformer, infect_context->sink);

  pc = GSIZE_TO_POINTER (GUM_CPU_CONTEXT_XIP (cpu_context));

  ctx->current_block = gum_exec_ctx_obtain_block_for (ctx, pc, &code_address);

  if (gum_exec_ctx_maybe_unfollow (ctx, NULL))
  {
    gum_stalker_destroy_exec_ctx (self, ctx);
    return;
  }

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (self, ctx->thunks, self->thunks_size);
  cw = &ctx->code_writer;
  gum_x86_writer_reset (cw, ctx->infect_thunk);

  /*
   * In case the thread is in a Linux system call we should allow it to be
   * restarted by bringing along the syscall instruction.
   */
  gum_x86_writer_put_bytes (cw, pc - max_syscall_size, max_syscall_size);

  ctx->infect_body = GUM_ADDRESS (gum_x86_writer_cur (cw));
  gum_exec_ctx_write_prolog (ctx, GUM_PROLOG_MINIMAL, cw);
  gum_x86_writer_put_call_address_with_aligned_arguments (cw, GUM_CALL_CAPI,
      GUM_ADDRESS (g_private_set), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (&gum_stalker_exec_ctx_private),
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx));
  gum_exec_ctx_write_epilog (ctx, GUM_PROLOG_MINIMAL, cw);

  gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (code_address));

  gum_x86_writer_flush (cw);
  gum_stalker_freeze (self, cw->base, gum_x86_writer_offset (cw));

  gum_spinlock_release (&ctx->code_lock);

  gum_event_sink_start (ctx->sink);
  ctx->sink_started = TRUE;

#ifdef HAVE_WINDOWS
  {
    gboolean probably_in_syscall;

    probably_in_syscall =
# if GLIB_SIZEOF_VOID_P == 8
        pc[0] == 0xc3 && pc[-2] == 0x0f && pc[-1] == 0x05;
# else
        (pc[0] == 0xc2 || pc[0] == 0xc3) &&
            pc[-2] == 0xff && (pc[-1] & 0xf8) == 0xd0;
# endif
    if (probably_in_syscall)
    {
      gboolean breakpoint_deployed = FALSE;
      HANDLE thread;

      thread = OpenThread (THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE,
          thread_id);
      if (thread != NULL)
      {
#ifdef _MSC_VER
        __declspec (align (64))
#endif
            CONTEXT tc
#ifndef _MSC_VER
              __attribute__ ((aligned (64)))
#endif
              = { 0, };

        tc.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (GetThreadContext (thread, &tc))
        {
          ctx->previous_pc = GPOINTER_TO_SIZE (pc);
          ctx->previous_dr0 = tc.Dr0;
          ctx->previous_dr7 = tc.Dr7;

          tc.Dr0 = GPOINTER_TO_SIZE (pc);
          tc.Dr7 = 0x00000700;
          gum_enable_hardware_breakpoint (&tc.Dr7, 0);

          breakpoint_deployed = SetThreadContext (thread, &tc);
        }

        CloseHandle (thread);
      }

      if (!breakpoint_deployed)
        gum_stalker_destroy_exec_ctx (self, ctx);

      return;
    }
  }
#endif

  GUM_CPU_CONTEXT_XIP (cpu_context) = ctx->infect_body;
}

static void
gum_stalker_disinfect (GumThreadId thread_id,
                       GumCpuContext * cpu_context,
                       gpointer user_data)
{
  GumDisinfectContext * disinfect_context = user_data;
  GumExecCtx * ctx = disinfect_context->exec_ctx;
  gboolean infection_not_active_yet;

#ifdef HAVE_WINDOWS
  infection_not_active_yet =
      GUM_CPU_CONTEXT_XIP (cpu_context) == ctx->previous_pc;
  if (infection_not_active_yet)
  {
    HANDLE thread;

    thread = OpenThread (THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE,
        thread_id);
    if (thread != NULL)
    {
#ifdef _MSC_VER
      __declspec (align (64))
#endif
          CONTEXT tc
#ifndef _MSC_VER
            __attribute__ ((aligned (64)))
#endif
            = { 0, };

      tc.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      if (GetThreadContext (thread, &tc))
      {
        tc.Dr0 = ctx->previous_dr0;
        tc.Dr7 = ctx->previous_dr7;

        ctx->previous_pc = 0;

        disinfect_context->success = SetThreadContext (thread, &tc);
      }

      CloseHandle (thread);
    }
  }
#else
  infection_not_active_yet =
      GUM_CPU_CONTEXT_XIP (cpu_context) == ctx->infect_body;
  if (infection_not_active_yet)
  {
    GUM_CPU_CONTEXT_XIP (cpu_context) =
        GPOINTER_TO_SIZE (ctx->current_block->real_start);

    disinfect_context->success = TRUE;
  }
#endif
}

#ifdef _MSC_VER

GUM_NOINLINE void
gum_stalker_activate (GumStalker * self,
                      gconstpointer target)
{
  gpointer * ret_addr_ptr;

  ret_addr_ptr = RETURN_ADDRESS_POINTER_FROM_FIRST_ARGUMENT (self);

  _gum_stalker_do_activate (self, target, ret_addr_ptr);
}

GUM_NOINLINE void
gum_stalker_deactivate (GumStalker * self)
{
  gpointer * ret_addr_ptr;

  ret_addr_ptr = RETURN_ADDRESS_POINTER_FROM_FIRST_ARGUMENT (self);

  _gum_stalker_do_deactivate (self, ret_addr_ptr);
}

#endif

void
_gum_stalker_do_activate (GumStalker * self,
                          gconstpointer target,
                          gpointer * ret_addr_ptr)
{
  guint8 * ret_addr = *ret_addr_ptr;
  GumExecCtx * ctx;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return;

  ctx->unfollow_called_while_still_following = FALSE;
  ctx->activation_target = target;

  if (!gum_exec_ctx_contains (ctx, ret_addr))
  {
    gpointer code_address;

    ctx->current_block =
        gum_exec_ctx_obtain_block_for (ctx, ret_addr, &code_address);

    if (gum_exec_ctx_maybe_unfollow (ctx, ret_addr))
      return;

    *ret_addr_ptr = code_address;
  }
}

void
_gum_stalker_do_deactivate (GumStalker * self,
                            gpointer * ret_addr_ptr)
{
  GumExecCtx * ctx;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return;

  ctx->unfollow_called_while_still_following = TRUE;
  ctx->activation_target = NULL;

  if (gum_exec_ctx_contains (ctx, *ret_addr_ptr))
  {
    ctx->pending_calls--;

    *ret_addr_ptr = ctx->pending_return_location;
  }
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
          call->code_offset, call->opened_prolog, call->ret_real_address,
          call->ret_code_offset);
      break;
    }
    case GUM_BACKPATCH_JMP:
    {
      const GumBackpatchJmp * jmp = &backpatch->jmp;
      gum_exec_block_backpatch_jmp (block_to, block_from, from_insn, jmp->id,
          jmp->code_offset, jmp->opened_prolog);
      break;
    }
    case GUM_BACKPATCH_INLINE_CACHE:
    {
      gum_exec_block_backpatch_inline_cache (block_to, block_from, from_insn);
      break;
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
  const guint8 * pc = GSIZE_TO_POINTER (GUM_CPU_CONTEXT_XIP (cpu_context));

  if (pc >= block->code_start &&
      pc < block->code_start + GUM_INVALIDATE_TRAMPOLINE_SIZE)
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
  guint8 * pc;
  GumX86Writer * cw;
  GumAddress cpu_context_copy;

  ctx = gum_stalker_create_exec_ctx (self, thread_id, NULL, NULL);

  pc = GSIZE_TO_POINTER (GUM_CPU_CONTEXT_XIP (cpu_context));

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (self, ctx->thunks, self->thunks_size);
  cw = &ctx->code_writer;
  gum_x86_writer_reset (cw, ctx->infect_thunk);

  cpu_context_copy = GUM_ADDRESS (gum_x86_writer_cur (cw));
  gum_x86_writer_put_bytes (cw, (guint8 *) cpu_context, sizeof (GumCpuContext));

#ifdef HAVE_LINUX
  /*
   * In case the thread is in a Linux system call we prefix with a couple of
   * NOPs so that when we restart, we don't re-attempt the syscall. We will
   * drop ourselves back to the syscall once we are done.
   */
  gum_x86_writer_put_nop_padding (cw, MAX (sizeof (gum_int80_code),
      sizeof (gum_syscall_code)));
#endif

  ctx->infect_body = GUM_ADDRESS (gum_x86_writer_cur (cw));
  gum_exec_ctx_write_prolog (ctx, GUM_PROLOG_MINIMAL, cw);
  gum_x86_writer_put_call_address_with_aligned_arguments (cw, GUM_CALL_CAPI,
      GUM_ADDRESS (func), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (cpu_context_copy),
      GUM_ARG_ADDRESS, GUM_ADDRESS (data));
  gum_x86_writer_put_call_address_with_aligned_arguments (cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_exec_ctx_unfollow), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (pc));
  gum_exec_ctx_write_epilog (ctx, GUM_PROLOG_MINIMAL, cw);

#ifdef HAVE_LINUX
  if (memcmp (&pc[-sizeof (gum_int80_code)], gum_int80_code,
        sizeof (gum_int80_code)) == 0)
  {
    gum_x86_writer_put_jmp_address (cw,
        GUM_ADDRESS (&pc[-sizeof (gum_int80_code)]));
  }
  else if (memcmp (&pc[-sizeof (gum_syscall_code)], gum_syscall_code,
        sizeof (gum_syscall_code)) == 0)
  {
    gum_x86_writer_put_jmp_address (cw,
        GUM_ADDRESS (&pc[-sizeof (gum_syscall_code)]));
  }
  else
  {
    gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (pc));
  }
#else
  gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (pc));
#endif

  gum_x86_writer_flush (cw);
  gum_stalker_freeze (self, cw->base, gum_x86_writer_offset (cw));

  gum_spinlock_release (&ctx->code_lock);

  GUM_CPU_CONTEXT_XIP (cpu_context) = ctx->infect_body;
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

static GumAddress
gum_stalker_translate_unwind_pc (GumUnwindPcTranslator * translator,
                                 GumAddress code_address)
{
#ifdef HAVE_LINUX
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
gum_stalker_install_unwind_resume_context (
    GumUnwindPcTranslator * translator,
    gpointer unwind_context,
    GumAddress real_resume_ip)
{
#ifdef HAVE_LINUX
  GumExecCtx * ctx;
  gpointer resume_ip;

  ctx = gum_stalker_get_exec_ctx ();
  if (ctx == NULL)
    return FALSE;

  resume_ip = gum_exec_ctx_switch_block (ctx, NULL,
      GSIZE_TO_POINTER (real_resume_ip), NULL);
  _Unwind_SetIP ((struct _Unwind_Context *) unwind_context,
      GPOINTER_TO_SIZE (resume_ip));

  ctx->pending_calls--;

  return TRUE;
#else
  return FALSE;
#endif
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
  return self->ic_entries * (2 * sizeof (gpointer));
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
  ctx->mode = GUM_EXEC_CTX_NORMAL;

  ctx->stalker = g_object_ref (stalker);
  ctx->thread_id = thread_id;

  gum_x86_writer_init (&ctx->code_writer, NULL);
  gum_x86_writer_init (&ctx->slow_writer, NULL);
  gum_x86_relocator_init (&ctx->relocator, NULL, &ctx->code_writer);

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
  gum_code_slab_init (code_slab, stalker->code_slab_size_initial,
      stalker->page_size);
  gum_exec_ctx_add_code_slab (ctx, code_slab);

  slow_slab = (GumSlowSlab *) (base + stalker->slow_slab_offset);
  gum_slow_slab_init (slow_slab, stalker->slow_slab_size_initial,
      stalker->page_size);
  gum_exec_ctx_add_slow_slab (ctx, slow_slab);

  data_slab = (GumDataSlab *) (base + stalker->data_slab_offset);
  gum_data_slab_init (data_slab, stalker->data_slab_size_initial);
  gum_exec_ctx_add_data_slab (ctx, data_slab);

  ctx->scratch_slab = (GumCodeSlab *) (base + stalker->scratch_slab_offset);
  gum_scratch_slab_init (ctx->scratch_slab, stalker->scratch_slab_size);

  ctx->mappings = gum_metal_hash_table_new (NULL, NULL);

  gum_exec_ctx_ensure_inline_helpers_reachable (ctx);

  ctx->depth = 0;

#ifdef HAVE_LINUX
  /*
   * We need to build an array of ranges in which the .plt.got and .plt.sec
   * sections of the loaded modules reside to allow us to treat tail calls into
   * them as excluded calls (even though they use a JMP instruction). However,
   * calling into the dynamic loader or even just allocating data on the heap is
   * dangerous when actually stalking a target since we could cause the target
   * to re-enter a section of code which is not designed to be. We will
   * therefore build up our picture of the memory map when Stalker is first
   * instantiated to avoid this potential problem. Should the memory map change
   * afterwards (e.g. another library is loaded) then we will not notice and
   * tail calls into the .plt.got and .plt.sec will not be optimized. However,
   * the application should continue to function as expected.
   */
  gum_exec_ctx_get_plt_got_ranges ();

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
  GumSlowSlab * slow_slab;

  gum_metal_hash_table_unref (ctx->mappings);

  data_slab = ctx->data_slab;
  while (TRUE)
  {
    GumDataSlab * next = (GumDataSlab *) data_slab->slab.next;
    gboolean is_initial;

    is_initial = next == NULL;
    if (is_initial)
      break;

    gum_data_slab_free (data_slab);

    data_slab = next;
  }

  code_slab = ctx->code_slab;
  while (TRUE)
  {
    GumCodeSlab * next = (GumCodeSlab *) code_slab->slab.next;
    gboolean is_initial;

    is_initial = next == NULL;
    if (is_initial)
      break;

    gum_code_slab_free (code_slab);

    code_slab = next;
  }

  slow_slab = ctx->slow_slab;
  while (TRUE)
  {
    GumSlowSlab * next = (GumSlowSlab *) slow_slab->slab.next;
    gboolean is_initial;

    is_initial = next == NULL;
    if (is_initial)
      break;

    gum_slow_slab_free (slow_slab);

    slow_slab = next;
  }

  g_object_unref (ctx->sink);
  g_object_unref (ctx->transformer);
  g_clear_object (&ctx->observer);

  gum_x86_relocator_clear (&ctx->relocator);
  gum_x86_writer_clear (&ctx->slow_writer);
  gum_x86_writer_clear (&ctx->code_writer);

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

#ifdef HAVE_LINUX
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

static void
gum_exec_ctx_compute_code_address_spec (GumExecCtx * ctx,
                                        gsize slab_size,
                                        GumAddressSpec * spec)
{
  GumStalker * stalker = ctx->stalker;

  /* Code must be able to reference ExecCtx fields using 32-bit offsets. */
  spec->near_address = ctx;
  spec->max_distance = G_MAXINT32 - stalker->ctx_size - slab_size;
}

static void
gum_exec_ctx_compute_data_address_spec (GumExecCtx * ctx,
                                        gsize slab_size,
                                        GumAddressSpec * spec)
{
  GumStalker * stalker = ctx->stalker;

  /* Code must be able to reference ExecBlock fields using 32-bit offsets. */
  spec->near_address = ctx->code_slab;
  spec->max_distance = G_MAXINT32 - stalker->code_slab_size_dynamic - slab_size;
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
  GumSlab * code_slab = &ctx->code_slab->slab;
  GumSlab * slow_slab = &ctx->slow_slab->slab;

  do
  {
    if ((const guint8 *) address >= code_slab->data &&
        (const guint8 *) address < (guint8 *) gum_slab_cursor (code_slab))
    {
      return TRUE;
    }

    code_slab = code_slab->next;
  }
  while (code_slab != NULL);

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
GUM_DEFINE_ENTRYGATE (call_mem)
GUM_DEFINE_ENTRYGATE (excluded_call_imm)
GUM_DEFINE_ENTRYGATE (ret_slow_path)

GUM_DEFINE_ENTRYGATE (jmp_imm)
GUM_DEFINE_ENTRYGATE (jmp_mem)
GUM_DEFINE_ENTRYGATE (jmp_reg)

GUM_DEFINE_ENTRYGATE (jmp_cond_imm)
GUM_DEFINE_ENTRYGATE (jmp_cond_mem)
GUM_DEFINE_ENTRYGATE (jmp_cond_reg)
GUM_DEFINE_ENTRYGATE (jmp_cond_jcxz)

GUM_DEFINE_ENTRYGATE (jmp_continuation)

#ifdef GUM_HAVE_SYSENTER_VIRTUALIZATION
GUM_DEFINE_ENTRYGATE (sysenter_slow_path)
#endif

static gpointer
gum_exec_ctx_switch_block (GumExecCtx * ctx,
                           GumExecBlock * block,
                           gpointer start_address,
                           gpointer from_insn)
{
  if (ctx->observer != NULL)
    gum_stalker_observer_increment_total (ctx->observer);

  if (start_address == gum_stalker_unfollow_me ||
      start_address == gum_stalker_deactivate)
  {
    ctx->unfollow_called_while_still_following = TRUE;
    ctx->current_block = NULL;
    ctx->resume_at = start_address;
  }
  else if (start_address == _gum_thread_exit_impl)
  {
    gum_exec_ctx_unfollow (ctx, start_address);
  }
  else if (gum_exec_ctx_maybe_unfollow (ctx, start_address))
  {
  }
  else if (gum_exec_ctx_contains (ctx, start_address))
  {
    ctx->resume_at = start_address;
    ctx->current_block = NULL;
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
                                         gint32 * distance_to_data)
{
  GumExecBlock * block;
  gpointer start_address;

  block = (GumExecBlock *) ((guint8 *) distance_to_data + *distance_to_data);
  start_address = block->real_start;

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
    GumExecBlock * cur;
    guint i;

    block = gum_exec_ctx_build_block (ctx, real_address);
    cur = block;

    /*
     * Fetch the next `n` blocks which are adjacent in the target application so
     * that they are more likely to also appear adjacently in the code_slab.
     * This allows us to transfer control-flow between adjacent blocks using a
     * NOP slide rather than a branch instruction giving an increase in
     * performance.
     */
    for (i = 0; i != ctx->stalker->adj_blocks; i++)
    {
      /*
       * If we reach the end of input (e.g. a RET instruction or a JMP) we
       * cannot be sure that what follows is actually code (it could just be
       * data, or we might reach the end of mapped memory), we must therefore
       * stop fetching blocks.
       */
      if (gum_x86_relocator_eoi (&ctx->relocator))
        break;

      real_address = cur->real_start + cur->real_size;

      /*
       * Don't prefetch adjacent blocks which are in the excluded range, as
       * their treatment depends on whether we have reached the
       * activation_target as to whether they are actually treated as excluded.
       * We don't know this unless we wait to compile the block until it is
       * actually run. This means we can't speculatively compile it early.
       */
      if (gum_stalker_is_excluding (ctx->stalker, real_address))
        break;

      /* Don't fetch any duplicates */
      /* TODO: Consider whether fetching duplicates will improve performance */
      if (gum_metal_hash_table_lookup (ctx->mappings, real_address) != NULL)
        break;

      cur = gum_exec_ctx_build_block (ctx, real_address);
    }

    gum_spinlock_release (&ctx->code_lock);
  }

  *code_address = block->code_start;

  return block;
}

static GumExecBlock *
gum_exec_ctx_build_block (GumExecCtx * ctx,
                          gpointer real_address)
{
  GumExecBlock * block = gum_exec_block_new (ctx);

  block->real_start = real_address;
  gum_exec_ctx_compile_block (ctx, block, real_address, block->code_start,
      GUM_ADDRESS (block->code_start), &block->real_size, &block->code_size,
      &block->slow_size);
  gum_exec_block_commit (block);

  gum_metal_hash_table_insert (ctx->mappings, real_address, block);

  gum_exec_ctx_maybe_emit_compile_event (ctx, block);

  return block;
}

static void
gum_exec_ctx_recompile_block (GumExecCtx * ctx,
                              GumExecBlock * block)
{
  GumStalker * stalker = ctx->stalker;
  guint8 * internal_code = block->code_start;
  GumCodeSlab * slab;
  guint8 * scratch_base;
  guint input_size, output_size, slow_size;
  gsize new_snapshot_size, new_block_size;

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (stalker, internal_code, block->capacity);

  if (block->storage_block != NULL)
    gum_exec_block_clear (block->storage_block);
  gum_exec_block_clear (block);

  slab = block->code_slab;
  block->code_slab = ctx->scratch_slab;
  block->slow_slab = ctx->slow_slab;
  scratch_base = ctx->scratch_slab->slab.data;
  ctx->scratch_slab->invalidator = slab->invalidator;

  gum_exec_ctx_compile_block (ctx, block, block->real_start, scratch_base,
      GUM_ADDRESS (internal_code), &input_size, &output_size, &slow_size);

  block->code_slab = slab;

  new_snapshot_size =
      gum_stalker_snapshot_space_needed_for (stalker, input_size);

  new_block_size = output_size + new_snapshot_size;

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
    GumX86Writer * cw = &ctx->code_writer;

    storage_block = gum_exec_block_new (ctx);
    storage_block->real_start = block->real_start;
    gum_exec_ctx_compile_block (ctx, block, block->real_start,
        storage_block->code_start, GUM_ADDRESS (storage_block->code_start),
        &storage_block->real_size, &storage_block->code_size,
        &storage_block->slow_size);
    gum_exec_block_commit (storage_block);
    block->storage_block = storage_block;

    gum_stalker_thaw (stalker, internal_code, block->capacity);
    gum_x86_writer_reset (cw, internal_code);

    gum_x86_writer_put_jmp_address (cw,
        GUM_ADDRESS (storage_block->code_start));

    gum_x86_writer_flush (cw);
    gum_stalker_freeze (stalker, internal_code, block->capacity);
  }

  gum_spinlock_release (&ctx->code_lock);

  gum_exec_ctx_maybe_emit_compile_event (ctx, block);
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
  GumX86Writer * cw = &ctx->code_writer;
  GumX86Writer * cws = &ctx->slow_writer;
  GumX86Relocator * rl = &ctx->relocator;
  GumGeneratorContext gc;
  GumStalkerIterator iterator;
  GumStalkerOutput output;
  gboolean all_labels_resolved;
  gboolean all_slow_labels_resolved;

  gum_x86_writer_reset (cw, output_code);
  cw->pc = output_pc;

  gum_x86_writer_reset (cws, block->slow_start);
  cws->pc = GUM_ADDRESS (block->slow_start);

  gum_x86_relocator_reset (rl, input_code, cw);

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

  output.writer.x86 = cw;
  output.encoding = GUM_INSTRUCTION_DEFAULT;

  gum_exec_block_maybe_write_call_probe_code (block, &gc);

  ctx->pending_calls++;
  ctx->transform_block_impl (ctx->transformer, &iterator, &output);
  ctx->pending_calls--;

  if (gc.continuation_real_address != NULL)
  {
    GumBranchTarget continue_target = { 0, };

    continue_target.is_indirect = FALSE;
    continue_target.absolute_address = gc.continuation_real_address;

    gum_exec_block_write_jmp_transfer_code (block, &continue_target,
        GUM_ENTRYGATE (jmp_continuation), &gc, X86_INS_JMP, GUM_ADDRESS (0));
  }

  gum_x86_writer_put_breakpoint (cw); /* Should never get here */

  all_labels_resolved = gum_x86_writer_flush (cw);
  if (!all_labels_resolved)
    gum_panic ("Failed to resolve labels");

  all_slow_labels_resolved = gum_x86_writer_flush (cws);
  if (!all_slow_labels_resolved)
    gum_panic ("Failed to resolve slow labels");

  *input_size = rl->input_cur - rl->input_start;
  *output_size = gum_x86_writer_offset (cw);
  *slow_size = gum_x86_writer_offset (cws);
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
  GumX86Relocator * rl = gc->relocator;
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
      gum_x86_relocator_skip_one_no_label (rl);
    }

    if (gum_stalker_iterator_is_out_of_space (self))
    {
      gc->continuation_real_address = instruction->end;
      return FALSE;
    }

    if (!skip_implicitly_requested && gum_x86_relocator_eob (rl))
      return FALSE;
  }

  instruction = &self->instruction;

  n_read = gum_x86_relocator_read_one (rl, &instruction->ci);
  if (n_read == 0)
    return FALSE;

  instruction->start = GSIZE_TO_POINTER (instruction->ci->address);
  instruction->end = instruction->start + instruction->ci->size;

  self->generator_context->instruction = instruction;

  if (is_first_instruction && (self->exec_context->sink_mask & GUM_BLOCK) != 0)
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
  gsize capacity, snapshot_size;

  capacity = (guint8 *) gum_slab_end (slab) -
      (guint8 *) gum_x86_writer_cur (self->generator_context->code_writer);

  snapshot_size = gum_stalker_snapshot_space_needed_for (
      self->exec_context->stalker,
      self->generator_context->instruction->end - block->real_start);

  return capacity < GUM_EXEC_BLOCK_MIN_CAPACITY + snapshot_size +
      gum_stalker_get_ic_entry_size (self->exec_context->stalker);
}

void
gum_stalker_iterator_keep (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  GumX86Relocator * rl = gc->relocator;
  const cs_insn * insn = gc->instruction->ci;
  GumVirtualizationRequirements requirements;

  if ((self->exec_context->sink_mask & GUM_EXEC) != 0)
    gum_exec_block_write_exec_event_code (block, gc, GUM_CODE_INTERRUPTIBLE);

  switch (insn->id)
  {
    case X86_INS_CALL:
    case X86_INS_JMP:
      requirements = gum_exec_block_virtualize_branch_insn (block, gc);
      break;
    case X86_INS_RET:
      requirements = gum_exec_block_virtualize_ret_insn (block, gc);
      break;
    case X86_INS_SYSENTER:
      requirements = gum_exec_block_virtualize_sysenter_insn (block, gc);
      break;
    case X86_INS_SYSCALL:
      requirements = gum_exec_block_virtualize_syscall_insn (block, gc);
      break;
    case X86_INS_INT:
      requirements = gum_exec_block_virtualize_int_insn (block, gc);
      break;
    case X86_INS_JECXZ:
    case X86_INS_JRCXZ:
      requirements = gum_exec_block_virtualize_branch_insn (block, gc);
      break;
    default:
      if (gum_x86_reader_insn_is_jcc (insn))
        requirements = gum_exec_block_virtualize_branch_insn (block, gc);
      else
        requirements = GUM_REQUIRE_RELOCATION;
      break;
  }

  gum_exec_block_close_prolog (block, gc, gc->code_writer);

  if ((requirements & GUM_REQUIRE_RELOCATION) != 0)
  {
    gum_x86_relocator_write_one_no_label (rl);
  }
  else if ((requirements & GUM_REQUIRE_SINGLE_STEP) != 0)
  {
    gum_x86_relocator_skip_one_no_label (rl);
    gum_exec_block_write_single_step_transfer_code (block, gc);
  }

  self->requirements = requirements;
}

GumMemoryAccess
gum_stalker_iterator_get_memory_access (GumStalkerIterator * self)
{
  return GUM_MEMORY_ACCESS_OPEN;
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

  GUM_CPU_CONTEXT_XIP (cpu_context) = GPOINTER_TO_SIZE (location);

  ctx->sink_process_impl (ctx->sink, &ev, cpu_context);
}

static void
gum_exec_ctx_emit_ret_event (GumExecCtx * ctx,
                             gpointer location,
                             GumCpuContext * cpu_context)
{
  GumEvent ev;
  GumRetEvent * ret = &ev.ret;

  ev.type = GUM_RET;

  ret->location = location;
  ret->target = *((gpointer *) ctx->app_stack);
  ret->depth = ctx->depth;

  GUM_CPU_CONTEXT_XIP (cpu_context) = GPOINTER_TO_SIZE (location);

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

  GUM_CPU_CONTEXT_XIP (cpu_context) = GPOINTER_TO_SIZE (location);

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

  GUM_CPU_CONTEXT_XIP (cpu_context) = GPOINTER_TO_SIZE (block->real_start);

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
  GumX86Writer * cw = gc->code_writer;
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
  gum_x86_writer_put_call_address_with_aligned_arguments (cw,
      GUM_CALL_CAPI, GUM_ADDRESS (gum_stalker_invoke_callout), 2,
      GUM_ARG_ADDRESS, entry_address,
      GUM_ARG_REGISTER, GUM_X86_XBX);
  gum_exec_block_close_prolog (block, gc, gc->code_writer);
}

static void
gum_stalker_invoke_callout (GumCalloutEntry * entry,
                            GumCpuContext * cpu_context)
{
  GumExecCtx * ec = entry->exec_context;

  GUM_CPU_CONTEXT_XIP (cpu_context) = GPOINTER_TO_SIZE (entry->pc);

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

  gum_exec_block_write_adjust_depth (block, gc->code_writer, -1);

  gum_exec_block_write_chaining_return_code (block, gc, 0);
}

csh
gum_stalker_iterator_get_capstone (GumStalkerIterator * self)
{
  return self->exec_context->relocator.capstone;
}

static void
gum_exec_ctx_write_prolog (GumExecCtx * ctx,
                           GumPrologType type,
                           GumX86Writer * cw)
{
  switch (type)
  {
    case GUM_PROLOG_MINIMAL:
    case GUM_PROLOG_FULL:
    {
      gpointer helper;

      helper = (type == GUM_PROLOG_MINIMAL)
          ? ctx->last_prolog_minimal
          : ctx->last_prolog_full;

      gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
          GUM_X86_XSP, -GUM_RED_ZONE_SIZE);
      gum_x86_writer_put_call_address (cw, GUM_ADDRESS (helper));

      break;
    }
    case GUM_PROLOG_IC:
    {
      gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
          GUM_X86_XSP, -GUM_RED_ZONE_SIZE);

      gum_x86_writer_put_push_reg (cw, GUM_X86_XAX);
      gum_x86_writer_put_lahf (cw);
      gum_x86_writer_put_push_reg (cw, GUM_X86_XAX);

      gum_x86_writer_put_push_reg (cw, GUM_X86_XBX);
      gum_x86_writer_put_mov_reg_reg (cw, GUM_X86_XBX, GUM_X86_XSP);

      gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XAX, GUM_X86_XSP,
          3 * sizeof (gpointer) + GUM_RED_ZONE_SIZE);
      gum_x86_writer_put_mov_near_ptr_reg (cw, GUM_ADDRESS (&ctx->app_stack),
          GUM_X86_XAX);

      break;
    }
    default:
    {
      g_assert_not_reached ();
      break;
    }
  }
}

static void
gum_exec_ctx_write_epilog (GumExecCtx * ctx,
                           GumPrologType type,
                           GumX86Writer * cw)
{
  switch (type)
  {
    case GUM_PROLOG_MINIMAL:
    case GUM_PROLOG_FULL:
    {
      gpointer helper;

      helper = (type == GUM_PROLOG_MINIMAL)
          ? ctx->last_epilog_minimal
          : ctx->last_epilog_full;

      gum_x86_writer_put_call_address (cw, GUM_ADDRESS (helper));
      gum_x86_writer_put_mov_reg_near_ptr (cw, GUM_X86_XSP,
          GUM_ADDRESS (&ctx->app_stack));

      break;
    }
    case GUM_PROLOG_IC:
    {
      gum_x86_writer_put_pop_reg (cw, GUM_X86_XBX);

      gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);
      gum_x86_writer_put_sahf (cw);
      gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);

      gum_x86_writer_put_mov_reg_near_ptr (cw, GUM_X86_XSP,
          GUM_ADDRESS (&ctx->app_stack));

      break;
    }
    default:
    {
      g_assert_not_reached ();
      break;
    }
  }
}

static void
gum_exec_ctx_ensure_inline_helpers_reachable (GumExecCtx * ctx)
{
  gum_exec_ctx_ensure_helper_reachable (ctx, &ctx->last_prolog_minimal,
      gum_exec_ctx_write_minimal_prolog_helper);
  gum_exec_ctx_ensure_helper_reachable (ctx, &ctx->last_epilog_minimal,
      gum_exec_ctx_write_minimal_epilog_helper);

  gum_exec_ctx_ensure_helper_reachable (ctx, &ctx->last_prolog_full,
      gum_exec_ctx_write_full_prolog_helper);
  gum_exec_ctx_ensure_helper_reachable (ctx, &ctx->last_epilog_full,
      gum_exec_ctx_write_full_epilog_helper);

  gum_exec_ctx_ensure_helper_reachable (ctx, &ctx->last_invalidator,
      gum_exec_ctx_write_invalidator);
  ctx->code_slab->invalidator = ctx->last_invalidator;

#ifdef HAVE_LINUX
  gum_exec_ctx_ensure_helper_reachable (ctx, &ctx->last_int80,
      gum_exec_ctx_write_int80_helper);

  gum_exec_ctx_ensure_helper_reachable (ctx, &ctx->last_syscall,
      gum_exec_ctx_write_syscall_helper);
#endif
}

static void
gum_exec_ctx_write_minimal_prolog_helper (GumExecCtx * ctx,
                                          GumX86Writer * cw)
{
  gum_exec_ctx_write_prolog_helper (ctx, GUM_PROLOG_MINIMAL, cw);
}

static void
gum_exec_ctx_write_minimal_epilog_helper (GumExecCtx * ctx,
                                          GumX86Writer * cw)
{
  gum_exec_ctx_write_epilog_helper (ctx, GUM_PROLOG_MINIMAL, cw);
}

static void
gum_exec_ctx_write_full_prolog_helper (GumExecCtx * ctx,
                                       GumX86Writer * cw)
{
  gum_exec_ctx_write_prolog_helper (ctx, GUM_PROLOG_FULL, cw);
}

static void
gum_exec_ctx_write_full_epilog_helper (GumExecCtx * ctx,
                                       GumX86Writer * cw)
{
  gum_exec_ctx_write_epilog_helper (ctx, GUM_PROLOG_FULL, cw);
}

static void
gum_exec_ctx_write_prolog_helper (GumExecCtx * ctx,
                                  GumPrologType type,
                                  GumX86Writer * cw)
{
  guint8 fxsave[] = {
    0x0f, 0xae, 0x04, 0x24 /* fxsave [esp] */
  };
  guint8 upper_ymm_saver[] = {
#if GLIB_SIZEOF_VOID_P == 8
    /* vextracti128 ymm0..ymm15, [rsp+0x0]..[rsp+0xF0], 1 */
    0xc4, 0xe3, 0x7d, 0x39, 0x04, 0x24, 0x01,
    0xc4, 0xe3, 0x7d, 0x39, 0x4c, 0x24, 0x10, 0x01,
    0xc4, 0xe3, 0x7d, 0x39, 0x54, 0x24, 0x20, 0x01,
    0xc4, 0xe3, 0x7d, 0x39, 0x5c, 0x24, 0x30, 0x01,
    0xc4, 0xe3, 0x7d, 0x39, 0x64, 0x24, 0x40, 0x01,
    0xc4, 0xe3, 0x7d, 0x39, 0x6c, 0x24, 0x50, 0x01,
    0xc4, 0xe3, 0x7d, 0x39, 0x74, 0x24, 0x60, 0x01,
    0xc4, 0xe3, 0x7d, 0x39, 0x7c, 0x24, 0x70, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0x8c, 0x24, 0x90, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0x94, 0x24, 0xa0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0x9c, 0x24, 0xb0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0xa4, 0x24, 0xc0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0xac, 0x24, 0xd0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0xb4, 0x24, 0xe0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x7d, 0x39, 0xbc, 0x24, 0xf0, 0x00, 0x00, 0x00, 0x01
#else
    /* vextracti128 ymm0..ymm7, [esp+0x0]..[esp+0x70], 1 */
    0xc4, 0xc3, 0x7d, 0x39, 0x04, 0x24, 0x01,
    0xc4, 0xc3, 0x7d, 0x39, 0x4c, 0x24, 0x10, 0x01,
    0xc4, 0xc3, 0x7d, 0x39, 0x54, 0x24, 0x20, 0x01,
    0xc4, 0xc3, 0x7d, 0x39, 0x5c, 0x24, 0x30, 0x01,
    0xc4, 0xc3, 0x7d, 0x39, 0x64, 0x24, 0x40, 0x01,
    0xc4, 0xc3, 0x7d, 0x39, 0x6c, 0x24, 0x50, 0x01,
    0xc4, 0xc3, 0x7d, 0x39, 0x74, 0x24, 0x60, 0x01,
    0xc4, 0xc3, 0x7d, 0x39, 0x7c, 0x24, 0x70, 0x01
#endif
  };

  gum_x86_writer_put_pushfx (cw);
  gum_x86_writer_put_cld (cw); /* C ABI mandates this */

  if (type == GUM_PROLOG_MINIMAL)
  {
    gum_x86_writer_put_push_reg (cw, GUM_X86_XAX);

    gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XAX, GUM_X86_XSP,
        3 * sizeof (gpointer) + GUM_RED_ZONE_SIZE);
    gum_x86_writer_put_mov_near_ptr_reg (cw, GUM_ADDRESS (&ctx->app_stack),
        GUM_X86_XAX);

    gum_x86_writer_put_push_reg (cw, GUM_X86_XCX);
    gum_x86_writer_put_push_reg (cw, GUM_X86_XDX);
    gum_x86_writer_put_push_reg (cw, GUM_X86_XBX);

#if GLIB_SIZEOF_VOID_P == 8
    gum_x86_writer_put_push_reg (cw, GUM_X86_XSI);
    gum_x86_writer_put_push_reg (cw, GUM_X86_XDI);
    gum_x86_writer_put_push_reg (cw, GUM_X86_R8);
    gum_x86_writer_put_push_reg (cw, GUM_X86_R9);
    gum_x86_writer_put_push_reg (cw, GUM_X86_R10);
    gum_x86_writer_put_push_reg (cw, GUM_X86_R11);
#endif
  }
  else /* GUM_PROLOG_FULL */
  {
    gum_x86_writer_put_sub_reg_imm (cw, GUM_X86_XSP,
        sizeof (((GumCpuContext *) NULL)->xmm)); /* GumCpuContext.xmm */
    gum_x86_writer_put_pushax (cw); /* All of GumCpuContext except xip/xmm */
    /* GumCpuContext.xip gets filled out later */
    gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
        -(gssize) sizeof (gpointer));

    gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XAX, GUM_X86_XSP,
        sizeof (GumCpuContext) + 2 * sizeof (gpointer) + GUM_RED_ZONE_SIZE);
    gum_x86_writer_put_mov_near_ptr_reg (cw, GUM_ADDRESS (&ctx->app_stack),
        GUM_X86_XAX);

    gum_x86_writer_put_mov_reg_offset_ptr_reg (cw,
        GUM_X86_XSP, GUM_CPU_CONTEXT_OFFSET_XSP,
        GUM_X86_XAX);
  }

  gum_x86_writer_put_mov_reg_reg (cw, GUM_X86_XBX, GUM_X86_XSP);
  gum_x86_writer_put_and_reg_u32 (cw, GUM_X86_XSP, (guint32) ~(16 - 1));
  gum_x86_writer_put_sub_reg_imm (cw, GUM_X86_XSP, 512);
  gum_x86_writer_put_bytes (cw, fxsave, sizeof (fxsave));

  if (type == GUM_PROLOG_FULL)
  {
    gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XAX,
        GUM_X86_XSP, GUM_FXSAVE_OFFSET_XMM);
    gum_x86_writer_put_mov_reg_offset_ptr_reg (cw,
        GUM_X86_XBX, G_STRUCT_OFFSET (GumCpuContext, xmm),
        GUM_X86_XAX);
  }

  /*
   * Skip the YMM upper-half save on Windows 7 32-bit (WoW64) processes.
   * The AVX2 save/restore corrupts the wow64 transition layer's per-thread
   * state when emitted from a JIT page on that OS, causing the next
   * syscall to fault. The lower 128 bits of YMM are still preserved by
   * fxsave, and the Windows x86 ABI does not preserve the upper halves
   * across calls.
   */
  if ((ctx->stalker->cpu_features & GUM_CPU_AVX2) != 0 &&
      !_gum_windows_is_win7_wow64 ())
  {
    gum_x86_writer_put_sub_reg_imm (cw, GUM_X86_XSP, 0x100);
    gum_x86_writer_put_bytes (cw, upper_ymm_saver, sizeof (upper_ymm_saver));

#if GLIB_SIZEOF_VOID_P == 8
    if ((ctx->stalker->cpu_features & GUM_CPU_AVX512) != 0)
    {
      guint i;

      gum_x86_writer_put_sub_reg_imm (cw, GUM_X86_XSP, 16 * 32);
      for (i = 0; i != 16; i++)
        gum_x86_writer_put_vextracti64x4_reg_offset_ptr_zmm (cw, GUM_X86_XSP,
            i * 32, i, 1);

      gum_x86_writer_put_sub_reg_imm (cw, GUM_X86_XSP, 16 * 64);
      for (i = 16; i != 32; i++)
        gum_x86_writer_put_vmovdqu64_reg_offset_ptr_zmm (cw, GUM_X86_XSP,
            (i - 16) * 64, i);

      gum_x86_writer_put_sub_reg_imm (cw, GUM_X86_XSP, 8 * 8);
      for (i = 0; i != 8; i++)
        gum_x86_writer_put_kmovq_reg_offset_ptr_kreg (cw, GUM_X86_XSP,
            i * 8, i);
    }
#endif
  }

  /* Jump to our caller but leave it on the stack */
  gum_x86_writer_put_jmp_reg_offset_ptr (cw,
      GUM_X86_XBX, (type == GUM_PROLOG_MINIMAL)
          ? GUM_MINIMAL_PROLOG_RETURN_OFFSET
          : GUM_FULL_PROLOG_RETURN_OFFSET);
}

static void
gum_exec_ctx_write_epilog_helper (GumExecCtx * ctx,
                                  GumPrologType type,
                                  GumX86Writer * cw)
{
  guint8 fxrstor[] = {
    0x0f, 0xae, 0x0c, 0x24 /* fxrstor [esp] */
  };
  guint8 upper_ymm_restorer[] = {
#if GLIB_SIZEOF_VOID_P == 8
    /* vinserti128 ymm0..ymm15, ymm0..ymm15, [rsp+0x0]..[rsp+0xF0], 1 */
    0xc4, 0xe3, 0x7d, 0x38, 0x04, 0x24, 0x01,
    0xc4, 0xe3, 0x75, 0x38, 0x4c, 0x24, 0x10, 0x01,
    0xc4, 0xe3, 0x6d, 0x38, 0x54, 0x24, 0x20, 0x01,
    0xc4, 0xe3, 0x65, 0x38, 0x5c, 0x24, 0x30, 0x01,
    0xc4, 0xe3, 0x5d, 0x38, 0x64, 0x24, 0x40, 0x01,
    0xc4, 0xe3, 0x55, 0x38, 0x6c, 0x24, 0x50, 0x01,
    0xc4, 0xe3, 0x4d, 0x38, 0x74, 0x24, 0x60, 0x01,
    0xc4, 0xe3, 0x45, 0x38, 0x7c, 0x24, 0x70, 0x01,
    0xc4, 0x63, 0x3d, 0x38, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x35, 0x38, 0x8c, 0x24, 0x90, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x2d, 0x38, 0x94, 0x24, 0xa0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x25, 0x38, 0x9c, 0x24, 0xb0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x1d, 0x38, 0xa4, 0x24, 0xc0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x15, 0x38, 0xac, 0x24, 0xd0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x0d, 0x38, 0xb4, 0x24, 0xe0, 0x00, 0x00, 0x00, 0x01,
    0xc4, 0x63, 0x05, 0x38, 0xbc, 0x24, 0xf0, 0x00, 0x00, 0x00, 0x01
#else
    /* vinserti128 ymm0..ymm7, ymm0..ymm7, [esp+0x0]..[esp+0x70], 1 */
    0xc4, 0xc3, 0x7d, 0x38, 0x04, 0x24, 0x01,
    0xc4, 0xc3, 0x75, 0x38, 0x4c, 0x24, 0x10, 0x01,
    0xc4, 0xc3, 0x6d, 0x38, 0x54, 0x24, 0x20, 0x01,
    0xc4, 0xc3, 0x65, 0x38, 0x5c, 0x24, 0x30, 0x01,
    0xc4, 0xc3, 0x5d, 0x38, 0x64, 0x24, 0x40, 0x01,
    0xc4, 0xc3, 0x55, 0x38, 0x6c, 0x24, 0x50, 0x01,
    0xc4, 0xc3, 0x4d, 0x38, 0x74, 0x24, 0x60, 0x01,
    0xc4, 0xc3, 0x45, 0x38, 0x7c, 0x24, 0x70, 0x01
#endif
  };

  /* Store our caller in the return address created by the prolog */
  gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_offset_ptr_reg (cw,
      GUM_X86_XBX, (type == GUM_PROLOG_MINIMAL)
          ? GUM_MINIMAL_PROLOG_RETURN_OFFSET
          : GUM_FULL_PROLOG_RETURN_OFFSET,
      GUM_X86_XAX);

  if ((ctx->stalker->cpu_features & GUM_CPU_AVX2) != 0 &&
      !_gum_windows_is_win7_wow64 ())
  {
#if GLIB_SIZEOF_VOID_P == 8
    if ((ctx->stalker->cpu_features & GUM_CPU_AVX512) != 0)
    {
      guint i;

      for (i = 0; i != 8; i++)
        gum_x86_writer_put_kmovq_kreg_reg_offset_ptr (cw, i, GUM_X86_XSP,
            i * 8);
      gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XSP, 8 * 8);

      for (i = 16; i != 32; i++)
        gum_x86_writer_put_vmovdqu64_zmm_reg_offset_ptr (cw, i, GUM_X86_XSP,
            (i - 16) * 64);
      gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XSP, 16 * 64);

      for (i = 0; i != 16; i++)
        gum_x86_writer_put_vinserti64x4_zmm_reg_offset_ptr (cw, i, GUM_X86_XSP,
            i * 32, 1);
      gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XSP, 16 * 32);
    }
#endif
    gum_x86_writer_put_bytes (cw, upper_ymm_restorer,
        sizeof (upper_ymm_restorer));
    gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XSP, 0x100);
  }

  gum_x86_writer_put_bytes (cw, fxrstor, sizeof (fxrstor));
  gum_x86_writer_put_mov_reg_reg (cw, GUM_X86_XSP, GUM_X86_XBX);

  if (type == GUM_PROLOG_MINIMAL)
  {
#if GLIB_SIZEOF_VOID_P == 8
    gum_x86_writer_put_pop_reg (cw, GUM_X86_R11);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_R10);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_R9);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_R8);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_XDI);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_XSI);
#endif

    gum_x86_writer_put_pop_reg (cw, GUM_X86_XBX);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_XDX);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_XCX);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);
  }
  else /* GUM_PROLOG_FULL */
  {
    gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX); /* Discard
                                                     GumCpuContext.xip */
    gum_x86_writer_put_popax (cw);
    gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
        sizeof (((GumCpuContext *) NULL)->xmm)); /* Discard GumCpuContext.xmm */
  }

  gum_x86_writer_put_popfx (cw);
  gum_x86_writer_put_ret (cw);
}

static void
gum_exec_ctx_write_invalidator (GumExecCtx * ctx,
                                GumX86Writer * cw)
{
  /* Swap XDI and the top-of-stack return address */
  gum_x86_writer_put_xchg_reg_reg_ptr (cw, GUM_X86_XDI, GUM_X86_XSP);

  gum_exec_ctx_write_prolog (ctx, GUM_PROLOG_MINIMAL, cw);

  gum_x86_writer_put_call_address_with_aligned_arguments (cw,
      GUM_CALL_CAPI, GUM_ADDRESS (gum_exec_ctx_recompile_and_switch_block), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_REGISTER, GUM_X86_XDI);

  gum_exec_ctx_write_epilog (ctx, GUM_PROLOG_MINIMAL, cw);

  gum_x86_writer_put_pop_reg (cw, GUM_X86_XDI);
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
      GUM_RED_ZONE_SIZE);

  gum_x86_writer_put_jmp_near_ptr (cw, GUM_ADDRESS (&ctx->resume_at));
}

static void
gum_exec_ctx_ensure_helper_reachable (GumExecCtx * ctx,
                                      gpointer * helper_ptr,
                                      GumExecHelperWriteFunc write)
{
  GumSlab * slab = &ctx->code_slab->slab;
  GumX86Writer * cw = &ctx->code_writer;
  gpointer start;

  if (gum_exec_ctx_is_helper_reachable (ctx, helper_ptr))
    return;

  start = gum_slab_cursor (slab);
  gum_stalker_thaw (ctx->stalker, start, gum_slab_available (slab));
  gum_x86_writer_reset (cw, start);
  *helper_ptr = gum_x86_writer_cur (cw);

  write (ctx, cw);

  gum_x86_writer_flush (cw);
  gum_stalker_freeze (ctx->stalker, cw->base, gum_x86_writer_offset (cw));

  gum_slab_reserve (slab, gum_x86_writer_offset (cw));
}

static gboolean
gum_exec_ctx_is_helper_reachable (GumExecCtx * ctx,
                                  gpointer * helper_ptr)
{
  GumSlab * slab = &ctx->code_slab->slab;
  GumAddress helper, start, end;

  helper = GUM_ADDRESS (*helper_ptr);
  if (helper == 0)
    return FALSE;

  start = GUM_ADDRESS (gum_slab_start (slab));
  end = GUM_ADDRESS (gum_slab_end (slab));

  if (!gum_x86_writer_can_branch_directly_between (start, helper))
    return FALSE;

  return gum_x86_writer_can_branch_directly_between (end, helper);
}

static void
gum_exec_ctx_get_branch_target_address (GumExecCtx * ctx,
                                        const GumBranchTarget * target,
                                        GumGeneratorContext * gc,
                                        GumX86Writer * cw)
{
  if (!target->is_indirect)
  {
    if (target->base == X86_REG_INVALID)
    {
      gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
          GUM_ADDRESS (target->absolute_address));
    }
    else
    {
      gum_exec_ctx_load_real_register_into (ctx, GUM_X86_XAX,
          gum_x86_reg_from_capstone (target->base), target->origin_ip, gc, cw);
    }
  }
  else if (target->base == X86_REG_INVALID && target->index == X86_REG_INVALID)
  {
    g_assert (target->scale == 1);
    g_assert (target->absolute_address != NULL);
    g_assert (target->relative_offset == 0);

#if GLIB_SIZEOF_VOID_P == 8
    gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
        GUM_ADDRESS (target->absolute_address));
    gum_write_segment_prefix (target->pfx_seg, cw);
    gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_RAX, GUM_X86_RAX);
#else
    gum_write_segment_prefix (target->pfx_seg, cw);
    gum_x86_writer_put_u8 (cw, 0xa1);
    gum_x86_writer_put_bytes (cw, (guint8 *) &target->absolute_address,
        sizeof (target->absolute_address));
#endif
  }
  else
  {
    gum_x86_writer_put_push_reg (cw, GUM_X86_XDX);

    gum_exec_ctx_load_real_register_into (ctx, GUM_X86_XAX,
        gum_x86_reg_from_capstone (target->base), target->origin_ip, gc, cw);
    gum_exec_ctx_load_real_register_into (ctx, GUM_X86_XDX,
        gum_x86_reg_from_capstone (target->index), target->origin_ip, gc, cw);
    gum_x86_writer_put_mov_reg_base_index_scale_offset_ptr (cw, GUM_X86_XAX,
        GUM_X86_XAX, GUM_X86_XDX, target->scale,
        target->relative_offset);

    gum_x86_writer_put_pop_reg (cw, GUM_X86_XDX);
  }
}

static void
gum_exec_ctx_load_real_register_into (GumExecCtx * ctx,
                                      GumX86Reg target_register,
                                      GumX86Reg source_register,
                                      gpointer ip,
                                      GumGeneratorContext * gc,
                                      GumX86Writer * cw)
{
  switch (gc->opened_prolog)
  {
    case GUM_PROLOG_MINIMAL:
      gum_exec_ctx_load_real_register_from_minimal_frame_into (ctx,
          target_register, source_register, ip, gc, cw);
      break;
    case GUM_PROLOG_FULL:
      gum_exec_ctx_load_real_register_from_full_frame_into (ctx,
          target_register, source_register, ip, gc, cw);
      break;
    case GUM_PROLOG_IC:
      gum_exec_ctx_load_real_register_from_ic_frame_into (ctx, target_register,
          source_register, ip, gc, cw);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gum_exec_ctx_load_real_register_from_minimal_frame_into (
    GumExecCtx * ctx,
    GumX86Reg target_register,
    GumX86Reg source_register,
    gpointer ip,
    GumGeneratorContext * gc,
    GumX86Writer * cw)
{
  GumX86Reg source_meta;

  source_meta = gum_x86_meta_reg_from_real_reg (source_register);

  if (source_meta >= GUM_X86_XAX && source_meta <= GUM_X86_XBX)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, target_register,
        GUM_X86_XBX,
        GUM_STATE_PRESERVE_TOPMOST_REGISTER_INDEX * sizeof (gpointer) -
        ((source_meta - GUM_X86_XAX) * sizeof (gpointer)));
  }
#if GLIB_SIZEOF_VOID_P == 8
  else if (source_meta >= GUM_X86_XSI && source_meta <= GUM_X86_XDI)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, target_register,
        GUM_X86_XBX,
        GUM_STATE_PRESERVE_TOPMOST_REGISTER_INDEX * sizeof (gpointer) -
        ((source_meta - 2 - GUM_X86_XAX) * sizeof (gpointer)));
  }
  else if (source_meta >= GUM_X86_R8 && source_meta <= GUM_X86_R11)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, target_register,
        GUM_X86_XBX,
        GUM_STATE_PRESERVE_TOPMOST_REGISTER_INDEX * sizeof (gpointer) -
        ((source_meta - 2 - GUM_X86_RAX) * sizeof (gpointer)));
  }
#endif
  else if (source_meta == GUM_X86_XSP)
  {
    gum_x86_writer_put_mov_reg_near_ptr (cw, target_register,
        GUM_ADDRESS (&ctx->app_stack));
  }
  else if (source_meta == GUM_X86_XIP)
  {
    gum_x86_writer_put_mov_reg_address (cw, target_register, GUM_ADDRESS (ip));
  }
  else if (source_meta == GUM_X86_NONE)
  {
    gum_x86_writer_put_xor_reg_reg (cw, target_register, target_register);
  }
  else
  {
    gum_x86_writer_put_mov_reg_reg (cw, target_register, source_register);
  }
}

static void
gum_exec_ctx_load_real_register_from_full_frame_into (GumExecCtx * ctx,
                                                      GumX86Reg target_register,
                                                      GumX86Reg source_register,
                                                      gpointer ip,
                                                      GumGeneratorContext * gc,
                                                      GumX86Writer * cw)
{
  GumX86Reg source_meta;

  source_meta = gum_x86_meta_reg_from_real_reg (source_register);

  if (source_meta >= GUM_X86_XAX && source_meta <= GUM_X86_XBX)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, target_register,
        GUM_X86_XBX, GUM_FULL_PROLOG_GPR_TOP_OFFSET -
        ((source_meta - GUM_X86_XAX + 1) * sizeof (gpointer)));
  }
  else if (source_meta >= GUM_X86_XBP && source_meta <= GUM_X86_XDI)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, target_register,
        GUM_X86_XBX, GUM_FULL_PROLOG_GPR_TOP_OFFSET -
        ((source_meta - GUM_X86_XAX + 1) * sizeof (gpointer)));
  }
#if GLIB_SIZEOF_VOID_P == 8
  else if (source_meta >= GUM_X86_R8 && source_meta <= GUM_X86_R15)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, target_register,
        GUM_X86_XBX, GUM_FULL_PROLOG_GPR_TOP_OFFSET -
        ((source_meta - GUM_X86_RAX + 1) * sizeof (gpointer)));
  }
#endif
  else if (source_meta == GUM_X86_XSP)
  {
    gum_x86_writer_put_mov_reg_near_ptr (cw, target_register,
        GUM_ADDRESS (&ctx->app_stack));
  }
  else if (source_meta == GUM_X86_XIP)
  {
    gum_x86_writer_put_mov_reg_address (cw, target_register, GUM_ADDRESS (ip));
  }
  else if (source_meta == GUM_X86_NONE)
  {
    gum_x86_writer_put_xor_reg_reg (cw, target_register, target_register);
  }
  else
  {
    gum_x86_writer_put_mov_reg_reg (cw, target_register, source_register);
  }
}

static void
gum_exec_ctx_load_real_register_from_ic_frame_into (GumExecCtx * ctx,
                                                    GumX86Reg target_register,
                                                    GumX86Reg source_register,
                                                    gpointer ip,
                                                    GumGeneratorContext * gc,
                                                    GumX86Writer * cw)
{
  GumX86Reg source_meta;

  source_meta = gum_x86_meta_reg_from_real_reg (source_register);

  if (source_meta == GUM_X86_XAX)
  {
    gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, target_register, GUM_X86_XBX,
        2 * sizeof (gpointer));
  }
  else if (source_meta == GUM_X86_XBX)
  {
    gum_x86_writer_put_mov_reg_reg_ptr (cw, target_register, GUM_X86_XBX);
  }
  else if (source_meta == GUM_X86_XSP)
  {
    gum_x86_writer_put_mov_reg_near_ptr (cw, target_register,
        GUM_ADDRESS (&ctx->app_stack));
  }
  else if (source_meta == GUM_X86_XIP)
  {
    gum_x86_writer_put_mov_reg_address (cw, target_register, GUM_ADDRESS (ip));
  }
  else if (source_meta == GUM_X86_NONE)
  {
    gum_x86_writer_put_xor_reg_reg (cw, target_register, target_register);
  }
  else
  {
    gum_x86_writer_put_mov_reg_reg (cw, target_register, source_register);
  }
}

static GumExecBlock *
gum_exec_block_new (GumExecCtx * ctx)
{
  GumExecBlock * block;
  GumStalker * stalker = ctx->stalker;
  GumCodeSlab * code_slab = ctx->code_slab;
  GumSlowSlab * slow_slab = ctx->slow_slab;
  GumDataSlab * data_slab = ctx->data_slab;
  gsize code_available, slow_available, data_available;

  /*
   * Whilst we don't write the inline cache entry into the code slab any more,
   * we do write an unrolled loop which walks the table looking for the right
   * entry, so we need to ensure we have some extra space for that anyway.
   */
  code_available = gum_slab_available (&code_slab->slab);
  if (code_available < GUM_EXEC_BLOCK_MIN_CAPACITY +
      gum_stalker_get_ic_entry_size (ctx->stalker))
  {
    GumAddressSpec data_spec;

    code_slab = gum_exec_ctx_add_code_slab (ctx, gum_code_slab_new (ctx));

    gum_exec_ctx_compute_data_address_spec (ctx, data_slab->slab.size,
        &data_spec);
    if (!gum_address_spec_is_satisfied_by (&data_spec,
            gum_slab_start (&data_slab->slab)))
    {
      data_slab = gum_exec_ctx_add_data_slab (ctx, gum_data_slab_new (ctx));
    }

    gum_exec_ctx_ensure_inline_helpers_reachable (ctx);

    code_available = gum_slab_available (&code_slab->slab);
  }

  slow_available = gum_slab_available (&slow_slab->slab);
  if (slow_available < GUM_EXEC_BLOCK_MIN_CAPACITY)
  {
    GumAddressSpec data_spec;

    slow_slab = gum_exec_ctx_add_slow_slab (ctx, gum_slow_slab_new (ctx));

    gum_exec_ctx_compute_data_address_spec (ctx, data_slab->slab.size,
        &data_spec);
    if (!gum_address_spec_is_satisfied_by (&data_spec,
          gum_slab_start (&data_slab->slab)))
    {
      data_slab = gum_exec_ctx_add_data_slab (ctx, gum_data_slab_new (ctx));
    }

    slow_available = gum_slab_available (&code_slab->slab);
  }

  data_available = gum_slab_available (&data_slab->slab);
  if (data_available < GUM_DATA_BLOCK_MIN_CAPACITY +
      gum_stalker_get_ic_entry_size (ctx->stalker))
  {
    data_slab = gum_exec_ctx_add_data_slab (ctx, gum_data_slab_new (ctx));
  }

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
  GumX86Writer * cw = &ctx->code_writer;
  const gsize max_size = GUM_INVALIDATE_TRAMPOLINE_SIZE;
  gint32 distance_to_data;

  gum_stalker_thaw (stalker, block->code_start, max_size);
  gum_x86_writer_reset (cw, block->code_start);

  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
      -GUM_RED_ZONE_SIZE);
  gum_x86_writer_put_call_address (cw,
      GUM_ADDRESS (block->code_slab->invalidator));
  distance_to_data = (guint8 *) block - (guint8 *) GSIZE_TO_POINTER (cw->pc);
  gum_x86_writer_put_bytes (cw, (const guint8 *) &distance_to_data,
      sizeof (distance_to_data));

  gum_x86_writer_flush (cw);
  g_assert (gum_x86_writer_offset (cw) == GUM_INVALIDATE_TRAMPOLINE_SIZE);
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
                               gpointer ret_real_address,
                               gsize ret_code_offset)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  gpointer target;
  guint8 * code_start = from->code_start + code_offset;
  const gsize code_max_size = from->code_size - code_offset;
  GumX86Writer * cw;

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
  gum_x86_writer_reset (cw, code_start);

  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
      -(gssize) sizeof (gpointer));
  gum_x86_writer_put_push_reg (cw, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
      GUM_ADDRESS (ret_real_address));
  gum_x86_writer_put_mov_reg_offset_ptr_reg (cw, GUM_X86_XSP, sizeof (gpointer),
      GUM_X86_XAX);
  gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);

  gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (target));

  gum_x86_writer_flush (cw);
  g_assert (gum_x86_writer_offset (cw) <= code_max_size);
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
    p.call.ret_code_offset = ret_code_offset;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

static void
gum_exec_block_backpatch_jmp (GumExecBlock * block,
                              GumExecBlock * from,
                              gpointer from_insn,
                              guint id,
                              gsize code_offset,
                              GumPrologType opened_prolog)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  gboolean is_eob;

  just_unfollowed = block == NULL;
  if (just_unfollowed)
    return;

  ctx = block->ctx;
  if (!gum_exec_ctx_may_now_backpatch (ctx, block))
    return;

  is_eob = gum_exec_block_get_eob (from_insn, id);

  switch (id)
  {
    case X86_INS_JMP:
      gum_exec_block_backpatch_unconditional_jmp (block, from, from_insn,
          is_eob, code_offset, opened_prolog);
      break;

    default:
      gum_exec_block_backpatch_conditional_jmp (block, from, from_insn, id,
          code_offset, opened_prolog);
      break;
  }

  if (ctx->observer != NULL)
  {
    GumBackpatch p;

    p.type = GUM_BACKPATCH_JMP;
    p.to = block->real_start;
    p.from = from->real_start;
    p.from_insn = from_insn;
    p.jmp.id = id;
    p.jmp.code_offset = code_offset;
    p.jmp.opened_prolog = opened_prolog;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

/*
 * This function uses the instruction which is being virtualized (from_insn) and
 * the instruction being generated in its place (id) to determine whether the
 * backpatch for such a pairing will occur at the end of the instrumented block.
 * (E.g. in the case of emulating a Jcc instruction, the resulting instrumented
 * block will contain two different locations in need of backpatching to
 * re-direct control flow depending on whether or not the branch is taken).
 * If this backpatching is occurring at the end of the block and the target
 * instrumented block is immediately adjacent then a NOP slide may be used in
 * place of a branch instruction.
 */
static gboolean
gum_exec_block_get_eob (gpointer from_insn,
                        guint id)
{
  gboolean eob = FALSE;
  cs_insn * ci = NULL;

  /*
   * If we have no instruction, then this means we are handling a block
   * continuation (e.g. an input block split into two instrumented blocks
   * because of its size), for these the backpatch is at the end of the
   * block.
   */
  if (from_insn == NULL)
  {
    eob = TRUE;
    goto beach;
  }

  /*
   * The backpatch location for non-conditional JMP and CALL instructions is
   * at the end of the block.
   */
  ci = gum_x86_reader_disassemble_instruction_at (from_insn);
  if (ci->id == X86_INS_JMP || ci->id == X86_INS_CALL)
  {
    eob = TRUE;
    goto beach;
  }

  /*
   * If we encounter a Jcc instruction then we emit instrumented code as
   * follows:
   *
   *   Jcc taken
   * not_taken:
   *   ...
   *   code to handle not taken branch
   * taken:
   *   ...
   *   code to handle taken branch
   *
   * If we are backpatching the `code to handle not taken branch` then this is
   * replaced with a JMP instruction (hence the id field won't match). In this
   * case as we can see above, our backpatch target is not at the end of the
   * block and therefore cannot be replaced with NOPs.
   */
  if (ci->id == id)
  {
    eob = TRUE;
    goto beach;
  }

beach:
  if (ci != NULL)
    cs_free (ci, 1);

  return eob;
}

static void
gum_exec_block_backpatch_conditional_jmp (GumExecBlock * block,
                                          GumExecBlock * from,
                                          gpointer from_insn,
                                          guint id,
                                          gsize code_offset,
                                          GumPrologType opened_prolog)
{
  GumExecCtx * ctx = block->ctx;
  guint8 * code_start = from->code_start + code_offset;
  const gsize code_max_size = from->code_size - code_offset;
  GumX86Writer * cw = &ctx->code_writer;
  gpointer target_taken = block->code_start;
  GumExecBlock * next_block;

  /*
   * If we encounter a Jcc instruction then we emit instrumented code as
   * follows:
   *
   *   Jcc taken
   * not_taken:
   *   ...
   *   code to handle not taken branch
   * taken:
   *   ...
   *   code to handle taken branch
   *
   * When we backpatch this code, we want to reduce the number of branches taken
   * to an absolute minimum. When we backpatch the not_taken branch we simply
   * replace the `code to handle not taken branch` with a JMP instruction to the
   * required block. We cannot use a NOP slide even if the target block is
   * adjacent since our backpatch is not at the end of our block and we would
   * end up overwriting the `code to handle taken branch`.
   *
   * If we execute the taken branch of the JMPcc, instead of backpatching
   * `code to handle taken branch`, we instead apply our backpatch to overwrite
   * the original Jcc instruction to take control flow direct to the
   * instrumented block and hence avoid taking two branches in quick succession.
   * This also means that since the `code to handle taken branch` is no longer
   * needed, if the instrumented block for the not taken branch is immediately
   * adjacent, we can simply fill remainder of the block with NOPs to avoid the
   * additional JMP for that not taken branch of execution too.
   */

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (ctx->stalker, code_start, code_max_size);

  gum_x86_writer_reset (cw, code_start);

  gum_exec_ctx_query_block_switch_callback (ctx, from, block->real_start,
      from_insn, &target_taken);

  g_assert (opened_prolog == GUM_PROLOG_NONE);

  gum_x86_writer_put_jcc_near (cw, id, target_taken, GUM_NO_HINT);

  next_block = gum_exec_block_get_adjacent (from);
  if (next_block != NULL)
  {
    gpointer target_not_taken = next_block->code_start;

    gum_exec_ctx_query_block_switch_callback (ctx, from, next_block->real_start,
        from_insn, &target_not_taken);

    if (gum_exec_block_is_adjacent (target_not_taken, from))
    {
      gsize remaining = code_max_size - gum_x86_writer_offset (cw);
      gum_x86_writer_put_nop_padding (cw, remaining);
    }
  }

  gum_x86_writer_flush (cw);
  g_assert (gum_x86_writer_offset (cw) <= code_max_size);
  gum_stalker_freeze (ctx->stalker, code_start, code_max_size);

  gum_spinlock_release (&ctx->code_lock);
}

static GumExecBlock *
gum_exec_block_get_adjacent (GumExecBlock * from)
{
  gpointer real_address = from->real_start + from->real_size;

  return gum_metal_hash_table_lookup (from->ctx->mappings, real_address);
}

static void
gum_exec_block_backpatch_unconditional_jmp (GumExecBlock * block,
                                            GumExecBlock * from,
                                            gpointer from_insn,
                                            gboolean is_eob,
                                            gsize code_offset,
                                            GumPrologType opened_prolog)
{
  GumExecCtx * ctx = block->ctx;
  guint8 * code_start = from->code_start + code_offset;
  const gsize code_max_size = from->code_size - code_offset;
  GumX86Writer * cw = &ctx->code_writer;
  gpointer target = block->code_start;

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (ctx->stalker, code_start, code_max_size);

  gum_x86_writer_reset (cw, code_start);

  gum_exec_ctx_query_block_switch_callback (ctx, from, block->real_start,
      from_insn, &target);

  if (opened_prolog != GUM_PROLOG_NONE)
  {
    gum_exec_ctx_write_epilog (block->ctx, opened_prolog, cw);
  }

  if (is_eob && gum_exec_block_is_adjacent (target, from))
  {
    gsize remaining = code_max_size - gum_x86_writer_offset (cw);
    gum_x86_writer_put_nop_padding (cw, remaining);
  }
  else
  {
    gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (target));
  }

  gum_x86_writer_flush (cw);
  g_assert (gum_x86_writer_offset (cw) <= code_max_size);
  gum_stalker_freeze (ctx->stalker, code_start, code_max_size);

  gum_spinlock_release (&ctx->code_lock);
}

static gboolean
gum_exec_block_is_adjacent (gpointer target,
                            GumExecBlock * from)
{
  if (from->code_start + from->code_size != target)
    return FALSE;

  return TRUE;
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
  gum_exec_ctx_query_block_switch_callback (ctx, from, block->real_start,
      from_insn, &target);

  ic_entries = from->ic_entries;
  if (ic_entries == NULL)
    return;

  num_ic_entries = ctx->stalker->ic_entries;

  for (i = 0; i != num_ic_entries; i++)
  {
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
  GumX86Writer * cw = gc->code_writer;
  gboolean is_conditional;
  cs_x86 * x86 = &insn->ci->detail->x86;
  cs_x86_op * op = &x86->operands[0];
  GumBranchTarget target = { 0, };

  is_conditional =
      (insn->ci->id != X86_INS_CALL && insn->ci->id != X86_INS_JMP);

  target.origin_ip = insn->end;

  if (op->type == X86_OP_IMM)
  {
    target.absolute_address = GSIZE_TO_POINTER (op->imm);
    target.is_indirect = FALSE;
    target.pfx_seg = X86_REG_INVALID;
    target.base = X86_REG_INVALID;
    target.index = X86_REG_INVALID;
    target.scale = 0;
  }
  else if (op->type == X86_OP_MEM)
  {
#if GLIB_SIZEOF_VOID_P == 4 && defined (HAVE_WINDOWS)
    if (op->mem.segment == X86_REG_INVALID &&
        op->mem.base == X86_REG_INVALID &&
        op->mem.index == X86_REG_INVALID)
    {
      GArray * impls = ctx->stalker->wow_transition_impls;
      guint i;

      for (i = 0; i != impls->len; i++)
      {
        gpointer impl = g_array_index (impls, gpointer, i);

        if (GSIZE_TO_POINTER (op->mem.disp) == impl)
          return gum_exec_block_virtualize_wow64_transition (block, gc, impl);
      }
    }
#endif

#ifdef HAVE_WINDOWS
    /* Can't follow WoW64 */
    if (op->mem.segment == X86_REG_FS && op->mem.disp == 0xc0)
      return GUM_REQUIRE_SINGLE_STEP;
#endif

    if (op->mem.base == X86_REG_INVALID && op->mem.index == X86_REG_INVALID)
      target.absolute_address = GSIZE_TO_POINTER (op->mem.disp);
    else
      target.relative_offset = op->mem.disp;

    target.is_indirect = TRUE;
    target.pfx_seg = op->mem.segment;
    target.base = op->mem.base;
    target.index = op->mem.index;
    target.scale = op->mem.scale;
  }
  else if (op->type == X86_OP_REG)
  {
    target.is_indirect = FALSE;
    target.pfx_seg = X86_REG_INVALID;
    target.base = op->reg;
    target.index = X86_REG_INVALID;
    target.scale = 0;
  }
  else
  {
    g_assert_not_reached ();
  }

  if (insn->ci->id == X86_INS_CALL)
  {
    gboolean target_is_excluded = FALSE;

    if ((ctx->sink_mask & GUM_CALL) != 0)
    {
      gum_exec_block_write_call_event_code (block, &target, gc,
          GUM_CODE_INTERRUPTIBLE);
    }

    gum_exec_block_write_adjust_depth (block, gc->code_writer, 1);

    if (!target.is_indirect && target.base == X86_REG_INVALID &&
        ctx->activation_target == NULL)
    {
      target_is_excluded =
          gum_stalker_is_excluding (ctx->stalker, target.absolute_address);
    }

    if (target_is_excluded)
    {
      GumBranchTarget next_instruction = { 0, };
#ifdef HAVE_LINUX
      gpointer start_of_call;
      guint call_length;
      gpointer end_of_call;
#endif

      gum_exec_block_open_prolog (block, GUM_PROLOG_IC, gc, gc->code_writer);
      gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
          GUM_ADDRESS (insn->end));
      gum_x86_writer_put_mov_near_ptr_reg (cw,
          GUM_ADDRESS (&ctx->pending_return_location), GUM_X86_XAX);
      gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
          GUM_ADDRESS (&ctx->pending_calls));
      gum_x86_writer_put_inc_reg_ptr (cw, GUM_X86_PTR_DWORD, GUM_X86_XAX);
      gum_exec_block_close_prolog (block, gc, gc->code_writer);

#ifdef HAVE_LINUX
      start_of_call = GSIZE_TO_POINTER (cw->pc);
#endif

      gum_x86_relocator_write_one_no_label (gc->relocator);

#ifdef HAVE_LINUX
      call_length = gum_x86_reader_insn_length (start_of_call);

      /*
       * We can't just write the instruction and then use cw->pc to get the
       * end of the call instruction since the relocator may need to embed the
       * target address in the code stream. In which case it is written
       * immediately after the instruction.
       */
      end_of_call =
          GSIZE_TO_POINTER (GPOINTER_TO_SIZE (start_of_call) + call_length);

      /*
       * We insert into our hashtable the real address of the next instruction
       * using the code address of the next instrumented instruction as a key.
       */
      gum_metal_hash_table_insert (ctx->excluded_calls, end_of_call, insn->end);
#endif

      gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc,
          gc->code_writer);

      gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
          GUM_ADDRESS (&ctx->pending_calls));
      gum_x86_writer_put_dec_reg_ptr (cw, GUM_X86_PTR_DWORD, GUM_X86_XAX);

      next_instruction.is_indirect = FALSE;
      next_instruction.absolute_address = insn->end;
      gum_exec_block_write_jmp_transfer_code (block, &next_instruction,
          GUM_ENTRYGATE (excluded_call_imm), gc, X86_INS_JMP, GUM_ADDRESS (0));

      return GUM_REQUIRE_NOTHING;
    }

    gum_x86_relocator_skip_one_no_label (gc->relocator);
    gum_exec_block_write_call_invoke_code (block, &target, gc);
  }
  else if (insn->ci->id == X86_INS_JECXZ || insn->ci->id == X86_INS_JRCXZ)
  {
    gpointer is_true;
    GumBranchTarget false_target = { 0, };

    gum_x86_relocator_skip_one_no_label (gc->relocator);

    is_true =
        GUINT_TO_POINTER ((GPOINTER_TO_UINT (insn->start) << 16) | 0xbeef);

    gum_exec_block_close_prolog (block, gc, gc->code_writer);

    gum_x86_writer_put_jcc_short_label (cw, X86_INS_JMP, is_true, GUM_NO_HINT);

    false_target.is_indirect = FALSE;
    false_target.absolute_address = insn->end;
    gum_exec_block_write_jmp_transfer_code (block, &false_target,
        GUM_ENTRYGATE (jmp_cond_jcxz), gc, X86_INS_JMP, GUM_ADDRESS (0));

    gum_x86_writer_put_label (cw, is_true);

    /*
     * x86/64 only supports short jumps for JECXZ/JRCXZ so we can't backpatch
     * the Jcc instruction itself.
     */
    gum_exec_block_write_jmp_transfer_code (block, &target,
        GUM_ENTRYGATE (jmp_cond_jcxz), gc, X86_INS_JMP, GUM_ADDRESS (0));
  }
  else if (gum_exec_block_is_direct_jmp_to_plt_got (block, gc, &target))
  {
    /*
     * Functions in Linux typically call thunks in the `.plt.got` or `.plt.sec`
     * to invoke functions located in other shared libraries. However, in the
     * event of a tail-call, rather than using a CALL instruction, a JMP
     * instruction will be used instead.
     *
     * We normally only handle CALLs to excluded ranges and therefore such
     * tail-calls will result in execution being followed into the excluded
     * range until a subsequent CALL instruction is encountered. Generally,
     * however, we cannot differentiate these tail-calls from a JMP to an
     * excluded range and therefore we must accept this additional overhead or
     * risk losing control of the target execution.
     *
     * However, if the tail-call is to the `.plt.got` or `.plt.sec`, then we
     * know that this is in fact a function call and can be treated as such. We
     * pop the return value from the stack and stash it in the data slab, then
     * emit a call into the target function from the instrumnented code so that
     * control returns there after the excluded function and follow this with
     * the standard jump handling code with the stashed value in the data slab
     * as an indirect target.
     */
    gum_exec_block_handle_direct_jmp_to_plt_got (block, gc, &target);
    return GUM_REQUIRE_NOTHING;
  }
  else
  {
    gpointer is_true;
    GumAddress jcc_address = 0;
    GumExecCtxReplaceCurrentBlockFunc regular_entry_func, cond_entry_func;

    gum_x86_relocator_skip_one_no_label (gc->relocator);

    is_true =
        GUINT_TO_POINTER ((GPOINTER_TO_UINT (insn->start) << 16) | 0xbeef);

    if (is_conditional)
    {
      g_assert (!target.is_indirect);

      gum_exec_block_close_prolog (block, gc, gc->code_writer);

      jcc_address = cw->pc;
      gum_x86_writer_put_jcc_near_label (cw, insn->ci->id, is_true,
          GUM_NO_HINT);
    }

    if (target.is_indirect)
    {
      regular_entry_func = GUM_ENTRYGATE (jmp_mem);
      cond_entry_func = GUM_ENTRYGATE (jmp_cond_mem);
    }
    else if (target.base != X86_REG_INVALID)
    {
      regular_entry_func = GUM_ENTRYGATE (jmp_reg);
      cond_entry_func = GUM_ENTRYGATE (jmp_cond_reg);
    }
    else
    {
      regular_entry_func = GUM_ENTRYGATE (jmp_imm);
      cond_entry_func = GUM_ENTRYGATE (jmp_cond_imm);
    }

    if (is_conditional)
    {
      GumBranchTarget cond_target = { 0, };

      cond_target.is_indirect = FALSE;
      cond_target.absolute_address = insn->end;

      gum_exec_block_write_jmp_transfer_code (block, &cond_target,
          cond_entry_func, gc, X86_INS_JMP, GUM_ADDRESS (0));

      gum_x86_writer_put_label (cw, is_true);

      gum_exec_block_write_jmp_transfer_code (block, &target, cond_entry_func,
          gc, insn->ci->id, jcc_address);
    }
    else
    {
      gum_exec_block_write_jmp_transfer_code (block, &target,
          regular_entry_func, gc, insn->ci->id, GUM_ADDRESS (0));
    }
  }

  return GUM_REQUIRE_NOTHING;
}

static gboolean
gum_exec_block_is_direct_jmp_to_plt_got (GumExecBlock * block,
                                         GumGeneratorContext * gc,
                                         GumBranchTarget * target)
{
#ifdef HAVE_LINUX
  GumExecCtx * ctx = block->ctx;
  const cs_insn * insn = gc->instruction->ci;
  GArray * ranges;
  guint i;

  if (target->is_indirect)
    return FALSE;

  if (target->base != X86_REG_INVALID)
    return FALSE;

  if (ctx->activation_target != NULL)
    return FALSE;

  if (!gum_stalker_is_excluding (ctx->stalker, target->absolute_address))
    return FALSE;

  if (insn->id != X86_INS_JMP)
    return FALSE;

  ranges = gum_exec_ctx_get_plt_got_ranges ();

  for (i = 0; i != ranges->len; i++)
  {
    GumMemoryRange * range = &g_array_index (ranges, GumMemoryRange, i);

    if (GUM_MEMORY_RANGE_INCLUDES (range,
        GPOINTER_TO_SIZE (target->absolute_address)))
    {
      return TRUE;
    }
  }
#endif

  return FALSE;
}

#ifdef HAVE_LINUX

static GArray *
gum_exec_ctx_get_plt_got_ranges (void)
{
  static gsize gonce_value;

  if (g_once_init_enter (&gonce_value))
  {
    GArray * ranges = g_array_new (FALSE, FALSE, sizeof (GumMemoryRange));

    gum_process_enumerate_modules (gum_collect_plt_got_in_module, ranges);

    _gum_register_early_destructor (gum_exec_ctx_deinit_plt_got_ranges);

    g_once_init_leave (&gonce_value, GPOINTER_TO_SIZE (ranges));
  }

  return GSIZE_TO_POINTER (gonce_value);
}

static void
gum_exec_ctx_deinit_plt_got_ranges (void)
{
  g_array_free (gum_exec_ctx_get_plt_got_ranges (), TRUE);
}

static gboolean
gum_collect_plt_got_in_module (GumModule * module,
                               gpointer user_data)
{
  GArray * ranges = user_data;

  gum_module_enumerate_sections (module, gum_collect_if_plt_got_section,
      ranges);

  return TRUE;
}

static gboolean
gum_collect_if_plt_got_section (const GumSectionDetails * details,
                                gpointer user_data)
{
  GArray * ranges = user_data;
  GumMemoryRange range;

  if (strcmp (details->name, ".plt.got") != 0 &&
      strcmp (details->name, ".plt.sec") != 0)
  {
    return TRUE;
  }

  range.base_address = details->address;
  range.size = details->size;
  g_array_append_val (ranges, range);

  return TRUE;
}

#endif

static void
gum_exec_block_handle_direct_jmp_to_plt_got (GumExecBlock * block,
                                             GumGeneratorContext * gc,
                                             GumBranchTarget * target)
{
  GumX86Writer * cw = gc->code_writer;
  GumSlab * data_slab = &block->ctx->data_slab->slab;
  gpointer * return_address;
  GumBranchTarget continue_target = { 0, };

  return_address = gum_slab_reserve (data_slab, sizeof (gpointer));

  gum_x86_writer_put_mov_reg_offset_ptr_reg (cw, GUM_X86_XSP,
      -(GUM_RED_ZONE_SIZE + (gssize) sizeof (gpointer)), GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XSP);
  gum_x86_writer_put_mov_near_ptr_reg (cw, GUM_ADDRESS (return_address),
      GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, GUM_X86_XAX, GUM_X86_XSP,
      -(GUM_RED_ZONE_SIZE + (gssize) sizeof (gpointer)));

  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
      sizeof (gpointer));

  gum_x86_writer_put_call_address (cw, GUM_ADDRESS (target->absolute_address));

  continue_target.is_indirect = TRUE;
  continue_target.absolute_address = return_address;
  gum_exec_block_write_jmp_transfer_code (block, &continue_target,
      GUM_ENTRYGATE (excluded_call_imm), gc, X86_INS_JMP, GUM_ADDRESS (0));
}

static GumVirtualizationRequirements
gum_exec_block_virtualize_ret_insn (GumExecBlock * block,
                                    GumGeneratorContext * gc)
{
  if ((block->ctx->sink_mask & GUM_RET) != 0)
    gum_exec_block_write_ret_event_code (block, gc, GUM_CODE_INTERRUPTIBLE);

  gum_exec_block_write_adjust_depth (block, gc->code_writer, -1);

  gum_x86_relocator_skip_one_no_label (gc->relocator);

  gum_exec_block_write_ret_transfer_code (block, gc);

  return GUM_REQUIRE_NOTHING;
}

static void
gum_exec_block_write_adjust_depth (GumExecBlock * block,
                                   GumX86Writer * cw,
                                   gssize adj)
{
  GumAddress depth_addr = GUM_ADDRESS (&block->ctx->depth);

  if ((block->ctx->sink_mask & (GUM_CALL | GUM_RET)) == 0)
    return;

  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
      -GUM_RED_ZONE_SIZE);
  gum_x86_writer_put_push_reg (cw, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_near_ptr (cw, GUM_X86_EAX, depth_addr);
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_EAX, GUM_X86_EAX, adj);
  gum_x86_writer_put_mov_near_ptr_reg (cw, depth_addr, GUM_X86_EAX);
  gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
      GUM_RED_ZONE_SIZE);
}

static GumVirtualizationRequirements
gum_exec_block_virtualize_sysenter_insn (GumExecBlock * block,
                                         GumGeneratorContext * gc)
{
#ifdef GUM_HAVE_SYSENTER_VIRTUALIZATION
  GumX86Writer * cw = gc->code_writer;
#if defined (HAVE_WINDOWS)
  guint8 code[] = {
    /* 00 */ 0x50,                                /* push eax              */
    /* 01 */ 0x8b, 0x02,                          /* mov eax, [edx]        */
    /* 03 */ 0xa3, 0xaa, 0xaa, 0xaa, 0xaa,        /* mov [0xaaaaaaaa], eax */
    /* 08 */ 0xc7, 0x02, 0xbb, 0xbb, 0xbb, 0xbb,  /* mov [edx], 0xbbbbbbbb */
    /* 0e */ 0x58,                                /* pop eax               */
    /* 0f */ 0x0f, 0x34,                          /* sysenter              */
    /* 11 */ 0xcc, 0xcc, 0xcc, 0xcc               /* <saved ret-addr here> */
  };
  const gsize store_ret_addr_offset = 0x03 + 1;
  const gsize load_continuation_addr_offset = 0x08 + 2;
  const gsize saved_ret_addr_offset = 0x11;
#elif defined (HAVE_DARWIN)
  guint8 code[] = {
    /* 00 */ 0x89, 0x15, 0xaa, 0xaa, 0xaa, 0xaa, /* mov [0xaaaaaaaa], edx */
    /* 06 */ 0xba, 0xbb, 0xbb, 0xbb, 0xbb,       /* mov edx, 0xbbbbbbbb   */
    /* 0b */ 0x0f, 0x34,                         /* sysenter              */
    /* 0d */ 0xcc, 0xcc, 0xcc, 0xcc              /* <saved ret-addr here> */
  };
  const gsize store_ret_addr_offset = 0x00 + 2;
  const gsize load_continuation_addr_offset = 0x06 + 1;
  const gsize saved_ret_addr_offset = 0x0d;
#elif defined (HAVE_LINUX) || defined (HAVE_FREEBSD)
  /* XXX: Not yet tested on FreeBSD. */
  guint8 code[] = {
    /* 00 */ 0x8b, 0x54, 0x24, 0x0c,             /* mov edx, [esp + 12]   */
    /* 04 */ 0x89, 0x15, 0xaa, 0xaa, 0xaa, 0xaa, /* mov [0xaaaaaaaa], edx */
    /* 0a */ 0xba, 0xbb, 0xbb, 0xbb, 0xbb,       /* mov edx, 0xbbbbbbbb   */
    /* 0f */ 0x89, 0x54, 0x24, 0x0c,             /* mov [esp + 12], edx   */
    /* 13 */ 0x8b, 0x54, 0x24, 0x04,             /* mov edx, [esp + 4]    */
    /* 17 */ 0x0f, 0x34,                         /* sysenter              */
    /* 19 */ 0xcc, 0xcc, 0xcc, 0xcc              /* <saved ret-addr here> */
  };
  const gsize store_ret_addr_offset = 0x04 + 2;
  const gsize load_continuation_addr_offset = 0x0a + 1;
  const gsize saved_ret_addr_offset = 0x19;
#endif
  gpointer * saved_ret_addr;
  gpointer continuation;

  gum_exec_block_close_prolog (block, gc, gc->code_writer);

  saved_ret_addr = GSIZE_TO_POINTER (cw->pc + saved_ret_addr_offset);
  continuation = GSIZE_TO_POINTER (cw->pc + saved_ret_addr_offset + 4);
  *((gpointer *) (code + store_ret_addr_offset)) = saved_ret_addr;
  *((gpointer *) (code + load_continuation_addr_offset)) = continuation;

  gum_x86_writer_put_bytes (cw, code, sizeof (code));

  gum_exec_block_write_sysenter_continuation_code (block, gc, saved_ret_addr);

  return GUM_REQUIRE_NOTHING;
#else
  return GUM_REQUIRE_RELOCATION;
#endif
}

static GumVirtualizationRequirements
gum_exec_block_virtualize_syscall_insn (GumExecBlock * block,
                                        GumGeneratorContext * gc)
{
#if GLIB_SIZEOF_VOID_P == 8 && defined (HAVE_LINUX)
  return gum_exec_block_virtualize_linux_syscall (block, gc);
#else
  return GUM_REQUIRE_RELOCATION;
#endif
}

static GumVirtualizationRequirements
gum_exec_block_virtualize_int_insn (GumExecBlock * block,
                                    GumGeneratorContext * gc)
{
#if GLIB_SIZEOF_VOID_P == 4 && defined (HAVE_LINUX)
  const cs_insn * insn = gc->instruction->ci;
  cs_x86 * x86 = &insn->detail->x86;
  cs_x86_op * op = &x86->operands[0];

  g_assert (x86->op_count == 1);
  g_assert (op->type == X86_OP_IMM);

  if (op->imm != 0x80)
    return GUM_REQUIRE_RELOCATION;

  return gum_exec_block_virtualize_linux_syscall (block, gc);
#else
  return GUM_REQUIRE_RELOCATION;
#endif
}

#ifdef HAVE_LINUX

/*
 * SYSCALL on x64 and INT 0x80 on x86 are synonymous and both result in a
 * mode switch, we can handle them both similarly.
 */
static GumVirtualizationRequirements
gum_exec_block_virtualize_linux_syscall (GumExecBlock * block,
                                         GumGeneratorContext * gc)
{
  GumX86Writer * cw = gc->code_writer;
  const cs_insn * insn = gc->instruction->ci;
  gconstpointer perform_clone_syscall = cw->code + 1;
  gconstpointer perform_regular_syscall = cw->code + 2;
  gconstpointer perform_next_instruction = cw->code + 3;

  gum_x86_relocator_skip_one (gc->relocator);

  if (gc->opened_prolog != GUM_PROLOG_NONE)
    gum_exec_block_close_prolog (block, gc, cw);

  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
      GUM_X86_XSP, -GUM_RED_ZONE_SIZE);
  gum_x86_writer_put_pushfx (cw);

  gum_x86_writer_put_cmp_reg_i32 (cw, GUM_X86_XAX, __NR_clone);
  gum_x86_writer_put_jcc_near_label (cw, X86_INS_JE, perform_clone_syscall,
      GUM_NO_HINT);
  gum_x86_writer_put_cmp_reg_i32 (cw, GUM_X86_XAX, __NR_clone3);
  gum_x86_writer_put_jcc_near_label (cw, X86_INS_JNE, perform_regular_syscall,
      GUM_NO_HINT);

  gum_x86_writer_put_label (cw, perform_clone_syscall);

  /*
   * Store the return address. Note that we cannot use the stack to store this
   * since the spawned child will be given its own copy of the stack. We cannot
   * reasonably expect any value stored to be copied into the child stack unless
   * we store it in a place which the target is expected to be using. We can't
   * do this without interfering with the program state.
   */
  gum_x86_writer_put_push_reg (cw, GUM_X86_XAX);
  gum_x86_writer_put_push_reg (cw, GUM_X86_XBX);
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
      GUM_ADDRESS (&block->ctx->syscall_end));
  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBX,
      GUM_ADDRESS (gc->instruction->end));
  gum_x86_writer_put_mov_reg_ptr_reg (cw, GUM_X86_XAX, GUM_X86_XBX);
  gum_x86_writer_put_pop_reg (cw, GUM_X86_XBX);
  gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);

  /*
   * If our syscall is a clone, then we must place the syscall instruction
   * itself on a page of its own to prevent the parent thread from changing the
   * protection of the page as it writes more code to the slab.
   */
  g_assert (insn->size == 2);

  if (memcmp (insn->bytes, gum_int80_code,
        sizeof (gum_int80_code)) == 0)
  {
    gum_x86_writer_put_call_address (cw,
        GUM_ADDRESS (block->ctx->last_int80));
  }
  else if (memcmp (insn->bytes, gum_syscall_code,
        sizeof (gum_syscall_code)) == 0)
  {
    gum_x86_writer_put_call_address (cw,
        GUM_ADDRESS (block->ctx->last_syscall));
  }
  else
  {
    g_assert_not_reached ();
  }

  gum_x86_writer_put_jmp_short_label (cw, perform_next_instruction);

  gum_x86_writer_put_label (cw, perform_regular_syscall);

  gum_x86_writer_put_popfx (cw);
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
      GUM_X86_XSP, GUM_RED_ZONE_SIZE);

  gum_x86_writer_put_bytes (cw, insn->bytes, insn->size);

  gum_x86_writer_put_label (cw, perform_next_instruction);

  return GUM_REQUIRE_NOTHING;
}

static void
gum_exec_ctx_write_int80_helper (GumExecCtx * ctx,
                                 GumX86Writer * cw)
{
  gum_exec_ctx_write_aligned_syscall (ctx, cw, gum_int80_code,
      sizeof (gum_int80_code));
}

static void
gum_exec_ctx_write_syscall_helper (GumExecCtx * ctx,
                                   GumX86Writer * cw)
{
  gum_exec_ctx_write_aligned_syscall (ctx, cw, gum_syscall_code,
      sizeof (gum_syscall_code));
}

static void
gum_exec_ctx_write_aligned_syscall (GumExecCtx * ctx,
                                    GumX86Writer * cw,
                                    const guint8 * syscall_insn,
                                    gsize syscall_size)
{
  gsize page_size, page_mask;
  guint page_offset_start, pad_start, i;
  guint page_offset_end, pad_end;
  gconstpointer start = cw->code + 1;
  gconstpointer parent = cw->code + 2;
  gconstpointer end = cw->code + 3;

  /*
   * If we have reached this point, then we know that the syscall being
   * performed was a clone. This means that both the calling thread and the
   * newly spawned thread will begin execution from the point immediately after
   * the syscall instruction. However, this causes a potential race condition,
   * if the calling thread attempts to either compile a new block, or backpatch
   * an existing one in the same page. During patching the block may be thawed
   * leading to the target thread (which may be stalled at the mercy of the
   * scheduler) attempting to execute a non-executable page.
   */

  page_size = gum_query_page_size ();
  page_mask = page_size - 1;

  /* Insert padding until we reach a page boundary */
  gum_x86_writer_put_jmp_near_label (cw, start);

  page_offset_start = GPOINTER_TO_SIZE (cw->code) & page_mask;
  pad_start = page_size - page_offset_start;

  for (i = 0; i != pad_start; i++)
    gum_x86_writer_put_breakpoint (cw);

  gum_x86_writer_put_label (cw, start);

  /* Pop the return address */
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
      GUM_X86_XSP, GLIB_SIZEOF_VOID_P);

  /* Restore state */
  gum_x86_writer_put_popfx (cw);
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
      GUM_X86_XSP, GUM_RED_ZONE_SIZE);

  /* Put the original syscall instruction */
  gum_x86_writer_put_bytes (cw, syscall_insn, syscall_size);

  /* Save state */
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
      GUM_X86_XSP, -GUM_RED_ZONE_SIZE);
  gum_x86_writer_put_pushfx (cw);

  /* Compare the return value from the syscall to see if we are the parent */
  gum_x86_writer_put_test_reg_reg (cw, GUM_X86_XAX, GUM_X86_XAX);
  gum_x86_writer_put_jcc_near_label (cw, X86_INS_JNE, parent, GUM_NO_HINT);

  /* Restore state */
  gum_x86_writer_put_popfx (cw);
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
      GUM_X86_XSP, GUM_RED_ZONE_SIZE);

  /*
   * Direct the child back to the real address, otherwise it will continue
   * running inside Stalker using the same GumExecCtx as the parent. This will
   * result in re-entrancy issues (since by design each thread must have its
   * own GumExecCtx) and in turn horrible corruption.
   */
  gum_x86_writer_put_jmp_near_ptr (cw, GUM_ADDRESS (&ctx->syscall_end));

  gum_x86_writer_put_label (cw, parent);

  /* Restore state */
  gum_x86_writer_put_popfx (cw);
  gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP,
      GUM_X86_XSP, GUM_RED_ZONE_SIZE);

  gum_x86_writer_put_jmp_near_label (cw, end);

  page_offset_end = GPOINTER_TO_SIZE (cw->code) & page_mask;
  pad_end = page_size - page_offset_end;

  for (i = 0; i != pad_end; i++)
    gum_x86_writer_put_breakpoint (cw);

  gum_x86_writer_put_label (cw, end);
  gum_x86_writer_put_jmp_reg_offset_ptr (cw, GUM_X86_XSP,
      -(GUM_RED_ZONE_SIZE + (2 * GLIB_SIZEOF_VOID_P)));
}

#endif

#if GLIB_SIZEOF_VOID_P == 4 && defined (HAVE_WINDOWS)

static GumVirtualizationRequirements
gum_exec_block_virtualize_wow64_transition (GumExecBlock * block,
                                            GumGeneratorContext * gc,
                                            gpointer impl)
{
  GumX86Writer * cw = gc->code_writer;
  guint8 code[] = {
    /* 00 */ 0x50,                        /* push eax */
    /* 01 */ 0x8b, 0x44, 0x24, 0x04,      /* mov eax, dword [esp + 4] */
    /* 05 */ 0x89, 0x05, 0xaa, 0xaa, 0xaa,
             0xaa,                        /* mov dword [0xaaaaaaaa], eax */
    /* 0b */ 0xc7, 0x44, 0x24, 0x04, 0xbb,
             0xbb, 0xbb, 0xbb,            /* mov dword [esp + 4], 0xbbbbbbbb */
    /* 13 */ 0x58,                        /* pop eax */
    /* 14 */ 0xff, 0x25, 0xcc, 0xcc, 0xcc,
             0xcc,                        /* jmp dword [0xcccccccc] */
    /* 1a */ 0x90, 0x90, 0x90, 0x90       /* <saved ret-addr here> */
  };
  const gsize store_ret_addr_offset = 0x05 + 2;
  const gsize load_continuation_addr_offset = 0x0b + 4;
  const gsize wow64_transition_addr_offset = 0x14 + 2;
  const gsize saved_ret_addr_offset = 0x1a;

  gum_exec_block_close_prolog (block, gc, gc->code_writer);

  gpointer * saved_ret_addr = GSIZE_TO_POINTER (cw->pc + saved_ret_addr_offset);
  gpointer continuation = GSIZE_TO_POINTER (cw->pc + saved_ret_addr_offset + 4);

  *((gpointer *) (code + store_ret_addr_offset)) = saved_ret_addr;
  *((gpointer *) (code + load_continuation_addr_offset)) = continuation;
  *((gpointer *) (code + wow64_transition_addr_offset)) = impl;

  gum_x86_writer_put_bytes (cw, code, sizeof (code));

  gum_exec_block_write_sysenter_continuation_code (block, gc, saved_ret_addr);

  return GUM_REQUIRE_NOTHING;
}

#endif

/*
 * We handle CALL instructions much like a JMP instruction, but we must push the
 * real return address onto the stack immediately before we branch so that the
 * application code sees the correct value on its stack (should it make use of
 * it). We don't need to emit a landing pad, since RET instructions are handled
 * in the same way as an indirect branch.
 */
static void
gum_exec_block_write_call_invoke_code (GumExecBlock * block,
                                       const GumBranchTarget * target,
                                       GumGeneratorContext * gc)
{
  GumStalker * stalker = block->ctx->stalker;
  const gint trust_threshold = stalker->trust_threshold;
  GumX86Writer * cw = gc->code_writer;
  const GumAddress call_code_start = cw->pc;
  GumX86Writer * cws = gc->slow_writer;
  const GumPrologType opened_prolog = gc->opened_prolog;
  gboolean can_backpatch_statically;
  gpointer * ic_match = NULL;
  GumExecCtxReplaceCurrentBlockFunc entry_func;
  GumAddress ret_code_address = 0;
  GumAddress ret_real_address = GUM_ADDRESS (gc->instruction->end);

  can_backpatch_statically =
      trust_threshold >= 0 &&
      !target->is_indirect &&
      target->base == X86_REG_INVALID;

  if (trust_threshold >= 0 && !can_backpatch_statically)
  {
    gum_exec_block_close_prolog (block, gc, gc->code_writer);

    gum_exec_block_open_prolog (block, GUM_PROLOG_IC, gc, gc->code_writer);
    gum_exec_ctx_get_branch_target_address (block->ctx, target, gc,
        gc->code_writer);

    ic_match = gum_exec_block_write_inline_cache_code (block, gc, cw, cws);
    gum_exec_ctx_write_epilog (block->ctx, GUM_PROLOG_IC, cw);

    /* Push the real return address */
    gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
        -(gssize) sizeof (gpointer));
    gum_x86_writer_put_push_reg (cw, GUM_X86_XAX);
    gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
        GUM_ADDRESS (ret_real_address));
    gum_x86_writer_put_mov_reg_offset_ptr_reg (cw, GUM_X86_XSP,
        sizeof (gpointer), GUM_X86_XAX);
    gum_x86_writer_put_pop_reg (cw, GUM_X86_XAX);

    gum_x86_writer_put_jmp_near_ptr (cw, GUM_ADDRESS (ic_match));
  }
  else
  {
    ret_code_address = cw->pc;
    gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (cws->code));

    /*
     * Our backpatch may be larger than the trivial jump to the slow slab above,
     * especially if we have to write a epilogue, leave a little wiggle room.
     */
    gum_x86_writer_put_nop_padding (cw, 50);
  }

  gum_exec_block_close_prolog (block, gc, cws);

  if (target->is_indirect)
  {
    entry_func = GUM_ENTRYGATE (call_mem);
  }
  else if (target->base != X86_REG_INVALID)
  {
    entry_func = GUM_ENTRYGATE (call_reg);
  }
  else
  {
    entry_func = GUM_ENTRYGATE (call_imm);
  }

  gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc, cws);

  /* Generate code for the target */
  gum_exec_ctx_get_branch_target_address (block->ctx, target, gc, cws);

  gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
      GUM_ADDRESS (entry_func), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, GUM_X86_XAX,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  if (trust_threshold >= 0)
  {
    gum_x86_writer_put_mov_reg_near_ptr (cws, GUM_X86_XAX,
        GUM_ADDRESS (&block->ctx->current_block));
  }

  if (can_backpatch_statically)
  {
    gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
        GUM_ADDRESS (gum_exec_block_backpatch_call), 7,
        GUM_ARG_REGISTER, GUM_X86_XAX,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
        GUM_ARG_ADDRESS, call_code_start - GUM_ADDRESS (block->code_start),
        GUM_ARG_ADDRESS, GUM_ADDRESS (opened_prolog),
        GUM_ARG_ADDRESS, GUM_ADDRESS (ret_real_address),
        GUM_ARG_ADDRESS, ret_code_address - GUM_ADDRESS (block->code_start));
  }

  if (block->ic_entries != NULL)
  {
    gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
        GUM_ADDRESS (gum_exec_block_backpatch_inline_cache), 3,
        GUM_ARG_REGISTER, GUM_X86_XAX,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  }

  /* Execute the generated code */
  gum_exec_block_close_prolog (block, gc, cws);

  /* Push the real return address */
  gum_x86_writer_put_lea_reg_reg_offset (cws, GUM_X86_XSP, GUM_X86_XSP,
      -(gssize) sizeof (gpointer));
  gum_x86_writer_put_push_reg (cws, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_address (cws, GUM_X86_XAX,
      GUM_ADDRESS (ret_real_address));
  gum_x86_writer_put_mov_reg_offset_ptr_reg (cws, GUM_X86_XSP,
      sizeof (gpointer), GUM_X86_XAX);
  gum_x86_writer_put_pop_reg (cws, GUM_X86_XAX);

  gum_x86_writer_put_jmp_near_ptr (cws, GUM_ADDRESS (&block->ctx->resume_at));
}

static void
gum_exec_block_write_jmp_transfer_code (GumExecBlock * block,
                                        const GumBranchTarget * target,
                                        GumExecCtxReplaceCurrentBlockFunc func,
                                        GumGeneratorContext * gc,
                                        guint id,
                                        GumAddress jcc_address)
{
  const gint trust_threshold = block->ctx->stalker->trust_threshold;
  GumX86Writer * cw = gc->code_writer;
  const GumAddress code_start = cw->pc;
  GumX86Writer * cws = gc->slow_writer;
  const GumPrologType opened_prolog = gc->opened_prolog;
  gboolean can_backpatch_statically;
  gpointer * ic_match = NULL;

  can_backpatch_statically =
      trust_threshold >= 0 &&
      !target->is_indirect &&
      target->base == X86_REG_INVALID;

  if (trust_threshold >= 0 && !can_backpatch_statically)
  {
    gum_exec_block_close_prolog (block, gc, gc->code_writer);

    gum_exec_block_open_prolog (block, GUM_PROLOG_IC, gc, gc->code_writer);
    gum_exec_ctx_get_branch_target_address (block->ctx, target, gc,
        gc->code_writer);
    ic_match = gum_exec_block_write_inline_cache_code (block, gc, cw, cws);

    /* Restore the target context and jump at ic_match */
    gum_exec_ctx_write_epilog (block->ctx, GUM_PROLOG_IC, cw);
    gum_x86_writer_put_jmp_near_ptr (cw, GUM_ADDRESS (ic_match));
  }
  else
  {
    gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (cws->code));

    /*
     * Our backpatch may be larger than the trivial jump to the slow slab above,
     * especially if we have to write a epilogue, leave a little wiggle room.
     */
    if (gc->opened_prolog != GUM_PROLOG_NONE)
      gum_x86_writer_put_nop_padding (cw, 11);
  }

  /* Cache miss, do it the hard way */
  gum_exec_block_close_prolog (block, gc, cws);

  /* Slow path */
  gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc, cws);
  gum_exec_ctx_get_branch_target_address (block->ctx, target, gc, cws);
  gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
      GUM_ADDRESS (func), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, GUM_X86_XAX,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  if (trust_threshold >= 0)
  {
    gum_x86_writer_put_mov_reg_near_ptr (cws, GUM_X86_XAX,
        GUM_ADDRESS (&block->ctx->current_block));
  }

  if (can_backpatch_statically)
  {
    switch (id)
    {
      case X86_INS_JMP:
      case X86_INS_CALL:
        gum_x86_writer_put_call_address_with_aligned_arguments (cws,
            GUM_CALL_CAPI, GUM_ADDRESS (gum_exec_block_backpatch_jmp), 6,
            GUM_ARG_REGISTER, GUM_X86_XAX,
            GUM_ARG_ADDRESS, GUM_ADDRESS (block),
            GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
            GUM_ARG_ADDRESS, GUM_ADDRESS (id),
            GUM_ARG_ADDRESS, code_start - GUM_ADDRESS (block->code_start),
            GUM_ARG_ADDRESS, GUM_ADDRESS (opened_prolog));
        break;
      default:
        gum_x86_writer_put_call_address_with_aligned_arguments (cws,
            GUM_CALL_CAPI, GUM_ADDRESS (gum_exec_block_backpatch_jmp), 6,
            GUM_ARG_REGISTER, GUM_X86_XAX,
            GUM_ARG_ADDRESS, GUM_ADDRESS (block),
            GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
            GUM_ARG_ADDRESS, GUM_ADDRESS (id),
            GUM_ARG_ADDRESS, jcc_address - GUM_ADDRESS (block->code_start),
            GUM_ARG_ADDRESS, GUM_ADDRESS (opened_prolog));
        break;
    }
  }

  if (block->ic_entries != NULL)
  {
    gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
        GUM_ADDRESS (gum_exec_block_backpatch_inline_cache), 3,
        GUM_ARG_REGISTER, GUM_X86_XAX,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  }

  gum_exec_block_close_prolog (block, gc, cws);

  gum_x86_writer_put_jmp_near_ptr (cws, GUM_ADDRESS (&block->ctx->resume_at));
}

/*
 * Return instructions are handled in a similar way to indirect branches using
 * an inline cache to determine the target. This avoids the overhead associated
 * with maintaining a shadow stack, and since most functions will have a very
 * limited number of call-sites, the inline cache should work very effectively.
 */
static void
gum_exec_block_write_ret_transfer_code (GumExecBlock * block,
                                        GumGeneratorContext * gc)
{
  GumInstruction * insn = gc->instruction;
  cs_x86 * x86 = &insn->ci->detail->x86;
  cs_x86_op * op = &x86->operands[0];
  guint16 npop = 0;

  if (x86->op_count != 0)
  {
    g_assert (x86->op_count == 1);
    g_assert (op->type == X86_OP_IMM);
    g_assert (op->imm <= G_MAXUINT16);
    npop = op->imm;
  }

  gum_exec_block_write_chaining_return_code (block, gc, npop);
}

static void
gum_exec_block_write_chaining_return_code (GumExecBlock * block,
                                           GumGeneratorContext * gc,
                                           guint16 npop)
{
  const gint trust_threshold = block->ctx->stalker->trust_threshold;
  GumX86Writer * cw = gc->code_writer;
  GumX86Writer * cws = gc->slow_writer;
  gpointer * ic_match;
  GumExecCtx * ctx = block->ctx;

  if (trust_threshold >= 0)
  {
    gum_exec_block_close_prolog (block, gc, gc->code_writer);

    gum_exec_block_open_prolog (block, GUM_PROLOG_IC, gc, gc->code_writer);

    gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XAX,
        GUM_ADDRESS (&ctx->app_stack));
    gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XAX);
    gum_x86_writer_put_mov_reg_reg_ptr (cw, GUM_X86_XAX, GUM_X86_XAX);

    ic_match = gum_exec_block_write_inline_cache_code (block, gc, cw, cws);

    /* Restore the target context and jump at ic_match */
    gum_exec_ctx_write_epilog (block->ctx, GUM_PROLOG_IC, cw);
    gum_x86_writer_put_lea_reg_reg_offset (cw, GUM_X86_XSP, GUM_X86_XSP,
        npop + sizeof (gpointer));
    gum_x86_writer_put_jmp_near_ptr (cw, GUM_ADDRESS (ic_match));
  }
  else
  {
    gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (cws->code));
  }

  /* Cache miss, do it the hard way */
  gum_exec_block_close_prolog (block, gc, cws);

  /* Slow path */
  gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc, cws);

  /*
   * If the user emits a CALL instruction from within their transformer, then
   * this will result in control flow returning back to the code slab when that
   * function returns. The target address for this RET is therefore not an
   * instrumented block (e.g. a real address within the application which has
   * been instrumented), but actually a code address within an instrumented
   * block itself. This therefore needs to be treated as a special case.
   *
   * Also since we cannot guarantee that code addresses between a stalker
   * instance and an observer are identical (hence prefetched backpatches are
   * communicated in terms of their real address), whilst these can be
   * backpatched by adding them to the inline cache, they cannot be prefetched.
   *
   * This block handles the backpatching of the entry into the inline cache, but
   * the block is still fetched by the call to `ret_slow_path` below, but the
   * ctx->current_block is not set and therefore the block is not backpatched by
   * gum_exec_block_backpatch_inline_cache in the traditional way.
   */
  if (trust_threshold >= 0)
  {
    gum_x86_writer_put_mov_reg_address (cws, GUM_X86_XAX,
        GUM_ADDRESS (&ctx->app_stack));
    gum_x86_writer_put_mov_reg_reg_ptr (cws, GUM_X86_XAX, GUM_X86_XAX);
    gum_x86_writer_put_mov_reg_reg_ptr (cws, GUM_X86_THUNK_REG_ARG1,
        GUM_X86_XAX);

    gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
        GUM_ADDRESS (gum_exec_block_backpatch_slab),
        2,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_REGISTER, GUM_X86_THUNK_REG_ARG1);
  }

  gum_x86_writer_put_mov_reg_address (cws, GUM_X86_XAX,
      GUM_ADDRESS (&ctx->app_stack));
  gum_x86_writer_put_mov_reg_reg_ptr (cws, GUM_X86_XAX, GUM_X86_XAX);
  gum_x86_writer_put_mov_reg_reg_ptr (cws, GUM_X86_THUNK_REG_ARG1, GUM_X86_XAX);

  gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
      GUM_ADDRESS (GUM_ENTRYGATE (ret_slow_path)), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, GUM_X86_THUNK_REG_ARG1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  if (trust_threshold >= 0)
  {
    gum_x86_writer_put_mov_reg_near_ptr (cws, GUM_X86_XAX,
        GUM_ADDRESS (&block->ctx->current_block));

    gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
        GUM_ADDRESS (gum_exec_block_backpatch_inline_cache), 3,
        GUM_ARG_REGISTER, GUM_X86_XAX,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  }

  gum_exec_block_close_prolog (block, gc, cws);
  gum_x86_writer_put_lea_reg_reg_offset (cws, GUM_X86_XSP, GUM_X86_XSP,
      npop + sizeof (gpointer));
  gum_x86_writer_put_jmp_near_ptr (cws, GUM_ADDRESS (&block->ctx->resume_at));
}

static gpointer *
gum_exec_block_write_inline_cache_code (GumExecBlock * block,
                                        GumGeneratorContext * gc,
                                        GumX86Writer * cw,
                                        GumX86Writer * cws)
{
  GumSlab * data_slab = &block->ctx->data_slab->slab;
  GumStalker * stalker = block->ctx->stalker;
  guint i;
  const gsize empty_val = GUM_IC_MAGIC_EMPTY;
  const gsize scratch_val = GUM_IC_MAGIC_SCRATCH;
  gpointer * ic_match;
  gconstpointer match = cw->code + 1;

  block->ic_entries = gum_slab_reserve (data_slab,
      gum_stalker_get_ic_entry_size (stalker));

  for (i = 0; i != stalker->ic_entries; i++)
  {
    block->ic_entries[i].real_start = NULL;
    block->ic_entries[i].code_start = GSIZE_TO_POINTER (empty_val);
  }

  /*
   * Write a token which we can replace with our matched ic entry code_start
   * so we can use it as scratch space and retrieve and jump to it once we
   * have restored the target application context.
   */
  ic_match = gum_slab_reserve (data_slab, sizeof (scratch_val));
  *ic_match = GSIZE_TO_POINTER (scratch_val);

  gum_x86_writer_put_mov_reg_address (cw, GUM_X86_XBX,
      GUM_ADDRESS (block->ic_entries));

  for (i = 0; i != stalker->ic_entries; i++)
  {
    gum_x86_writer_put_cmp_reg_offset_ptr_reg (cw, GUM_X86_XBX,
        G_STRUCT_OFFSET (GumIcEntry, real_start), GUM_X86_XAX);
    gum_x86_writer_put_jcc_near_label (cw, X86_INS_JE, match, GUM_NO_HINT);
    gum_x86_writer_put_add_reg_imm (cw, GUM_X86_XBX, sizeof (GumIcEntry));
  }

  gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (cws->code));

  gum_x86_writer_put_label (cw, match);

  /* We found a match, stash the code_start value in the ic_match */
  gum_x86_writer_put_mov_reg_reg_offset_ptr (cw, GUM_X86_XAX, GUM_X86_XBX,
      G_STRUCT_OFFSET (GumIcEntry, code_start));
  gum_x86_writer_put_mov_near_ptr_reg (cw, GUM_ADDRESS (ic_match),
      GUM_X86_XAX);

  return ic_match;
}

/*
 * This function is responsible for backpatching code_slab addresses into the
 * inline cache. This may be encountered, for example when control flow returns
 * following execution of a CALL instruction emitted by a transformer.
 */
static void
gum_exec_block_backpatch_slab (GumExecBlock * block,
                               gpointer target)
{
  GumExecCtx * ctx = block->ctx;
  GumStalker * stalker = ctx->stalker;
  GumIcEntry * ic_entries = block->ic_entries;
  guint i;

  if (!gum_exec_ctx_contains (ctx, target))
    return;

  for (i = 0; i != stalker->ic_entries; i++)
  {
    if (ic_entries[i].real_start == target)
      return;
  }

  gum_spinlock_acquire (&ctx->code_lock);

  memmove (&ic_entries[1], &ic_entries[0],
      (stalker->ic_entries - 1) * sizeof (GumIcEntry));

  ic_entries[0].real_start = target;
  ic_entries[0].code_start = target;

  gum_spinlock_release (&ctx->code_lock);
}

static void
gum_exec_block_write_single_step_transfer_code (GumExecBlock * block,
                                                GumGeneratorContext * gc)
{
  guint8 code[] = {
    0xc6, 0x05, 0x78, 0x56, 0x34, 0x12,       /* mov byte [X], state */
          GUM_EXEC_CTX_SINGLE_STEPPING_ON_CALL,
    0x9c,                                     /* pushfd              */
    0x81, 0x0c, 0x24, 0x00, 0x01, 0x00, 0x00, /* or [esp], 0x100     */
    0x9d                                      /* popfd               */
  };

  *((GumExecCtxMode **) (code + 2)) = &block->ctx->mode;
  gum_x86_writer_put_bytes (gc->code_writer, code, sizeof (code));
  gum_x86_writer_put_jmp_address (gc->code_writer,
      GUM_ADDRESS (gc->instruction->start));
}

#ifdef GUM_HAVE_SYSENTER_VIRTUALIZATION

static void
gum_exec_block_write_sysenter_continuation_code (GumExecBlock * block,
                                                 GumGeneratorContext * gc,
                                                 gpointer saved_ret_addr)
{
  GumStalker * stalker = block->ctx->stalker;
  const gint trust_threshold = stalker->trust_threshold;
  GumX86Writer * cw = gc->code_writer;
  GumX86Writer * cws = gc->slow_writer;
  gpointer * ic_match;

  if (trust_threshold >= 0)
  {
    if ((block->ctx->sink_mask & GUM_RET) != 0)
    {
      gum_exec_block_write_ret_event_code (block, gc, GUM_CODE_UNINTERRUPTIBLE);
    }

    gum_exec_block_open_prolog (block, GUM_PROLOG_IC, gc, gc->code_writer);

    /*
     * But first, check if we've been asked to unfollow, in which case we'll
     * enter the Stalker so the unfollow can be completed...
     */
    gum_x86_writer_put_mov_reg_near_ptr (cw, GUM_X86_EAX,
        GUM_ADDRESS (&block->ctx->state));
    gum_x86_writer_put_cmp_reg_i32 (cw, GUM_X86_EAX,
        GUM_EXEC_CTX_UNFOLLOW_PENDING);
    gum_x86_writer_put_jcc_near (cw, X86_INS_JE, cws->code, GUM_UNLIKELY);

    gum_x86_writer_put_mov_reg_near_ptr (cw, GUM_X86_EAX,
        GUM_ADDRESS (saved_ret_addr));

    ic_match = gum_exec_block_write_inline_cache_code (block, gc, cw, cws);

    /* Restore the target context and jump at ic_match */
    gum_exec_ctx_write_epilog (block->ctx, GUM_PROLOG_IC, cw);
    gum_x86_writer_put_jmp_near_ptr (cw, GUM_ADDRESS (ic_match));
  }
  else
  {
    gum_x86_writer_put_jmp_address (cw, GUM_ADDRESS (cws->code));
  }

  /* Cache miss, do it the hard way */
  gum_exec_block_close_prolog (block, gc, cws);

  /*
   * Slow path (resolve dynamically)
   */
  gum_exec_block_open_prolog (block, GUM_PROLOG_MINIMAL, gc, cws);

  gum_x86_writer_put_mov_reg_near_ptr (cws, GUM_X86_THUNK_REG_ARG1,
      GUM_ADDRESS (saved_ret_addr));
  gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
      GUM_ADDRESS (GUM_ENTRYGATE (sysenter_slow_path)), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, GUM_X86_THUNK_REG_ARG1,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  if (trust_threshold >= 0)
  {
    gum_x86_writer_put_mov_reg_near_ptr (cws, GUM_X86_XAX,
        GUM_ADDRESS (&block->ctx->current_block));

    gum_x86_writer_put_call_address_with_aligned_arguments (cws, GUM_CALL_CAPI,
        GUM_ADDRESS (gum_exec_block_backpatch_inline_cache), 3,
        GUM_ARG_REGISTER, GUM_X86_XAX,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  }

  gum_exec_block_close_prolog (block, gc, cws);
  gum_x86_writer_put_jmp_near_ptr (cws, GUM_ADDRESS (&block->ctx->resume_at));

  gum_x86_relocator_skip_one_no_label (gc->relocator);
}

#endif

static void
gum_exec_block_write_call_event_code (GumExecBlock * block,
                                      const GumBranchTarget * target,
                                      GumGeneratorContext * gc,
                                      GumCodeContext cc)
{
  GumX86Writer * cw = gc->code_writer;

  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_exec_ctx_get_branch_target_address (block->ctx, target, gc,
      gc->code_writer);

  gum_x86_writer_put_call_address_with_aligned_arguments (cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_exec_ctx_emit_call_event), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, GUM_X86_XAX,
      GUM_ARG_REGISTER, GUM_X86_XBX);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
  gum_exec_block_close_prolog (block, gc, gc->code_writer);
}

static void
gum_exec_block_write_ret_event_code (GumExecBlock * block,
                                     GumGeneratorContext * gc,
                                     GumCodeContext cc)
{
  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_x86_writer_put_call_address_with_aligned_arguments (gc->code_writer,
      GUM_CALL_CAPI, GUM_ADDRESS (gum_exec_ctx_emit_ret_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, GUM_X86_XBX);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
  gum_exec_block_close_prolog (block, gc, gc->code_writer);
}

static void
gum_exec_block_write_exec_event_code (GumExecBlock * block,
                                      GumGeneratorContext * gc,
                                      GumCodeContext cc)
{
  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_x86_writer_put_call_address_with_aligned_arguments (gc->code_writer,
      GUM_CALL_CAPI, GUM_ADDRESS (gum_exec_ctx_emit_exec_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, GUM_X86_XBX);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
  gum_exec_block_close_prolog (block, gc, gc->code_writer);
}

static void
gum_exec_block_write_block_event_code (GumExecBlock * block,
                                       GumGeneratorContext * gc,
                                       GumCodeContext cc)
{
  gum_exec_block_open_prolog (block, GUM_PROLOG_FULL, gc, gc->code_writer);

  gum_x86_writer_put_call_address_with_aligned_arguments (gc->code_writer,
      GUM_CALL_CAPI, GUM_ADDRESS (gum_exec_ctx_emit_block_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, GUM_X86_XBX);

  gum_exec_block_write_unfollow_check_code (block, gc, cc);
  gum_exec_block_close_prolog (block, gc, gc->code_writer);
}

static void
gum_exec_block_write_unfollow_check_code (GumExecBlock * block,
                                          GumGeneratorContext * gc,
                                          GumCodeContext cc)
{
  GumExecCtx * ctx = block->ctx;
  GumX86Writer * cw = gc->code_writer;
  gconstpointer beach = cw->code + 1;
  GumPrologType opened_prolog;

  if (cc != GUM_CODE_INTERRUPTIBLE)
    return;

  gum_x86_writer_put_call_address_with_aligned_arguments (cw, GUM_CALL_CAPI,
      GUM_ADDRESS (gum_exec_ctx_maybe_unfollow), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
  gum_x86_writer_put_test_reg_reg (cw, GUM_X86_EAX, GUM_X86_EAX);
  gum_x86_writer_put_jcc_near_label (cw, X86_INS_JE, beach, GUM_LIKELY);

  opened_prolog = gc->opened_prolog;
  gum_exec_block_close_prolog (block, gc, gc->code_writer);
  gc->opened_prolog = opened_prolog;

  gum_x86_writer_put_jmp_near_ptr (cw, GUM_ADDRESS (&ctx->resume_at));

  gum_x86_writer_put_label (cw, beach);
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

  gum_x86_writer_put_call_address_with_aligned_arguments (gc->code_writer,
      GUM_CALL_CAPI, GUM_ADDRESS (gum_exec_block_invoke_call_probes),
      2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, GUM_X86_XBX);
}

static void
gum_exec_block_invoke_call_probes (GumExecBlock * block,
                                   GumCpuContext * cpu_context)
{
  GumStalker * stalker = block->ctx->stalker;
  const gpointer target_address = block->real_start;
  GumCallProbe ** probes_copy;
  guint num_probes, i;
  gpointer * return_address_slot;
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

  return_address_slot = GSIZE_TO_POINTER (GUM_CPU_CONTEXT_XSP (cpu_context));

  d.target_address = target_address;
  d.return_address = *return_address_slot;
  d.stack_data = return_address_slot;
  d.cpu_context = cpu_context;

  GUM_CPU_CONTEXT_XIP (cpu_context) = GPOINTER_TO_SIZE (target_address);

  for (i = 0; i != num_probes; i++)
  {
    GumCallProbe * probe = probes_copy[i];

    probe->callback (&d, probe->user_data);

    gum_call_probe_unref (probe);
  }
}

static gpointer
gum_exec_block_write_inline_data (GumX86Writer * cw,
                                  gconstpointer data,
                                  gsize size,
                                  GumAddress * address)
{
  gpointer location;
  gconstpointer after_data = cw->code + 1;

  while (gum_x86_writer_offset (cw) < GUM_INVALIDATE_TRAMPOLINE_SIZE)
  {
    gum_x86_writer_put_nop (cw);
  }

  if (GUM_IS_WITHIN_UINT8_RANGE (size))
    gum_x86_writer_put_jmp_short_label (cw, after_data);
  else
    gum_x86_writer_put_jmp_near_label (cw, after_data);

  location = gum_x86_writer_cur (cw);
  if (address != NULL)
    *address = cw->pc;
  gum_x86_writer_put_bytes (cw, data, size);

  gum_x86_writer_put_label (cw, after_data);

  return location;
}

static void
gum_exec_block_open_prolog (GumExecBlock * block,
                            GumPrologType type,
                            GumGeneratorContext * gc,
                            GumX86Writer * cw)
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
                             GumX86Writer * cw)
{
  if (gc->opened_prolog == GUM_PROLOG_NONE)
    return;

  gum_exec_ctx_write_epilog (block->ctx, gc->opened_prolog, cw);

  gc->opened_prolog = GUM_PROLOG_NONE;
}

static GumCodeSlab *
gum_code_slab_new (GumExecCtx * ctx)
{
  GumCodeSlab * slab;
  GumStalker * stalker = ctx->stalker;
  const gsize slab_size = stalker->code_slab_size_dynamic;
  GumAddressSpec spec;

  gum_exec_ctx_compute_code_address_spec (ctx, slab_size, &spec);

  slab = gum_memory_allocate_near (&spec, slab_size, stalker->page_size,
      stalker->is_rwx_supported ? GUM_PAGE_RWX : GUM_PAGE_RW);

  gum_code_slab_init (slab, slab_size, stalker->page_size);

  return slab;
}

static GumSlowSlab *
gum_slow_slab_new (GumExecCtx * ctx)
{
  GumSlowSlab * slab;
  GumStalker * stalker = ctx->stalker;
  const gsize slab_size = stalker->code_slab_size_dynamic;
  GumAddressSpec spec;

  gum_exec_ctx_compute_code_address_spec (ctx, slab_size, &spec);

  slab = gum_memory_allocate_near (&spec, slab_size, stalker->page_size,
      stalker->is_rwx_supported ? GUM_PAGE_RWX : GUM_PAGE_RW);

  gum_slow_slab_init (slab, slab_size, stalker->page_size);

  return slab;
}

static void
gum_code_slab_free (GumCodeSlab * code_slab)
{
  gum_slab_free (&code_slab->slab);
}

static void
gum_code_slab_init (GumCodeSlab * code_slab,
                    gsize slab_size,
                    gsize page_size)
{
  /*
   * We don't want to thaw and freeze the header just to update the offset,
   * so we trade a little memory for speed.
   */
  const gsize header_size = GUM_ALIGN_SIZE (sizeof (GumCodeSlab), page_size);

  gum_slab_init (&code_slab->slab, slab_size, header_size);

  code_slab->invalidator = NULL;
}

static void
gum_slow_slab_init (GumSlowSlab * slow_slab,
                    gsize slab_size,
                    gsize page_size)
{
  /*
   * We don't want to thaw and freeze the header just to update the offset,
   * so we trade a little memory for speed.
   */
  const gsize header_size = GUM_ALIGN_SIZE (sizeof (GumCodeSlab), page_size);

  gum_slab_init (&slow_slab->slab, slab_size, header_size);

  slow_slab->invalidator = NULL;
}

static void
gum_slow_slab_free (GumSlowSlab * slow_slab)
{
  gum_slab_free (&slow_slab->slab);
}

static GumDataSlab *
gum_data_slab_new (GumExecCtx * ctx)
{
  GumDataSlab * slab;
  GumStalker * stalker = ctx->stalker;
  const gsize slab_size = stalker->data_slab_size_dynamic;
  GumAddressSpec spec;

  gum_exec_ctx_compute_data_address_spec (ctx, slab_size, &spec);

  slab = gum_memory_allocate_near (&spec, slab_size, stalker->page_size,
      GUM_PAGE_RW);

  gum_data_slab_init (slab, slab_size);

  return slab;
}

static void
gum_data_slab_free (GumDataSlab * data_slab)
{
  gum_slab_free (&data_slab->slab);
}

static void
gum_data_slab_init (GumDataSlab * data_slab,
                    gsize slab_size)
{
  GumSlab * slab = &data_slab->slab;
  const gsize header_size = sizeof (GumDataSlab);

  gum_slab_init (slab, slab_size, header_size);
}

static void
gum_scratch_slab_init (GumCodeSlab * scratch_slab,
                       gsize slab_size)
{
  const gsize header_size = sizeof (GumCodeSlab);

  gum_slab_init (&scratch_slab->slab, slab_size, header_size);

  scratch_slab->invalidator = NULL;
}

static void
gum_slab_free (GumSlab * slab)
{
  const gsize header_size = slab->data - (guint8 *) slab;

  gum_memory_free (slab, header_size + slab->size);
}

static void
gum_slab_init (GumSlab * slab,
               gsize slab_size,
               gsize header_size)
{
  slab->data = (guint8 *) slab + header_size;
  slab->offset = 0;
  slab->size = slab_size - header_size;
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

static void
gum_write_segment_prefix (uint8_t segment,
                          GumX86Writer * cw)
{
  switch (segment)
  {
    case X86_REG_INVALID: break;

    case X86_REG_CS: gum_x86_writer_put_u8 (cw, 0x2e); break;
    case X86_REG_SS: gum_x86_writer_put_u8 (cw, 0x36); break;
    case X86_REG_DS: gum_x86_writer_put_u8 (cw, 0x3e); break;
    case X86_REG_ES: gum_x86_writer_put_u8 (cw, 0x26); break;
    case X86_REG_FS: gum_x86_writer_put_u8 (cw, 0x64); break;
    case X86_REG_GS: gum_x86_writer_put_u8 (cw, 0x65); break;

    default:
      g_assert_not_reached ();
      break;
  }
}

static GumX86Reg
gum_x86_meta_reg_from_real_reg (GumX86Reg reg)
{
  if (reg >= GUM_X86_EAX && reg <= GUM_X86_EDI)
    return (GumX86Reg) (GUM_X86_XAX + reg - GUM_X86_EAX);
  else if (reg >= GUM_X86_RAX && reg <= GUM_X86_RDI)
    return (GumX86Reg) (GUM_X86_XAX + reg - GUM_X86_RAX);
#if GLIB_SIZEOF_VOID_P == 8
  else if (reg >= GUM_X86_R8D && reg <= GUM_X86_R15D)
    return reg;
  else if (reg >= GUM_X86_R8 && reg <= GUM_X86_R15)
    return reg;
#endif
  else if (reg == GUM_X86_RIP)
    return GUM_X86_XIP;
  else if (reg != GUM_X86_NONE)
    g_assert_not_reached ();

  return GUM_X86_NONE;
}

static GumX86Reg
gum_x86_reg_from_capstone (x86_reg reg)
{
  switch (reg)
  {
    case X86_REG_EAX: return GUM_X86_EAX;
    case X86_REG_ECX: return GUM_X86_ECX;
    case X86_REG_EDX: return GUM_X86_EDX;
    case X86_REG_EBX: return GUM_X86_EBX;
    case X86_REG_ESP: return GUM_X86_ESP;
    case X86_REG_EBP: return GUM_X86_EBP;
    case X86_REG_ESI: return GUM_X86_ESI;
    case X86_REG_EDI: return GUM_X86_EDI;
    case X86_REG_R8D: return GUM_X86_R8D;
    case X86_REG_R9D: return GUM_X86_R9D;
    case X86_REG_R10D: return GUM_X86_R10D;
    case X86_REG_R11D: return GUM_X86_R11D;
    case X86_REG_R12D: return GUM_X86_R12D;
    case X86_REG_R13D: return GUM_X86_R13D;
    case X86_REG_R14D: return GUM_X86_R14D;
    case X86_REG_R15D: return GUM_X86_R15D;
    case X86_REG_EIP: return GUM_X86_EIP;

    case X86_REG_RAX: return GUM_X86_RAX;
    case X86_REG_RCX: return GUM_X86_RCX;
    case X86_REG_RDX: return GUM_X86_RDX;
    case X86_REG_RBX: return GUM_X86_RBX;
    case X86_REG_RSP: return GUM_X86_RSP;
    case X86_REG_RBP: return GUM_X86_RBP;
    case X86_REG_RSI: return GUM_X86_RSI;
    case X86_REG_RDI: return GUM_X86_RDI;
    case X86_REG_R8: return GUM_X86_R8;
    case X86_REG_R9: return GUM_X86_R9;
    case X86_REG_R10: return GUM_X86_R10;
    case X86_REG_R11: return GUM_X86_R11;
    case X86_REG_R12: return GUM_X86_R12;
    case X86_REG_R13: return GUM_X86_R13;
    case X86_REG_R14: return GUM_X86_R14;
    case X86_REG_R15: return GUM_X86_R15;
    case X86_REG_RIP: return GUM_X86_RIP;

    default:
      return GUM_X86_NONE;
  }
}

#ifdef HAVE_WINDOWS

static gboolean
gum_stalker_on_exception (GumExceptionDetails * details,
                          gpointer user_data)
{
  GumStalker * self = GUM_STALKER (user_data);
  GumCpuContext * cpu_context = &details->context;
  CONTEXT * tc = details->native_context;
  GumExecCtx * candidate_ctx;

  if (details->type != GUM_EXCEPTION_SINGLE_STEP)
    return FALSE;

  candidate_ctx =
      gum_stalker_find_exec_ctx_by_thread_id (self, details->thread_id);
  if (candidate_ctx != NULL &&
      GUM_CPU_CONTEXT_XIP (cpu_context) == candidate_ctx->previous_pc)
  {
    GumExecCtx * pending_ctx = candidate_ctx;

    tc->Dr0 = pending_ctx->previous_dr0;
    tc->Dr7 = pending_ctx->previous_dr7;

    pending_ctx->previous_pc = 0;

    GUM_CPU_CONTEXT_XIP (cpu_context) = pending_ctx->infect_body;

    return TRUE;
  }

# if GLIB_SIZEOF_VOID_P == 8
  return FALSE;
# else
  {
    GumExecCtx * ctx;

    ctx = gum_stalker_get_exec_ctx ();
    if (ctx == NULL)
      return FALSE;

    switch (ctx->mode)
    {
      case GUM_EXEC_CTX_NORMAL:
      case GUM_EXEC_CTX_SINGLE_STEPPING_ON_CALL:
      {
        DWORD instruction_after_call_here;
        DWORD instruction_after_call_above_us;

        ctx->previous_dr0 = tc->Dr0;
        ctx->previous_dr1 = tc->Dr1;
        ctx->previous_dr2 = tc->Dr2;
        ctx->previous_dr7 = tc->Dr7;

        tc->Dr7 = 0x00000700;

        instruction_after_call_here = cpu_context->eip +
            gum_x86_reader_insn_length ((guint8 *) cpu_context->eip);
        tc->Dr0 = instruction_after_call_here;
        gum_enable_hardware_breakpoint (&tc->Dr7, 0);

        tc->Dr1 = (DWORD) self->ki_user_callback_dispatcher_impl;
        gum_enable_hardware_breakpoint (&tc->Dr7, 1);

        instruction_after_call_above_us =
            (DWORD) gum_find_system_call_above_us (self,
                (gpointer *) cpu_context->esp);
        if (instruction_after_call_above_us != 0)
        {
          tc->Dr2 = instruction_after_call_above_us;
          gum_enable_hardware_breakpoint (&tc->Dr7, 2);
        }

        ctx->mode = GUM_EXEC_CTX_SINGLE_STEPPING_THROUGH_CALL;

        break;
      }
      case GUM_EXEC_CTX_SINGLE_STEPPING_THROUGH_CALL:
      {
        tc->Dr0 = ctx->previous_dr0;
        tc->Dr1 = ctx->previous_dr1;
        tc->Dr2 = ctx->previous_dr2;
        tc->Dr7 = ctx->previous_dr7;

        gum_exec_ctx_switch_block (ctx, NULL,
            GSIZE_TO_POINTER (cpu_context->eip),
            GSIZE_TO_POINTER (cpu_context->eip));
        cpu_context->eip = (DWORD) ctx->resume_at;

        ctx->mode = GUM_EXEC_CTX_NORMAL;

        break;
      }
      default:
        g_assert_not_reached ();
    }

    return TRUE;
  }
#endif
}

static void
gum_enable_hardware_breakpoint (GumNativeRegisterValue * dr7_reg,
                                guint index)
{
  /* Set both RWn and LENn to 00. */
  *dr7_reg &= ~((GumNativeRegisterValue) 0xf << (16 + (2 * index)));

  /* Set LE bit. */
  *dr7_reg |= (GumNativeRegisterValue) (1 << (2 * index));
}

# if GLIB_SIZEOF_VOID_P == 4

static void
gum_collect_export (GArray * impls,
                    const WCHAR * module_name,
                    const gchar * export_name)
{
  HMODULE module_handle;

  module_handle = GetModuleHandleW (module_name);
  if (module_handle == NULL)
    return;

  gum_collect_export_by_handle (impls, module_handle, export_name);
}

static void
gum_collect_export_by_handle (GArray * impls,
                              HMODULE module_handle,
                              const gchar * export_name)
{
  gsize impl;

  impl = GPOINTER_TO_SIZE (GetProcAddress (module_handle, export_name));
  if (impl == 0)
    return;

  g_array_append_val (impls, impl);
}

static gpointer
gum_find_system_call_above_us (GumStalker * stalker,
                               gpointer * start_esp)
{
  gpointer * top_esp, * cur_esp;
  guint8 call_fs_c0_code[] = { 0x64, 0xff, 0x15, 0xc0, 0x00, 0x00, 0x00 };
  guint8 call_ebp_8_code[] = { 0xff, 0x55, 0x08 };
  guint8 * minimum_address, * maximum_address;

#ifdef _MSC_VER
  __asm
  {
    mov eax, fs:[4];
    mov [top_esp], eax;
  }
#else
  asm volatile (
      "movl %%fs:4, %k0"
      : "=q" (top_esp)
  );
#endif

  if ((guint) ABS (top_esp - start_esp) > stalker->page_size)
  {
    top_esp = (gpointer *) ((GPOINTER_TO_SIZE (start_esp) +
        (stalker->page_size - 1)) & ~(stalker->page_size - 1));
  }

  /* These boundaries are quite artificial... */
  minimum_address = (guint8 *) stalker->user32_start + sizeof (call_fs_c0_code);
  maximum_address = (guint8 *) stalker->user32_end - 1;

  for (cur_esp = start_esp + 1; cur_esp < top_esp; cur_esp++)
  {
    guint8 * address = (guint8 *) *cur_esp;

    if (address >= minimum_address && address <= maximum_address)
    {
      if (memcmp (address - sizeof (call_fs_c0_code), call_fs_c0_code,
          sizeof (call_fs_c0_code)) == 0
          || memcmp (address - sizeof (call_ebp_8_code), call_ebp_8_code,
          sizeof (call_ebp_8_code)) == 0)
      {
        return address;
      }
    }
  }

  return NULL;
}

# endif

#endif

static gpointer
gum_find_thread_exit_implementation (void)
{
#if defined (HAVE_DARWIN)
  GumAddress result = 0;
  GumModule * pthread;
  GumMatchPattern * pattern;

  pthread = gum_process_find_module_by_name (
      "/usr/lib/system/libsystem_pthread.dylib");

  pattern = gum_match_pattern_new_from_string (
#if GLIB_SIZEOF_VOID_P == 8
      /*
       * Verified on macOS:
       * - 10.14.6
       * - 10.15.6
       * - 11.0 Beta 3
       */
      "55 "            /* push rbp                       */
      "48 89 e5 "      /* mov rbp, rsp                   */
      "41 57 "         /* push r15                       */
      "41 56 "         /* push r14                       */
      "53 "            /* push rbx                       */
      "50 "            /* push rax                       */
      "49 89 f6 "      /* mov r14, rsi                   */
      "49 89 ff"       /* mov r15, rdi                   */
      "bf 01 00 00 00" /* mov edi, 0x1                   */
#else
      /*
       * Verified on macOS:
       * - 10.14.6
       */
      "55 "            /* push ebp                       */
      "89 e5 "         /* mov ebp, esp                   */
      "53 "            /* push ebx                       */
      "57 "            /* push edi                       */
      "56 "            /* push esi                       */
      "83 ec 0c "      /* sub esp, 0xc                   */
      "89 d6 "         /* mov esi, edx                   */
      "89 cf"          /* mov edi, ecx                   */
#endif
  );

  gum_memory_scan (gum_module_get_range (pthread), pattern,
      gum_store_thread_exit_match, &result);

  gum_match_pattern_unref (pattern);

  /* Non-public symbols are all <redacted> on iOS. */
#ifndef HAVE_IOS
  if (result == 0)
    result = gum_module_find_symbol_by_name (pthread, "_pthread_exit");
#endif

  g_object_unref (pthread);

  return GSIZE_TO_POINTER (result);
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

  libthr = gum_process_find_module_by_name ("libthr.so.3");
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

#ifdef HAVE_DARWIN

static gboolean
gum_store_thread_exit_match (GumAddress address,
                             gsize size,
                             gpointer user_data)
{
  GumAddress * result = user_data;

  *result = address;

  return FALSE;
}

#endif
