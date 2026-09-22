/*
 * Copyright (C) 2017-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumstalker.h"

#include "gumstalker-priv.h"

typedef struct _GumRunOnThreadCtx GumRunOnThreadCtx;
typedef struct _GumRunOnThreadSyncCtx GumRunOnThreadSyncCtx;

struct _GumRunOnThreadCtx
{
  GumStalker * stalker;
  GumStalkerRunOnThreadFunc func;
  gpointer data;
  GDestroyNotify data_destroy;
};

struct _GumRunOnThreadSyncCtx
{
  GMutex mutex;
  GCond cond;
  gboolean done;
  GumStalkerRunOnThreadFunc func;
  gpointer data;
};

struct _GumDefaultStalkerTransformer
{
  GObject parent;
};

struct _GumCallbackStalkerTransformer
{
  GObject parent;

  GumStalkerTransformerCallback callback;
  gpointer data;
  GDestroyNotify data_destroy;
};

static void gum_modify_to_run_on_thread (GumThreadId thread_id,
    GumCpuContext * cpu_context, gpointer user_data);
static void gum_do_run_on_thread (const GumCpuContext * cpu_context,
    gpointer user_data);
static void gum_do_run_on_thread_sync (const GumCpuContext * cpu_context,
    gpointer user_data);

static void gum_default_stalker_transformer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_default_stalker_transformer_transform_block (
    GumStalkerTransformer * transformer, GumStalkerIterator * iterator,
    GumStalkerOutput * output);

static void gum_callback_stalker_transformer_iface_init (gpointer g_iface,
    gpointer iface_data);
static void gum_callback_stalker_transformer_finalize (GObject * object);
static void gum_callback_stalker_transformer_transform_block (
    GumStalkerTransformer * transformer, GumStalkerIterator * iterator,
    GumStalkerOutput * output);

static void gum_stalker_observer_default_init (
    GumStalkerObserverInterface * iface);

G_DEFINE_INTERFACE (GumStalkerTransformer, gum_stalker_transformer,
                    G_TYPE_OBJECT)

G_DEFINE_TYPE_EXTENDED (GumDefaultStalkerTransformer,
                        gum_default_stalker_transformer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_STALKER_TRANSFORMER,
                            gum_default_stalker_transformer_iface_init))

G_DEFINE_TYPE_EXTENDED (GumCallbackStalkerTransformer,
                        gum_callback_stalker_transformer,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_STALKER_TRANSFORMER,
                            gum_callback_stalker_transformer_iface_init))

G_DEFINE_INTERFACE (GumStalkerObserver, gum_stalker_observer, G_TYPE_OBJECT)

/**
 * GumStalker:
 *
 * Traces execution by dynamically recompiling code as a thread runs.
 *
 * Stalker follows a thread one basic block at a time, JIT-compiling a shadow
 * copy of each block just before it executes. That shadow copy can be observed
 * and rewritten on the way through, which makes it possible to:
 *
 * - Receive a stream of events — every call, return, block and instruction —
 *   through a [iface@Gum.EventSink].
 * - Inspect and rewrite the instructions of each block through a
 *   [iface@Gum.StalkerTransformer], for instance to insert a
 *   [callback@Gum.StalkerCallout] that runs your own C function with access to
 *   the live register state. For hot paths, emitting instrumentation inline is
 *   much cheaper than a callout — see [method@Gum.StalkerIterator.put_callout].
 *
 * Compiled blocks are cached per thread; the trust threshold
 * ([method@Gum.Stalker.set_trust_threshold]) governs how aggressively that
 * cache is reused in the presence of self-modifying code. Ranges of memory can
 * be left to run uninstrumented with [method@Gum.Stalker.exclude], which is
 * also much faster for code you have no interest in.
 *
 * Tracing begins with [method@Gum.Stalker.follow_me] for the calling thread, or
 * [method@Gum.Stalker.follow] for another thread, and ends at the matching
 * unfollow.
 *
 * ## Tracing the current thread
 *
 * ```c
 * static void on_exec (GumStalkerIterator * iterator,
 *     GumStalkerOutput * output, gpointer user_data);
 *
 * void
 * trace (void)
 * {
 *   g_autoptr(GumStalker) stalker = gum_stalker_new ();
 *   GumStalkerTransformer * transformer =
 *       gum_stalker_transformer_make_from_callback (on_exec, NULL, NULL);
 *
 *   gum_stalker_follow_me (stalker, transformer, NULL);
 *   // ... code to trace runs here ...
 *   gum_stalker_unfollow_me (stalker);
 *
 *   g_object_unref (transformer);
 * }
 *
 * static void
 * on_exec (GumStalkerIterator * iterator,
 *          GumStalkerOutput * output,
 *          gpointer user_data)
 * {
 *   const cs_insn * insn;
 *
 *   while (gum_stalker_iterator_next (iterator, &insn))
 *   {
 *     // Inspect insn here, then emit it into the recompiled block.
 *     gum_stalker_iterator_keep (iterator);
 *   }
 * }
 * ```
 */

/**
 * gum_stalker_is_supported:
 *
 * Checks whether Stalker is available on the current architecture and OS.
 *
 * Returns: %TRUE if Stalker is supported
 */

/**
 * gum_stalker_new:
 *
 * Creates a new Stalker.
 *
 * Returns: (transfer full): a new #GumStalker
 */

/**
 * gum_stalker_exclude:
 * @self: a #GumStalker
 * @range: the memory range to exclude
 *
 * Marks @range as off-limits: while a followed thread executes inside it, it
 * runs natively instead of being traced, resuming tracing once it returns.
 * Besides skipping code you have no interest in — which is also much faster —
 * this is essential for ranges that must not be instrumented, such as the
 * runtime backing Stalker itself.
 */

/**
 * gum_stalker_get_trust_threshold:
 * @self: a #GumStalker
 *
 * Gets the trust threshold; see [method@Gum.Stalker.set_trust_threshold].
 *
 * Returns: the current trust threshold
 */

/**
 * gum_stalker_set_trust_threshold:
 * @self: a #GumStalker
 * @trust_threshold: number of times a block must be seen unchanged before it is
 *   trusted
 *
 * Controls how Stalker copes with self-modifying code. Each recompiled block is
 * cached alongside a snapshot of the original bytes; when the block is reached
 * again Stalker compares the current code against that snapshot and recompiles
 * only if it actually changed. The trust threshold is how many times a block
 * must be found unchanged before Stalker stops doing that comparison and trusts
 * the cached copy outright.
 *
 * Higher values keep re-validating for longer, paying for the comparison (plus
 * the prolog/epilog needed to enter Stalker) on each execution. 0 trusts a
 * block immediately, never re-checking it. -1 never trusts the cache, so the
 * snapshot is always compared — but even then a block is only recompiled when
 * the code has genuinely changed, not on every execution. The default is 1.
 */

/**
 * gum_stalker_flush:
 * @self: a #GumStalker
 *
 * Flushes the event sinks of all current sessions, ensuring any events still
 * buffered are delivered.
 */

/**
 * gum_stalker_stop:
 * @self: a #GumStalker
 *
 * Unfollows every thread still being traced and removes all call probes, then
 * reclaims the associated resources.
 */

/**
 * gum_stalker_garbage_collect:
 * @self: a #GumStalker
 *
 * Reclaims resources left behind by threads that have unfollowed or exited but
 * could not be cleaned up synchronously, e.g. because they might still have
 * been executing instrumented code. Call this periodically until it returns
 * %FALSE to drain such garbage.
 *
 * Returns: %TRUE if garbage still remains and another pass should be made later
 */

/**
 * gum_stalker_follow_me:
 * @self: a #GumStalker
 * @transformer: (nullable) (transfer none): transformer to rewrite the traced
 *   code, or %NULL for the default
 * @sink: (nullable) (transfer none): sink to receive the events, or %NULL
 *
 * Starts tracing the calling thread, continuing from the caller's return
 * address. Each subsequently executed block is passed through @transformer and
 * any generated events are delivered to @sink. Stop with
 * [method@Gum.Stalker.unfollow_me].
 */

/**
 * gum_stalker_unfollow_me:
 * @self: a #GumStalker
 *
 * Stops tracing the calling thread, which must currently be followed by
 * [method@Gum.Stalker.follow_me].
 */

/**
 * gum_stalker_is_following_me:
 * @self: a #GumStalker
 *
 * Checks whether the calling thread is currently being traced.
 *
 * Returns: %TRUE if the calling thread is currently being traced
 */

/**
 * gum_stalker_follow:
 * @self: a #GumStalker
 * @thread_id: ID of the thread to trace
 * @transformer: (nullable) (transfer none): transformer to rewrite the traced
 *   code, or %NULL for the default
 * @sink: (nullable) (transfer none): sink to receive the events, or %NULL
 *
 * Starts tracing the thread identified by @thread_id. When @thread_id is the
 * calling thread this behaves like [method@Gum.Stalker.follow_me]; otherwise
 * the target thread is briefly suspended so it can be made to start executing
 * instrumented code. Stop with [method@Gum.Stalker.unfollow].
 */

/**
 * gum_stalker_unfollow:
 * @self: a #GumStalker
 * @thread_id: ID of the thread to stop tracing
 *
 * Stops tracing the thread identified by @thread_id. The thread resumes running
 * its original, uninstrumented code.
 */

/**
 * gum_stalker_activate:
 * @self: a #GumStalker
 * @target: address at which tracing should resume in earnest
 *
 * Resumes tracing on the calling thread after a
 * [method@Gum.Stalker.deactivate], following it even through excluded ranges
 * until execution reaches @target, at which point normal behavior resumes.
 * @target must be the start of a basic block. This is primarily useful for
 * fuzzing; see [method@Gum.Stalker.prefetch] for the bigger picture.
 */

/**
 * gum_stalker_deactivate:
 * @self: a #GumStalker
 *
 * Pauses tracing on the calling thread while leaving it followed, so it runs
 * natively until [method@Gum.Stalker.activate] is called. Has no effect if the
 * thread is not being traced.
 */

/**
 * gum_stalker_set_observer:
 * @self: a #GumStalker
 * @observer: (nullable) (transfer none): the observer, or %NULL to remove it
 *
 * Sets a [iface@Gum.StalkerObserver] that receives fine-grained notifications
 * about Stalker's internals, such as backpatches and engine statistics. Mainly
 * of interest for diagnostics and performance work.
 */

/**
 * gum_stalker_prefetch:
 *
 * This API is intended for use during fuzzing scenarios such as AFL forkserver.
 * It allows for the child to feed back the addresses of instrumented blocks to
 * the parent so that the next time a child is forked from the parent, it will
 * already inherit the instrumented block rather than having to re-instrument
 * every basic block again from scratch.
 *
 * This API has the following caveats:
 *
 * 1. This API MUST be called from the thread which will be executed in the
 *    child. Since blocks are cached in the GumExecCtx which is stored on a
 *    per-thread basis and accessed through Thread Local Storage, it is not
 *    possible to prefetch blocks into the cache of another thread.
 *
 * 2. This API should be called after gum_stalker_follow_me(). It is likely that
 *    the parent will wish to call gum_stalker_deactivate() immediately after
 *    following. Subsequently, gum_stalker_activate() can be called within the
 *    child after it is forked to start stalking the thread once more. The child
 *    can then communicate newly discovered basic blocks back to the parent via
 *    inter-process communications. The parent can then call
 *    gum_stalker_prefetch() to instrument those blocks before forking the next
 *    child. As a result of the fork, the child inherits a deactivated Stalker
 *    instance, thus both parent and child should release their Stalker
 *    instances upon completion if required.
 *
 * 3. Note that gum_stalker_activate() takes a `target` pointer which is used to
 *    allow Stalker to be reactivated whilst executing in an excluded range and
 *    guarantee that the thread is followed until the “activation target”
 *    address is reached. Typically for e.g. a fuzzer the target would be the
 *    function you're about to hit with inputs. When this target isn't known,
 *    the simplest solution to this is to define an empty function (marked as
 *    non-inlineable) and then subsequently call it immediately after activation
 *    to return Stalker to its normal behavior. It is important that `target` is
 *    at the start of a basic block, otherwise Stalker will not detect it.
 *    Failure to do so may mean that Stalker continues to follow the thread into
 *    code which it should not, including any calls to Stalker itself. Thus care
 *    should be taken to ensure that the function is not inlined, or optimized
 *    away by the compiler.
 *
 *    __attribute__ ((noinline))
 *    static void
 *    activation_target (void)
 *    {
        // Avoid calls being optimized out
 *      asm ("");
 *    }
 *
 * 4. Note that since both parent and child have an identical Stalker instance,
 *    they each have the exact same Transformer. Since this Transformer will
 *    be used both to generate blocks to execute in the child and to prefetch
 *    blocks in the parent, care should be taken to identify in which scenario
 *    the transformer is operating. The parent will likely also transform and
 *    execute a few blocks even if it is deactivated immediately afterwards.
 *    Thus care should also be taken when any callouts are executed to determine
 *    whether they are running in the parent or child context.
 *
 * 5. For optimal performance, the recycle_count should be set to the same value
 *    as gum_stalker_get_trust_threshold(). Unless the trust threshold is set to
 *    `-1` or `0`. When adding instrumented blocks into the cache, Stalker also
 *    retains a copy of the original bytes of the code which was instrumented.
 *    When recalling blocks from the cache, this is compared in order to detect
 *    self-modifying code. If the block is the same, then the recycle_count is
 *    incremented. The trust threshold sets the limit of how many times a block
 *    should be identical (e.g. the code has not been modified) before this
 *    comparison can be omitted. Thus when prefetching, we can also set the
 *    recycle_count to control whether this comparison takes place. When the
 *    trust threshold is less than `1`, the block_recycle count has not effect.
 *
 * 6. This API does not change the trust threshold as it is a global setting
 *    which affects all Stalker sessions running on all threads.
 *
 * 7. It is inadvisable to prefetch self-modifying code blocks, since it will
 *    mean a single static instrumented block will always be used when it is
 *    executed. The detection of self-modifying code in the child is left to the
 *    user, just as the user is free to choose which blocks to prefetch by
 *    calling the API. It may also be helpful to avoid sending the same block
 *    address to be prefetched to the parent multiple times to reduce I/O
 *    required via IPC, particularly if the same block is executed multiple
 *    times. If you are fuzzing self-modifying code, then your day is probably
 *    already going badly.
 *
 * The following is provided as an example workflow for initializing a fork
 * server based fuzzer:
 *
 *    p -> setup IPC mechanism with child (e.g. pipe)
 *    p -> create custom Transformer to send address of instrumented block to
 *         parent via IPC. Transformer should be inert until latched. Callouts
 *         should still be generated as required when not latched, but should
 *         themselves be inert until latched.
 *    p -> gum_stalker_follow_me ()
 *    p -> gum_stalker_deactivate ()
 *
 *    BEGIN LOOP:
 *
 *    p -> fork ()
 *    p -> waitpid ()
 *
 *    c -> set latch to trigger Transformer (note that this affects only the
 *         child process).
 *    c -> gum_stalker_activate (activation_target)
 *    c -> activation_target ()
 *    c -> <RUN CODE UNDER TEST HERE>
 *    c -> gum_stalker_unfollow_me () or simply exit ()
 *
 *    p -> gum_stalker_set_trust_threshold (0)
 *    p -> gum_stalker_prefetch (x) (n times for each)
 *    p -> gum_stalker_set_trust_threshold (n)
 *
 *    END LOOP:
 */

/**
 * gum_stalker_prefetch_backpatch:
 * @self: a #GumStalker
 * @notification: the backpatch to apply
 *
 * Applies a backpatch previously reported through a
 * [iface@Gum.StalkerObserver]. The companion to [method@Gum.Stalker.prefetch]
 * for fork-server fuzzing: it lets the parent reproduce the inter-block
 * backpatches discovered by a child so forked children inherit them too.
 */

/**
 * gum_stalker_recompile:
 * @self: a #GumStalker
 * @address: address of the block to recompile
 *
 * Forces the cached block at @address to be recompiled, e.g. after something
 * the transformer depends on has changed.
 */

/**
 * gum_stalker_backpatch_get_from:
 * @backpatch: a #GumBackpatch
 *
 * Gets the address of the block a backpatch originates from.
 *
 * Returns: the address of the block the backpatch originates from
 */

/**
 * gum_stalker_backpatch_get_to:
 * @backpatch: a #GumBackpatch
 *
 * Gets the address of the block a backpatch points to.
 *
 * Returns: the address of the block the backpatch points to
 */

/**
 * gum_stalker_invalidate:
 * @self: a #GumStalker
 * @address: address whose cached block should be invalidated
 *
 * Drops the cached instrumented block covering @address on every followed
 * thread, so it is recompiled the next time it runs. Use this when the
 * underlying code, or the way you want it instrumented, has changed.
 */

/**
 * gum_stalker_invalidate_for_thread:
 * @self: a #GumStalker
 * @thread_id: ID of the thread whose cache should be invalidated
 * @address: address whose cached block should be invalidated
 *
 * Like [method@Gum.Stalker.invalidate], but limited to the thread identified by
 * @thread_id.
 */

/**
 * gum_stalker_add_call_probe:
 * @self: a #GumStalker
 * @target_address: address of the function whose calls should be probed
 * @callback: (scope notified): function to call for each observed call
 * @data: data to pass to @callback
 * @notify: (nullable): destroy notify for @data
 *
 * Registers @callback to be invoked whenever a followed thread calls
 * @target_address, giving it access to the call's arguments through the
 * supplied [struct@Gum.CallDetails]. Cheaper than a transformer when all you
 * need is to observe calls to a specific function.
 *
 * Returns: an ID that can be passed to [method@Gum.Stalker.remove_call_probe]
 */

/**
 * gum_stalker_remove_call_probe:
 * @self: a #GumStalker
 * @id: ID returned by [method@Gum.Stalker.add_call_probe]
 *
 * Removes the call probe identified by @id.
 */

/**
 * gum_stalker_run_on_thread:
 * @self: a #GumStalker
 * @thread_id: ID of the thread to run @func on
 * @func: (scope notified): function to run on the target thread
 * @data: data to pass to @func
 * @data_destroy: (nullable): destroy notify for @data
 *
 * Arranges for @func to run on the thread identified by @thread_id, with that
 * thread's register state, by briefly hijacking it through Stalker. Returns
 * once @func has been scheduled; it may not have run yet. Use
 * [method@Gum.Stalker.run_on_thread_sync] to wait for completion.
 *
 * Returns: %TRUE if the request was accepted
 */
gboolean
gum_stalker_run_on_thread (GumStalker * self,
                           GumThreadId thread_id,
                           GumStalkerRunOnThreadFunc func,
                           gpointer data,
                           GDestroyNotify data_destroy)
{
  gboolean accepted = TRUE;
  gboolean finished = TRUE;

  if (thread_id == gum_process_get_current_thread_id ())
  {
    func (NULL, data);
  }
  else
  {
    GumRunOnThreadCtx * rc;

    rc = g_slice_new (GumRunOnThreadCtx);
    rc->stalker = self;
    rc->func = func;
    rc->data = data;
    rc->data_destroy = data_destroy;

    accepted = gum_process_modify_thread (thread_id,
        gum_modify_to_run_on_thread, rc, GUM_MODIFY_THREAD_FLAGS_NONE);
    if (accepted)
      finished = FALSE;
    else
      g_slice_free (GumRunOnThreadCtx, rc);
  }

  if (finished && data_destroy != NULL)
    data_destroy (data);

  return accepted;
}

static void
gum_modify_to_run_on_thread (GumThreadId thread_id,
                             GumCpuContext * cpu_context,
                             gpointer user_data)
{
  GumRunOnThreadCtx * rc = user_data;

  _gum_stalker_modify_to_run_on_thread (rc->stalker, thread_id, cpu_context,
      gum_do_run_on_thread, rc);
}

static void
gum_do_run_on_thread (const GumCpuContext * cpu_context,
                      gpointer user_data)
{
  GumRunOnThreadCtx * rc = user_data;

  rc->func (cpu_context, rc->data);

  if (rc->data_destroy != NULL)
    rc->data_destroy (rc->data);
  g_slice_free (GumRunOnThreadCtx, rc);
}

/**
 * gum_stalker_run_on_thread_sync:
 * @self: stalker
 * @thread_id: the thread to run on
 * @func: (scope call): function to run on the thread
 * @data: data to pass to @func
 *
 * Synchronously runs @func on the specified thread.
 *
 * Returns: whether the function was successfully run
 */
gboolean
gum_stalker_run_on_thread_sync (GumStalker * self,
                                GumThreadId thread_id,
                                GumStalkerRunOnThreadFunc func,
                                gpointer data)
{
  gboolean success = TRUE;

  if (thread_id == gum_process_get_current_thread_id ())
  {
    func (NULL, data);
  }
  else
  {
    GumRunOnThreadSyncCtx rc;

    g_mutex_init (&rc.mutex);
    g_cond_init (&rc.cond);
    rc.done = FALSE;
    rc.func = func;
    rc.data = data;

    g_mutex_lock (&rc.mutex);

    if (gum_stalker_run_on_thread (self, thread_id, gum_do_run_on_thread_sync,
          &rc, NULL))
    {
      while (!rc.done)
        g_cond_wait (&rc.cond, &rc.mutex);
    }
    else
    {
      success = FALSE;
    }

    g_mutex_unlock (&rc.mutex);

    g_cond_clear (&rc.cond);
    g_mutex_clear (&rc.mutex);
  }

  return success;
}

static void
gum_do_run_on_thread_sync (const GumCpuContext * cpu_context,
                           gpointer user_data)
{
  GumRunOnThreadSyncCtx * rc = user_data;

  rc->func (cpu_context, rc->data);

  g_mutex_lock (&rc->mutex);
  rc->done = TRUE;
  g_cond_signal (&rc->cond);
  g_mutex_unlock (&rc->mutex);
}

static void
gum_stalker_transformer_default_init (GumStalkerTransformerInterface * iface)
{
}

/**
 * gum_stalker_transformer_make_default:
 *
 * Creates a default #GumStalkerTransformer that recompiles code without any
 * custom transformations.
 *
 * Returns: (transfer full): a newly created #GumStalkerTransformer
 */
GumStalkerTransformer *
gum_stalker_transformer_make_default (void)
{
  return g_object_new (GUM_TYPE_DEFAULT_STALKER_TRANSFORMER, NULL);
}

/**
 * gum_stalker_transformer_make_from_callback:
 * @callback: (not nullable): function called to transform each basic block
 * @data: (nullable): data to pass to @callback
 * @data_destroy: (nullable) (destroy data): function to destroy @data
 *
 * Creates a #GumStalkerTransformer that recompiles code by letting @callback
 * apply custom transformations for any given basic block.
 *
 * Returns: (transfer full): a newly created #GumStalkerTransformer
 */
GumStalkerTransformer *
gum_stalker_transformer_make_from_callback (
    GumStalkerTransformerCallback callback,
    gpointer data,
    GDestroyNotify data_destroy)
{
  GumCallbackStalkerTransformer * transformer;

  transformer = g_object_new (GUM_TYPE_CALLBACK_STALKER_TRANSFORMER, NULL);
  transformer->callback = callback;
  transformer->data = data;
  transformer->data_destroy = data_destroy;

  return GUM_STALKER_TRANSFORMER (transformer);
}

/**
 * gum_stalker_transformer_transform_block:
 * @self: a #GumStalkerTransformer
 * @iterator: iterator over the instructions of the block being recompiled
 * @output: where the recompiled instructions are written
 *
 * Transforms a single basic block, reading its instructions from @iterator and
 * emitting the recompiled form into @output. Stalker calls this for each block;
 * a [iface@Gum.StalkerTransformer] implementation overrides it to inspect and
 * rewrite the code, typically driving @iterator with
 * [method@Gum.StalkerIterator.next] and [method@Gum.StalkerIterator.keep].
 */
void
gum_stalker_transformer_transform_block (GumStalkerTransformer * self,
                                         GumStalkerIterator * iterator,
                                         GumStalkerOutput * output)
{
  GumStalkerTransformerInterface * iface =
      GUM_STALKER_TRANSFORMER_GET_IFACE (self);

  g_assert (iface->transform_block != NULL);

  iface->transform_block (self, iterator, output);
}

static void
gum_default_stalker_transformer_class_init (
    GumDefaultStalkerTransformerClass * klass)
{
}

static void
gum_default_stalker_transformer_iface_init (gpointer g_iface,
                                            gpointer iface_data)
{
  GumStalkerTransformerInterface * iface = g_iface;

  iface->transform_block = gum_default_stalker_transformer_transform_block;
}

static void
gum_default_stalker_transformer_init (GumDefaultStalkerTransformer * self)
{
}

static void
gum_default_stalker_transformer_transform_block (
    GumStalkerTransformer * transformer,
    GumStalkerIterator * iterator,
    GumStalkerOutput * output)
{
  while (gum_stalker_iterator_next (iterator, NULL))
  {
    gum_stalker_iterator_keep (iterator);
  }
}

static void
gum_callback_stalker_transformer_class_init (
    GumCallbackStalkerTransformerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gum_callback_stalker_transformer_finalize;
}

static void
gum_callback_stalker_transformer_iface_init (gpointer g_iface,
                                             gpointer iface_data)
{
  GumStalkerTransformerInterface * iface = g_iface;

  iface->transform_block = gum_callback_stalker_transformer_transform_block;
}

static void
gum_callback_stalker_transformer_init (GumCallbackStalkerTransformer * self)
{
}

static void
gum_callback_stalker_transformer_finalize (GObject * object)
{
  GumCallbackStalkerTransformer * self =
      GUM_CALLBACK_STALKER_TRANSFORMER (object);

  if (self->data_destroy != NULL)
    self->data_destroy (self->data);

  G_OBJECT_CLASS (gum_callback_stalker_transformer_parent_class)->finalize (
      object);
}

static void
gum_callback_stalker_transformer_transform_block (
    GumStalkerTransformer * transformer,
    GumStalkerIterator * iterator,
    GumStalkerOutput * output)
{
  GumCallbackStalkerTransformer * self =
      (GumCallbackStalkerTransformer *) transformer;

  self->callback (iterator, output, self->data);
}

/**
 * gum_stalker_iterator_next:
 * @self: a #GumStalkerIterator
 * @insn: (type gpointer*) (out) (transfer none) (optional): return location for
 *        a pointer to the next instruction, or %NULL
 *
 * Advances the iterator to the next instruction.
 *
 * Returns: %TRUE if there is a next instruction, else %FALSE
 */

/**
 * gum_stalker_iterator_keep:
 * @self: a #GumStalkerIterator
 *
 * Emits the current instruction — the one most recently returned by
 * [method@Gum.StalkerIterator.next] — unchanged into the recompiled block. A
 * transformer that wants to preserve an instruction must call this; skipping it
 * drops the instruction from the output.
 */

/**
 * gum_stalker_iterator_get_memory_access:
 * @self: a #GumStalkerIterator
 *
 * Reports whether it is safe to insert instrumentation at the current position.
 * Within an exclusive load/store sequence the access is reported as restricted,
 * letting a transformer avoid inserting callouts that would clobber the
 * exclusive monitor and break the sequence.
 *
 * Returns: the memory access restriction at the current instruction
 */

/**
 * gum_stalker_iterator_put_callout:
 * @self: a #GumStalkerIterator
 * @callout: (scope notified): function to call at this point during execution
 * @data: data to pass to @callout
 * @data_destroy: (nullable): destroy notify for @data
 *
 * Inserts a call to @callout at the current position in the recompiled block.
 * At run time @callout is invoked with the live [struct@Gum.CpuContext], which
 * it may also modify, making this the most convenient way to run your own C
 * code inline with the traced thread.
 *
 * Be aware that a callout is relatively expensive: calling out to C means
 * saving and restoring a fair amount of register state to honor the platform's
 * ABI, on every execution of the instrumented point. For hot paths such as
 * logging, it is often far faster to emit your own instructions directly into
 * the block through the [struct@Gum.StalkerOutput] writer instead — for
 * example dedicating a scratch register that points into a buffer you fill as
 * instructions execute and flush at the end of the block. This is considerably
 * harder to get right, but can move performance to a different level. (Even so,
 * a callout is still far cheaper than a tracer that single-steps the CPU.)
 */

/**
 * gum_stalker_iterator_put_chaining_return:
 * @self: a #GumStalkerIterator
 *
 * Puts a chaining return at the current location in the output
 * instruction stream.
 */

/**
 * gum_stalker_iterator_get_capstone: (skip)
 * @self: the iterator
 *
 * Returns the Capstone handle for the current iterator.
 */

static void
gum_stalker_observer_default_init (GumStalkerObserverInterface * iface)
{
}

#define GUM_DEFINE_OBSERVER_INCREMENT(name) \
    void \
    gum_stalker_observer_increment_##name (GumStalkerObserver * observer) \
    { \
      GumStalkerObserverInterface * iface; \
      \
      iface = GUM_STALKER_OBSERVER_GET_IFACE (observer); \
      g_assert (iface != NULL); \
      \
      if (iface->increment_##name == NULL) \
        return; \
      \
      iface->increment_##name (observer); \
    }

GUM_DEFINE_OBSERVER_INCREMENT (total)

GUM_DEFINE_OBSERVER_INCREMENT (call_imm)
GUM_DEFINE_OBSERVER_INCREMENT (call_reg)

GUM_DEFINE_OBSERVER_INCREMENT (call_mem)

GUM_DEFINE_OBSERVER_INCREMENT (excluded_call_reg)

GUM_DEFINE_OBSERVER_INCREMENT (ret_slow_path)

GUM_DEFINE_OBSERVER_INCREMENT (ret)

GUM_DEFINE_OBSERVER_INCREMENT (post_call_invoke)
GUM_DEFINE_OBSERVER_INCREMENT (excluded_call_imm)

GUM_DEFINE_OBSERVER_INCREMENT (jmp_imm)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_reg)

GUM_DEFINE_OBSERVER_INCREMENT (jmp_mem)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_imm)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_mem)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_reg)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_jcxz)

GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_cc)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_cbz)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_cbnz)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_tbz)
GUM_DEFINE_OBSERVER_INCREMENT (jmp_cond_tbnz)

GUM_DEFINE_OBSERVER_INCREMENT (jmp_continuation)

GUM_DEFINE_OBSERVER_INCREMENT (sysenter_slow_path)

void
gum_stalker_observer_notify_backpatch (GumStalkerObserver * observer,
                                       const GumBackpatch * backpatch,
                                       gsize size)
{
  GumStalkerObserverInterface * iface;

  iface = GUM_STALKER_OBSERVER_GET_IFACE (observer);
  g_assert (iface != NULL);

  if (iface->notify_backpatch == NULL)
    return;

  iface->notify_backpatch (observer, backpatch, size);
}

void
gum_stalker_observer_switch_callback (GumStalkerObserver * observer,
                                      gpointer from_address,
                                      gpointer start_address,
                                      gpointer from_insn,
                                      gpointer * target)
{
  GumStalkerObserverInterface * iface;

  iface = GUM_STALKER_OBSERVER_GET_IFACE (observer);
  g_assert (iface != NULL);

  if (iface->switch_callback == NULL)
    return;

  iface->switch_callback (observer, from_address, start_address, from_insn,
      target);
}
