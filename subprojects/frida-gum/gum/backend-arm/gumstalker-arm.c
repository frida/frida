/*
 * Copyright (C) 2009-2024 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023 Håvard Sørbø <havard@hsorbo.no>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "gumarmreg.h"
#include "gumarmreader.h"
#include "gumarmrelocator.h"
#include "gumarmwriter.h"
#include "gummemory.h"
#include "gummetalhash.h"
#include "gumspinlock.h"
#include "gumstalker-priv.h"
#include "gumunwindbroker.h"
#include "gumunwindbroker-priv.h"
#include "gumthumbreader.h"
#include "gumthumbrelocator.h"
#include "gumthumbwriter.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_LINUX
# include <unwind.h>
# include <sys/syscall.h>
#endif

#if defined (HAVE_DARWIN) || defined (HAVE_LINUX)
# define GUM_HAVE_UNWIND_PC_TRANSLATION 1
#endif

#define GUM_CODE_SLAB_SIZE_INITIAL  (128 * 1024)
#define GUM_CODE_SLAB_SIZE_DYNAMIC  (4 * 1024 * 1024)
#define GUM_DATA_SLAB_SIZE_INITIAL  (GUM_CODE_SLAB_SIZE_INITIAL / 5)
#define GUM_DATA_SLAB_SIZE_DYNAMIC  (GUM_CODE_SLAB_SIZE_DYNAMIC / 5)
#define GUM_SCRATCH_SLAB_SIZE       16384
#define GUM_EXEC_BLOCK_MIN_CAPACITY 1024

#define GUM_INVALIDATE_TRAMPOLINE_SIZE 12
#define GUM_EXCLUSIVE_ACCESS_MAX_DEPTH  8

#define GUM_INSTRUCTION_OFFSET_NONE (-1)

#define GUM_STALKER_LOCK(o) g_mutex_lock (&(o)->mutex)
#define GUM_STALKER_UNLOCK(o) g_mutex_unlock (&(o)->mutex)

typedef struct _GumInfectContext GumInfectContext;
typedef struct _GumDisinfectContext GumDisinfectContext;
typedef struct _GumActivation GumActivation;
typedef struct _GumInvalidateContext GumInvalidateContext;
typedef struct _GumCallProbe GumCallProbe;

typedef struct _GumExecCtx GumExecCtx;
typedef void (* GumArmHelperWriteFunc) (GumExecCtx * ctx, GumArmWriter * cw);
typedef void (* GumThumbHelperWriteFunc) (GumExecCtx * ctx,
    GumThumbWriter * cw);

typedef struct _GumExecBlock GumExecBlock;

typedef guint GumExecBlockFlags;

typedef struct _GumExecFrame GumExecFrame;

typedef struct _GumCodeSlab GumCodeSlab;
typedef struct _GumDataSlab GumDataSlab;
typedef struct _GumSlab GumSlab;

typedef guint GumPrologState;
typedef struct _GumGeneratorContext GumGeneratorContext;
typedef struct _GumCalloutEntry GumCalloutEntry;
typedef struct _GumInstruction GumInstruction;
typedef guint GumBranchTargetType;
typedef guint GumArmMode;
typedef struct _GumBranchTarget GumBranchTarget;
typedef struct _GumBranchDirectAddress GumBranchDirectAddress;
typedef struct _GumBranchDirectRegOffset GumBranchDirectRegOffset;
typedef struct _GumBranchDirectRegShift GumBranchDirectRegShift;
typedef struct _GumBranchIndirectRegOffset GumBranchIndirectRegOffset;
typedef struct _GumBranchIndirectRegShift GumBranchIndirectRegShift;
typedef struct _GumBranchIndirectPcrelTable GumBranchIndirectPcrelTable;
typedef struct _GumWriteback GumWriteback;
typedef guint GumBackpatchType;

typedef gboolean (* GumCheckExcludedFunc) (GumExecCtx * ctx,
    gconstpointer address);

struct _GumStalker
{
  GObject parent;

  gsize ctx_size;
  gsize ctx_header_size;

  goffset frames_offset;
  gsize frames_size;

  goffset thunks_offset;
  gsize thunks_size;

  goffset code_slab_offset;
  gsize code_slab_size_initial;
  gsize code_slab_size_dynamic;

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

  GumArmWriter arm_writer;
  GumArmRelocator arm_relocator;

  GumThumbWriter thumb_writer;
  GumThumbRelocator thumb_relocator;

  GumStalkerTransformer * transformer;
  void (* transform_block_impl) (GumStalkerTransformer * self,
      GumStalkerIterator * iterator, GumStalkerOutput * output);
  GumEventSink * sink;
  gboolean sink_started;
  GumEventType sink_mask;
  gpointer last_exec_location;
  void (* sink_process_impl) (GumEventSink * self, const GumEvent * event,
      GumCpuContext * cpu_context);
  GumStalkerObserver * observer;

  gboolean unfollow_called_while_still_following;
  GumExecBlock * current_block;
  gpointer pending_return_location;
  guint pending_calls;
  GumExecFrame * current_frame;
  GumExecFrame * first_frame;
  GumExecFrame * frames;

  gpointer resume_at;
  gpointer kuh_target;
  gconstpointer activation_target;
  guint32 cpsr;

  gpointer thunks;
  gpointer infect_thunk;

  GumSpinlock code_lock;
  GumCodeSlab * code_slab;
  GumDataSlab * data_slab;
  GumCodeSlab * scratch_slab;
  GumMetalHashTable * mappings;
  gpointer last_arm_invalidator;
  gpointer last_thumb_invalidator;

  GumExecBlock * block_list;

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
  GumExecBlock * next;

  GumExecCtx * ctx;
  GumCodeSlab * code_slab;
  GumExecBlock * storage_block;

  guint8 * real_start;
  guint8 * code_start;
  guint real_size;
  guint code_size;
  guint capacity;
  guint last_callout_offset;

  GumExecBlockFlags flags;
  gint recycle_count;
};

enum _GumExecBlockFlags
{
  GUM_EXEC_BLOCK_THUMB                 = 1 << 0,
  GUM_EXEC_BLOCK_ACTIVATION_TARGET     = 1 << 1,
  GUM_EXEC_BLOCK_HAS_EXCLUSIVE_LOAD    = 1 << 2,
  GUM_EXEC_BLOCK_HAS_EXCLUSIVE_STORE   = 1 << 3,
  GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS = 1 << 4,
};

struct _GumExecFrame
{
  gpointer real_address;
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

  gpointer arm_invalidator;
  gpointer thumb_invalidator;
};

struct _GumDataSlab
{
  GumSlab slab;
};

enum _GumPrologState
{
  GUM_PROLOG_CLOSED,
  GUM_PROLOG_OPEN
};

struct _GumGeneratorContext
{
  GumInstruction * instruction;
  gboolean is_thumb;

  GumArmRelocator * arm_relocator;
  GumArmWriter * arm_writer;

  GumThumbRelocator * thumb_relocator;
  GumThumbWriter * thumb_writer;

  gpointer continuation_real_address;
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

enum _GumBranchTargetType
{
  GUM_TARGET_DIRECT_ADDRESS,
  GUM_TARGET_DIRECT_REG_OFFSET,
  GUM_TARGET_DIRECT_REG_SHIFT,
  GUM_TARGET_INDIRECT_REG_OFFSET,
  GUM_TARGET_INDIRECT_REG_SHIFT,
  GUM_TARGET_INDIRECT_PCREL_TABLE
};

enum _GumArmMode
{
  GUM_ARM_MODE_AUTO,
  GUM_ARM_MODE_CURRENT
};

struct _GumBranchDirectAddress
{
  gpointer address;
};

struct _GumBranchDirectRegOffset
{
  arm_reg reg;
  gssize offset;
  GumArmMode mode;
};

struct _GumBranchDirectRegShift
{
  arm_reg base;
  arm_reg index;
  arm_shifter shifter;
  guint32 shift_value;
};

struct _GumBranchIndirectRegOffset
{
  arm_reg reg;
  gssize offset;
  gboolean write_back;
};

struct _GumBranchIndirectRegShift
{
  arm_reg base;
  arm_reg index;
  arm_shifter shifter;
  guint32 shift_value;
};

struct _GumBranchIndirectPcrelTable
{
  arm_reg base;
  arm_reg index;
  guint element_size;
};

struct _GumBranchTarget
{
  GumBranchTargetType type;

  union
  {
    GumBranchDirectAddress direct_address;
    GumBranchDirectRegOffset direct_reg_offset;
    GumBranchDirectRegShift direct_reg_shift;
    GumBranchIndirectRegOffset indirect_reg_offset;
    GumBranchIndirectRegShift indirect_reg_shift;
    GumBranchIndirectPcrelTable indirect_pcrel_table;
  } value;
};

struct _GumWriteback
{
  arm_reg target;
  gssize offset;
};

enum _GumBackpatchType
{
  GUM_BACKPATCH_ARM,
  GUM_BACKPATCH_THUMB,
};

struct _GumBackpatch
{
  GumBackpatchType type;
  gpointer to;
  gpointer from;
  gpointer from_insn;
  gsize code_offset;
  GumPrologState opened_prolog;
};

static void gum_stalker_unwind_translator_iface_init (gpointer iface,
    gpointer iface_data);
static void gum_stalker_dispose (GObject * object);
static void gum_stalker_finalize (GObject * object);

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

static void gum_stalker_thaw (GumStalker * self, gpointer code, gsize size);
static void gum_stalker_freeze (GumStalker * self, gpointer code, gsize size);

static GumAddress gum_stalker_translate_unwind_pc (
    GumUnwindPcTranslator * translator, GumAddress code_address);
static gboolean gum_stalker_install_unwind_resume_context (
    GumUnwindPcTranslator * translator, gpointer unwind_context,
    GumAddress real_resume_ip);

static GumExecCtx * gum_exec_ctx_new (GumStalker * self, GumThreadId thread_id,
    GumStalkerTransformer * transformer, GumEventSink * sink);
static void gum_exec_ctx_free (GumExecCtx * ctx);
static void gum_exec_ctx_dispose (GumExecCtx * ctx);
static GumCodeSlab * gum_exec_ctx_add_code_slab (GumExecCtx * ctx,
    GumCodeSlab * code_slab);
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
static void gum_exec_ctx_recompile_block (GumExecCtx * ctx,
    GumExecBlock * block);
static void gum_exec_ctx_compile_block (GumExecCtx * ctx, GumExecBlock * block,
    gconstpointer input_code, gpointer output_code, GumAddress output_pc,
    guint * input_size, guint * output_size);
static void gum_exec_ctx_compile_arm_block (GumExecCtx * ctx,
    GumExecBlock * block, gconstpointer input_code, gpointer output_code,
    GumAddress output_pc, guint * input_size, guint * output_size);
static void gum_exec_ctx_compile_thumb_block (GumExecCtx * ctx,
    GumExecBlock * block, gconstpointer input_code, gpointer output_code,
    GumAddress output_pc, guint * input_size, guint * output_size);
static void gum_exec_ctx_maybe_emit_compile_event (GumExecCtx * ctx,
    GumExecBlock * block);

static void gum_exec_ctx_begin_call (GumExecCtx * ctx, gpointer ret_addr);
static void gum_exec_ctx_end_call (GumExecCtx * ctx);

static gboolean gum_stalker_iterator_arm_next (GumStalkerIterator * self,
    const cs_insn ** insn);
static gboolean gum_stalker_iterator_thumb_next (GumStalkerIterator * self,
    const cs_insn ** insn);
static gboolean gum_stalker_iterator_is_out_of_space (
    GumStalkerIterator * self);
static void gum_stalker_iterator_arm_keep (GumStalkerIterator * self);
static void gum_stalker_iterator_thumb_keep (GumStalkerIterator * self);
static void gum_stalker_iterator_handle_thumb_branch_insn (
    GumStalkerIterator * self, const cs_insn * insn);
static void gum_stalker_iterator_handle_thumb_it_insn (
    GumStalkerIterator * self);

static void gum_stalker_save_cpsr (GumCpuContext * cpu_context,
    GumExecCtx * ctx);
static void gum_stalker_restore_cpsr (GumCpuContext * cpu_context,
    GumExecCtx * ctx);

static void gum_stalker_get_target_address (const cs_insn * insn,
    gboolean thumb, GumBranchTarget * target, guint16 * mask);
static void gum_stalker_get_writeback (const cs_insn * insn,
    GumWriteback * writeback);

static void gum_stalker_invoke_callout (GumCalloutEntry * entry,
    GumCpuContext * cpu_context);

static void gum_exec_ctx_write_arm_prolog (GumExecCtx * ctx, GumArmWriter * cw);
static void gum_exec_ctx_write_arm_epilog (GumExecCtx * ctx, GumArmWriter * cw);

static void gum_exec_ctx_ensure_inline_helpers_reachable (GumExecCtx * ctx);
static void gum_exec_ctx_write_arm_invalidator (GumExecCtx * ctx,
    GumArmWriter * cw);
static void gum_exec_ctx_write_thumb_invalidator (GumExecCtx * ctx,
    GumThumbWriter * cw);
static void gum_exec_ctx_ensure_arm_helper_reachable (GumExecCtx * ctx,
    gpointer * helper_ptr, GumArmHelperWriteFunc write);
static void gum_exec_ctx_ensure_thumb_helper_reachable (GumExecCtx * ctx,
    gpointer * helper_ptr, GumThumbHelperWriteFunc write);
static gboolean gum_exec_ctx_is_arm_helper_reachable (GumExecCtx * ctx,
    gpointer * helper_ptr);
static gboolean gum_exec_ctx_is_thumb_helper_reachable (GumExecCtx * ctx,
    gpointer * helper_ptr);

static void gum_exec_ctx_arm_load_real_register_into (GumExecCtx * ctx,
    arm_reg target_register, arm_reg source_register, GumGeneratorContext * gc);
static void gum_exec_ctx_thumb_load_real_register_into (GumExecCtx * ctx,
    arm_reg target_register, arm_reg source_register, GumGeneratorContext * gc);

static GumExecBlock * gum_exec_block_new (GumExecCtx * ctx);
static void gum_exec_block_clear (GumExecBlock * block);
static void gum_exec_block_commit (GumExecBlock * block);
static void gum_exec_block_invalidate (GumExecBlock * block);
static gpointer gum_exec_block_encode_instruction_pointer (
    const GumExecBlock * block, gpointer ptr);
static gpointer gum_exec_block_get_snapshot_start (GumExecBlock * block);
static GumCalloutEntry * gum_exec_block_get_last_callout_entry (
    const GumExecBlock * block);
static void gum_exec_block_set_last_callout_entry (GumExecBlock * block,
    GumCalloutEntry * entry);

static void gum_exec_ctx_backpatch_arm_branch_to_current (GumExecBlock * block,
    GumExecBlock * from, gpointer from_insn, gsize code_offset,
    GumPrologState opened_prolog);
static void gum_exec_ctx_backpatch_thumb_branch_to_current (
    GumExecBlock * block, GumExecBlock * from, gpointer from_insn,
    gsize code_offset, GumPrologState opened_prolog);

static void gum_exec_block_virtualize_arm_branch_insn (GumExecBlock * block,
    const GumBranchTarget * target, arm_cc cc, GumWriteback * writeback,
    GumGeneratorContext * gc);
static void gum_exec_block_virtualize_thumb_branch_insn (GumExecBlock * block,
    const GumBranchTarget * target, arm_cc cc, arm_reg cc_reg,
    GumWriteback * writeback, GumGeneratorContext * gc);
static void gum_exec_block_virtualize_arm_call_insn (GumExecBlock * block,
    const GumBranchTarget * target, arm_cc cc, GumGeneratorContext * gc);
static void gum_exec_block_virtualize_thumb_call_insn (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_virtualize_arm_ret_insn (GumExecBlock * block,
    const GumBranchTarget * target, arm_cc cc, gboolean pop, guint16 mask,
    GumGeneratorContext * gc);
static void gum_exec_block_virtualize_thumb_ret_insn (GumExecBlock * block,
    const GumBranchTarget * target, gboolean pop, guint16 mask,
    GumGeneratorContext * gc);
static void gum_exec_block_virtualize_arm_svc_insn (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_virtualize_thumb_svc_insn (GumExecBlock * block,
    GumGeneratorContext * gc);

static void gum_exec_block_write_arm_handle_kuser_helper (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_handle_kuser_helper (
    GumExecBlock * block, const GumBranchTarget * target,
    GumGeneratorContext * gc);
static void gum_exec_block_write_arm_call_switch_block (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_call_switch_block (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);

static void gum_exec_block_dont_virtualize_arm_insn (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_dont_virtualize_thumb_insn (GumExecBlock * block,
    GumGeneratorContext * gc);

static void gum_exec_block_write_arm_handle_excluded (GumExecBlock * block,
    const GumBranchTarget * target, gboolean call, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_handle_excluded (GumExecBlock * block,
    const GumBranchTarget * target, gboolean call, GumGeneratorContext * gc);
static void gum_exec_block_write_arm_handle_not_taken (GumExecBlock * block,
    const GumBranchTarget * target, arm_cc cc, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_handle_not_taken (GumExecBlock * block,
    const GumBranchTarget * target, arm_cc cc, arm_reg cc_reg,
    GumGeneratorContext * gc);
static void gum_exec_block_write_arm_handle_continue (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_handle_continue (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_arm_handle_writeback (GumExecBlock * block,
    const GumWriteback * writeback, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_handle_writeback (GumExecBlock * block,
    const GumWriteback * writeback, GumGeneratorContext * gc);
static void gum_exec_block_write_arm_exec_generated_code (GumArmWriter * cw,
    GumExecCtx * ctx);
static void gum_exec_block_write_thumb_exec_generated_code (GumThumbWriter * cw,
    GumExecCtx * ctx);

static void gum_exec_block_write_arm_call_event_code (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_call_event_code (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_arm_ret_event_code (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_ret_event_code (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_arm_exec_event_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_exec_event_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_arm_block_event_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_block_event_code (GumExecBlock * block,
    GumGeneratorContext * gc);

static void gum_exec_block_write_arm_push_stack_frame (GumExecBlock * block,
    gpointer ret_real_address, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_push_stack_frame (GumExecBlock * block,
    gpointer ret_real_address, GumGeneratorContext * gc);
static void gum_exec_block_push_stack_frame (GumExecCtx * ctx,
    gpointer ret_real_address);
static void gum_exec_block_write_arm_pop_stack_frame (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_pop_stack_frame (GumExecBlock * block,
    const GumBranchTarget * target, GumGeneratorContext * gc);
static void gum_exec_block_pop_stack_frame (GumExecCtx * ctx,
    gpointer ret_real_address);

static void gum_exec_block_maybe_write_call_probe_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_arm_call_probe_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_write_thumb_call_probe_code (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_invoke_call_probes (GumExecBlock * block,
    GumCpuContext * cpu_context);

static gpointer gum_exec_block_write_arm_inline_data (GumArmWriter * cw,
    gconstpointer data, gsize size, GumAddress * address);
static gpointer gum_exec_block_write_thumb_inline_data (GumThumbWriter * cw,
    gconstpointer data, gsize size, GumAddress * address);

static void gum_exec_block_arm_open_prolog (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_thumb_open_prolog (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_arm_close_prolog (GumExecBlock * block,
    GumGeneratorContext * gc);
static void gum_exec_block_thumb_close_prolog (GumExecBlock * block,
    GumGeneratorContext * gc);

static void gum_exec_block_maybe_inherit_exclusive_access_state (
    GumExecBlock * block, GumExecBlock * reference);
static void gum_exec_block_propagate_exclusive_access_state (
    GumExecBlock * block);

static GumCodeSlab * gum_code_slab_new (GumExecCtx * ctx);
static void gum_code_slab_free (GumCodeSlab * code_slab);
static void gum_code_slab_init (GumCodeSlab * code_slab, gsize slab_size,
    gsize page_size);

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
static gpointer gum_slab_align_cursor (GumSlab * self, guint alignment);
static gpointer gum_slab_reserve (GumSlab * self, gsize size);
static gpointer gum_slab_try_reserve (GumSlab * self, gsize size);

static gpointer gum_find_thread_exit_implementation (void);

static gpointer gum_strip_thumb_bit (gpointer address);
static gboolean gum_is_thumb (gconstpointer address);
static gboolean gum_is_kuser_helper (gconstpointer address);
static gboolean gum_is_exclusive_load_insn (const cs_insn * insn);
static gboolean gum_is_exclusive_store_insn (const cs_insn * insn);

static guint gum_count_bits_set (guint16 value);
static guint gum_count_trailing_zeros (guint16 value);

G_DEFINE_TYPE_WITH_CODE (GumStalker, gum_stalker, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GUM_TYPE_UNWIND_PC_TRANSLATOR,
                             gum_stalker_unwind_translator_iface_init))

static GPrivate gum_stalker_exec_ctx_private;

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

  self->frames_size = page_size;
  g_assert (self->frames_size % sizeof (GumExecFrame) == 0);
  self->thunks_size = page_size;
  self->code_slab_size_initial =
      GUM_ALIGN_SIZE (GUM_CODE_SLAB_SIZE_INITIAL, page_size);
  self->data_slab_size_initial =
      GUM_ALIGN_SIZE (GUM_DATA_SLAB_SIZE_INITIAL, page_size);
  self->code_slab_size_dynamic =
      GUM_ALIGN_SIZE (GUM_CODE_SLAB_SIZE_DYNAMIC, page_size);
  self->data_slab_size_dynamic =
      GUM_ALIGN_SIZE (GUM_DATA_SLAB_SIZE_DYNAMIC, page_size);
  self->scratch_slab_size = GUM_ALIGN_SIZE (GUM_SCRATCH_SLAB_SIZE, page_size);
  self->ctx_header_size = GUM_ALIGN_SIZE (sizeof (GumExecCtx), page_size);
  self->ctx_size =
      self->ctx_header_size +
      self->frames_size +
      self->thunks_size +
      self->code_slab_size_initial +
      self->data_slab_size_initial +
      self->scratch_slab_size +
      0;

  self->frames_offset = self->ctx_header_size;
  self->thunks_offset = self->frames_offset + self->frames_size;
  self->code_slab_offset = self->thunks_offset + self->thunks_size;
  self->data_slab_offset =
      self->code_slab_offset + self->code_slab_size_initial;
  self->scratch_slab_offset =
      self->data_slab_offset + self->data_slab_size_initial;

  self->page_size = page_size;
  self->cpu_features = gum_query_cpu_features ();
  self->is_rwx_supported = gum_query_rwx_support () != GUM_RWX_NONE;

  g_mutex_init (&self->mutex);
  self->contexts = NULL;

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
gum_stalker_is_call_excluding (GumExecCtx * ctx,
                               gconstpointer address)
{
  GArray * exclusions = ctx->stalker->exclusions;
  guint i;

  if (ctx->activation_target != NULL)
    return FALSE;

  if (gum_is_kuser_helper (address))
    return TRUE;

  for (i = 0; i != exclusions->len; i++)
  {
    GumMemoryRange * r = &g_array_index (exclusions, GumMemoryRange, i);

    if (GUM_MEMORY_RANGE_INCLUDES (r, GUM_ADDRESS (address)))
      return TRUE;
  }

  return FALSE;
}

static gboolean
gum_stalker_is_branch_excluding (GumExecCtx * ctx,
                                 gconstpointer address)
{
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

  return code_address;
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
  guint32 pc;
  GumArmWriter * cw;

  ctx = gum_stalker_create_exec_ctx (self, thread_id,
      infect_context->transformer, infect_context->sink);

  if ((cpu_context->cpsr & GUM_PSR_T_BIT) == 0)
    pc = cpu_context->pc;
  else
    pc = cpu_context->pc + 1;

  ctx->current_block = gum_exec_ctx_obtain_block_for (ctx,
      GSIZE_TO_POINTER (pc), &ctx->resume_at);

  if (gum_exec_ctx_maybe_unfollow (ctx, NULL))
  {
    gum_stalker_destroy_exec_ctx (self, ctx);
    return;
  }

  cpu_context->cpsr &= ~GUM_PSR_T_BIT;
  cpu_context->pc = GPOINTER_TO_SIZE (ctx->infect_thunk);

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (self, ctx->thunks, self->thunks_size);
  cw = &ctx->arm_writer;
  gum_arm_writer_reset (cw, ctx->infect_thunk);

  gum_exec_ctx_write_arm_prolog (ctx, cw);
  gum_arm_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (g_private_set), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (&gum_stalker_exec_ctx_private),
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx));
  gum_exec_ctx_write_arm_epilog (ctx, cw);

  gum_exec_block_write_arm_exec_generated_code (cw, ctx);

  gum_arm_writer_flush (cw);
  gum_stalker_freeze (self, cw->base, gum_arm_writer_offset (cw));

  gum_spinlock_release (&ctx->code_lock);

  gum_event_sink_start (ctx->sink);
  ctx->sink_started = TRUE;
}

static void
gum_stalker_disinfect (GumThreadId thread_id,
                       GumCpuContext * cpu_context,
                       gpointer user_data)
{
  GumDisinfectContext * disinfect_context = user_data;
  GumExecCtx * ctx = disinfect_context->exec_ctx;
  gboolean infection_not_active_yet;

  infection_not_active_yet =
      cpu_context->pc == GPOINTER_TO_SIZE (ctx->infect_thunk);
  if (infection_not_active_yet)
  {
    GumExecBlock * block = ctx->current_block;

    cpu_context->pc = GPOINTER_TO_SIZE (
        gum_exec_block_encode_instruction_pointer (block, block->real_start));

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
  ctx->activation_target = target;

  if (!gum_exec_ctx_contains (ctx, ret_addr))
  {
    gpointer code_address;

    ctx->current_block =
        gum_exec_ctx_obtain_block_for (ctx, ret_addr, &code_address);

    if (gum_exec_ctx_maybe_unfollow (ctx, ret_addr))
      return ret_addr;

    return code_address;
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
    case GUM_BACKPATCH_ARM:
    {
      gum_exec_ctx_backpatch_arm_branch_to_current (block_to, block_from,
          from_insn, backpatch->code_offset, backpatch->opened_prolog);
      break;
    }
    case GUM_BACKPATCH_THUMB:
    {
      gum_exec_ctx_backpatch_thumb_branch_to_current (block_to, block_from,
          from_insn, backpatch->code_offset, backpatch->opened_prolog);
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
  const guint8 * pc = GSIZE_TO_POINTER (cpu_context->pc);

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
  guint32 pc;
  GumArmWriter * cw;
  GumAddress cpu_context_copy, infect_body;

  ctx = gum_stalker_create_exec_ctx (self, thread_id, NULL, NULL);

  if ((cpu_context->cpsr & GUM_PSR_T_BIT) == 0)
    pc = cpu_context->pc;
  else
    pc = cpu_context->pc + 1;

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (self, ctx->thunks, self->thunks_size);
  cw = &ctx->arm_writer;
  gum_arm_writer_reset (cw, ctx->infect_thunk);

  cpu_context_copy = GUM_ADDRESS (gum_arm_writer_cur (cw));
  gum_arm_writer_put_bytes (cw, (guint8 *) cpu_context, sizeof (GumCpuContext));

  infect_body = GUM_ADDRESS (gum_arm_writer_cur (cw));

  gum_exec_ctx_write_arm_prolog (ctx, cw);

  gum_arm_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (func), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (cpu_context_copy),
      GUM_ARG_ADDRESS, GUM_ADDRESS (data));

  gum_arm_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_unfollow), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (pc));

  gum_exec_ctx_write_arm_epilog (ctx, cw);

  gum_arm_writer_put_push_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);
  gum_arm_writer_put_ldr_reg_address (cw, ARM_REG_R0, pc);
  gum_arm_writer_put_str_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_SP, 4);
  gum_arm_writer_put_pop_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);

  gum_arm_writer_flush (cw);
  gum_stalker_freeze (self, cw->base, gum_arm_writer_offset (cw));

  gum_spinlock_release (&ctx->code_lock);

  cpu_context->cpsr &= ~GUM_PSR_T_BIT;
  cpu_context->pc = infect_body;
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

static void
gum_stalker_thaw (GumStalker * self,
                  gpointer code,
                  gsize size)
{
  if (!self->is_rwx_supported)
    gum_mprotect (code, size, GUM_PAGE_RW);
}

static void
gum_stalker_freeze (GumStalker * self,
                    gpointer code,
                    gsize size)
{
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

  resume_ip = gum_exec_ctx_switch_block (ctx, NULL,
      GSIZE_TO_POINTER (real_resume_ip), NULL);
  _gum_unwind_broker_set_ip (unwind_context, GPOINTER_TO_SIZE (resume_ip));

  ctx->pending_calls--;

  return TRUE;
#else
  return FALSE;
#endif
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
  GumDataSlab * data_slab;

  base = gum_memory_allocate (NULL, stalker->ctx_size, stalker->page_size,
      stalker->is_rwx_supported ? GUM_PAGE_RWX : GUM_PAGE_RW);

  ctx = (GumExecCtx *) base;

  ctx->state = GUM_EXEC_CTX_ACTIVE;

  ctx->stalker = g_object_ref (stalker);
  ctx->thread_id = thread_id;

  gum_arm_writer_init (&ctx->arm_writer, NULL);
  ctx->arm_writer.cpu_features = gum_query_cpu_features ();
  gum_arm_relocator_init (&ctx->arm_relocator, NULL, &ctx->arm_writer);

  gum_thumb_writer_init (&ctx->thumb_writer, NULL);
  gum_thumb_relocator_init (&ctx->thumb_relocator, NULL, &ctx->thumb_writer);
  gum_thumb_relocator_set_it_branch_type (&ctx->thumb_relocator,
      GUM_IT_BRANCH_LONG);

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

  ctx->frames = (GumExecFrame *) (base + stalker->frames_offset);
  ctx->first_frame =
      ctx->frames + (stalker->frames_size / sizeof (GumExecFrame)) - 1;
  ctx->current_frame = ctx->first_frame;

  ctx->thunks = base + stalker->thunks_offset;
  ctx->infect_thunk = ctx->thunks;

  gum_spinlock_init (&ctx->code_lock);

  code_slab = (GumCodeSlab *) (base + stalker->code_slab_offset);
  gum_code_slab_init (code_slab, stalker->code_slab_size_initial,
      stalker->page_size);
  gum_exec_ctx_add_code_slab (ctx, code_slab);

  data_slab = (GumDataSlab *) (base + stalker->data_slab_offset);
  gum_data_slab_init (data_slab, stalker->data_slab_size_initial);
  gum_exec_ctx_add_data_slab (ctx, data_slab);

  ctx->scratch_slab = (GumCodeSlab *) (base + stalker->scratch_slab_offset);
  gum_scratch_slab_init (ctx->scratch_slab, stalker->scratch_slab_size);

  ctx->mappings = gum_metal_hash_table_new (NULL, NULL);

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  ctx->excluded_calls = gum_metal_hash_table_new (NULL, NULL);
#endif

  gum_exec_ctx_ensure_inline_helpers_reachable (ctx);

  return ctx;
}

static void
gum_exec_ctx_free (GumExecCtx * ctx)
{
  GumStalker * stalker = ctx->stalker;
  GumDataSlab * data_slab;
  GumCodeSlab * code_slab;

  gum_metal_hash_table_unref (ctx->mappings);

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  gum_metal_hash_table_unref (ctx->excluded_calls);
#endif

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

  g_object_unref (ctx->sink);
  g_object_unref (ctx->transformer);
  g_clear_object (&ctx->observer);

  gum_thumb_relocator_clear (&ctx->thumb_relocator);
  gum_thumb_writer_clear (&ctx->thumb_writer);

  gum_arm_relocator_clear (&ctx->arm_relocator);
  gum_arm_writer_clear (&ctx->arm_writer);

  g_object_unref (stalker);

  gum_memory_free (ctx, stalker->ctx_size);
}

static void
gum_exec_ctx_dispose (GumExecCtx * ctx)
{
  GumStalker * stalker = ctx->stalker;
  GumSlab * slab;

  for (slab = &ctx->code_slab->slab; slab != NULL; slab = slab->next)
  {
    gum_stalker_thaw (stalker, gum_slab_start (slab), slab->offset);
  }

  for (slab = &ctx->data_slab->slab; slab != NULL; slab = slab->next)
  {
    GumExecBlock * blocks;
    guint num_blocks;
    guint i;

    blocks = gum_slab_start (slab);
    num_blocks = slab->offset / sizeof (GumExecBlock);

    for (i = 0; i != num_blocks; i++)
    {
      GumExecBlock * block = &blocks[i];

      gum_exec_block_clear (block);
    }
  }
}

static GumCodeSlab *
gum_exec_ctx_add_code_slab (GumExecCtx * ctx,
                            GumCodeSlab * code_slab)
{
  code_slab->slab.next = &ctx->code_slab->slab;
  ctx->code_slab = code_slab;
  return code_slab;
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
  GumSlab * cur = &ctx->code_slab->slab;

  do {
    if ((const guint8 *) address >= cur->data &&
        (const guint8 *) address < (guint8 *) gum_slab_cursor (cur))
    {
      return TRUE;
    }

    cur = cur->next;
  } while (cur != NULL);

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
  else if (start_address == NULL || start_address == gum_thread_exit_address)
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
                                         gpointer trampoline_return_address)
{
  GumExecBlock ** block_ptr, * block;
  gpointer start_address;

  block_ptr = gum_strip_thumb_bit (trampoline_return_address);
  block = *block_ptr;

  start_address =
      gum_exec_block_encode_instruction_pointer (block, block->real_start);

  if (gum_exec_ctx_maybe_unfollow (ctx, start_address))
    return;

  gum_exec_ctx_recompile_block (ctx, block);

  ctx->current_block = block;
  ctx->resume_at =
      gum_exec_block_encode_instruction_pointer (block, block->code_start);

  if (start_address == ctx->activation_target)
  {
    ctx->activation_target = NULL;
    ctx->current_block->flags |= GUM_EXEC_BLOCK_ACTIVATION_TARGET;
  }

  gum_exec_ctx_maybe_unfollow (ctx, start_address);
}

static void
gum_exec_ctx_begin_call (GumExecCtx * ctx,
                         gpointer ret_addr)
{
  ctx->pending_return_location = ret_addr;
  ctx->pending_calls++;
}

static void
gum_exec_ctx_end_call (GumExecCtx * ctx)
{
  ctx->pending_calls--;
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
    gpointer aligned_address;

    block = gum_exec_block_new (ctx);
    if (gum_is_thumb (real_address))
    {
      block->flags |= GUM_EXEC_BLOCK_THUMB;
      aligned_address = gum_strip_thumb_bit (real_address);
    }
    else
    {
      aligned_address = real_address;
    }
    block->real_start = aligned_address;
    gum_exec_block_maybe_inherit_exclusive_access_state (block, block->next);
    gum_exec_ctx_compile_block (ctx, block, aligned_address, block->code_start,
        GUM_ADDRESS (block->code_start), &block->real_size, &block->code_size);
    gum_exec_block_commit (block);
    gum_exec_block_propagate_exclusive_access_state (block);

    gum_metal_hash_table_insert (ctx->mappings, real_address, block);

    gum_spinlock_release (&ctx->code_lock);

    gum_exec_ctx_maybe_emit_compile_event (ctx, block);
  }

  *code_address =
      gum_exec_block_encode_instruction_pointer (block, block->code_start);

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
  guint input_size, output_size;
  gsize new_snapshot_size, new_block_size;

  gum_spinlock_acquire (&ctx->code_lock);

  gum_stalker_thaw (stalker, internal_code, block->capacity);

  if (block->storage_block != NULL)
    gum_exec_block_clear (block->storage_block);
  gum_exec_block_clear (block);

  slab = block->code_slab;
  block->code_slab = ctx->scratch_slab;
  scratch_base = ctx->scratch_slab->slab.data;
  ctx->scratch_slab->arm_invalidator = slab->arm_invalidator;
  ctx->scratch_slab->thumb_invalidator = slab->thumb_invalidator;

  gum_exec_ctx_compile_block (ctx, block, block->real_start, scratch_base,
      GUM_ADDRESS (internal_code), &input_size, &output_size);

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
    GumAddress external_code_address;

    storage_block = gum_exec_block_new (ctx);
    storage_block->flags = block->flags & 2; /* THUMB flag */
    storage_block->real_start = block->real_start;
    gum_exec_ctx_compile_block (ctx, block, block->real_start,
        storage_block->code_start, GUM_ADDRESS (storage_block->code_start),
        &storage_block->real_size, &storage_block->code_size);
    gum_exec_block_commit (storage_block);

    block->storage_block = storage_block;

    gum_stalker_thaw (stalker, internal_code, block->capacity);

    external_code_address = GUM_ADDRESS (storage_block->code_start);

    if ((block->flags & GUM_EXEC_BLOCK_THUMB) != 0)
    {
      GumThumbWriter * cw = &ctx->thumb_writer;

      gum_thumb_writer_reset (cw, internal_code);
      gum_thumb_writer_put_branch_address (cw, external_code_address);
      gum_thumb_writer_flush (cw);
    }
    else
    {
      GumArmWriter * cw = &ctx->arm_writer;

      gum_arm_writer_reset (cw, internal_code);
      gum_arm_writer_put_branch_address (cw, external_code_address);
      gum_arm_writer_flush (cw);
    }

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
                            guint * output_size)
{
  if ((block->flags & GUM_EXEC_BLOCK_THUMB) != 0)
  {
    gum_exec_ctx_compile_thumb_block (ctx, block, input_code, output_code,
        output_pc, input_size, output_size);
  }
  else
  {
    gum_exec_ctx_compile_arm_block (ctx, block, input_code, output_code,
        output_pc, input_size, output_size);
  }
}

static void
gum_exec_ctx_compile_arm_block (GumExecCtx * ctx,
                                GumExecBlock * block,
                                gconstpointer input_code,
                                gpointer output_code,
                                GumAddress output_pc,
                                guint * input_size,
                                guint * output_size)
{
  GumArmWriter * cw = &ctx->arm_writer;
  GumArmRelocator * rl = &ctx->arm_relocator;
  GumGeneratorContext gc;
  GumStalkerIterator iterator;
  GumStalkerOutput output;
  gboolean all_labels_resolved;

  gum_arm_writer_reset (cw, output_code);
  cw->pc = output_pc;
  gum_arm_relocator_reset (rl, input_code, cw);

  gum_ensure_code_readable (input_code, ctx->stalker->page_size);

  gc.instruction = NULL;
  gc.is_thumb = FALSE;
  gc.arm_relocator = rl;
  gc.arm_writer = cw;
  gc.continuation_real_address = NULL;

  iterator.exec_context = ctx;
  iterator.exec_block = block;
  iterator.generator_context = &gc;

  iterator.instruction.ci = NULL;
  iterator.instruction.start = NULL;
  iterator.instruction.end = NULL;

  output.writer.arm = cw;
  output.encoding = GUM_INSTRUCTION_DEFAULT;

  gum_exec_block_maybe_write_call_probe_code (block, &gc);

  ctx->pending_calls++;

  ctx->transform_block_impl (ctx->transformer, &iterator, &output);

  ctx->pending_calls--;

  if (gc.continuation_real_address != NULL)
  {
    gum_exec_block_arm_open_prolog (block, &gc);

    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
        GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc.continuation_real_address),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc.instruction->start));

    gum_exec_block_arm_close_prolog (block, &gc);

    gum_exec_block_write_arm_exec_generated_code (cw, ctx);
  }

  gum_arm_writer_put_breakpoint (cw);

  all_labels_resolved = gum_arm_writer_flush (cw);
  if (!all_labels_resolved)
    gum_panic ("Failed to resolve labels");

  *input_size = rl->input_cur - rl->input_start;
  *output_size = gum_arm_writer_offset (cw);
}

static void
gum_exec_ctx_compile_thumb_block (GumExecCtx * ctx,
                                  GumExecBlock * block,
                                  gconstpointer input_code,
                                  gpointer output_code,
                                  GumAddress output_pc,
                                  guint * input_size,
                                  guint * output_size)
{
  GumThumbWriter * cw = &ctx->thumb_writer;
  GumThumbRelocator * rl = &ctx->thumb_relocator;
  GumGeneratorContext gc;
  GumStalkerIterator iterator;
  GumStalkerOutput output;
  gboolean all_labels_resolved;

  gum_thumb_writer_reset (cw, output_code);
  cw->pc = output_pc;
  gum_thumb_relocator_reset (rl, input_code, cw);

  gum_ensure_code_readable (input_code, ctx->stalker->page_size);

  gc.instruction = NULL;
  gc.is_thumb = TRUE;
  gc.thumb_relocator = rl;
  gc.thumb_writer = cw;
  gc.continuation_real_address = NULL;

  iterator.exec_context = ctx;
  iterator.exec_block = block;
  iterator.generator_context = &gc;

  iterator.instruction.ci = NULL;
  iterator.instruction.start = NULL;
  iterator.instruction.end = NULL;

  output.writer.thumb = cw;
  output.encoding = GUM_INSTRUCTION_SPECIAL;

  gum_exec_block_maybe_write_call_probe_code (block, &gc);

  ctx->pending_calls++;

  ctx->transform_block_impl (ctx->transformer, &iterator, &output);

  ctx->pending_calls--;

  if (gc.continuation_real_address != NULL)
  {
    gum_exec_block_thumb_open_prolog (block, &gc);

    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
        GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc.continuation_real_address) + 1,
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc.instruction->start) + 1);

    gum_exec_block_thumb_close_prolog (block, &gc);

    gum_exec_block_write_thumb_exec_generated_code (cw, ctx);
  }

  gum_thumb_writer_put_breakpoint (cw);

  all_labels_resolved = gum_thumb_writer_flush (cw);
  if (!all_labels_resolved)
    gum_panic ("Failed to resolve labels");

  *input_size = rl->input_cur - rl->input_start;
  *output_size = gum_thumb_writer_offset (cw);
}

static void
gum_exec_ctx_maybe_emit_compile_event (GumExecCtx * ctx,
                                       GumExecBlock * block)
{
  if ((ctx->sink_mask & GUM_COMPILE) != 0)
  {
    GumEvent ev;

    ev.type = GUM_COMPILE;
    ev.compile.start = gum_exec_block_encode_instruction_pointer (block,
        block->real_start);
    ev.compile.end = gum_exec_block_encode_instruction_pointer (block,
        block->real_start + block->real_size);

    ctx->sink_process_impl (ctx->sink, &ev, NULL);
  }
}

gboolean
gum_stalker_iterator_next (GumStalkerIterator * self,
                           const cs_insn ** insn)
{
  GumGeneratorContext * gc = self->generator_context;

  if (gc->is_thumb)
    return gum_stalker_iterator_thumb_next (self, insn);
  else
    return gum_stalker_iterator_arm_next (self, insn);
}

static gboolean
gum_stalker_iterator_arm_next (GumStalkerIterator * self,
                               const cs_insn ** insn)
{
  GumGeneratorContext * gc = self->generator_context;
  GumArmRelocator * rl = gc->arm_relocator;
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
      gum_arm_relocator_skip_one (rl);
    }

    if (gum_stalker_iterator_is_out_of_space (self))
    {
      gc->continuation_real_address = instruction->end;
      return FALSE;
    }

    if (!skip_implicitly_requested && gum_arm_relocator_eob (rl))
      return FALSE;
  }

  instruction = &self->instruction;

  n_read = gum_arm_relocator_read_one (rl, &instruction->ci);
  if (n_read == 0)
    return FALSE;

  instruction->start = GSIZE_TO_POINTER (instruction->ci->address);
  instruction->end = (guint8 *) rl->input_cur;

  self->generator_context->instruction = instruction;

  if (is_first_instruction && (self->exec_context->sink_mask & GUM_BLOCK) != 0)
  {
    GumExecBlock * block = self->exec_block;

    gum_exec_block_arm_open_prolog (block, gc);
    gum_exec_block_write_arm_block_event_code (block, gc);
    gum_exec_block_arm_close_prolog (block, gc);
  }

  if (insn != NULL)
    *insn = instruction->ci;

  return TRUE;
}

static gboolean
gum_stalker_iterator_thumb_next (GumStalkerIterator * self,
                                 const cs_insn ** insn)
{
  GumGeneratorContext * gc = self->generator_context;
  GumThumbRelocator * rl = gc->thumb_relocator;
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
      gum_thumb_relocator_skip_one (rl);
    }

    if (gum_stalker_iterator_is_out_of_space (self))
    {
      gc->continuation_real_address = instruction->end;
      return FALSE;
    }

    if (!skip_implicitly_requested && gum_thumb_relocator_eob (rl))
      return FALSE;
  }

  instruction = &self->instruction;

  n_read = gum_thumb_relocator_read_one (rl, &instruction->ci);
  if (n_read == 0)
    return FALSE;

  instruction->start = GSIZE_TO_POINTER (instruction->ci->address);
  instruction->end = (guint8 *) rl->input_cur;

  self->generator_context->instruction = instruction;

  if (is_first_instruction && (self->exec_context->sink_mask & GUM_BLOCK) != 0)
  {
    GumExecBlock * block = self->exec_block;

    gum_exec_block_thumb_open_prolog (block, gc);
    gum_exec_block_write_thumb_block_event_code (block, gc);
    gum_exec_block_thumb_close_prolog (block, gc);
  }

  if (insn != NULL)
    *insn = instruction->ci;

  return TRUE;
}

static gboolean
gum_stalker_iterator_is_out_of_space (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  GumSlab * slab = &block->code_slab->slab;
  guint8 * cursor;
  gsize capacity, snapshot_size, pending_literals_size;

  cursor = gc->is_thumb
      ? gum_thumb_writer_cur (gc->thumb_writer)
      : gum_arm_writer_cur (gc->arm_writer);

  capacity = (guint8 *) gum_slab_end (slab) - cursor;

  snapshot_size = gum_stalker_snapshot_space_needed_for (
      self->exec_context->stalker,
      gc->instruction->end - block->real_start);

  if (gc->is_thumb)
  {
    GumThumbWriter * tw = gc->thumb_writer;
    gsize alignment_padding = sizeof (guint16);

    pending_literals_size = (tw->literal_refs.data != NULL)
        ? (tw->literal_refs.length * sizeof (guint32)) + alignment_padding
        : 0;
  }
  else
  {
    GumArmWriter * aw = gc->arm_writer;

    pending_literals_size = (aw->literal_refs.data != NULL)
        ? aw->literal_refs.length * sizeof (guint32)
        : 0;
  }

  return capacity < GUM_EXEC_BLOCK_MIN_CAPACITY + snapshot_size +
      pending_literals_size;
}

void
gum_stalker_iterator_keep (GumStalkerIterator * self)
{
  GumGeneratorContext * gc = self->generator_context;
  const cs_insn * insn = gc->instruction->ci;

  if (gum_is_exclusive_load_insn (insn))
    self->exec_block->flags |= GUM_EXEC_BLOCK_HAS_EXCLUSIVE_LOAD;
  else if (gum_is_exclusive_store_insn (insn))
    self->exec_block->flags |= GUM_EXEC_BLOCK_HAS_EXCLUSIVE_STORE;

  if (gc->is_thumb)
    gum_stalker_iterator_thumb_keep (self);
  else
    gum_stalker_iterator_arm_keep (self);
}

GumMemoryAccess
gum_stalker_iterator_get_memory_access (GumStalkerIterator * self)
{
  return ((self->exec_block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) != 0)
      ? GUM_MEMORY_ACCESS_EXCLUSIVE
      : GUM_MEMORY_ACCESS_OPEN;
}

static void
gum_stalker_iterator_arm_keep (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  const cs_insn * insn = gc->instruction->ci;

  if (gum_arm_relocator_eob (gc->arm_relocator))
  {
    GumBranchTarget target;
    guint16 mask;
    cs_arm * arm = &insn->detail->arm;
    GumWriteback writeback = { .target = ARM_REG_INVALID };

    mask = 0;

    gum_stalker_get_target_address (insn, FALSE, &target, &mask);

    switch (insn->id)
    {
      case ARM_INS_LDR:
        gum_stalker_get_writeback (insn, &writeback);
        /* Deliberate fall-through */
      case ARM_INS_SUB:
      case ARM_INS_ADD:
      case ARM_INS_B:
      case ARM_INS_BX:
        gum_exec_block_virtualize_arm_branch_insn (block, &target, arm->cc,
            &writeback, gc);
        break;
      case ARM_INS_BL:
      case ARM_INS_BLX:
        gum_exec_block_virtualize_arm_call_insn (block, &target, arm->cc, gc);
        break;
      case ARM_INS_MOV:
        gum_exec_block_virtualize_arm_ret_insn (block, &target, arm->cc, FALSE,
            0, gc);
        break;
      case ARM_INS_POP:
      case ARM_INS_LDM:
        gum_exec_block_virtualize_arm_ret_insn (block, &target, arm->cc, TRUE,
            mask, gc);
        break;
      case ARM_INS_SMC:
      case ARM_INS_HVC:
        gum_panic ("not implemented");
        break;
      default:
        g_assert_not_reached ();
        break;
    }
  }
  else if (insn->id == ARM_INS_SVC)
  {
    gum_exec_block_virtualize_arm_svc_insn (block, gc);
  }
  else
  {
    gum_exec_block_dont_virtualize_arm_insn (block, gc);
  }
}

static void
gum_stalker_iterator_thumb_keep (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  const cs_insn * insn = gc->instruction->ci;

  if (gum_thumb_relocator_eob (gc->thumb_relocator))
    gum_stalker_iterator_handle_thumb_branch_insn (self, insn);
  else if (insn->id == ARM_INS_SVC)
    gum_exec_block_virtualize_thumb_svc_insn (block, gc);
  else
    gum_exec_block_dont_virtualize_thumb_insn (block, gc);
}

static void
gum_stalker_iterator_handle_thumb_branch_insn (GumStalkerIterator * self,
                                               const cs_insn * insn)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  cs_arm * arm = &insn->detail->arm;
  GumBranchTarget target;
  guint16 mask;
  GumWriteback writeback = { .target = ARM_REG_INVALID };

  switch (insn->id)
  {
    case ARM_INS_LDR:
      gum_stalker_get_writeback (insn, &writeback);
      /* Deliberate fall-through */
    case ARM_INS_B:
    case ARM_INS_BX:
    case ARM_INS_TBB:
    case ARM_INS_TBH:
      gum_stalker_get_target_address (insn, TRUE, &target, &mask);
      gum_exec_block_virtualize_thumb_branch_insn (block, &target, arm->cc,
          ARM_REG_INVALID, &writeback, gc);
      break;
    case ARM_INS_CBZ:
      g_assert (arm->operands[0].type == ARM_OP_REG);
      gum_stalker_get_target_address (insn, TRUE, &target, &mask);
      gum_exec_block_virtualize_thumb_branch_insn (block, &target, ARM_CC_EQ,
          arm->operands[0].reg, &writeback, gc);
      break;
    case ARM_INS_CBNZ:
      g_assert (arm->operands[0].type == ARM_OP_REG);
      gum_stalker_get_target_address (insn, TRUE, &target, &mask);
      gum_exec_block_virtualize_thumb_branch_insn (block, &target, ARM_CC_NE,
          arm->operands[0].reg, &writeback, gc);
      break;
    case ARM_INS_BL:
    case ARM_INS_BLX:
      gum_stalker_get_target_address (insn, TRUE, &target, &mask);
      gum_exec_block_virtualize_thumb_call_insn (block, &target, gc);
      break;
    case ARM_INS_MOV:
      gum_stalker_get_target_address (insn, TRUE, &target, &mask);
      gum_exec_block_virtualize_thumb_ret_insn (block, &target, FALSE, 0, gc);
      gum_thumb_relocator_skip_one (gc->thumb_relocator);
      break;
    case ARM_INS_POP:
    case ARM_INS_LDM:
      gum_stalker_get_target_address (insn, TRUE, &target, &mask);
      gum_exec_block_virtualize_thumb_ret_insn (block, &target, TRUE, mask, gc);
      gum_thumb_relocator_skip_one (gc->thumb_relocator);
      break;
    case ARM_INS_SMC:
    case ARM_INS_HVC:
      gum_panic ("Unsupported");
      break;
    case ARM_INS_IT:
      gum_stalker_iterator_handle_thumb_it_insn (self);
      break;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
gum_stalker_iterator_handle_thumb_it_insn (GumStalkerIterator * self)
{
  GumExecBlock * block = self->exec_block;
  GumGeneratorContext * gc = self->generator_context;
  GumThumbRelocator * rl = gc->thumb_relocator;
  const cs_insn * insn;

  /*
   * This function needs only to handle IT blocks which terminate with a branch
   * instruction. Those which contain no branches will not set the EOB condition
   * when read by the relocator and will be handled without the need for
   * virtualization. The block will simply be processed as usual by the
   * relocator.
   */

  /*
   * We emit a single EXEC event for an IT block. Execution of a final branch
   * instruction contained within it can result in additional events being
   * generated. We cannot emit one event for each instruction that is contained
   * within the IT block since they are re-ordered by the relocator. This is
   * necessary since the original IT block must be replaced with branches and
   * labels as individual instructions may need to be replaced by multiple
   * instructions as a result of relocation.
   */
  if ((block->ctx->sink_mask & GUM_EXEC) != 0)
  {
    gum_exec_block_thumb_open_prolog (block, gc);
    gum_exec_block_write_thumb_exec_event_code (block, gc);
    gum_exec_block_thumb_close_prolog (block, gc);
  }

  for (insn = gum_thumb_relocator_peek_next_write_insn (rl);
      insn != NULL;
      insn = gum_thumb_relocator_peek_next_write_insn (rl))
  {
    if (gum_thumb_relocator_is_eob_instruction (insn))
    {
      /*
       * Remove unnecessary conditional execution of the instruction since it is
       * wrapped within a series of branches by the relocator to handle the
       * if/then/else conditional execution.
       */
      insn->detail->arm.cc = ARM_CC_AL;
      gum_stalker_iterator_handle_thumb_branch_insn (self, insn);
    }
    else
    {
      gboolean preserve_flags;

      switch (insn->id)
      {
        case ARM_INS_CMN:
        case ARM_INS_CMP:
        case ARM_INS_TST:
          preserve_flags = FALSE;
          break;
        default:
          preserve_flags = TRUE;
          break;
      }

      /*
       * If the instruction in the IT block is not a branch, then just emit the
       * relocated instruction as normal. We must preserve the flags in the CPSR
       * unless we are executing a CMN, CMP or TST instruction:
       *
       * https://developer.arm.com/documentation/dui0489/c/
       *     arm-and-thumb-instructions/branch-and-control-instructions/it
       */

      if (preserve_flags)
      {
        gum_exec_block_thumb_open_prolog (block, gc);

        gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
            GUM_ADDRESS (gum_stalker_save_cpsr), 2,
            GUM_ARG_REGISTER, ARM_REG_R10,
            GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx));

        gum_exec_block_thumb_close_prolog (block, gc);
      }

      gum_thumb_relocator_write_one (gc->thumb_relocator);

      if (preserve_flags)
      {
        gum_exec_block_thumb_open_prolog (block, gc);

        gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
            GUM_ADDRESS (gum_stalker_restore_cpsr), 2,
            GUM_ARG_REGISTER, ARM_REG_R10,
            GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx));

        gum_exec_block_thumb_close_prolog (block, gc);
      }
    }
  }

  /*
   * Should we reach the end of the IT block (e.g. we did not take the branch)
   * we write code here to continue with the next instruction after the IT block
   * just as we do following a branch or call instruction. (We do this for
   * branches too as we cannot detect tail-calls and we can't be sure the callee
   * won't return). This results in the continuation code being written twice,
   * which is not strictly necessary. However, attempting to optimize this is
   * likely to be quite tricky.
   */
  gum_exec_block_thumb_open_prolog (block, gc);
  gum_exec_block_write_thumb_handle_continue (block, gc);
}

static void
gum_stalker_save_cpsr (GumCpuContext * cpu_context,
                       GumExecCtx * ctx)
{
  ctx->cpsr = cpu_context->cpsr;
}

static void
gum_stalker_restore_cpsr (GumCpuContext * cpu_context,
                          GumExecCtx * ctx)
{
  cpu_context->cpsr = ctx->cpsr;
}

static void
gum_stalker_get_target_address (const cs_insn * insn,
                                gboolean thumb,
                                GumBranchTarget * target,
                                guint16 * mask)
{
  cs_arm * arm = &insn->detail->arm;
  cs_arm_op * op1 = &arm->operands[0];

  /*
   * The complex nature of the ARM32 instruction set means that determining the
   * target address for an instruction which affects control flow is also
   * complex.
   *
   * Instructions such as 'BL label' will make use of the absolute_address
   * field. 'BL reg' and 'BLX' reg will make use of the reg field. 'LDR pc,
   * [reg]' however makes use of the reg field and sets is_indirect to TRUE.
   * This means that the reg field doesn't contain the target itself, but the
   * address in memory where the target is stored. 'LDR pc, [reg, #x]'
   * additionally sets the offset field which needs to be added to the register
   * before it is dereferenced.
   *
   * The POP and LDM instructions both read multiple values from where a base
   * register points, and store them into a listed set of registers. In the case
   * of the POP instruction, this base register is always SP, i.e. the stack
   * pointer. Again the is_indirect field is set and the value of the offset
   * field is determined by how many registers are included in the register list
   * before PC.
   *
   * Finally, ADD and SUB instructions can be used to modify control flow. ADD
   * instructions have two main forms. Firstly 'ADD pc, reg, #x', in this case
   * the reg and offset fields are both set accordingly. Secondly, if the form
   * 'ADD pc, reg, reg2' is used, then the values of reg and reg2 are set
   * accordingly. This form has the additional complexity of allowing a suffix
   * which can describe a shift operation (one of 4 types) and a value
   * indicating how many places to shift to be applied to reg2 before it is
   * added. This information is encoded in the shifter and shift_value fields
   * accordingly. Lastly the SUB instruction is identical to ADD except the
   * offset is negative, to indicate that reg2 should be subtracted rather than
   * added.
   *
   * This complex target field is processed by write_$mode_mov_branch_target()
   * in order to write instructions into the instrumented block to recover the
   * target address from these different forms of instruction.
   *
   * Lastly, we should note that many of these instructions can be conditionally
   * executed depending on the status of processor flags. For example, BLEQ will
   * only take affect if the previous instruction which set the flags indicated
   * the result of the operation was equal.
   */

  /*
   * The mask is used when POP or LDMIA instructions are encountered. This is
   * used to encode the other registers which are included in the operation.
   * Note, however, that the register PC is omitted from this mask.
   *
   * This is processed by virtualize_ret_insn() so that after the epilogue has
   * been executed and the application registers are restored. A replacement POP
   * or LDMIA instruction can be generated to restore the values of the other
   * registers from the stack. Note that we don't restore the value of PC from
   * the stack and instead simply increment the stack pointer since we instead
   * want to pass control back into Stalker to instrument the next block.
   */
  *mask = 0;

  switch (insn->id)
  {
    case ARM_INS_B:
    case ARM_INS_BL:
    {
      GumBranchDirectAddress * value = &target->value.direct_address;

      g_assert (op1->type == ARM_OP_IMM);

      target->type = GUM_TARGET_DIRECT_ADDRESS;

      /*
       * If the case of the B and BL instructions, the instruction mode never
       * changes from ARM to Thumb or vice-versa and hence the low bit of the
       * target address should be retained.
       */
      if (thumb)
        value->address = GSIZE_TO_POINTER (op1->imm + 1);
      else
        value->address = GSIZE_TO_POINTER (op1->imm);

      break;
    }
    case ARM_INS_BX:
    case ARM_INS_BLX:
    {
      if (op1->type == ARM_OP_REG)
      {
        GumBranchDirectRegOffset * value = &target->value.direct_reg_offset;

        target->type = GUM_TARGET_DIRECT_REG_OFFSET;

        value->reg = op1->reg;
        value->offset = 0;
        value->mode = GUM_ARM_MODE_AUTO;
      }
      else
      {
        GumBranchDirectAddress * value = &target->value.direct_address;

        target->type = GUM_TARGET_DIRECT_ADDRESS;

        /*
         * In the case of the BX and BLX instructions, the instruction mode
         * always changes from ARM to Thumb or vice-versa and hence the low
         * bit of the target address should be inverted.
         */
        if (thumb)
          value->address = GSIZE_TO_POINTER (op1->imm);
        else
          value->address = GSIZE_TO_POINTER (op1->imm) + 1;
      }

      break;
    }
    case ARM_INS_CBZ:
    case ARM_INS_CBNZ:
    {
      GumBranchDirectAddress * value = &target->value.direct_address;
      cs_arm_op * op2 = &arm->operands[1];

      /*
       * If the case of the CBZ and CBNZ instructions, the instruction mode
       * never changes and hence the low bit of the target address should be
       * retained. These are only supported in Thumb mode.
       */
      g_assert (thumb);

      target->type = GUM_TARGET_DIRECT_ADDRESS;

      value->address = GSIZE_TO_POINTER (op2->imm + 1);

      break;
    }
    case ARM_INS_POP:
    {
      GumBranchIndirectRegOffset * value = &target->value.indirect_reg_offset;
      guint8 i;

      target->type = GUM_TARGET_INDIRECT_REG_OFFSET;

      value->reg = ARM_REG_SP;
      value->offset = 0;
      value->write_back = TRUE;

      for (i = 0; i != insn->detail->arm.op_count; i++)
      {
        cs_arm_op * op = &arm->operands[i];

        if (op->reg == ARM_REG_PC)
        {
          value->offset = i * 4;
        }
        else
        {
          GumArmRegInfo ri;
          gum_arm_reg_describe (op->reg, &ri);
          *mask |= 1 << ri.index;
        }
      }

      break;
    }
    case ARM_INS_LDM:
    {
      GumBranchIndirectRegOffset * value = &target->value.indirect_reg_offset;
      guint8 i;

      target->type = GUM_TARGET_INDIRECT_REG_OFFSET;

      value->reg = op1->reg;
      value->offset = 0;
      value->write_back = insn->detail->arm.writeback;

      for (i = 1; i != insn->detail->arm.op_count; i++)
      {
        cs_arm_op * op = &arm->operands[i];

        if (op->reg == ARM_REG_PC)
        {
          value->offset = (i - 1) * 4;
        }
        else
        {
          GumArmRegInfo ri;
          gum_arm_reg_describe (op->reg, &ri);
          *mask |= 1 << ri.index;
        }
      }

      break;
    }
    case ARM_INS_LDR:
    {
      cs_arm_op * op2 = &arm->operands[1];
      g_assert (op2->type == ARM_OP_MEM);

      if (op2->shift.value == 0)
      {
        GumBranchIndirectRegOffset * value = &target->value.indirect_reg_offset;

        target->type = GUM_TARGET_INDIRECT_REG_OFFSET;

        value->reg = op2->mem.base;
        g_assert (op2->mem.index == ARM_REG_INVALID);
        value->offset = op2->mem.disp;
        value->write_back = FALSE;
      }
      else
      {
        GumBranchIndirectRegShift * value = &target->value.indirect_reg_shift;

        target->type = GUM_TARGET_INDIRECT_REG_SHIFT;

        value->base = op2->mem.base;
        value->index = op2->mem.index;
        value->shifter = op2->shift.type;
        value->shift_value = op2->shift.value;
      }

      break;
    }
    case ARM_INS_MOV:
    {
      GumBranchDirectRegOffset * value = &target->value.direct_reg_offset;

      cs_arm_op * op2 = &arm->operands[1];

      target->type = GUM_TARGET_DIRECT_REG_OFFSET;

      value->reg = op2->reg;
      value->offset = 0;
      value->mode = GUM_ARM_MODE_CURRENT;

      break;
    }
    case ARM_INS_ADD:
    case ARM_INS_SUB:
    {
      cs_arm_op * base = &arm->operands[1];
      cs_arm_op * index = &arm->operands[2];

      g_assert (base->type == ARM_OP_REG);

      if (index->type == ARM_OP_REG)
      {
        GumBranchDirectRegShift * value = &target->value.direct_reg_shift;

        target->type = GUM_TARGET_DIRECT_REG_SHIFT;

        value->base = base->reg;
        value->index = index->reg;
        value->shifter = index->shift.type;
        value->shift_value = index->shift.value;
      }
      else
      {
        GumBranchDirectRegOffset * value = &target->value.direct_reg_offset;

        target->type = GUM_TARGET_DIRECT_REG_OFFSET;

        value->reg = base->reg;
        value->offset = (insn->id == ARM_INS_SUB) ? -index->imm : index->imm;
        value->mode = GUM_ARM_MODE_CURRENT;
      }

      break;
    }
    case ARM_INS_TBB:
    case ARM_INS_TBH:
    {
      arm_op_mem * op = &arm->operands[0].mem;
      GumBranchIndirectPcrelTable * value = &target->value.indirect_pcrel_table;

      target->type = GUM_TARGET_INDIRECT_PCREL_TABLE;

      value->base = op->base;
      value->index = op->index;

      value->element_size = (insn->id == ARM_INS_TBB)
          ? sizeof (guint8)
          : sizeof (guint16);

      break;
    }
    default:
      g_assert_not_reached ();
  }
}

static void
gum_stalker_get_writeback (const cs_insn * insn,
                           GumWriteback * writeback)
{
  cs_arm * arm = &insn->detail->arm;
  cs_arm_op * op2 = &arm->operands[1];

  writeback->target = ARM_REG_INVALID;
  writeback->offset = 0;

  if (!arm->writeback)
    return;

  if (insn->id != ARM_INS_LDR)
    gum_panic ("Writeback for unexpected op-code: %d", insn->id);

  if (op2->type != ARM_OP_MEM)
    gum_panic ("Writeback for unexpected operand");

  if (op2->mem.index != ARM_REG_INVALID)
    gum_panic ("Writeback for register operands not supported");

  writeback->target = op2->mem.base;

  switch (arm->op_count)
  {
    case 2: /* pre-increment/decrement */
    {
      writeback->offset = op2->mem.disp;
      break;
    }
    case 3: /* post-increment/decrement */
    {
      cs_arm_op * op3 = &arm->operands[2];

      g_assert (op3->type == ARM_OP_IMM);

      writeback->offset = op3->subtracted ? -op3->imm : op3->imm;

      break;
    }
    default:
    {
      g_assert_not_reached ();
    }
  }
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
  call->depth = ctx->first_frame - ctx->current_frame;

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
  ret->depth = ctx->first_frame - ctx->current_frame;

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

  /*
   * Suppress generation of multiple EXEC events for IT blocks. An exec event
   * is already generated for the IT block, but a subsequent one may be
   * generated by the handling of a virtualized branch instruction if it is
   * taken. We simply ignore the request if the location is the same as the
   * previously emitted event.
   */
  if (location == ctx->last_exec_location)
    return;

  ctx->last_exec_location = location;

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

  bev->start = gum_exec_block_encode_instruction_pointer (block,
      block->real_start);
  bev->end = gum_exec_block_encode_instruction_pointer (block,
      block->real_start + block->real_size);

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
  GumCalloutEntry entry, * live_entry;
  GumAddress entry_address;

  entry.callout = callout;
  entry.data = data;
  entry.data_destroy = data_destroy;
  entry.pc = gc->instruction->start;
  entry.exec_context = self->exec_context;
  entry.next = NULL;

  if (gc->is_thumb)
  {
    GumThumbWriter * cw = gc->thumb_writer;

    live_entry = gum_exec_block_write_thumb_inline_data (cw, &entry,
        sizeof (entry), &entry_address);

    gum_exec_block_thumb_open_prolog (block, gc);
    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_stalker_invoke_callout), 2,
        GUM_ARG_ADDRESS, entry_address,
        GUM_ARG_REGISTER, ARM_REG_R10);
    gum_exec_block_thumb_close_prolog (block, gc);
  }
  else
  {
    GumArmWriter * cw = gc->arm_writer;

    live_entry = gum_exec_block_write_arm_inline_data (cw, &entry,
        sizeof (entry), &entry_address);

    gum_exec_block_arm_open_prolog (block, gc);
    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_stalker_invoke_callout), 2,
        GUM_ARG_ADDRESS, entry_address,
        GUM_ARG_REGISTER, ARM_REG_R10);
    gum_exec_block_arm_close_prolog (block, gc);
  }

  live_entry->next = gum_exec_block_get_last_callout_entry (block);
  gum_exec_block_set_last_callout_entry (block,
      GSIZE_TO_POINTER (entry_address));
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
   GumBranchTarget target;
   GumBranchDirectRegOffset * value;

   target.type = GUM_TARGET_DIRECT_REG_OFFSET;
   value = &target.value.direct_reg_offset;
   value->reg = ARM_REG_LR;
   value->offset = 0;
   value->mode = GUM_ARM_MODE_CURRENT;

  if (gc->is_thumb)
  {
    gum_exec_block_virtualize_thumb_ret_insn (block, &target, FALSE, 0, gc);
  }
  else
  {
    gum_exec_block_virtualize_arm_ret_insn (block, &target, ARM_CC_AL, FALSE, 0,
        gc);
  }
}

csh
gum_stalker_iterator_get_capstone (GumStalkerIterator * self)
{
  const GumGeneratorContext * gc = self->generator_context;

  return gc->is_thumb
      ? gc->thumb_relocator->capstone
      : gc->arm_relocator->capstone;
}

static void
gum_exec_ctx_write_arm_prolog (GumExecCtx * ctx,
                               GumArmWriter * cw)
{
  const GumCpuFeatures cpu_features = ctx->stalker->cpu_features;

  /*
   * For our context, we want to build up the following structure so that
   * Stalker can read the register state of the application.
   *
   * struct _GumArmCpuContext
   * {
   *   guint32 pc;
   *   guint32 sp;
   *   guint32 cpsr;
   *
   *   guint32 r8;
   *   guint32 r9;
   *   guint32 r10;
   *   guint32 r11;
   *   guint32 r12;
   *
   *   GumArmVectorReg q[16];
   *
   *   guint32 _padding;
   *
   *   guint32 r[8];
   *   guint32 lr;
   * };
   */

  /* Store R0 through R7 and LR */
  gum_arm_writer_put_push_regs (cw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_LR);

  /* Take note of CPSR and where GumCpuContext ends/application stack begins */
  gum_arm_writer_put_mov_reg_cpsr (cw, ARM_REG_R5);
  gum_arm_writer_put_add_reg_reg_imm (cw, ARM_REG_R4, ARM_REG_SP, 9 * 4);

  /* Add padding followed by VFP registers */
  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_arm_writer_put_sub_reg_u16 (cw, ARM_REG_SP, 4);
      gum_arm_writer_put_vpush_range (cw, ARM_REG_Q8, ARM_REG_Q15);
    }
    else
    {
      gum_arm_writer_put_sub_reg_u16 (cw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }

    gum_arm_writer_put_vpush_range (cw, ARM_REG_Q0, ARM_REG_Q7);
  }
  else
  {
    gum_arm_writer_put_sub_reg_u16 (cw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  /* Store SP, CPSR, followed by R8-R12 */
  gum_arm_writer_put_push_regs (cw, 7,
      ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  /* Reserve space for the PC placeholder */
  gum_arm_writer_put_sub_reg_u16 (cw, ARM_REG_SP, 4);

  /*
   * Now that the context structure has been pushed onto the stack, we store the
   * address of this structure on the stack into register R10. This register can
   * be chosen fairly arbitrarily but it should be a callee saved register so
   * that any C code called from our instrumented code is obliged by the calling
   * convention to preserve its value across the function call. In particular
   * register R12 is a caller saved register and as such any C function can
   * modify its value and not restore it. Similary registers R0 through R3
   * contain the arguments to the function and the return result and are
   * accordingly not preserved.
   *
   * We have elected not to use R11 since this can be used as a frame pointer by
   * some compilers and as such can confuse some debuggers. The function
   * load_real_register_into() makes use of this register R10 in order to access
   * this context structure.
   */
  gum_arm_writer_put_mov_reg_reg (cw, ARM_REG_R10, ARM_REG_SP);

  /*
   * We must now ensure that the stack is 8 byte aligned, since this is expected
   * by the ABI. Since the context was on the top of the stack and we retain
   * this address in R10, we don't need to save the original stack pointer for
   * re-alignment in the epilogue since we can simply restore SP from R10.
   */
  gum_arm_writer_put_ands_reg_reg_imm (cw, ARM_REG_R0, ARM_REG_SP, 7);
  gum_arm_writer_put_sub_reg_reg_reg (cw, ARM_REG_SP, ARM_REG_SP, ARM_REG_R0);
}

static void
gum_exec_ctx_write_thumb_prolog (GumExecCtx * ctx,
                                 GumThumbWriter * cw)
{
  const GumCpuFeatures cpu_features = ctx->stalker->cpu_features;

  gum_thumb_writer_put_push_regs (cw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_LR);

  /*
   * Note that we stash the CPSR (flags) here first since the Thumb instruction
   * set doesn't support short form instructions for SUB. Hence, the ADD/SUB
   * instructions below where the destination is not SP are actually ADDS/SUBS
   * and will clobber the flags.
   */
  gum_thumb_writer_put_mov_reg_cpsr (cw, ARM_REG_R5);
  gum_thumb_writer_put_add_reg_reg_imm (cw, ARM_REG_R4, ARM_REG_SP, 9 * 4);

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_thumb_writer_put_sub_reg_imm (cw, ARM_REG_SP, 4);
      gum_thumb_writer_put_vpush_range (cw, ARM_REG_Q8, ARM_REG_Q15);
    }
    else
    {
      gum_thumb_writer_put_sub_reg_imm (cw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }

    gum_thumb_writer_put_vpush_range (cw, ARM_REG_Q0, ARM_REG_Q7);
  }
  else
  {
    gum_thumb_writer_put_sub_reg_imm (cw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  gum_thumb_writer_put_push_regs (cw, 7,
      ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  gum_thumb_writer_put_sub_reg_imm (cw, ARM_REG_SP, 4);

  gum_thumb_writer_put_mov_reg_reg (cw, ARM_REG_R10, ARM_REG_SP);

  /*
   * Like in the ARM prolog we must now ensure that the stack is 8 byte aligned.
   * Note that unlike the ARM prolog, which simply rounds down the stack
   * pointer, the Thumb instruction set often requires wide, or Thumb v2
   * instructions to work with registers other than R0-R7. We therefore retard
   * the stack by 8, before rounding back up. This works as we know the stack
   * must be 4 byte aligned since ARM architecture does not support unaligned
   * data access. e.g. if the stack was already aligned, we simply retard the
   * pointer by 8 (although wasting a few bytes of stack space, this still
   * retains alignment), if it was misaligned, we retard the pointer by 8 before
   * advancing back 4 bytes.
   */
  gum_thumb_writer_put_and_reg_reg_imm (cw, ARM_REG_R0, ARM_REG_SP, 7);
  gum_thumb_writer_put_sub_reg_reg_imm (cw, ARM_REG_SP, ARM_REG_SP, 8);
  gum_thumb_writer_put_add_reg_reg_reg (cw, ARM_REG_SP, ARM_REG_SP, ARM_REG_R0);
}

static void
gum_exec_ctx_write_arm_epilog (GumExecCtx * ctx,
                               GumArmWriter * cw)
{
  const GumCpuFeatures cpu_features = ctx->stalker->cpu_features;

  /*
   * We know that the context structure was at the top of the stack at the end
   * of the prolog, before the stack was aligned. Rather than working out how
   * much alignment was needed, we can simply restore R10 back into SP to
   * retrieve our stack pointer pre-alignment before we continue restoring the
   * rest of the context.
   */
  gum_arm_writer_put_mov_reg_reg (cw, ARM_REG_SP, ARM_REG_R10);

  gum_arm_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R5, ARM_REG_SP,
      G_STRUCT_OFFSET (GumCpuContext, cpsr));

  /* Skip PC, SP, and CPSR */
  gum_arm_writer_put_add_reg_u16 (cw, ARM_REG_SP, 3 * 4);

  gum_arm_writer_put_pop_regs (cw, 5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    gum_arm_writer_put_vpop_range (cw, ARM_REG_Q0, ARM_REG_Q7);

    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_arm_writer_put_vpop_range (cw, ARM_REG_Q8, ARM_REG_Q15);
      gum_arm_writer_put_add_reg_u16 (cw, ARM_REG_SP, 4);
    }
    else
    {
      gum_arm_writer_put_add_reg_u16 (cw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }
  }
  else
  {
    gum_arm_writer_put_add_reg_u16 (cw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  gum_arm_writer_put_mov_cpsr_reg (cw, ARM_REG_R5);

  gum_arm_writer_put_pop_regs (cw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_LR);
}

static void
gum_exec_ctx_write_thumb_epilog (GumExecCtx * ctx,
                                 GumThumbWriter * cw)
{
  const GumCpuFeatures cpu_features = ctx->stalker->cpu_features;

  gum_thumb_writer_put_mov_reg_reg (cw, ARM_REG_SP, ARM_REG_R10);

  gum_thumb_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R5, ARM_REG_SP,
      G_STRUCT_OFFSET (GumCpuContext, cpsr));

  gum_thumb_writer_put_add_reg_imm (cw, ARM_REG_SP, 3 * 4);

  gum_thumb_writer_put_pop_regs (cw, 5,
      ARM_REG_R8, ARM_REG_R9, ARM_REG_R10, ARM_REG_R11, ARM_REG_R12);

  if ((cpu_features & GUM_CPU_VFP2) != 0)
  {
    gum_thumb_writer_put_vpop_range (cw, ARM_REG_Q0, ARM_REG_Q7);

    if ((cpu_features & GUM_CPU_VFPD32) != 0)
    {
      gum_thumb_writer_put_vpop_range (cw, ARM_REG_Q8, ARM_REG_Q15);
      gum_thumb_writer_put_add_reg_imm (cw, ARM_REG_SP, 4);
    }
    else
    {
      gum_thumb_writer_put_add_reg_imm (cw, ARM_REG_SP,
          (8 * sizeof (GumArmVectorReg)) + 4);
    }
  }
  else
  {
    gum_thumb_writer_put_add_reg_imm (cw, ARM_REG_SP,
        (16 * sizeof (GumArmVectorReg)) + 4);
  }

  gum_thumb_writer_put_mov_cpsr_reg (cw, ARM_REG_R5);

  gum_thumb_writer_put_pop_regs (cw, 9,
      ARM_REG_R0, ARM_REG_R1, ARM_REG_R2,
      ARM_REG_R3, ARM_REG_R4, ARM_REG_R5,
      ARM_REG_R6, ARM_REG_R7, ARM_REG_LR);
}

static void
gum_exec_ctx_ensure_inline_helpers_reachable (GumExecCtx * ctx)
{
  gum_exec_ctx_ensure_arm_helper_reachable (ctx, &ctx->last_arm_invalidator,
      gum_exec_ctx_write_arm_invalidator);
  gum_exec_ctx_ensure_thumb_helper_reachable (ctx, &ctx->last_thumb_invalidator,
      gum_exec_ctx_write_thumb_invalidator);
  ctx->code_slab->arm_invalidator = ctx->last_arm_invalidator;
  ctx->code_slab->thumb_invalidator = ctx->last_thumb_invalidator;
}

static void
gum_exec_ctx_write_arm_invalidator (GumExecCtx * ctx,
                                    GumArmWriter * cw)
{
  gum_exec_ctx_write_arm_prolog (ctx, cw);

  gum_arm_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_recompile_and_switch_block), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_REGISTER, ARM_REG_LR);

  gum_exec_ctx_write_arm_epilog (ctx, cw);
  gum_arm_writer_put_pop_regs (cw, 1, ARM_REG_LR);

  gum_exec_block_write_arm_exec_generated_code (cw, ctx);
}

static void
gum_exec_ctx_write_thumb_invalidator (GumExecCtx * ctx,
                                      GumThumbWriter * cw)
{
  gum_exec_ctx_write_thumb_prolog (ctx, cw);

  gum_thumb_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_recompile_and_switch_block), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ctx),
      GUM_ARG_REGISTER, ARM_REG_LR);

  gum_exec_ctx_write_thumb_epilog (ctx, cw);
  gum_thumb_writer_put_pop_regs (cw, 2, ARM_REG_R0, ARM_REG_LR);

  gum_exec_block_write_thumb_exec_generated_code (cw, ctx);
}

static void
gum_exec_ctx_ensure_arm_helper_reachable (GumExecCtx * ctx,
                                          gpointer * helper_ptr,
                                          GumArmHelperWriteFunc write)
{
  GumSlab * slab = &ctx->code_slab->slab;
  GumArmWriter * cw = &ctx->arm_writer;
  gpointer start;

  if (gum_exec_ctx_is_arm_helper_reachable (ctx, helper_ptr))
    return;

  start = gum_slab_align_cursor (slab, 4);
  gum_stalker_thaw (ctx->stalker, start, gum_slab_available (slab));
  gum_arm_writer_reset (cw, start);
  *helper_ptr = gum_arm_writer_cur (cw);

  write (ctx, cw);

  gum_arm_writer_flush (cw);
  gum_stalker_freeze (ctx->stalker, cw->base, gum_arm_writer_offset (cw));

  gum_slab_reserve (slab, gum_arm_writer_offset (cw));
}

static void
gum_exec_ctx_ensure_thumb_helper_reachable (GumExecCtx * ctx,
                                            gpointer * helper_ptr,
                                            GumThumbHelperWriteFunc write)
{
  GumSlab * slab = &ctx->code_slab->slab;
  GumThumbWriter * cw = &ctx->thumb_writer;
  gpointer start;

  if (gum_exec_ctx_is_thumb_helper_reachable (ctx, helper_ptr))
    return;

  start = gum_slab_align_cursor (slab, 2);
  gum_stalker_thaw (ctx->stalker, start, gum_slab_available (slab));
  gum_thumb_writer_reset (cw, start);
  *helper_ptr = gum_thumb_writer_cur (cw);

  write (ctx, cw);

  gum_thumb_writer_flush (cw);
  gum_stalker_freeze (ctx->stalker, cw->base, gum_thumb_writer_offset (cw));

  gum_slab_reserve (slab, gum_thumb_writer_offset (cw));
}

static gboolean
gum_exec_ctx_is_arm_helper_reachable (GumExecCtx * ctx,
                                      gpointer * helper_ptr)
{
  GumSlab * slab = &ctx->code_slab->slab;
  GumArmWriter * cw = &ctx->arm_writer;
  GumAddress helper, start, end;

  helper = GUM_ADDRESS (*helper_ptr);
  if (helper == 0)
    return FALSE;

  start = GUM_ADDRESS (gum_slab_start (slab));
  end = GUM_ADDRESS (gum_slab_end (slab));

  if (!gum_arm_writer_can_branch_directly_between (cw, start, helper))
    return FALSE;

  return gum_arm_writer_can_branch_directly_between (cw, end, helper);
}

static gboolean
gum_exec_ctx_is_thumb_helper_reachable (GumExecCtx * ctx,
                                        gpointer * helper_ptr)
{
  GumSlab * slab = &ctx->code_slab->slab;
  GumThumbWriter * cw = &ctx->thumb_writer;
  GumAddress helper, start, end;

  helper = GUM_ADDRESS (*helper_ptr);
  if (helper == 0)
    return FALSE;

  start = GUM_ADDRESS (gum_slab_start (slab));
  end = GUM_ADDRESS (gum_slab_end (slab));

  if (!gum_thumb_writer_can_branch_directly_between (cw, start, helper))
    return FALSE;

  return gum_thumb_writer_can_branch_directly_between (cw, end, helper);
}

static void
gum_exec_ctx_write_arm_mov_branch_target (GumExecCtx * ctx,
                                          const GumBranchTarget * target,
                                          arm_reg reg,
                                          GumGeneratorContext * gc)
{
  GumArmWriter * cw = gc->arm_writer;

  switch (target->type)
  {
    case GUM_TARGET_DIRECT_ADDRESS: /* E.g. 'B #1234' */
    {
      const GumBranchDirectAddress * value = &target->value.direct_address;

      gum_arm_writer_put_ldr_reg_address (cw, reg,
          GUM_ADDRESS (value->address));

      break;
    }
    case GUM_TARGET_DIRECT_REG_OFFSET: /* E.g. 'ADD/SUB pc, r1, #32' */
    {
      const GumBranchDirectRegOffset * value = &target->value.direct_reg_offset;

      gum_exec_ctx_arm_load_real_register_into (ctx, reg, value->reg, gc);

      if (value->offset >= 0)
        gum_arm_writer_put_add_reg_reg_imm (cw, reg, reg, value->offset);
      else
        gum_arm_writer_put_sub_reg_reg_imm (cw, reg, reg, -value->offset);

      break;
    }
    case GUM_TARGET_DIRECT_REG_SHIFT: /* E.g. 'ADD pc, r1, r2 lsl #4' */
    {
      const GumBranchDirectRegShift * value = &target->value.direct_reg_shift;

      gum_exec_ctx_arm_load_real_register_into (ctx, reg, value->base, gc);

      /*
       * Here we are going to use R12 as additional scratch space. Since we
       * typically use this function to load the target into a register so that
       * it can be used as a function parameter, R12 is unlikely to be our
       * target register. Although R12 is not callee saved, the instruction
       * being handled may not represent a function call or return, but rather a
       * transition between blocks within a function and therefore we store and
       * restore its contents. Note that we do this above the redzone to avoid
       * clobbering any data stored there, and also do so without modifying SP
       * since that may be one of the operands of the branch instruction.
       */
      if (reg == ARM_REG_R12)
      {
        gum_panic ("Cannot support ADD/SUB reg, reg, reg when target is "
            "ARM_REG_R12");
      }

      gum_arm_writer_put_str_reg_reg_offset (cw, ARM_REG_R12, ARM_REG_SP,
          -GUM_RED_ZONE_SIZE - 4);

      /*
       * Load the second register value from the context into R12 before adding
       * to the original and applying any necessary shift.
       */
      gum_exec_ctx_arm_load_real_register_into (ctx, ARM_REG_R12, value->index,
          gc);

      gum_arm_writer_put_add_reg_reg_reg_shift (cw, reg, reg, ARM_REG_R12,
          value->shifter, value->shift_value);

      gum_arm_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R12, ARM_REG_SP,
          -GUM_RED_ZONE_SIZE - 4);

      break;
    }
    case GUM_TARGET_INDIRECT_REG_OFFSET: /* E.g. 'LDR pc, [r3, #4]' */
    {
      const GumBranchIndirectRegOffset * value =
          &target->value.indirect_reg_offset;

      gum_exec_ctx_arm_load_real_register_into (ctx, reg, value->reg, gc);

      /*
       * If the target is indirect, then we need to dereference it.
       * E.g. LDR pc, [r3, #4]
       */
      gum_arm_writer_put_ldr_reg_reg_offset (cw, reg, reg, value->offset);

      break;
    }
    case GUM_TARGET_INDIRECT_REG_SHIFT: /* E.g. 'LDR pc, [pc, r0, lsl #2]' */
    {
      const GumBranchIndirectRegShift * value =
          &target->value.indirect_reg_shift;

      gum_exec_ctx_arm_load_real_register_into (ctx, reg, value->base, gc);

      /*
       * Here we are going to use R12 as additional scratch space. Since we
       * typically use this function to load the target into a register so that
       * it can be used as a function parameter, R12 is unlikely to be our
       * target register. Although R12 is not callee saved, the instruction
       * being handled may not represent a function call or return, but rather a
       * transition between blocks within a function and therefore we store and
       * restore its contents. Note that we do this above the redzone to avoid
       * clobbering any data stored there, and also do so without modifying SP
       * since that may be one of the operands of the branch instruction.
       */
      if (reg == ARM_REG_R12)
      {
        gum_panic ("Cannot support LDR reg, reg, SHIFT when target is "
            "ARM_REG_R12");
      }

      gum_arm_writer_put_str_reg_reg_offset (cw, ARM_REG_R12, ARM_REG_SP,
          -GUM_RED_ZONE_SIZE - 4);

      /*
       * Load the second register value from the context into R12 before adding
       * to the original and applying any necessary shift.
       */
      gum_exec_ctx_arm_load_real_register_into (ctx, ARM_REG_R12, value->index,
          gc);

      gum_arm_writer_put_add_reg_reg_reg_shift (cw, reg, reg, ARM_REG_R12,
          value->shifter, value->shift_value);

      gum_arm_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R12, ARM_REG_SP,
          -GUM_RED_ZONE_SIZE - 4);

      gum_arm_writer_put_ldr_reg_reg (cw, reg, reg);

      break;
    }
    default:
      g_assert_not_reached ();
  }
}

static void
gum_exec_ctx_write_thumb_mov_branch_target (GumExecCtx * ctx,
                                            const GumBranchTarget * target,
                                            arm_reg reg,
                                            GumGeneratorContext * gc)
{
  GumThumbWriter * cw = gc->thumb_writer;

  switch (target->type)
  {
    case GUM_TARGET_DIRECT_ADDRESS:
    {
      const GumBranchDirectAddress * value = &target->value.direct_address;

      gum_thumb_writer_put_ldr_reg_address (cw, reg,
          GUM_ADDRESS (value->address));

      break;
    }
    case GUM_TARGET_DIRECT_REG_OFFSET:
    {
      const GumBranchDirectRegOffset * value = &target->value.direct_reg_offset;

      g_assert (value->offset >= 0);

      gum_exec_ctx_thumb_load_real_register_into (ctx, reg, value->reg, gc);

      gum_thumb_writer_put_add_reg_reg_imm (cw, reg, reg, value->offset);

      if (value->mode == GUM_ARM_MODE_CURRENT)
        gum_thumb_writer_put_or_reg_reg_imm (cw, reg, reg, 0x1);

      break;
    }
    case GUM_TARGET_DIRECT_REG_SHIFT:
    {
      g_assert_not_reached ();
      break;
    }
    case GUM_TARGET_INDIRECT_REG_OFFSET:
    {
      const GumBranchIndirectRegOffset * value =
          &target->value.indirect_reg_offset;

      g_assert (value->offset >= 0);

      gum_exec_ctx_thumb_load_real_register_into (ctx, reg, value->reg, gc);

      /*
       * If the target is indirect, then we need to dereference it.
       * E.g. LDR pc, [r3, #4]
       */
      gum_thumb_writer_put_ldr_reg_reg_offset (cw, reg, reg, value->offset);

      break;
    }
    case GUM_TARGET_INDIRECT_PCREL_TABLE:
    {
      const GumBranchIndirectPcrelTable * value =
          &target->value.indirect_pcrel_table;
      arm_reg offset_reg;

      gum_exec_ctx_thumb_load_real_register_into (ctx, reg, value->base, gc);

      offset_reg = (reg != ARM_REG_R0) ? ARM_REG_R0 : ARM_REG_R1;
      gum_thumb_writer_put_push_regs (cw, 1, offset_reg);

      gum_exec_ctx_thumb_load_real_register_into (ctx, offset_reg, value->index,
          gc);
      if (value->element_size == 2)
      {
        /* Transform index to offset. */
        gum_thumb_writer_put_lsl_reg_reg_imm (cw, offset_reg, offset_reg, 1);
      }

      /* Add base address. */
      gum_thumb_writer_put_add_reg_reg (cw, offset_reg, reg);

      /* Read the uint8 or uint16 at the given index. */
      if (value->element_size == 1)
        gum_thumb_writer_put_ldrb_reg_reg (cw, offset_reg, offset_reg);
      else
        gum_thumb_writer_put_ldrh_reg_reg (cw, offset_reg, offset_reg);
      /* Transform index to offset. */
      gum_thumb_writer_put_lsl_reg_reg_imm (cw, offset_reg, offset_reg, 1);

      /* Add Thumb bit. */
      gum_thumb_writer_put_add_reg_imm (cw, offset_reg, 1);

      /* Now we have an offset we can add to the base. */
      gum_thumb_writer_put_add_reg_reg_reg (cw, reg, reg, offset_reg);

      gum_thumb_writer_put_pop_regs (cw, 1, offset_reg);

      break;
    }
    case GUM_TARGET_INDIRECT_REG_SHIFT: /* E.g. 'LDR pc, [pc, r0, lsl #2]' */
    {
      g_assert_not_reached ();
      break;
    }
    default:
      g_assert_not_reached ();
  }
}

static void
gum_exec_ctx_arm_load_real_register_into (GumExecCtx * ctx,
                                          arm_reg target_register,
                                          arm_reg source_register,
                                          GumGeneratorContext * gc)
{
  GumArmWriter * cw = gc->arm_writer;

  /*
   * For the most part, we simply need to identify the offset of the
   * source_register within the GumCpuContext structure and load the value
   * accordingly. However, in the case of the PC, we instead load the address of
   * the current instruction in the iterator. Note that we add the fixed offset
   * of 8 since the value of PC is always interpreted in ARM32 as being 8 bytes
   * past the start of the instruction.
   */
  if (source_register >= ARM_REG_R0 && source_register <= ARM_REG_R7)
  {
    gum_arm_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, r) +
        ((source_register - ARM_REG_R0) * 4));
  }
  else if (source_register >= ARM_REG_R8 && source_register <= ARM_REG_R12)
  {
    gum_arm_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, r8) +
        ((source_register - ARM_REG_R8) * 4));
  }
  else if (source_register == ARM_REG_LR)
  {
    gum_arm_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, lr));
  }
  else if (source_register == ARM_REG_SP)
  {
    gum_arm_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, sp));
  }
  else if (source_register == ARM_REG_PC)
  {
    gum_arm_writer_put_ldr_reg_address (cw, target_register,
        GUM_ADDRESS (gc->instruction->start + 8));
  }
  else
  {
    g_assert_not_reached ();
  }
}

static void
gum_exec_ctx_thumb_load_real_register_into (GumExecCtx * ctx,
                                            arm_reg target_register,
                                            arm_reg source_register,
                                            GumGeneratorContext * gc)
{
  GumThumbWriter * cw = gc->thumb_writer;

  if (source_register >= ARM_REG_R0 && source_register <= ARM_REG_R7)
  {
    gum_thumb_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, r) +
        ((source_register - ARM_REG_R0) * 4));
  }
  else if (source_register >= ARM_REG_R8 && source_register <= ARM_REG_R12)
  {
    gum_thumb_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, r8) +
        ((source_register - ARM_REG_R8) * 4));
  }
  else if (source_register == ARM_REG_LR)
  {
    gum_thumb_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, lr));
  }
  else if (source_register == ARM_REG_SP)
  {
    gum_thumb_writer_put_ldr_reg_reg_offset (cw, target_register, ARM_REG_R10,
        G_STRUCT_OFFSET (GumCpuContext, sp));
  }
  else if (source_register == ARM_REG_PC)
  {
    gum_thumb_writer_put_ldr_reg_address (cw, target_register,
        GUM_ADDRESS (gc->instruction->start + 4));
  }
  else
  {
    g_assert_not_reached ();
  }
}

static GumExecBlock *
gum_exec_block_new (GumExecCtx * ctx)
{
  GumExecBlock * block;
  GumStalker * stalker = ctx->stalker;
  GumCodeSlab * code_slab = ctx->code_slab;
  GumDataSlab * data_slab = ctx->data_slab;
  gsize code_available;

  code_available = gum_slab_available (&code_slab->slab);
  if (code_available < GUM_EXEC_BLOCK_MIN_CAPACITY)
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

  block = gum_slab_try_reserve (&data_slab->slab, sizeof (GumExecBlock));
  if (block == NULL)
  {
    data_slab = gum_exec_ctx_add_data_slab (ctx, gum_data_slab_new (ctx));
    block = gum_slab_reserve (&data_slab->slab, sizeof (GumExecBlock));
  }

  /*
   * Instrumented ARM code needs to be 4 byte aligned. We will make all code
   * blocks (both ARM and Thumb) 4 byte aligned for simplicity.
   */
  gum_slab_align_cursor (&code_slab->slab, 4);

  block->next = ctx->block_list;
  ctx->block_list = block;

  block->ctx = ctx;
  block->code_slab = code_slab;

  block->code_start = gum_slab_cursor (&code_slab->slab);

  gum_stalker_thaw (stalker, block->code_start, code_available);

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
}

static void
gum_exec_block_invalidate (GumExecBlock * block)
{
  GumExecCtx * ctx = block->ctx;
  GumStalker * stalker = ctx->stalker;
  const gsize trampoline_size = GUM_INVALIDATE_TRAMPOLINE_SIZE;

  gum_stalker_thaw (stalker, block->code_start, trampoline_size);

  if ((block->flags & GUM_EXEC_BLOCK_THUMB) != 0)
  {
    GumThumbWriter * cw = &ctx->thumb_writer;

    gum_thumb_writer_reset (cw, block->code_start);

    gum_thumb_writer_put_nop (cw);
    gum_thumb_writer_put_push_regs (cw, 2, ARM_REG_R0, ARM_REG_LR);
    gum_thumb_writer_put_bl_imm (cw,
        GUM_ADDRESS (block->code_slab->thumb_invalidator));
    gum_thumb_writer_put_bytes (cw, (guint8 *) &block, sizeof (GumExecBlock *));

    gum_thumb_writer_flush (cw);
    g_assert (gum_thumb_writer_offset (cw) == trampoline_size);
  }
  else
  {
    GumArmWriter * cw = &ctx->arm_writer;

    gum_arm_writer_reset (cw, block->code_start);

    gum_arm_writer_put_push_regs (cw, 1, ARM_REG_LR);
    gum_arm_writer_put_bl_imm (cw,
        GUM_ADDRESS (block->code_slab->arm_invalidator));
    gum_arm_writer_put_bytes (cw, (guint8 *) &block, sizeof (GumExecBlock *));

    gum_arm_writer_flush (cw);
    g_assert (gum_arm_writer_offset (cw) == trampoline_size);
  }

  gum_stalker_freeze (stalker, block->code_start, trampoline_size);
}

static gpointer
gum_exec_block_encode_instruction_pointer (const GumExecBlock * block,
                                           gpointer ptr)
{
  gpointer result = ptr;

  if ((block->flags & GUM_EXEC_BLOCK_THUMB) != 0)
    result = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (result) | 1);

  return result;
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
gum_exec_ctx_backpatch_arm_branch_to_current (GumExecBlock * block,
                                              GumExecBlock * from,
                                              gpointer from_insn,
                                              gsize code_offset,
                                              GumPrologState opened_prolog)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  gpointer target;
  guint8 * code_start = from->code_start + code_offset;
  const gsize code_max_size = from->code_size - code_offset;
  GumArmWriter * cw;

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

  cw = &ctx->arm_writer;
  gum_arm_writer_reset (cw, code_start);

  if (opened_prolog == GUM_PROLOG_OPEN)
    gum_exec_ctx_write_arm_epilog (ctx, cw);

  gum_arm_writer_put_branch_address (cw, GUM_ADDRESS (target));

  gum_arm_writer_flush (cw);
  g_assert (gum_arm_writer_offset (cw) <= code_max_size);
  gum_stalker_freeze (ctx->stalker, code_start, code_max_size);

  gum_spinlock_release (&ctx->code_lock);

  if (ctx->observer)
  {
    GumBackpatch p;

    p.type = GUM_BACKPATCH_ARM;
    p.to = block->real_start;
    p.from = from->real_start;
    p.from_insn = from_insn;
    p.code_offset = code_offset;
    p.opened_prolog = opened_prolog;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

static void
gum_exec_ctx_backpatch_thumb_branch_to_current (GumExecBlock * block,
                                                GumExecBlock * from,
                                                gpointer from_insn,
                                                gsize code_offset,
                                                GumPrologState opened_prolog)
{
  gboolean just_unfollowed;
  GumExecCtx * ctx;
  gpointer target;
  guint8 * code_start = from->code_start + code_offset;
  const gsize code_max_size = from->code_size - code_offset;
  GumThumbWriter * cw;

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

  cw = &ctx->thumb_writer;
  gum_thumb_writer_reset (cw, code_start);

  if (opened_prolog == GUM_PROLOG_OPEN)
    gum_exec_ctx_write_thumb_epilog (ctx, cw);

  gum_thumb_writer_put_branch_address (cw, GUM_ADDRESS (target));

  gum_thumb_writer_flush (cw);
  g_assert (gum_thumb_writer_offset (cw) <= code_max_size);
  gum_stalker_freeze (ctx->stalker, code_start, code_max_size);

  gum_spinlock_release (&ctx->code_lock);

  if (ctx->observer)
  {
    GumBackpatch p;

    p.type = GUM_BACKPATCH_THUMB;
    p.to = block->real_start;
    p.from = from->real_start;
    p.from_insn = from_insn;
    p.code_offset = code_offset;
    p.opened_prolog = opened_prolog;

    gum_stalker_observer_notify_backpatch (ctx->observer, &p, sizeof (p));
  }
}

static void
gum_exec_block_virtualize_arm_branch_insn (GumExecBlock * block,
                                           const GumBranchTarget * target,
                                           arm_cc cc,
                                           GumWriteback * writeback,
                                           GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;
  GumArmWriter * cw = gc->arm_writer;
  GumPrologState backpatch_prolog_state;
  GumAddress backpatch_code_start;

  gum_exec_block_write_arm_handle_not_taken (block, target, cc, gc);

  if ((ec->sink_mask & GUM_EXEC) != 0 &&
      (block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) == 0)
  {
    gum_exec_block_arm_open_prolog (block, gc);
    backpatch_prolog_state = GUM_PROLOG_OPEN;

    gum_exec_block_write_arm_exec_event_code (block, gc);
  }
  else
  {
    backpatch_prolog_state = GUM_PROLOG_CLOSED;
  }

  backpatch_code_start = cw->pc;

  if (backpatch_prolog_state == GUM_PROLOG_CLOSED)
    gum_exec_block_arm_open_prolog (block, gc);

  gum_exec_block_write_arm_handle_excluded (block, target, FALSE, gc);
  gum_exec_block_write_arm_handle_kuser_helper (block, target, gc);
  gum_exec_block_write_arm_call_switch_block (block, target, gc);
  gum_exec_block_write_arm_pop_stack_frame (block, target, gc);

  if (ec->stalker->trust_threshold >= 0 &&
      target->type == GUM_TARGET_DIRECT_ADDRESS &&
      !gum_is_thumb (target->value.direct_address.address))
  {
    gum_arm_writer_put_ldr_reg_address (cw, ARM_REG_R5,
        GUM_ADDRESS (&block->ctx->current_block));
    gum_arm_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R5,
        ARM_REG_R5, 0);

    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_backpatch_arm_branch_to_current), 5,
        GUM_ARG_REGISTER, ARM_REG_R5,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
        GUM_ARG_ADDRESS, backpatch_code_start - GUM_ADDRESS (block->code_start),
        GUM_ARG_ADDRESS, GUM_ADDRESS (backpatch_prolog_state));
  }

  gum_exec_block_arm_close_prolog (block, gc);

  gum_exec_block_write_arm_handle_writeback (block, writeback, gc);
  gum_exec_block_write_arm_exec_generated_code (gc->arm_writer, block->ctx);
}

static void
gum_exec_block_virtualize_thumb_branch_insn (GumExecBlock * block,
                                             const GumBranchTarget * target,
                                             arm_cc cc,
                                             arm_reg cc_reg,
                                             GumWriteback * writeback,
                                             GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;
  GumThumbWriter * cw = gc->thumb_writer;
  GumPrologState backpatch_prolog_state;
  GumAddress backpatch_code_start;

  gum_exec_block_write_thumb_handle_not_taken (block, target, cc, cc_reg, gc);

  if ((ec->sink_mask & GUM_EXEC) != 0 &&
      (block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) == 0)
  {
    gum_exec_block_thumb_open_prolog (block, gc);
    backpatch_prolog_state = GUM_PROLOG_OPEN;

    gum_exec_block_write_thumb_exec_event_code (block, gc);
  }
  else
  {
    backpatch_prolog_state = GUM_PROLOG_CLOSED;
  }

  backpatch_code_start = cw->pc;

  if (backpatch_prolog_state == GUM_PROLOG_CLOSED)
    gum_exec_block_thumb_open_prolog (block, gc);

  gum_exec_block_write_thumb_handle_excluded (block, target, FALSE, gc);
  gum_exec_block_write_thumb_handle_kuser_helper (block, target, gc);

  gum_exec_block_write_thumb_call_switch_block (block, target, gc);
  gum_exec_block_write_thumb_pop_stack_frame (block, target, gc);

  if (ec->stalker->trust_threshold >= 0 &&
      target->type == GUM_TARGET_DIRECT_ADDRESS &&
      gum_is_thumb (target->value.direct_address.address))
  {
    gum_thumb_writer_put_ldr_reg_address (cw, ARM_REG_R5,
        GUM_ADDRESS (&block->ctx->current_block));
    gum_thumb_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R5,
        ARM_REG_R5, 0);

    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_backpatch_thumb_branch_to_current), 5,
        GUM_ARG_REGISTER, ARM_REG_R5,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
        GUM_ARG_ADDRESS, backpatch_code_start - GUM_ADDRESS (block->code_start),
        GUM_ARG_ADDRESS, GUM_ADDRESS (backpatch_prolog_state));
  }

  gum_exec_block_thumb_close_prolog (block, gc);

  gum_exec_block_write_thumb_handle_writeback (block, writeback, gc);
  gum_exec_block_write_thumb_exec_generated_code (cw, block->ctx);

  /*
   * We MUST do this last to account for IT blocks.
   * gum_thumb_relocator_skip_one() will complete the IT branch, so if we do
   * this early (like on ARM), then the end branch will be relocated into the
   * middle of the relocated branch.
   */
  gum_thumb_relocator_skip_one (gc->thumb_relocator);
}

static void
gum_exec_block_virtualize_arm_call_insn (GumExecBlock * block,
                                         const GumBranchTarget * target,
                                         arm_cc cc,
                                         GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;
  gpointer ret_real_address = gc->instruction->end;

  gum_exec_block_write_arm_handle_not_taken (block, target, cc, gc);

  gum_exec_block_arm_open_prolog (block, gc);

  if ((ec->sink_mask & GUM_EXEC) != 0)
    gum_exec_block_write_arm_exec_event_code (block, gc);

  if ((ec->sink_mask & GUM_CALL) != 0)
    gum_exec_block_write_arm_call_event_code (block, target, gc);

  gum_exec_block_write_arm_handle_excluded (block, target, TRUE, gc);
  gum_exec_block_write_arm_push_stack_frame (block, ret_real_address, gc);
  gum_exec_block_write_arm_call_switch_block (block, target, gc);

  gum_exec_block_arm_close_prolog (block, gc);

  gum_arm_writer_put_ldr_reg_address (gc->arm_writer, ARM_REG_LR,
      GUM_ADDRESS (ret_real_address));
  gum_exec_block_write_arm_exec_generated_code (gc->arm_writer, block->ctx);
}

static void
gum_exec_block_virtualize_thumb_call_insn (GumExecBlock * block,
                                           const GumBranchTarget * target,
                                           GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;
  gpointer ret_real_address = gc->instruction->end + 1;

  gum_exec_block_thumb_open_prolog (block, gc);

  if ((ec->sink_mask & GUM_EXEC) != 0)
    gum_exec_block_write_thumb_exec_event_code (block, gc);

  if ((ec->sink_mask & GUM_CALL) != 0)
    gum_exec_block_write_thumb_call_event_code (block, target, gc);

  gum_exec_block_write_thumb_handle_excluded (block, target, TRUE, gc);
  gum_exec_block_write_thumb_push_stack_frame (block, ret_real_address, gc);
  gum_exec_block_write_thumb_call_switch_block (block, target, gc);

  gum_exec_block_thumb_close_prolog (block, gc);

  gum_thumb_writer_put_ldr_reg_address (gc->thumb_writer, ARM_REG_LR,
      GUM_ADDRESS (ret_real_address));
  gum_exec_block_write_thumb_exec_generated_code (gc->thumb_writer, block->ctx);

  /*
   * We MUST do this last to account for IT blocks.
   * gum_thumb_relocator_skip_one() will complete the IT branch, so if we do
   * this early (like on ARM), then the end branch will be relocated into the
   * middle of the relocated branch.
   */
  gum_thumb_relocator_skip_one (gc->thumb_relocator);
}

static void
gum_exec_block_virtualize_arm_ret_insn (GumExecBlock * block,
                                        const GumBranchTarget * target,
                                        arm_cc cc,
                                        gboolean pop,
                                        guint16 mask,
                                        GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;

  gum_exec_block_write_arm_handle_not_taken (block, target, cc, gc);

  gum_exec_block_arm_open_prolog (block, gc);

  if ((ec->sink_mask & GUM_EXEC) != 0)
    gum_exec_block_write_arm_exec_event_code (block, gc);

  gum_exec_block_write_arm_pop_stack_frame (block, target, gc);

  if ((ec->sink_mask & GUM_RET) != 0)
    gum_exec_block_write_arm_ret_event_code (block, target, gc);

  gum_exec_block_write_arm_call_switch_block (block, target, gc);

  gum_exec_block_arm_close_prolog (block, gc);

  /*
   * If the instruction we are virtualizing is a POP (or indeed LDMIA)
   * instruction, then as well as determining the location at which control flow
   * should continue, we must ensure we load any other registers in the register
   * list of the instruction from the stack. Lastly, we must increment that
   * stack pointer to remove the value of PC which would have been restored,
   * since we will instead control PC to continue execution of instrumented
   * code.
   */
  if (pop)
  {
    const GumBranchIndirectRegOffset * tv = &target->value.indirect_reg_offset;

    g_assert (target->type == GUM_TARGET_INDIRECT_REG_OFFSET);

    if (mask != 0)
    {
      if (tv->write_back)
        gum_arm_writer_put_ldmia_reg_mask_wb (gc->arm_writer, tv->reg, mask);
      else
        gum_arm_writer_put_ldmia_reg_mask (gc->arm_writer, tv->reg, mask);
    }

    if (tv->write_back)
    {
      gum_arm_writer_put_add_reg_reg_imm (gc->arm_writer, tv->reg, tv->reg,
          4);
    }
  }

  gum_exec_block_write_arm_exec_generated_code (gc->arm_writer, block->ctx);

  gum_arm_relocator_skip_one (gc->arm_relocator);
}

static void
gum_exec_block_virtualize_thumb_ret_insn (GumExecBlock * block,
                                          const GumBranchTarget * target,
                                          gboolean pop,
                                          guint16 mask,
                                          GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;

  gum_exec_block_thumb_open_prolog (block, gc);

  if ((ec->sink_mask & GUM_EXEC) != 0)
    gum_exec_block_write_thumb_exec_event_code (block, gc);

  gum_exec_block_write_thumb_pop_stack_frame (block, target, gc);

  if ((ec->sink_mask & GUM_RET) != 0)
    gum_exec_block_write_thumb_ret_event_code (block, target, gc);

  gum_exec_block_write_thumb_call_switch_block (block, target, gc);

  gum_exec_block_thumb_close_prolog (block, gc);

  if (pop)
  {
    const GumBranchIndirectRegOffset * tv = &target->value.indirect_reg_offset;
    guint displacement = 4;

    g_assert (target->type == GUM_TARGET_INDIRECT_REG_OFFSET);

    if (mask != 0)
    {
      if (gum_count_bits_set (mask) == 1)
      {
        arm_reg target_register = ARM_REG_R0 + gum_count_trailing_zeros (mask);
        gum_thumb_writer_put_ldr_reg_reg_offset (gc->thumb_writer,
            target_register, tv->reg, 0);
        displacement += 4;
      }
      else
      {
        gum_thumb_writer_put_ldmia_reg_mask (gc->thumb_writer, tv->reg, mask);
      }
    }

    gum_thumb_writer_put_add_reg_reg_imm (gc->thumb_writer, tv->reg, tv->reg,
        displacement);
  }

  gum_exec_block_write_thumb_exec_generated_code (gc->thumb_writer, block->ctx);
}

static void
gum_exec_block_virtualize_arm_svc_insn (GumExecBlock * block,
                                        GumGeneratorContext * gc)
{
  gum_exec_block_dont_virtualize_arm_insn (block, gc);

#ifdef HAVE_LINUX
  {
    GumArmWriter * cw = gc->arm_writer;
    gconstpointer not_cloned_child = cw->code + 1;

    /* Save the flags */
    gum_arm_writer_put_push_regs (cw, 1, ARM_REG_R1);
    gum_arm_writer_put_mov_reg_cpsr (cw, ARM_REG_R1);

    /* Check the SVC number */
    gum_arm_writer_put_cmp_reg_imm (cw, ARM_REG_R7, __NR_clone);
    gum_arm_writer_put_b_cond_label (cw, ARM_CC_NE, not_cloned_child);

    /* Check the returned TID */
    gum_arm_writer_put_cmp_reg_imm (cw, ARM_REG_R0, 0);
    gum_arm_writer_put_b_cond_label (cw, ARM_CC_NE, not_cloned_child);

    /* Restore the flags */
    gum_arm_writer_put_mov_cpsr_reg (cw, ARM_REG_R1);
    gum_arm_writer_put_pop_regs (cw, 1, ARM_REG_R1);

    /* Vector to the original next instruction */
    gum_arm_writer_put_push_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);
    gum_arm_writer_put_ldr_reg_address (cw, ARM_REG_R0,
        GUM_ADDRESS (gc->instruction->end));
    gum_arm_writer_put_str_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_SP, 4);
    gum_arm_writer_put_pop_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);

    gum_arm_writer_put_label (cw, not_cloned_child);

    /* Restore the flags */
    gum_arm_writer_put_mov_cpsr_reg (cw, ARM_REG_R1);
    gum_arm_writer_put_pop_regs (cw, 1, ARM_REG_R1);
  }
#endif
}

static void
gum_exec_block_virtualize_thumb_svc_insn (GumExecBlock * block,
                                          GumGeneratorContext * gc)
{
  gum_exec_block_dont_virtualize_thumb_insn (block, gc);

#ifdef HAVE_LINUX
  {
    GumThumbWriter * cw = gc->thumb_writer;
    gconstpointer goto_not_cloned_child = cw->code + 1;
    gconstpointer cloned_child = cw->code + 2;
    gconstpointer not_cloned_child = cw->code + 3;

    /* Save the SVC number */
    gum_thumb_writer_put_push_regs (cw, 1, ARM_REG_R7);

    /* Check the SVC number */
    gum_thumb_writer_put_sub_reg_imm (cw, ARM_REG_R7, __NR_clone);
    gum_thumb_writer_put_cbnz_reg_label (cw, ARM_REG_R7, goto_not_cloned_child);

    /* Check the returned TID */
    gum_thumb_writer_put_cbnz_reg_label (cw, ARM_REG_R0, goto_not_cloned_child);
    gum_thumb_writer_put_b_label (cw, cloned_child);

    gum_thumb_writer_put_label (cw, goto_not_cloned_child);
    gum_thumb_writer_put_b_label (cw, not_cloned_child);

    gum_thumb_writer_put_label (cw, cloned_child);
    /* Restore the SVC number */
    gum_thumb_writer_put_pop_regs (cw, 1, ARM_REG_R7);

    /* Vector to the original next instruction */

    /*
     * We can't push PC in Thumb encoding without Thumb 2, and we clobber the
     * value so it doesn't matter what we push. It ends up popped back into PC.
     */
    gum_thumb_writer_put_push_regs (cw, 2, ARM_REG_R0, ARM_REG_R1);
    gum_thumb_writer_put_ldr_reg_address (cw, ARM_REG_R0,
        GUM_ADDRESS (gc->instruction->end + 1));
    gum_thumb_writer_put_str_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_SP, 4);
    gum_thumb_writer_put_pop_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);

    gum_thumb_writer_put_label (cw, not_cloned_child);

    /* Restore the SVC number */
    gum_thumb_writer_put_pop_regs (cw, 1, ARM_REG_R7);
  }
#endif
}

static void
gum_exec_block_write_arm_handle_kuser_helper (GumExecBlock * block,
                                              const GumBranchTarget * target,
                                              GumGeneratorContext * gc)
{
#ifdef HAVE_LINUX
  GumExecCtx * ctx = block->ctx;
  GumArmWriter * cw = gc->arm_writer;
  gconstpointer not_kuh = cw->code + 1;
  GumBranchTarget ret_target;

  /*
   * The kuser_helper is a mechanism implemented by the Linux kernel to expose a
   * page of code into the user address space so that it can be used by glibc in
   * order to carry out a number of heavily architecture specific operations
   * without having to perform architecture detection of its own (see
   * https://www.kernel.org/doc/Documentation/arm/kernel_user_helpers.txt).
   *
   * When running in qemu-user (which is useful for running target code on the
   * bench, or simply testing when you don't have access to a target), since
   * qemu-user is emulating the ARM instructions on an alterative architecture
   * (likely x86_64) then the code page exposed by the host kernel will be of
   * the native architecture accordingly. Rather than attempting to emulate this
   * very machine specific code, QEMU instead detects when the application
   * attempts to execute one of these handlers (see the function do_kernel_trap
   * in https://github.com/qemu/qemu/blob/master/linux-user/arm/cpu_loop.c) and
   * performs the necessary emulation on behalf of the application. Thus it is
   * not possible to read the page at this address and retrieve ARM code to be
   * instrumented.
   *
   * Rather than attempt to detect the target on which we are running, as it is
   * vanishingly unlikely that a user will care to stalk this platform specific
   * code we simply execute it outside of the Stalker engine, similar to the way
   * in which an excluded range is handled.
   */

  /*
   * If the branch target is deterministic (e.g. not based on register
   * contents), we can perform the check during instrumentation rather than at
   * runtime and omit this code if we can determine we will not enter a
   * kuser_helper.
   */
  if (target->type == GUM_TARGET_DIRECT_ADDRESS)
  {
    if (!gum_is_kuser_helper (target->value.direct_address.address))
      return;
  }

  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
  {
    gum_exec_ctx_write_arm_mov_branch_target (block->ctx, target, ARM_REG_R0,
        gc);
    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_is_kuser_helper), 1,
        GUM_ARG_REGISTER, ARM_REG_R0);
    gum_arm_writer_put_cmp_reg_imm (cw, ARM_REG_R0, 0);
    gum_arm_writer_put_b_cond_label (cw, ARM_CC_EQ, not_kuh);
  }

  gum_exec_ctx_write_arm_mov_branch_target (block->ctx, target, ARM_REG_R0, gc);
  gum_arm_writer_put_ldr_reg_address (cw, ARM_REG_R1,
      GUM_ADDRESS (&ctx->kuh_target));
  gum_arm_writer_put_str_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_R1, 0);

  gum_exec_block_arm_close_prolog (block, gc);

  gum_arm_writer_put_ldr_reg_address (cw, ARM_REG_R12,
      GUM_ADDRESS (&ctx->kuh_target));
  gum_arm_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R12, ARM_REG_R12, 0);

  /*
   * Unlike an excluded range, where the function is executed by a call
   * instruction. It is quite common for kuser_helpers to instead be executed by
   * the following instruction sequence.
   *
   * mvn r0, #0xf000
   * sub pc, r0, #0x1f
   *
   * This is effectively a tail call within a thunk function. But as we can see
   * when we vector to the kuser_helper we actually find a branch instruction
   * and not a call and hence were we to simply emit it, then we would not be
   * able to return to the Stalker engine after it has completed in order to
   * continue stalking. We must therefore emit a call instead.
   *
   * Note, however that per the documentation at kernel.org, the helpers are in
   * fact functions and so we can simply call and they will return control back
   * to the address contained in LR. One last thing to note here, is that we
   * store and restore the application LR value either side of our call so that
   * we can preserve it. This does have the effect of changing the normal
   * application stack pointer during the duration of the call, but the
   * documentation states that all of the input and output make use of registers
   * rather than the stack.
   */
  gum_arm_writer_put_push_regs (cw, 1, ARM_REG_LR);
  gum_arm_writer_put_call_reg (cw, ARM_REG_R12);
  gum_arm_writer_put_pop_regs (cw, 1, ARM_REG_LR);

  gum_exec_block_arm_open_prolog (block, gc);

  ret_target.type = GUM_TARGET_DIRECT_REG_OFFSET;
  ret_target.value.direct_reg_offset.reg = ARM_REG_LR;
  ret_target.value.direct_reg_offset.offset = 0;
  ret_target.value.direct_reg_offset.mode = GUM_ARM_MODE_AUTO;

  /*
   * We pop the stack frame here since the actual kuser_helper will have been
   * called by a thunk which looks like this:
   *
   * thunk_EXT_FUN_ffff0fe0:
   *     mvn r0, #0xf000
   *     sub pc => SUB_ffff0fe0, r0, #0x1f
   *
   * This will result in the stack frame being pushed for the call to this
   * thunk. Since this performs a tail call, and we don't stalk the actual
   * helper, we don't instrument the eventual return instruction and hence
   * include the stack pop at that point. We therefore pop the stack here
   * to make things line up.
   */
  gum_exec_block_write_arm_pop_stack_frame (block, &ret_target, gc);
  gum_exec_block_write_arm_call_switch_block (block, &ret_target, gc);
  gum_exec_block_arm_close_prolog (block, gc);

  gum_exec_block_write_arm_exec_generated_code (cw, block->ctx);

  gum_arm_writer_put_breakpoint (cw);

  /*
   * This label is only required if we weren't able to determine at
   * instrumentation time whether the target was a kuser_helper. If we could
   * then we either emit the handler if it is, or do nothing if not.
   */
  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
    gum_arm_writer_put_label (cw, not_kuh);
#endif
}

static void
gum_exec_block_write_thumb_handle_kuser_helper (GumExecBlock * block,
                                                const GumBranchTarget * target,
                                                GumGeneratorContext * gc)
{
#ifdef HAVE_LINUX
  GumExecCtx * ctx = block->ctx;
  GumThumbWriter * cw = gc->thumb_writer;
  gconstpointer kuh = cw->code + 1;
  gconstpointer not_kuh = cw->code + 2;
  GumBranchTarget ret_target;

  if (target->type == GUM_TARGET_DIRECT_ADDRESS)
  {
    if (!gum_is_kuser_helper (target->value.direct_address.address))
      return;
  }

  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
  {
    gum_exec_ctx_write_thumb_mov_branch_target (block->ctx,
        target, ARM_REG_R0, gc);
    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_is_kuser_helper), 1,
        GUM_ARG_REGISTER, ARM_REG_R0);
    gum_thumb_writer_put_cbnz_reg_label (cw, ARM_REG_R0, kuh);
    gum_thumb_writer_put_b_label (cw, not_kuh);
    gum_thumb_writer_put_label (cw, kuh);
  }

  gum_exec_ctx_write_thumb_mov_branch_target (block->ctx, target, ARM_REG_R0,
      gc);
  gum_thumb_writer_put_ldr_reg_address (cw, ARM_REG_R1,
      GUM_ADDRESS (&ctx->kuh_target));
  gum_thumb_writer_put_str_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_R1, 0);

  gum_exec_block_thumb_close_prolog (block, gc);

  gum_thumb_writer_put_ldr_reg_address (cw, ARM_REG_R12,
      GUM_ADDRESS (&ctx->kuh_target));
  gum_thumb_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R12, ARM_REG_R12, 0);

  gum_thumb_writer_put_push_regs (cw, 1, ARM_REG_LR);
  gum_thumb_writer_put_bx_reg (cw, ARM_REG_R12);
  gum_thumb_writer_put_pop_regs (cw, 1, ARM_REG_LR);

  gum_exec_block_thumb_open_prolog (block, gc);

  ret_target.type = GUM_TARGET_DIRECT_REG_OFFSET;
  ret_target.value.direct_reg_offset.reg = ARM_REG_LR;
  ret_target.value.direct_reg_offset.offset = 0;
  ret_target.value.direct_reg_offset.mode = GUM_ARM_MODE_AUTO;

  gum_exec_block_write_thumb_pop_stack_frame (block, &ret_target, gc);
  gum_exec_block_write_thumb_call_switch_block (block, &ret_target,
      gc);
  gum_exec_block_thumb_close_prolog (block, gc);

  gum_exec_block_write_thumb_exec_generated_code (cw, block->ctx);

  gum_thumb_writer_put_breakpoint (cw);

  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
    gum_thumb_writer_put_label (cw, not_kuh);
#endif
}

static void
gum_exec_block_write_arm_call_switch_block (GumExecBlock * block,
                                            const GumBranchTarget * target,
                                            GumGeneratorContext * gc)
{
  gum_exec_ctx_write_arm_mov_branch_target (block->ctx, target, ARM_REG_R2, gc);
  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));
}

static void
gum_exec_block_write_thumb_call_switch_block (GumExecBlock * block,
                                              const GumBranchTarget * target,
                                              GumGeneratorContext * gc)
{
  gum_exec_ctx_write_thumb_mov_branch_target (block->ctx, target, ARM_REG_R2,
      gc);
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start) + 1);
}

static void
gum_exec_block_dont_virtualize_arm_insn (GumExecBlock * block,
                                         GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;

  if ((ec->sink_mask & GUM_EXEC) != 0)
  {
    gum_exec_block_arm_open_prolog (block, gc);
    gum_exec_block_write_arm_exec_event_code (block, gc);
    gum_exec_block_arm_close_prolog (block, gc);
  }

  gum_arm_relocator_write_all (gc->arm_relocator);
}

static void
gum_exec_block_dont_virtualize_thumb_insn (GumExecBlock * block,
                                           GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;

  if ((ec->sink_mask & GUM_EXEC) != 0)
  {
    gum_exec_block_thumb_open_prolog (block, gc);
    gum_exec_block_write_thumb_exec_event_code (block, gc);
    gum_exec_block_thumb_close_prolog (block, gc);
  }

  gum_thumb_relocator_write_all (gc->thumb_relocator);
}

static void
gum_exec_block_write_arm_handle_excluded (GumExecBlock * block,
                                          const GumBranchTarget * target,
                                          gboolean call,
                                          GumGeneratorContext * gc)
{
  GumArmWriter * cw = gc->arm_writer;
  gconstpointer not_excluded = cw->code + 1;
  GumCheckExcludedFunc check;

  if (call)
    check = gum_stalker_is_call_excluding;
  else
    check = gum_stalker_is_branch_excluding;

  /*
   * If the branch target is deterministic (e.g. not based on register
   * contents). We can perform the check during instrumentation rather than at
   * runtime and omit this code if we can determine we will not enter an
   * excluded range.
   */
  if (target->type == GUM_TARGET_DIRECT_ADDRESS)
  {
    if (!check (block->ctx, target->value.direct_address.address))
    {
      gum_arm_relocator_skip_one (gc->arm_relocator);
      return;
    }
  }

  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
  {
    gum_exec_ctx_write_arm_mov_branch_target (block->ctx, target, ARM_REG_R1,
        gc);
    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (check), 2,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
        GUM_ARG_REGISTER, ARM_REG_R1);
    gum_arm_writer_put_cmp_reg_imm (cw, ARM_REG_R0, 0);
    gum_arm_writer_put_b_cond_label (cw, ARM_CC_EQ, not_excluded);
  }

  if (call)
  {
    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_begin_call), 2,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->end));
  }

  gum_exec_block_arm_close_prolog (block, gc);

  /* Emit the original instruction (relocated) */
  gum_arm_relocator_write_one (gc->arm_relocator);

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  if (call)
  {
    gum_metal_hash_table_insert (block->ctx->excluded_calls, cw->code,
        gc->instruction->end);
  }
#endif

  gum_exec_block_arm_open_prolog (block, gc);

  if (call)
  {
    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_end_call), 1,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx));
  }

  gum_exec_block_write_arm_handle_continue (block, gc);

  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
    gum_arm_writer_put_label (cw, not_excluded);
}

static void
gum_exec_block_write_thumb_handle_excluded (GumExecBlock * block,
                                            const GumBranchTarget * target,
                                            gboolean call,
                                            GumGeneratorContext * gc)
{
  GumThumbWriter * cw = gc->thumb_writer;
  gsize unique_id = GPOINTER_TO_SIZE (cw->code) << 1;
  gconstpointer is_excluded = GSIZE_TO_POINTER (unique_id | 1);
  gconstpointer not_excluded = GSIZE_TO_POINTER (unique_id | 0);
  GumCheckExcludedFunc check;

  if (call)
    check = gum_stalker_is_call_excluding;
  else
    check = gum_stalker_is_branch_excluding;

  if (target->type == GUM_TARGET_DIRECT_ADDRESS)
  {
    if (!check (block->ctx, target->value.direct_address.address))
      return;
  }

  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
  {
    gum_exec_ctx_write_thumb_mov_branch_target (block->ctx, target, ARM_REG_R1,
        gc);
    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (check), 2,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
        GUM_ARG_REGISTER, ARM_REG_R1);
    gum_thumb_writer_put_cbnz_reg_label (cw, ARM_REG_R0, is_excluded);
    gum_thumb_writer_put_b_label (cw, not_excluded);
    gum_thumb_writer_put_label (cw, is_excluded);
  }

  if (call)
  {
    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_begin_call), 2,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->end + 1));
  }

  gum_exec_block_thumb_close_prolog (block, gc);

  gum_thumb_relocator_copy_one (gc->thumb_relocator);

#ifdef GUM_HAVE_UNWIND_PC_TRANSLATION
  if (call)
  {
    gum_metal_hash_table_insert (block->ctx->excluded_calls, cw->code,
        gc->instruction->end);
  }
#endif

  gum_exec_block_thumb_open_prolog (block, gc);

  if (call)
  {
    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_end_call), 1,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx));
  }

  gum_exec_block_write_thumb_handle_continue (block, gc);

  if (target->type != GUM_TARGET_DIRECT_ADDRESS)
    gum_thumb_writer_put_label (cw, not_excluded);
}

static void
gum_exec_block_write_arm_handle_not_taken (GumExecBlock * block,
                                           const GumBranchTarget * target,
                                           arm_cc cc,
                                           GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;
  GumArmWriter * cw = gc->arm_writer;
  gconstpointer taken = cw->code + 1;
  GumPrologState backpatch_prolog_state;
  GumAddress backpatch_code_start;

  /*
   * Many ARM instructions can be conditionally executed based upon the state of
   * register flags. If our instruction is not always executed (ARM_CC_AL), we
   * emit a branch with the same condition code as the original instruction to
   * bypass the continuation handler.
   */

  if (cc == ARM_CC_AL)
    return;

  gum_arm_writer_put_b_cond_label (cw, cc, taken);

  /*
   * If the branch is not taken on account that the instruction is conditionally
   * executed, then emit any necessary events and continue execution by
   * instrumenting and vectoring to the block immediately after the conditional
   * instruction.
   */

  if ((ec->sink_mask & GUM_EXEC) != 0 &&
      (block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) == 0)
  {
    gum_exec_block_arm_open_prolog (block, gc);
    backpatch_prolog_state = GUM_PROLOG_OPEN;

    gum_exec_block_write_arm_exec_event_code (block, gc);
  }
  else
  {
    backpatch_prolog_state = GUM_PROLOG_CLOSED;
  }

  backpatch_code_start = cw->pc;

  if (backpatch_prolog_state == GUM_PROLOG_CLOSED)
    gum_exec_block_arm_open_prolog (block, gc);

  gum_arm_writer_put_call_address_with_arguments (cw,
      GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (ec),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->end),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  if (ec->stalker->trust_threshold >= 0 &&
      target->type == GUM_TARGET_DIRECT_ADDRESS &&
      !gum_is_thumb (target->value.direct_address.address))
  {
    const guint padding_size = 4;
    guint i;

    for (i = 0; i != padding_size; i++)
      gum_arm_writer_put_nop (cw);

    gum_arm_writer_put_ldr_reg_address (cw, ARM_REG_R5,
        GUM_ADDRESS (&block->ctx->current_block));
    gum_arm_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R5,
        ARM_REG_R5, 0);

    gum_arm_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_backpatch_arm_branch_to_current), 5,
        GUM_ARG_REGISTER, ARM_REG_R5,
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
        GUM_ARG_ADDRESS, backpatch_code_start - GUM_ADDRESS (block->code_start),
        GUM_ARG_ADDRESS, GUM_ADDRESS (backpatch_prolog_state));
  }

  gum_exec_block_arm_close_prolog (block, gc);

  gum_exec_block_write_arm_exec_generated_code (cw, ec);

  gum_arm_writer_put_label (cw, taken);
}

static void
gum_exec_block_write_thumb_handle_not_taken (GumExecBlock * block,
                                             const GumBranchTarget * target,
                                             arm_cc cc,
                                             arm_reg cc_reg,
                                             GumGeneratorContext * gc)
{
  GumExecCtx * ec = block->ctx;
  GumThumbWriter * cw = gc->thumb_writer;
  gconstpointer cb_not_taken = cw->code + 1;
  gconstpointer taken = cw->code + 2;

  if (cc_reg != ARM_REG_INVALID)
  {
    if (cc == ARM_CC_EQ)
      gum_thumb_writer_put_cbnz_reg_label (cw, cc_reg, cb_not_taken);
    else if (cc == ARM_CC_NE)
      gum_thumb_writer_put_cbz_reg_label (cw, cc_reg, cb_not_taken);
    else
      g_assert_not_reached ();

    gum_thumb_writer_put_b_label_wide (cw, taken);

    gum_thumb_writer_put_label (cw, cb_not_taken);
  }
  else if (cc != ARM_CC_AL)
  {
    gum_thumb_writer_put_b_cond_label_wide (cw, cc, taken);
  }

  if (cc != ARM_CC_AL)
  {
    GumPrologState backpatch_prolog_state;
    GumAddress backpatch_code_start;

    if ((ec->sink_mask & GUM_EXEC) != 0 &&
        (block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) == 0)
    {
      gum_exec_block_thumb_open_prolog (block, gc);
      backpatch_prolog_state = GUM_PROLOG_OPEN;

      gum_exec_block_write_thumb_exec_event_code (block, gc);
    }
    else
    {
      backpatch_prolog_state = GUM_PROLOG_CLOSED;
    }

    backpatch_code_start = cw->pc;

    if (backpatch_prolog_state == GUM_PROLOG_CLOSED)
      gum_exec_block_thumb_open_prolog (block, gc);

    gum_thumb_writer_put_call_address_with_arguments (cw,
        GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
        GUM_ARG_ADDRESS, GUM_ADDRESS (ec),
        GUM_ARG_ADDRESS, GUM_ADDRESS (block),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->end + 1),
        GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start + 1));

    if (ec->stalker->trust_threshold >= 0 &&
        target->type == GUM_TARGET_DIRECT_ADDRESS &&
        gum_is_thumb (target->value.direct_address.address))
    {
      const guint padding_size = 5;
      guint i;

      for (i = 0; i != padding_size; i++)
        gum_thumb_writer_put_nop (cw);

      gum_thumb_writer_put_ldr_reg_address (cw, ARM_REG_R5,
          GUM_ADDRESS (&block->ctx->current_block));
      gum_thumb_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R5, ARM_REG_R5, 0);

      gum_thumb_writer_put_call_address_with_arguments (cw,
          GUM_ADDRESS (gum_exec_ctx_backpatch_thumb_branch_to_current), 5,
          GUM_ARG_REGISTER, ARM_REG_R5,
          GUM_ARG_ADDRESS, GUM_ADDRESS (block),
          GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
          GUM_ARG_ADDRESS,
            backpatch_code_start - GUM_ADDRESS (block->code_start),
          GUM_ARG_ADDRESS, GUM_ADDRESS (backpatch_prolog_state));
    }

    gum_exec_block_thumb_close_prolog (block, gc);

    gum_exec_block_write_thumb_exec_generated_code (cw, ec);

    gum_thumb_writer_put_label (cw, taken);
  }
}

static void
gum_exec_block_write_arm_handle_continue (GumExecBlock * block,
                                          GumGeneratorContext * gc)
{
  /*
   * Use the address of the end of the current instruction as the address of the
   * next block to execute. This is the case after handling a call instruction
   * to an excluded range whereby upon return you want to continue with the next
   * instruction, or when a conditional instruction is not executed resulting in
   * a branch not being taken. Instrument the block and then vector to it.
   */

  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->end),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start));

  gum_exec_block_arm_close_prolog (block, gc);

  gum_exec_block_write_arm_exec_generated_code (gc->arm_writer, block->ctx);
}

static void
gum_exec_block_write_thumb_handle_continue (GumExecBlock * block,
                                            GumGeneratorContext * gc)
{
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_ctx_switch_block), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->end + 1),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start + 1));

  gum_exec_block_thumb_close_prolog (block, gc);

  gum_exec_block_write_thumb_exec_generated_code (gc->thumb_writer, block->ctx);
}

static void
gum_exec_block_write_arm_handle_writeback (GumExecBlock * block,
                                           const GumWriteback * writeback,
                                           GumGeneratorContext * gc)
{
  GumArmWriter * cw = gc->arm_writer;
  gssize offset;

  if (writeback->target == ARM_REG_INVALID)
    return;

  offset = writeback->offset;
  if (offset >= 0)
    gum_arm_writer_put_add_reg_u32 (cw, writeback->target, offset);
  else
    gum_arm_writer_put_sub_reg_u32 (cw, writeback->target, -offset);
}

static void
gum_exec_block_write_thumb_handle_writeback (GumExecBlock * block,
                                             const GumWriteback * writeback,
                                             GumGeneratorContext * gc)
{
  GumThumbWriter * cw = gc->thumb_writer;
  gssize offset;

  if (writeback->target == ARM_REG_INVALID)
    return;

  offset = writeback->offset;

  if (offset >= 0)
    gum_thumb_writer_put_add_reg_imm (cw, writeback->target, offset);
  else
    gum_thumb_writer_put_sub_reg_imm (cw, writeback->target, -offset);
}

static void
gum_exec_block_write_arm_exec_generated_code (GumArmWriter * cw,
                                              GumExecCtx * ctx)
{
  /*
   * This function writes code to vector to the address of the last instrumented
   * block. Given that this code is emitted before the block is actually
   * instrumented, the value of ctx->resume_at will change between this code
   * being emitted and it being executed. It must therefore use &ctx->resume_at
   * and re-read the value from memory at runtime.
   *
   * This however means that we must use a scratch register to calculate the
   * final address. Given that this is also used to transition between blocks
   * within a function, we cannot rely upon the fact the R12 is a callee saved
   * register and clobber the value since it may be being used within the
   * function scope. We therefore push and pop this register to and from the
   * stack.
   */

  /*
   * Here we push the values of R0 which we will use as scratch space and PC we
   * will overwrite the value of PC on the stack at (SP+4) with the value from
   * resume_at before we subsequently pop both registers. This allows us to
   * branch to an arbitrary address without clobbering any registers.
   */
  gum_arm_writer_put_push_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);
  gum_arm_writer_put_ldr_reg_address (cw, ARM_REG_R0,
      GUM_ADDRESS (&ctx->resume_at));
  gum_arm_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_R0, 0);

  gum_arm_writer_put_str_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_SP, 4);
  gum_arm_writer_put_pop_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);
}

static void
gum_exec_block_write_thumb_exec_generated_code (GumThumbWriter * cw,
                                                GumExecCtx * ctx)
{
  /*
   * We can't push PC in Thumb encoding without Thumb 2, and we clobber the
   * value so it doesn't matter what we push. It ends up popped back into PC.
   */
  gum_thumb_writer_put_push_regs (cw, 2, ARM_REG_R0, ARM_REG_R1);
  gum_thumb_writer_put_ldr_reg_address (cw, ARM_REG_R0,
      GUM_ADDRESS (&ctx->resume_at));
  gum_thumb_writer_put_ldr_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_R0, 0);
  gum_thumb_writer_put_str_reg_reg_offset (cw, ARM_REG_R0, ARM_REG_SP, 4);
  gum_thumb_writer_put_pop_regs (cw, 2, ARM_REG_R0, ARM_REG_PC);
}

static void
gum_exec_block_write_arm_call_event_code (GumExecBlock * block,
                                          const GumBranchTarget * target,
                                          GumGeneratorContext * gc)
{
  gum_exec_ctx_write_arm_mov_branch_target (block->ctx, target, ARM_REG_R2, gc);
  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_call_event), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_thumb_call_event_code (GumExecBlock * block,
                                            const GumBranchTarget * target,
                                            GumGeneratorContext * gc)
{
  gum_exec_ctx_write_thumb_mov_branch_target (block->ctx, target, ARM_REG_R2,
      gc);
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_call_event), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start + 1),
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_arm_ret_event_code (GumExecBlock * block,
                                         const GumBranchTarget * target,
                                         GumGeneratorContext * gc)
{
  gum_exec_ctx_write_arm_mov_branch_target (block->ctx, target, ARM_REG_R2, gc);
  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_ret_event), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_thumb_ret_event_code (GumExecBlock * block,
                                           const GumBranchTarget * target,
                                           GumGeneratorContext * gc)
{
  gum_exec_ctx_write_thumb_mov_branch_target (block->ctx, target, ARM_REG_R2,
      gc);
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_ret_event), 4,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start + 1),
      GUM_ARG_REGISTER, ARM_REG_R2,
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_arm_exec_event_code (GumExecBlock * block,
                                          GumGeneratorContext * gc)
{
  if ((block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) != 0)
    return;

  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_exec_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start),
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_thumb_exec_event_code (GumExecBlock * block,
                                            GumGeneratorContext * gc)
{
  if ((block->flags & GUM_EXEC_BLOCK_USES_EXCLUSIVE_ACCESS) != 0)
    return;

  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_exec_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (gc->instruction->start + 1),
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_arm_block_event_code (GumExecBlock * block,
                                           GumGeneratorContext * gc)
{
  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_block_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_thumb_block_event_code (GumExecBlock * block,
                                             GumGeneratorContext * gc)
{
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_ctx_emit_block_event), 3,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM_REG_R10);
}

static void
gum_exec_block_write_arm_push_stack_frame (GumExecBlock * block,
                                           gpointer ret_real_address,
                                           GumGeneratorContext * gc)
{
  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_block_push_stack_frame), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (ret_real_address));
}

static void
gum_exec_block_write_thumb_push_stack_frame (GumExecBlock * block,
                                             gpointer ret_real_address,
                                             GumGeneratorContext * gc)
{
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_block_push_stack_frame), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_ADDRESS, GUM_ADDRESS (ret_real_address));
}

static void
gum_exec_block_push_stack_frame (GumExecCtx * ctx,
                                 gpointer ret_real_address)
{
  if (ctx->current_frame != ctx->frames)
  {
    ctx->current_frame->real_address = ret_real_address;
    ctx->current_frame--;
  }
}

static void
gum_exec_block_write_arm_pop_stack_frame (GumExecBlock * block,
                                          const GumBranchTarget * target,
                                          GumGeneratorContext * gc)
{
  gum_exec_ctx_write_arm_mov_branch_target (block->ctx, target, ARM_REG_R1, gc);
  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_block_pop_stack_frame), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_REGISTER, ARM_REG_R1);
}

static void
gum_exec_block_write_thumb_pop_stack_frame (GumExecBlock * block,
                                            const GumBranchTarget * target,
                                            GumGeneratorContext * gc)
{
  gum_exec_ctx_write_thumb_mov_branch_target (block->ctx, target, ARM_REG_R1,
      gc);
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_block_pop_stack_frame), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block->ctx),
      GUM_ARG_REGISTER, ARM_REG_R1);
}

static void
gum_exec_block_pop_stack_frame (GumExecCtx * ctx,
                                gpointer ret_real_address)
{
  /*
   * Since with ARM32, there is no clear CALL and RET instructions, it is
   * difficult to determine the difference between instructions being used to
   * perform a CALL, a BRANCH or a RETURN. We therefore check to see if the
   * target address is the return address from a previous call instruction to
   * determine whether to pop the frame from the stack. Note, however, that this
   * approach may not work in the event that the user be allowed to make
   * call-outs to modify the control flow.
   */
  if (ctx->current_frame != ctx->first_frame)
  {
    GumExecFrame * next_frame = ctx->current_frame + 1;
    if (next_frame->real_address == ret_real_address)
      ctx->current_frame = next_frame;
  }
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
          gum_exec_block_encode_instruction_pointer (block, block->real_start)))
  {
    if ((block->flags & GUM_EXEC_BLOCK_THUMB) != 0)
      gum_exec_block_write_thumb_call_probe_code (block, gc);
    else
      gum_exec_block_write_arm_call_probe_code (block, gc);
  }

  gum_spinlock_release (&stalker->probe_lock);
}

static void
gum_exec_block_write_arm_call_probe_code (GumExecBlock * block,
                                          GumGeneratorContext * gc)
{
  gum_exec_block_arm_open_prolog (block, gc);
  gum_arm_writer_put_call_address_with_arguments (gc->arm_writer,
      GUM_ADDRESS (gum_exec_block_invoke_call_probes), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM_REG_R10);
  gum_exec_block_arm_close_prolog (block, gc);
}

static void
gum_exec_block_write_thumb_call_probe_code (GumExecBlock * block,
                                            GumGeneratorContext * gc)
{
  gum_exec_block_thumb_open_prolog (block, gc);
  gum_thumb_writer_put_call_address_with_arguments (gc->thumb_writer,
      GUM_ADDRESS (gum_exec_block_invoke_call_probes), 2,
      GUM_ARG_ADDRESS, GUM_ADDRESS (block),
      GUM_ARG_REGISTER, ARM_REG_R10);
  gum_exec_block_thumb_close_prolog (block, gc);
}

static void
gum_exec_block_invoke_call_probes (GumExecBlock * block,
                                   GumCpuContext * cpu_context)
{
  GumStalker * stalker = block->ctx->stalker;
  gpointer target_address;
  GumCallProbe ** probes_copy;
  guint num_probes, i;
  GumCallDetails d;

  target_address =
      gum_exec_block_encode_instruction_pointer (block, block->real_start);

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

  cpu_context->pc = GPOINTER_TO_SIZE (block->real_start);

  for (i = 0; i != num_probes; i++)
  {
    GumCallProbe * probe = probes_copy[i];

    probe->callback (&d, probe->user_data);

    gum_call_probe_unref (probe);
  }
}

static gpointer
gum_exec_block_write_arm_inline_data (GumArmWriter * cw,
                                      gconstpointer data,
                                      gsize size,
                                      GumAddress * address)
{
  gpointer location;
  gconstpointer after_data = cw->code + 1;

  g_assert (size % 4 == 0);

  while (gum_arm_writer_offset (cw) < GUM_INVALIDATE_TRAMPOLINE_SIZE)
  {
    gum_arm_writer_put_nop (cw);
  }

  gum_arm_writer_put_b_label (cw, after_data);

  location = gum_arm_writer_cur (cw);
  if (address != NULL)
    *address = cw->pc;
  gum_arm_writer_put_bytes (cw, data, size);

  gum_arm_writer_put_label (cw, after_data);

  return location;
}

static gpointer
gum_exec_block_write_thumb_inline_data (GumThumbWriter * cw,
                                        gconstpointer data,
                                        gsize size,
                                        GumAddress * address)
{
  gpointer location;
  gconstpointer after_data = cw->code + 1;

  g_assert (size % 4 == 0);

  while (gum_thumb_writer_offset (cw) < GUM_INVALIDATE_TRAMPOLINE_SIZE)
  {
    gum_thumb_writer_put_nop (cw);
  }

  gum_thumb_writer_put_b_label (cw, after_data);

  if (gum_thumb_writer_offset (cw) % 4 != 0)
    gum_thumb_writer_put_nop (cw);
  location = gum_thumb_writer_cur (cw);
  if (address != NULL)
    *address = cw->pc;
  gum_thumb_writer_put_bytes (cw, data, size);

  gum_thumb_writer_put_label (cw, after_data);

  return location;
}

static void
gum_exec_block_arm_open_prolog (GumExecBlock * block,
                                GumGeneratorContext * gc)
{
  gum_exec_ctx_write_arm_prolog (block->ctx, gc->arm_writer);
}

static void
gum_exec_block_thumb_open_prolog (GumExecBlock * block,
                                  GumGeneratorContext * gc)
{
  gum_exec_ctx_write_thumb_prolog (block->ctx, gc->thumb_writer);
}

static void
gum_exec_block_arm_close_prolog (GumExecBlock * block,
                                 GumGeneratorContext * gc)
{
  gum_exec_ctx_write_arm_epilog (block->ctx, gc->arm_writer);
}

static void
gum_exec_block_thumb_close_prolog (GumExecBlock * block,
                                   GumGeneratorContext * gc)
{
  gum_exec_ctx_write_thumb_epilog (block->ctx, gc->thumb_writer);
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

  code_slab->arm_invalidator = NULL;
  code_slab->thumb_invalidator = NULL;
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

  scratch_slab->arm_invalidator = NULL;
  scratch_slab->thumb_invalidator = NULL;
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
gum_slab_align_cursor (GumSlab * self,
                       guint alignment)
{
  self->offset = GUM_ALIGN_SIZE (self->offset, alignment);

  return gum_slab_cursor (self);
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
#if defined (HAVE_GLIBC)
  return GSIZE_TO_POINTER (gum_module_find_export_by_name (
        gum_process_get_libc_module (),
        "__call_tls_dtors"));
#elif defined (HAVE_ANDROID)
  return GSIZE_TO_POINTER (gum_module_find_export_by_name (
        gum_process_get_libc_module (),
        "pthread_exit"));
#else
  return NULL;
#endif
}

static gpointer
gum_strip_thumb_bit (gpointer address)
{
  return GSIZE_TO_POINTER (GPOINTER_TO_SIZE (address) & ~0x1);
}

static gboolean
gum_is_thumb (gconstpointer address)
{
  return (GPOINTER_TO_SIZE (address) & 0x1) != 0;
}

static gboolean
gum_is_kuser_helper (gconstpointer address)
{
#ifdef HAVE_LINUX
  switch (GPOINTER_TO_SIZE (address))
  {
    case 0xffff0fa0: /* __kernel_memory_barrier */
    case 0xffff0fc0: /* __kernel_cmpxchg */
    case 0xffff0fe0: /* __kernel_get_tls */
    case 0xffff0f60: /* __kernel_cmpxchg64 */
      return TRUE;
    default:
      return FALSE;
  }
#else
  return FALSE;
#endif
}

static gboolean
gum_is_exclusive_load_insn (const cs_insn * insn)
{
  switch (insn->id)
  {
    case ARM_INS_LDAEX:
    case ARM_INS_LDAEXB:
    case ARM_INS_LDAEXD:
    case ARM_INS_LDAEXH:
    case ARM_INS_LDREX:
    case ARM_INS_LDREXB:
    case ARM_INS_LDREXD:
    case ARM_INS_LDREXH:
      return TRUE;
    default:
      return FALSE;
  }
}

static gboolean
gum_is_exclusive_store_insn (const cs_insn * insn)
{
  switch (insn->id)
  {
    case ARM_INS_STREX:
    case ARM_INS_STREXB:
    case ARM_INS_STREXD:
    case ARM_INS_STREXH:
    case ARM_INS_STLEX:
    case ARM_INS_STLEXB:
    case ARM_INS_STLEXD:
    case ARM_INS_STLEXH:
      return TRUE;
    default:
      return FALSE;
  }
}

static guint
gum_count_bits_set (guint16 value)
{
#if defined (HAVE_POPCOUNT)
  return __builtin_popcount (value);
#else
  guint num_ones = 0;
  guint16 bits = value;
  guint i;

  for (i = 0; i != 16; i++)
  {
    if ((bits & 1) == 1)
      num_ones++;
    bits >>= 1;
  }

  return num_ones;
#endif
}

static guint
gum_count_trailing_zeros (guint16 value)
{
#if defined (HAVE_CLTZ)
  return __builtin_ctz (value);
#else
  guint num_zeros = 0;
  guint16 bits = value;

  while ((bits & 1) == 0)
  {
    num_zeros++;
    bits >>= 1;
  }

  return num_zeros;
#endif
}
