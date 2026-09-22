/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumv8platform.h"

#include "gumscriptbackend.h"
#include "gumv8scope.h"

#include <algorithm>
#include <gum/gumcloak.h>
#include <gum/gumcodesegment.h>
#include <gum/gummemory.h>
#ifdef HAVE_DARWIN
# include <mach/mach.h>
# include <sys/mman.h>
#endif

using namespace v8;

namespace
{
  constexpr size_t kMaxWorkerPerJob = 32;
}

static GumPageProtection gum_page_protection_from_v8 (
    PageAllocator::Permission permission);

class GumV8MainContextOperation : public GumV8Operation
{
public:
  GumV8MainContextOperation (GumV8Platform * platform,
      std::function<void ()> func, GSource * source,
      guint delay_in_milliseconds);
  ~GumV8MainContextOperation () override;

  void Perform ();
  void Cancel () override;
  void Await () override;

private:
  enum State
  {
    kScheduled,
    kRunning,
    kCompleted,
    kCanceling,
    kCanceled
  };

  GumV8Platform * platform;
  std::function<void ()> func;
  GSource * source;
  guint delay_in_milliseconds;
  volatile State state;
  GCond cond;

  friend class GumV8Platform;
};

class GumV8ThreadPoolOperation : public GumV8Operation
{
public:
  GumV8ThreadPoolOperation (GumV8Platform * platform,
      std::function<void ()> func);
  ~GumV8ThreadPoolOperation () override;

  void Perform ();
  void Cancel () override;
  void Await () override;

private:
  enum State
  {
    kScheduled,
    kRunning,
    kCompleted,
    kCanceled
  };

  GumV8Platform * platform;
  std::function<void ()> func;
  volatile State state;
  GCond cond;

  friend class GumV8Platform;
};

class GumV8DelayedThreadPoolOperation : public GumV8Operation
{
public:
  GumV8DelayedThreadPoolOperation (GumV8Platform * platform,
      std::function<void ()> func, GSource * source);
  ~GumV8DelayedThreadPoolOperation () override;

  void Perform ();
  void Cancel () override;
  void Await () override;

private:
  enum State
  {
    kScheduled,
    kRunning,
    kCompleted,
    kCanceling,
    kCanceled
  };

  GumV8Platform * platform;
  std::function<void ()> func;
  GSource * source;
  volatile State state;
  GCond cond;

  friend class GumV8Platform;
};

class GumV8ForegroundTaskRunner : public TaskRunner
{
public:
  GumV8ForegroundTaskRunner (GumV8Platform * platform, Isolate * isolate);
  ~GumV8ForegroundTaskRunner () override;

  void PostTask (std::unique_ptr<Task> task) override;
  void PostNonNestableTask (std::unique_ptr<Task> task) override;
  void PostDelayedTask (std::unique_ptr<Task> task,
      double delay_in_seconds) override;
  void PostNonNestableDelayedTask (std::unique_ptr<Task> task,
      double delay_in_seconds) override;
  void PostIdleTask (std::unique_ptr<IdleTask> task) override;
  bool IdleTasksEnabled () override;
  bool NonNestableTasksEnabled () const override;
  bool NonNestableDelayedTasksEnabled () const override;

private:
  void Run (Task * task);
  void Run (IdleTask * task);

  GumV8Platform * platform;
  Isolate * isolate;
  GHashTable * pending;
};

/* The following three classes are based on the default implementation in V8. */

class GumV8JobState : public std::enable_shared_from_this<GumV8JobState>
{
public:
  GumV8JobState (GumV8Platform * platform, std::unique_ptr<JobTask> job_task,
      TaskPriority priority, size_t num_worker_threads);
  GumV8JobState (const GumV8JobState &) = delete;
  GumV8JobState & operator= (const GumV8JobState &) = delete;
  virtual ~GumV8JobState ();

  void NotifyConcurrencyIncrease ();
  uint8_t AcquireTaskId ();
  void ReleaseTaskId (uint8_t task_id);
  void Join ();
  void CancelAndWait ();
  void CancelAndDetach ();
  bool IsActive ();
  void UpdatePriority (TaskPriority new_priority);
  bool CanRunFirstTask ();
  bool DidRunTask ();

private:
  bool WaitForParticipationOpportunityLocked ();
  size_t CappedMaxConcurrency (size_t worker_count) const;
  void CallOnWorkerThread (TaskPriority with_priority,
      std::unique_ptr<Task> task);

public:
  class JobDelegate : public v8::JobDelegate
  {
  public:
    explicit JobDelegate (GumV8JobState * parent, bool is_joining_thread);
    virtual ~JobDelegate ();

    void NotifyConcurrencyIncrease () override;
    bool ShouldYield () override;
    uint8_t GetTaskId () override;
    bool IsJoiningThread () const override { return is_joining_thread; }

  private:
    static constexpr uint8_t kInvalidTaskId = G_MAXUINT8;

    GumV8JobState * parent;
    uint8_t task_id = kInvalidTaskId;
    bool is_joining_thread;
  };

private:
  GMutex mutex;
  Isolate * isolate;
  GumV8Platform * platform;
  std::unique_ptr<JobTask> job_task;
  TaskPriority priority;
  size_t num_worker_threads;
  size_t active_workers = 0;
  GCond worker_released_cond;
  size_t pending_tasks = 0;
  std::atomic<uint32_t> assigned_task_ids { 0 };
  std::atomic_bool is_canceled { false };
};

class GumV8JobHandle : public JobHandle
{
public:
  GumV8JobHandle (std::shared_ptr<GumV8JobState> state);
  GumV8JobHandle (const GumV8JobHandle &) = delete;
  GumV8JobHandle & operator= (const GumV8JobHandle &) = delete;
  ~GumV8JobHandle () override;

  void NotifyConcurrencyIncrease () override;
  void Join () override;
  void Cancel () override;
  void CancelAndDetach () override;
  bool IsActive () override;
  bool IsValid () override { return state != nullptr; }
  bool UpdatePriorityEnabled () const override { return true; }
  void UpdatePriority (TaskPriority new_priority) override;

private:
  std::shared_ptr<GumV8JobState> state;
};

class GumV8JobWorker : public Task
{
public:
  GumV8JobWorker (std::weak_ptr<GumV8JobState> state, JobTask * job_task);
  GumV8JobWorker (const GumV8JobWorker &) = delete;
  GumV8JobWorker & operator= (const GumV8JobWorker &) = delete;
  ~GumV8JobWorker () override = default;

  void Run () override;

private:
  std::weak_ptr<GumV8JobState> state;
  JobTask * job_task;
};

class GumV8PageAllocator : public PageAllocator
{
public:
  GumV8PageAllocator () = default;

  size_t AllocatePageSize () override;
  size_t CommitPageSize () override;
  void SetRandomMmapSeed (int64_t seed) override;
  void * GetRandomMmapAddr () override;
  void * AllocatePages (void * address, size_t length, size_t alignment,
      Permission permissions) override;
  bool FreePages (void * address, size_t length) override;
  bool ReleasePages (void * address, size_t length, size_t new_length) override;
  bool SetPermissions (void * address, size_t length,
      Permission permissions) override;
  bool RecommitPages (void * address, size_t length, Permission permissions)
      override;
  bool DiscardSystemPages (void * address, size_t size) override;
  bool DecommitPages (void * address, size_t size) override;
};

class GumV8ArrayBufferAllocator : public ArrayBuffer::Allocator
{
public:
  GumV8ArrayBufferAllocator () = default;

  void * Allocate (size_t length) override;
  void * AllocateUninitialized (size_t length) override;
  void Free (void * data, size_t length) override;
  void * Reallocate (void * data, size_t old_length, size_t new_length)
      override;
};

class GumV8ThreadingBackend : public ThreadingBackend
{
public:
  GumV8ThreadingBackend () = default;

  MutexImpl * CreatePlainMutex () override;
  MutexImpl * CreateRecursiveMutex () override;
  SharedMutexImpl * CreateSharedMutex () override;
  ConditionVariableImpl * CreateConditionVariable () override;
};

class GumMutex : public MutexImpl
{
public:
  GumMutex ();
  ~GumMutex () override;

  void Lock () override;
  void Unlock () override;
  bool TryLock () override;

private:
  GMutex mutex;

  friend class GumConditionVariable;
};

class GumRecursiveMutex : public MutexImpl
{
public:
  GumRecursiveMutex ();
  ~GumRecursiveMutex () override;

  void Lock () override;
  void Unlock () override;
  bool TryLock () override;

private:
  GRecMutex mutex;
};

class GumSharedMutex : public SharedMutexImpl
{
public:
  GumSharedMutex ();
  ~GumSharedMutex () override;

  void LockShared () override;
  void LockExclusive () override;
  void UnlockShared () override;
  void UnlockExclusive () override;
  bool TryLockShared () override;
  bool TryLockExclusive () override;

private:
  GRWLock lock;
};

class GumConditionVariable : public ConditionVariableImpl
{
public:
  GumConditionVariable ();
  ~GumConditionVariable () override;

  void NotifyOne () override;
  void NotifyAll () override;
  void Wait (MutexImpl * mutex) override;
  bool WaitFor (MutexImpl * mutex, int64_t delta_in_microseconds) override;

private:
  GCond cond;
};

class GumMutexLocker
{
public:
  GumMutexLocker (GMutex * mutex)
    : mutex (mutex)
  {
    g_mutex_lock (mutex);
  }

  GumMutexLocker (const GumMutexLocker &) = delete;

  GumMutexLocker & operator= (const GumMutexLocker &) = delete;

  ~GumMutexLocker ()
  {
    g_mutex_unlock (mutex);
  }

private:
  GMutex * mutex;
};

class GumMutexUnlocker
{
public:
  GumMutexUnlocker (GMutex * mutex)
    : mutex (mutex)
  {
    g_mutex_unlock (mutex);
  }

  GumMutexUnlocker (const GumMutexUnlocker &) = delete;

  GumMutexUnlocker & operator= (const GumMutexUnlocker &) = delete;

  ~GumMutexUnlocker ()
  {
    g_mutex_lock (mutex);
  }

private:
  GMutex * mutex;
};

class GumV8PlatformLocker
{
public:
  GumV8PlatformLocker (GumV8Platform * platform)
    : locker (&platform->mutex)
  {
  }

private:
  GumMutexLocker locker;
};

class GumV8PlatformUnlocker
{
public:
  GumV8PlatformUnlocker (GumV8Platform * platform)
    : unlocker (&platform->mutex)
  {
  }

private:
  GumMutexUnlocker unlocker;
};

GumV8Platform::GumV8Platform ()
  : disposing (false),
    scheduler (gum_script_backend_get_scheduler ()),
    page_allocator (new GumV8PageAllocator ()),
    array_buffer_allocator (new GumV8ArrayBufferAllocator ()),
    threading_backend (new GumV8ThreadingBackend ()),
    tracing_controller (new TracingController ())
{
  g_mutex_init (&mutex);

  g_object_ref (scheduler);

  V8::InitializePlatform (this);
  V8::Initialize ();
}

GumV8Platform::~GumV8Platform ()
{
  PerformOnJSThread (G_PRIORITY_HIGH, [=]() { Dispose (); });

  g_object_unref (scheduler);

  g_mutex_clear (&mutex);
}

void
GumV8Platform::Dispose ()
{
  disposing = true;

  CancelPendingOperations ();

  for (const auto & isolate : dying_isolates)
    isolate->Dispose ();
  dying_isolates.clear ();

  V8::Dispose ();
  V8::DisposePlatform ();
}

void
GumV8Platform::CancelPendingOperations ()
{
  GMainContext * main_context = gum_script_scheduler_get_js_context (scheduler);

  while (true)
  {
    std::unordered_set<std::shared_ptr<GumV8Operation>> js_ops_copy;
    std::unordered_set<std::shared_ptr<GumV8Operation>> pool_ops_copy;
    {
      GumV8PlatformLocker locker (this);

      js_ops_copy = js_ops;
      pool_ops_copy = pool_ops;
    }

    for (const auto & op : js_ops_copy)
      op->Cancel ();

    for (const auto & op : pool_ops_copy)
      op->Cancel ();
    for (const auto & op : pool_ops_copy)
      op->Await ();

    {
      GumV8PlatformLocker locker (this);
      if (js_ops.empty () && pool_ops.empty ())
        break;
    }

    bool anything_pending = false;
    while (g_main_context_pending (main_context))
    {
      anything_pending = true;
      g_main_context_iteration (main_context, FALSE);
    }
    if (!anything_pending)
      g_thread_yield ();
  }
}

void
GumV8Platform::DisposeIsolate (Isolate ** isolate)
{
  Isolate * i = (Isolate *) g_steal_pointer (isolate);

  {
    GumV8PlatformLocker locker (this);
    dying_isolates.insert (i);
  }

  MaybeDisposeIsolate (i);
}

void
GumV8Platform::MaybeDisposeIsolate (Isolate * isolate)
{
  auto isolate_ops = GetPendingOperationsFor (isolate);
  for (const auto & op : isolate_ops)
    op->Cancel ();
  if (!isolate_ops.empty ())
    return;

  {
    GumV8PlatformLocker locker (this);

    if (disposing || dying_isolates.find (isolate) == dying_isolates.end ())
      return;

    foreground_runners.erase (isolate);
    dying_isolates.erase (isolate);
  }

  isolate->Dispose ();
}

void
GumV8Platform::ForgetIsolate (Isolate * isolate)
{
  std::unordered_set<std::shared_ptr<GumV8Operation>> isolate_ops;
  do
  {
    isolate_ops = GetPendingOperationsFor (isolate);

    for (const auto & op : isolate_ops)
      op->Cancel ();
    for (const auto & op : isolate_ops)
      op->Await ();
  }
  while (!isolate_ops.empty ());

  {
    GumV8PlatformLocker locker (this);

    foreground_runners.erase (isolate);
  }
}

std::unordered_set<std::shared_ptr<GumV8Operation>>
GumV8Platform::GetPendingOperationsFor (Isolate * isolate)
{
  std::unordered_set<std::shared_ptr<GumV8Operation>> isolate_ops;

  GumV8PlatformLocker locker (this);

  for (const auto & op : js_ops)
  {
    if (op->IsAnchoredTo (isolate))
      isolate_ops.insert (op);
  }

  for (const auto & op : pool_ops)
  {
    if (op->IsAnchoredTo (isolate))
      isolate_ops.insert (op);
  }

  return isolate_ops;
}

void
GumV8Platform::OnOperationRemoved (GumV8Operation * op)
{
  Isolate * isolate = op->isolate;
  if (isolate == nullptr)
    return;

  {
    GumV8PlatformLocker locker (this);
    if (dying_isolates.find (isolate) == dying_isolates.end ())
      return;
  }

  ScheduleOnJSThread (G_PRIORITY_HIGH, [=]()
      {
        MaybeDisposeIsolate (isolate);
      });
}

std::shared_ptr<GumV8Operation>
GumV8Platform::ScheduleOnJSThread (std::function<void ()> f)
{
  return ScheduleOnJSThreadDelayed (0, G_PRIORITY_DEFAULT, f);
}

std::shared_ptr<GumV8Operation>
GumV8Platform::ScheduleOnJSThread (gint priority,
                                   std::function<void ()> f)
{
  return ScheduleOnJSThreadDelayed (0, priority, f);
}

std::shared_ptr<GumV8Operation>
GumV8Platform::ScheduleOnJSThreadDelayed (guint delay_in_milliseconds,
                                          std::function<void ()> f)
{
  return ScheduleOnJSThreadDelayed (delay_in_milliseconds, G_PRIORITY_DEFAULT,
      f);
}

std::shared_ptr<GumV8Operation>
GumV8Platform::ScheduleOnJSThreadDelayed (guint delay_in_milliseconds,
                                          gint priority,
                                          std::function<void ()> f)
{
  GSource * source = (delay_in_milliseconds != 0)
      ? g_timeout_source_new (delay_in_milliseconds)
      : g_idle_source_new ();
  g_source_set_priority (source, priority);

  auto op = std::make_shared<GumV8MainContextOperation> (this, f, source,
      delay_in_milliseconds);

  {
    GumV8PlatformLocker locker (this);
    js_ops.insert (op);
  }

  g_source_set_callback (source, PerformMainContextOperation,
      new std::shared_ptr<GumV8MainContextOperation> (op),
      ReleaseMainContextOperation);
  g_source_attach (source, gum_script_scheduler_get_js_context (scheduler));

  return op;
}

void
GumV8Platform::PerformOnJSThread (std::function<void ()> f)
{
  PerformOnJSThread (G_PRIORITY_DEFAULT, f);
}

void
GumV8Platform::PerformOnJSThread (gint priority,
                                  std::function<void ()> f)
{
  GSource * source = g_idle_source_new ();
  g_source_set_priority (source, priority);

  auto op = std::make_shared<GumV8MainContextOperation> (this, f, source, 0);

  g_source_set_callback (source, PerformMainContextOperation,
      new std::shared_ptr<GumV8MainContextOperation> (op),
      ReleaseSynchronousMainContextOperation);
  g_source_attach (source, gum_script_scheduler_get_js_context (scheduler));

  op->Await ();
}

std::shared_ptr<GumV8Operation>
GumV8Platform::ScheduleOnThreadPool (std::function<void ()> f)
{
  auto op = std::make_shared<GumV8ThreadPoolOperation> (this, f);

  {
    GumV8PlatformLocker locker (this);
    pool_ops.insert (op);
  }

  gum_script_scheduler_push_job_on_thread_pool (scheduler,
      PerformThreadPoolOperation,
      new std::shared_ptr<GumV8ThreadPoolOperation> (op),
      ReleaseThreadPoolOperation);

  return op;
}

std::shared_ptr<GumV8Operation>
GumV8Platform::ScheduleOnThreadPoolDelayed (guint delay_in_milliseconds,
                                            std::function<void ()> f)
{
  GSource * source = g_timeout_source_new (delay_in_milliseconds);
  g_source_set_priority (source, G_PRIORITY_HIGH);

  auto op = std::make_shared<GumV8DelayedThreadPoolOperation> (this, f, source);

  {
    GumV8PlatformLocker locker (this);
    pool_ops.insert (op);
  }

  g_source_set_callback (source, StartDelayedThreadPoolOperation,
      new std::shared_ptr<GumV8DelayedThreadPoolOperation> (op),
      ReleaseDelayedThreadPoolOperation);
  g_source_attach (source, gum_script_scheduler_get_js_context (scheduler));

  return op;
}

gboolean
GumV8Platform::PerformMainContextOperation (gpointer data)
{
  auto operation = (std::shared_ptr<GumV8MainContextOperation> *) data;

  (*operation)->Perform ();

  return FALSE;
}

void
GumV8Platform::ReleaseMainContextOperation (gpointer data)
{
  auto ptr = (std::shared_ptr<GumV8MainContextOperation> *) data;

  auto op = *ptr;
  auto platform = op->platform;
  {
    GumV8PlatformLocker locker (platform);

    platform->js_ops.erase (op);
  }

  platform->OnOperationRemoved (op.get ());

  delete ptr;
}

void
GumV8Platform::ReleaseSynchronousMainContextOperation (gpointer data)
{
  auto ptr = (std::shared_ptr<GumV8MainContextOperation> *) data;

  delete ptr;
}

void
GumV8Platform::PerformThreadPoolOperation (gpointer data)
{
  auto operation = (std::shared_ptr<GumV8ThreadPoolOperation> *) data;

  (*operation)->Perform ();
}

void
GumV8Platform::ReleaseThreadPoolOperation (gpointer data)
{
  auto ptr = (std::shared_ptr<GumV8ThreadPoolOperation> *) data;

  auto op = *ptr;
  auto platform = op->platform;
  {
    GumV8PlatformLocker locker (platform);

    platform->pool_ops.erase (op);
  }

  platform->OnOperationRemoved (op.get ());

  delete ptr;
}

gboolean
GumV8Platform::StartDelayedThreadPoolOperation (gpointer data)
{
  auto ptr = (std::shared_ptr<GumV8DelayedThreadPoolOperation> *) data;
  auto op = *ptr;

  gum_script_scheduler_push_job_on_thread_pool (op->platform->scheduler,
      PerformDelayedThreadPoolOperation,
      new std::shared_ptr<GumV8DelayedThreadPoolOperation> (op),
      ReleaseDelayedThreadPoolOperation);

  return FALSE;
}

void
GumV8Platform::PerformDelayedThreadPoolOperation (gpointer data)
{
  auto operation = (std::shared_ptr<GumV8DelayedThreadPoolOperation> *) data;

  (*operation)->Perform ();
}

void
GumV8Platform::ReleaseDelayedThreadPoolOperation (gpointer data)
{
  auto ptr = (std::shared_ptr<GumV8DelayedThreadPoolOperation> *) data;

  auto op = *ptr;
  auto platform = op->platform;
  bool removed = false;
  {
    GumV8PlatformLocker locker (platform);

    switch (op->state)
    {
      case GumV8DelayedThreadPoolOperation::kScheduled:
      case GumV8DelayedThreadPoolOperation::kRunning:
        break;
      case GumV8DelayedThreadPoolOperation::kCompleted:
      case GumV8DelayedThreadPoolOperation::kCanceling:
      case GumV8DelayedThreadPoolOperation::kCanceled:
        platform->pool_ops.erase (op);
        removed = true;
        break;
    }
  }

  if (removed)
    platform->OnOperationRemoved (op.get ());

  delete ptr;
}

PageAllocator *
GumV8Platform::GetPageAllocator ()
{
  return page_allocator.get ();
}

int
GumV8Platform::NumberOfWorkerThreads ()
{
  return g_get_num_processors ();
}

std::shared_ptr<TaskRunner>
GumV8Platform::GetForegroundTaskRunner (Isolate * isolate)
{
  GumV8PlatformLocker locker (this);

  auto runner = foreground_runners[isolate];
  if (!runner)
  {
    runner = std::make_shared<GumV8ForegroundTaskRunner> (this, isolate);
    foreground_runners[isolate] = runner;
  }

  return runner;
}

void
GumV8Platform::CallOnWorkerThread (std::unique_ptr<Task> task)
{
  std::shared_ptr<Task> t (std::move (task));
  ScheduleOnThreadPool ([=]() { t->Run (); });
}

void
GumV8Platform::CallDelayedOnWorkerThread (std::unique_ptr<Task> task,
                                          double delay_in_seconds)
{
  std::shared_ptr<Task> t (std::move (task));
  ScheduleOnThreadPoolDelayed (delay_in_seconds * 1000.0, [=]()
      {
        t->Run ();
      });
}

bool
GumV8Platform::IdleTasksEnabled (Isolate * isolate)
{
  return true;
}

std::unique_ptr<JobHandle>
GumV8Platform::CreateJob (TaskPriority priority,
                          std::unique_ptr<JobTask> job_task)
{
  size_t num_worker_threads = NumberOfWorkerThreads ();
  if (priority == TaskPriority::kBestEffort)
    num_worker_threads = std::min (num_worker_threads, (size_t) 2);

  return std::make_unique<GumV8JobHandle> (std::make_shared<GumV8JobState> (
      this, std::move (job_task), priority, num_worker_threads));
}

double
GumV8Platform::MonotonicallyIncreasingTime ()
{
  gint64 usec = g_get_monotonic_time ();

  double result = (double) (usec / G_USEC_PER_SEC);
  result += (double) (usec % G_USEC_PER_SEC) / (double) G_USEC_PER_SEC;
  return result;
}

double
GumV8Platform::CurrentClockTimeMillis ()
{
  gint64 usec = g_get_real_time ();

  double result = (double) (usec / 1000);
  result += (double) (usec % 1000) / 1000.0;
  return result;
}

ThreadingBackend *
GumV8Platform::GetThreadingBackend ()
{
  return threading_backend.get ();
}

TracingController *
GumV8Platform::GetTracingController ()
{
  return tracing_controller.get ();
}

ArrayBuffer::Allocator *
GumV8Platform::GetArrayBufferAllocator () const
{
  return array_buffer_allocator.get ();
}

GumV8Operation::GumV8Operation ()
  : isolate (Isolate::TryGetCurrent ())
{
}

void
GumV8Operation::AnchorTo (v8::Isolate * i)
{
  isolate = i;
}

bool
GumV8Operation::IsAnchoredTo (Isolate * i) const
{
  return isolate == i;
}

GumV8MainContextOperation::GumV8MainContextOperation (
    GumV8Platform * platform,
    std::function<void ()> func,
    GSource * source,
    guint delay_in_milliseconds)
  : platform (platform),
    func (func),
    source (source),
    delay_in_milliseconds (delay_in_milliseconds),
    state (kScheduled)
{
  g_cond_init (&cond);
}

GumV8MainContextOperation::~GumV8MainContextOperation ()
{
  g_source_unref (source);
  g_cond_clear (&cond);
}

void
GumV8MainContextOperation::Perform ()
{
  {
    GumV8PlatformLocker locker (platform);
    if (state != kScheduled)
      return;
    state = kRunning;
  }

  func ();

  {
    GumV8PlatformLocker locker (platform);
    state = kCompleted;
    g_cond_signal (&cond);
  }
}

void
GumV8MainContextOperation::Cancel ()
{
  if (delay_in_milliseconds == 0)
    return;

  {
    GumV8PlatformLocker locker (platform);
    if (state != kScheduled)
      return;
    state = kCanceling;
  }

  g_source_destroy (source);

  {
    GumV8PlatformLocker locker (platform);
    state = kCanceled;
    g_cond_signal (&cond);
  }
}

void
GumV8MainContextOperation::Await ()
{
  GumV8PlatformLocker locker (platform);

  GMainContext * context =
      gum_script_scheduler_get_js_context (platform->scheduler);
  gboolean called_from_js_thread = g_main_context_is_owner (context);

  while (state != kCompleted && state != kCanceled)
  {
    if (called_from_js_thread)
    {
      g_mutex_unlock (&platform->mutex);
      g_main_context_iteration (context, TRUE);
      g_mutex_lock (&platform->mutex);
    }
    else
    {
      g_cond_wait (&cond, &platform->mutex);
    }
  }
}

GumV8ThreadPoolOperation::GumV8ThreadPoolOperation (
    GumV8Platform * platform,
    std::function<void ()> func)
  : platform (platform),
    func (func),
    state (kScheduled)
{
  g_cond_init (&cond);
}

GumV8ThreadPoolOperation::~GumV8ThreadPoolOperation ()
{
  g_cond_clear (&cond);
}

void
GumV8ThreadPoolOperation::Perform ()
{
  {
    GumV8PlatformLocker locker (platform);
    if (state != kScheduled)
      return;
    state = kRunning;
  }

  func ();

  {
    GumV8PlatformLocker locker (platform);
    state = kCompleted;
    g_cond_signal (&cond);
  }
}

void
GumV8ThreadPoolOperation::Cancel ()
{
}

void
GumV8ThreadPoolOperation::Await ()
{
  GumV8PlatformLocker locker (platform);
  while (state != kCompleted && state != kCanceled)
    g_cond_wait (&cond, &platform->mutex);
}

GumV8DelayedThreadPoolOperation::GumV8DelayedThreadPoolOperation (
    GumV8Platform * platform,
    std::function<void ()> func,
    GSource * source)
  : platform (platform),
    func (func),
    source (source),
    state (kScheduled)
{
  g_cond_init (&cond);
}

GumV8DelayedThreadPoolOperation::~GumV8DelayedThreadPoolOperation ()
{
  g_source_unref (source);
  g_cond_clear (&cond);
}

void
GumV8DelayedThreadPoolOperation::Perform ()
{
  {
    GumV8PlatformLocker locker (platform);
    if (state != kScheduled)
      return;
    state = kRunning;
  }

  func ();

  {
    GumV8PlatformLocker locker (platform);
    state = kCompleted;
    g_cond_signal (&cond);
  }
}

void
GumV8DelayedThreadPoolOperation::Cancel ()
{
  {
    GumV8PlatformLocker locker (platform);
    if (state != kScheduled)
      return;
    state = kCanceling;
  }

  g_source_destroy (source);

  {
    GumV8PlatformLocker locker (platform);
    state = kCanceled;
    g_cond_signal (&cond);
  }
}

void
GumV8DelayedThreadPoolOperation::Await ()
{
  GumV8PlatformLocker locker (platform);
  while (state != kCompleted && state != kCanceled)
    g_cond_wait (&cond, &platform->mutex);
}

GumV8ForegroundTaskRunner::GumV8ForegroundTaskRunner (GumV8Platform * platform,
                                                      Isolate * isolate)
  : platform (platform),
    isolate (isolate),
    pending (g_hash_table_new (NULL, NULL))
{
}

GumV8ForegroundTaskRunner::~GumV8ForegroundTaskRunner ()
{
  g_hash_table_unref (pending);
}

void
GumV8ForegroundTaskRunner::PostTask (std::unique_ptr<Task> task)
{
  std::shared_ptr<Task> t (std::move (task));
  platform->ScheduleOnJSThread ([=]()
      {
        Run (t.get ());
      });
}

void
GumV8ForegroundTaskRunner::PostNonNestableTask (std::unique_ptr<Task> task)
{
  PostTask (std::move (task));
}

void
GumV8ForegroundTaskRunner::PostDelayedTask (std::unique_ptr<Task> task,
                                            double delay_in_seconds)
{
  std::shared_ptr<Task> t (std::move (task));
  platform->ScheduleOnJSThreadDelayed (delay_in_seconds * 1000.0, [=]()
      {
        Run (t.get ());
      });
}

void
GumV8ForegroundTaskRunner::PostNonNestableDelayedTask (
    std::unique_ptr<Task> task,
    double delay_in_seconds)
{
  PostDelayedTask (std::move (task), delay_in_seconds);
}

void
GumV8ForegroundTaskRunner::PostIdleTask (std::unique_ptr<IdleTask> task)
{
  std::shared_ptr<IdleTask> t (std::move (task));
  platform->ScheduleOnJSThread (G_PRIORITY_LOW, [=]()
      {
        Run (t.get ());
      });
}

bool
GumV8ForegroundTaskRunner::IdleTasksEnabled ()
{
  return true;
}

bool
GumV8ForegroundTaskRunner::NonNestableTasksEnabled () const
{
  return true;
}

bool
GumV8ForegroundTaskRunner::NonNestableDelayedTasksEnabled () const
{
  return true;
}

void
GumV8ForegroundTaskRunner::Run (Task * task)
{
  Locker locker (isolate);
  Isolate::Scope isolate_scope (isolate);
  HandleScope handle_scope (isolate);

  task->Run ();
}

void
GumV8ForegroundTaskRunner::Run (IdleTask * task)
{
  Locker locker (isolate);
  Isolate::Scope isolate_scope (isolate);
  HandleScope handle_scope (isolate);

  const double deadline_in_seconds =
      platform->MonotonicallyIncreasingTime () + (1.0 / 60.0);
  task->Run (deadline_in_seconds);
}

GumV8JobState::GumV8JobState (GumV8Platform * platform,
                              std::unique_ptr<JobTask> job_task,
                              TaskPriority priority,
                              size_t num_worker_threads)
  : isolate (Isolate::GetCurrent ()),
    platform (platform),
    job_task (std::move (job_task)),
    priority (priority),
    num_worker_threads (std::min (num_worker_threads, kMaxWorkerPerJob))
{
  g_mutex_init (&mutex);
  g_cond_init (&worker_released_cond);
}

GumV8JobState::~GumV8JobState ()
{
  g_assert (active_workers == 0);

  g_cond_clear (&worker_released_cond);
  g_mutex_clear (&mutex);
}

void
GumV8JobState::NotifyConcurrencyIncrease ()
{
  if (is_canceled.load (std::memory_order_relaxed))
    return;

  size_t num_tasks_to_post = 0;
  TaskPriority priority_to_use;
  {
    GumMutexLocker locker (&mutex);

    const size_t max_concurrency = CappedMaxConcurrency (active_workers);
    if (active_workers + pending_tasks < max_concurrency)
    {
      num_tasks_to_post = max_concurrency - active_workers - pending_tasks;
      pending_tasks += num_tasks_to_post;
    }

    priority_to_use = priority;
  }

  for (size_t i = 0; i != num_tasks_to_post; i++)
  {
    CallOnWorkerThread (priority_to_use, std::make_unique<GumV8JobWorker> (
        shared_from_this (), job_task.get ()));
  }
}

uint8_t
GumV8JobState::AcquireTaskId ()
{
  uint32_t task_ids = assigned_task_ids.load (std::memory_order_relaxed);
  uint32_t new_task_ids = 0;

  uint8_t task_id = 0;
  do
  {
    task_id = g_bit_nth_lsf (~task_ids, -1);
    new_task_ids = task_ids | (uint32_t (1) << task_id);
  }
  while (!assigned_task_ids.compare_exchange_weak (task_ids, new_task_ids,
      std::memory_order_acquire, std::memory_order_relaxed));

  return task_id;
}

void
GumV8JobState::ReleaseTaskId (uint8_t task_id)
{
  assigned_task_ids.fetch_and (~(uint32_t (1) << task_id),
      std::memory_order_release);
}

void
GumV8JobState::Join ()
{
  bool can_run = false;

  {
    GumMutexLocker locker (&mutex);

    priority = TaskPriority::kUserBlocking;
    num_worker_threads = platform->NumberOfWorkerThreads () + 1;
    active_workers++;

    can_run = WaitForParticipationOpportunityLocked ();
  }

  GumV8JobState::JobDelegate delegate (this, true);
  while (can_run)
  {
    {
      Locker locker (isolate);
      job_task->Run (&delegate);
    }

    GumMutexLocker locker (&mutex);
    can_run = WaitForParticipationOpportunityLocked ();
  }
}

void
GumV8JobState::CancelAndWait ()
{
  GumMutexLocker locker (&mutex);

  is_canceled.store (true, std::memory_order_relaxed);

  while (active_workers > 0)
    g_cond_wait (&worker_released_cond, &mutex);
}

void
GumV8JobState::CancelAndDetach ()
{
  GumMutexLocker locker (&mutex);

  is_canceled.store (true, std::memory_order_relaxed);
}

bool
GumV8JobState::IsActive ()
{
  GumMutexLocker locker (&mutex);

  return job_task->GetMaxConcurrency (active_workers) != 0 ||
      active_workers != 0;
}

void
GumV8JobState::UpdatePriority (TaskPriority new_priority)
{
  GumMutexLocker locker (&mutex);

  priority = new_priority;
}

bool
GumV8JobState::CanRunFirstTask ()
{
  GumMutexLocker locker (&mutex);

  pending_tasks--;

  if (is_canceled.load (std::memory_order_relaxed))
    return false;

  const size_t max_workers = std::min (
      job_task->GetMaxConcurrency (active_workers), num_worker_threads);
  if (active_workers >= max_workers)
    return false;

  active_workers++;
  return true;
}

bool
GumV8JobState::DidRunTask ()
{
  size_t num_tasks_to_post = 0;
  TaskPriority priority_to_use;
  {
    GumMutexLocker locker (&mutex);

    const size_t max_concurrency = CappedMaxConcurrency (active_workers - 1);
    if (is_canceled.load (std::memory_order_relaxed) ||
        active_workers > max_concurrency)
    {
      active_workers--;
      g_cond_signal (&worker_released_cond);
      return false;
    }

    if (active_workers + pending_tasks < max_concurrency)
    {
      num_tasks_to_post = max_concurrency - active_workers - pending_tasks;
      pending_tasks += num_tasks_to_post;
    }

    priority_to_use = priority;
  }

  for (size_t i = 0; i != num_tasks_to_post; i++)
  {
    CallOnWorkerThread (priority_to_use, std::make_unique<GumV8JobWorker> (
        shared_from_this (), job_task.get ()));
  }

  return true;
}

bool
GumV8JobState::WaitForParticipationOpportunityLocked ()
{
  size_t max_concurrency = CappedMaxConcurrency (active_workers - 1);
  while (active_workers > max_concurrency && active_workers > 1)
  {
    g_cond_wait (&worker_released_cond, &mutex);
    max_concurrency = CappedMaxConcurrency (active_workers - 1);
  }

  if (active_workers <= max_concurrency)
    return true;

  g_assert (active_workers == 1);
  g_assert (max_concurrency == 0);

  active_workers = 0;
  is_canceled.store (true, std::memory_order_relaxed);

  return false;
}

size_t
GumV8JobState::CappedMaxConcurrency (size_t worker_count) const
{
  return std::min (job_task->GetMaxConcurrency (worker_count),
      num_worker_threads);
}

void
GumV8JobState::CallOnWorkerThread (TaskPriority with_priority,
                                   std::unique_ptr<Task> task)
{
  std::shared_ptr<Task> t (std::move (task));
  Isolate * job_isolate = this->isolate;
  auto op = platform->ScheduleOnThreadPool ([=]()
      {
        Locker locker (job_isolate);
        t->Run ();
      });

  {
    GumV8PlatformLocker locker (platform);
    op->AnchorTo (isolate);
  }
}

GumV8JobState::JobDelegate::JobDelegate (GumV8JobState * parent,
                                         bool is_joining_thread)
  : parent (parent),
    is_joining_thread (is_joining_thread)
{
}

GumV8JobState::JobDelegate::~JobDelegate ()
{
  if (task_id != kInvalidTaskId)
    parent->ReleaseTaskId (task_id);
}

void
GumV8JobState::JobDelegate::NotifyConcurrencyIncrease ()
{
  parent->NotifyConcurrencyIncrease ();
}

bool
GumV8JobState::JobDelegate::ShouldYield ()
{
  return parent->is_canceled.load (std::memory_order_relaxed);
}

uint8_t
GumV8JobState::JobDelegate::GetTaskId ()
{
  if (task_id == kInvalidTaskId)
    task_id = parent->AcquireTaskId ();
  return task_id;
}

GumV8JobHandle::GumV8JobHandle (std::shared_ptr<GumV8JobState> state)
  : state (std::move (state))
{
}

GumV8JobHandle::~GumV8JobHandle ()
{
  g_assert (state == nullptr);
}

void
GumV8JobHandle::NotifyConcurrencyIncrease ()
{
  state->NotifyConcurrencyIncrease ();
}

void
GumV8JobHandle::Join ()
{
  state->Join ();
  state = nullptr;
}

void
GumV8JobHandle::Cancel ()
{
  state->CancelAndWait ();
  state = nullptr;
}

void
GumV8JobHandle::CancelAndDetach ()
{
  state->CancelAndDetach ();
  state = nullptr;
}

bool
GumV8JobHandle::IsActive ()
{
  return state->IsActive ();
}

void
GumV8JobHandle::UpdatePriority (TaskPriority new_priority)
{
  state->UpdatePriority (new_priority);
}

GumV8JobWorker::GumV8JobWorker (std::weak_ptr<GumV8JobState> state,
                                JobTask * job_task)
  : state (std::move (state)),
    job_task (job_task)
{
}

void
GumV8JobWorker::Run ()
{
  auto shared_state = state.lock ();
  if (shared_state == nullptr)
    return;

  if (!shared_state->CanRunFirstTask ())
    return;

  do
  {
    GumV8JobState::JobDelegate delegate (shared_state.get (), false);
    job_task->Run (&delegate);
  }
  while (shared_state->DidRunTask ());
}

size_t
GumV8PageAllocator::AllocatePageSize ()
{
  return gum_query_page_size ();
}

size_t
GumV8PageAllocator::CommitPageSize ()
{
  return gum_query_page_size ();
}

void
GumV8PageAllocator::SetRandomMmapSeed (int64_t seed)
{
}

void *
GumV8PageAllocator::GetRandomMmapAddr ()
{
  return GSIZE_TO_POINTER (16384);
}

void *
GumV8PageAllocator::AllocatePages (void * address,
                                   size_t length,
                                   size_t alignment,
                                   Permission permissions)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  gpointer base = NULL;
  /*
   * Restricted to macOS: on iOS these MAP_JIT pages resist our
   * SetPermissions(), so we let gum_memory_allocate() serve V8's code range.
   */
#ifdef HAVE_MACOS
  if (permissions == PageAllocator::kNoAccessWillJitLater)
  {
    g_assert (alignment == gum_query_page_size ());

    base = mmap (address, length, PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, VM_MAKE_TAG (255), 0);
    if (base == MAP_FAILED)
      base = NULL;
  }
#endif
  if (base == NULL)
  {
    base = gum_memory_allocate (address, length, alignment,
        gum_page_protection_from_v8 (permissions));
  }
  if (base == NULL)
    return nullptr;

  GumMemoryRange range;
  range.base_address = GPOINTER_TO_SIZE (base);
  range.size = length;
  gum_cloak_add_range (&range);

  return base;
}

bool
GumV8PageAllocator::FreePages (void * address,
                               size_t length)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  if (!gum_memory_free (address, length))
    return false;

  GumMemoryRange range;
  range.base_address = GPOINTER_TO_SIZE (address);
  range.size = length;
  gum_cloak_remove_range (&range);

  return true;
}

bool
GumV8PageAllocator::ReleasePages (void * address,
                                  size_t length,
                                  size_t new_length)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  const gpointer released_base = (guint8 *) address + new_length;
  const gsize released_size = length - new_length;
  if (!gum_memory_release (released_base, released_size))
    return false;

#ifndef HAVE_WINDOWS
  GumMemoryRange range;
  range.base_address = GPOINTER_TO_SIZE (released_base);
  range.size = released_size;
  gum_cloak_remove_range (&range);
#endif

  return true;
}

bool
GumV8PageAllocator::SetPermissions (void * address,
                                    size_t length,
                                    Permission permissions)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  GumPageProtection prot = gum_page_protection_from_v8 (permissions);

  gboolean success;
#if defined (HAVE_WINDOWS)
  if (permissions == PageAllocator::kNoAccess)
    success = gum_memory_decommit (address, length);
  else
    success = gum_memory_recommit (address, length, prot);
#elif defined (HAVE_DARWIN)
  if (permissions == PageAllocator::kReadExecute &&
      gum_code_segment_is_supported ())
  {
    success = gum_code_segment_mark (address, length, NULL);
  }
  else
  {
    int bsd_prot = 0;
    switch (permissions)
    {
      case PageAllocator::kNoAccess:
      case PageAllocator::kNoAccessWillJitLater:
        bsd_prot = PROT_NONE;
        break;
      case PageAllocator::kRead:
        bsd_prot = PROT_READ;
        break;
      case PageAllocator::kReadWrite:
        bsd_prot = PROT_READ | PROT_WRITE;
        break;
      case PageAllocator::kReadWriteExecute:
        bsd_prot = PROT_READ | PROT_WRITE | PROT_EXEC;
        break;
      case PageAllocator::kReadExecute:
        bsd_prot = PROT_READ | PROT_EXEC;
        break;
      default:
        g_assert_not_reached ();
    }

    success = mprotect (address, length, bsd_prot) == 0;

    if (!success && permissions == PageAllocator::kNoAccess)
    {
      /*
       * XNU refuses to transition from ReadWriteExecute to NoAccess, so do what
       * the default v8::PageAllocator does and just discard the pages.
       */
      return gum_memory_discard (address, length) != FALSE;
    }
  }

  if (success && permissions == PageAllocator::kNoAccess)
    gum_memory_discard (address, length);

  if (permissions != PageAllocator::kNoAccess)
    gum_memory_recommit (address, length, prot);
#else
  success = gum_try_mprotect (address, length, prot);

  if (success && permissions == PageAllocator::kNoAccess)
    gum_memory_discard (address, length);
#endif

  return success != FALSE;
}

bool
GumV8PageAllocator::RecommitPages (void * address,
                                   size_t length,
                                   Permission permissions)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  return gum_memory_recommit (address, length,
      gum_page_protection_from_v8 (permissions)) != FALSE;
}

bool
GumV8PageAllocator::DiscardSystemPages (void * address,
                                        size_t size)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  return gum_memory_discard (address, size) != FALSE;
}

bool
GumV8PageAllocator::DecommitPages (void * address,
                                   size_t size)
{
  GumV8InterceptorIgnoreScope interceptor_ignore_scope;

  return gum_memory_decommit (address, size) != FALSE;
}

void *
GumV8ArrayBufferAllocator::Allocate (size_t length)
{
  return g_malloc0 (MAX (length, 1));
}

void *
GumV8ArrayBufferAllocator::AllocateUninitialized (size_t length)
{
  return g_malloc (MAX (length, 1));
}

void
GumV8ArrayBufferAllocator::Free (void * data,
                                 size_t length)
{
  g_free (data);
}

void *
GumV8ArrayBufferAllocator::Reallocate (void * data,
                                       size_t old_length,
                                       size_t new_length)
{
  return gum_realloc (data, new_length);
}

MutexImpl *
GumV8ThreadingBackend::CreatePlainMutex ()
{
  return new GumMutex ();
}

MutexImpl *
GumV8ThreadingBackend::CreateRecursiveMutex ()
{
  return new GumRecursiveMutex ();
}

SharedMutexImpl *
GumV8ThreadingBackend::CreateSharedMutex ()
{
  return new GumSharedMutex ();
}

ConditionVariableImpl *
GumV8ThreadingBackend::CreateConditionVariable ()
{
  return new GumConditionVariable ();
}

GumMutex::GumMutex ()
{
  g_mutex_init (&mutex);
}

GumMutex::~GumMutex ()
{
  g_mutex_clear (&mutex);
}

void
GumMutex::Lock ()
{
  g_mutex_lock (&mutex);
}

void
GumMutex::Unlock ()
{
  g_mutex_unlock (&mutex);
}

bool
GumMutex::TryLock ()
{
  return !!g_mutex_trylock (&mutex);
}

GumRecursiveMutex::GumRecursiveMutex ()
{
  g_rec_mutex_init (&mutex);
}

GumRecursiveMutex::~GumRecursiveMutex ()
{
  g_rec_mutex_clear (&mutex);
}

void
GumRecursiveMutex::Lock ()
{
  g_rec_mutex_lock (&mutex);
}

void
GumRecursiveMutex::Unlock ()
{
  g_rec_mutex_unlock (&mutex);
}

bool
GumRecursiveMutex::TryLock ()
{
  return !!g_rec_mutex_trylock (&mutex);
}

GumSharedMutex::GumSharedMutex ()
{
  g_rw_lock_init (&lock);
}

GumSharedMutex::~GumSharedMutex ()
{
  g_rw_lock_clear (&lock);
}

void
GumSharedMutex::LockShared ()
{
  g_rw_lock_reader_lock (&lock);
}

void
GumSharedMutex::LockExclusive ()
{
  g_rw_lock_writer_lock (&lock);
}

void
GumSharedMutex::UnlockShared ()
{
  g_rw_lock_reader_unlock (&lock);
}

void
GumSharedMutex::UnlockExclusive ()
{
  g_rw_lock_writer_unlock (&lock);
}

bool
GumSharedMutex::TryLockShared ()
{
  return !!g_rw_lock_reader_trylock (&lock);
}

bool
GumSharedMutex::TryLockExclusive ()
{
  return !!g_rw_lock_writer_trylock (&lock);
}

GumConditionVariable::GumConditionVariable ()
{
  g_cond_init (&cond);
}

GumConditionVariable::~GumConditionVariable ()
{
  g_cond_clear (&cond);
}

void
GumConditionVariable::NotifyOne ()
{
  g_cond_signal (&cond);
}

void
GumConditionVariable::NotifyAll ()
{
  g_cond_broadcast (&cond);
}

void
GumConditionVariable::Wait (MutexImpl * mutex)
{
  GumMutex * m = (GumMutex *) mutex;
  g_cond_wait (&cond, &m->mutex);
}

bool
GumConditionVariable::WaitFor (MutexImpl * mutex,
                               int64_t delta_in_microseconds)
{
  GumMutex * m = (GumMutex *) mutex;
  gint64 deadline = g_get_monotonic_time () + delta_in_microseconds;
  return !!g_cond_wait_until (&cond, &m->mutex, deadline);
}

static GumPageProtection
gum_page_protection_from_v8 (PageAllocator::Permission permission)
{
  switch (permission)
  {
    case PageAllocator::kNoAccess:
    case PageAllocator::kNoAccessWillJitLater:
      return GUM_PAGE_NO_ACCESS;
    case PageAllocator::kRead:
      return GUM_PAGE_READ;
    case PageAllocator::kReadWrite:
      return GUM_PAGE_RW;
    case PageAllocator::kReadWriteExecute:
      return GUM_PAGE_RWX;
    case PageAllocator::kReadExecute:
      return GUM_PAGE_RX;
    default:
      g_assert_not_reached ();
      return GUM_PAGE_NO_ACCESS;
  }
}
