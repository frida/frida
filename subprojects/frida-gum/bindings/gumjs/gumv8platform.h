/*
 * Copyright (C) 2015-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_V8_PLATFORM_H__
#define __GUM_V8_PLATFORM_H__

#include "gumscriptscheduler.h"

#include <functional>
#include <map>
#include <unordered_set>
#include <v8.h>
#include <v8-platform.h>

class GumV8Operation;
class GumV8MainContextOperation;
class GumV8ThreadPoolOperation;
class GumV8DelayedThreadPoolOperation;
class GumV8PlatformLocker;
class GumV8PlatformUnlocker;

class GumV8Platform : public v8::Platform
{
public:
  GumV8Platform ();
  GumV8Platform (const GumV8Platform &) = delete;
  GumV8Platform & operator= (const GumV8Platform &) = delete;
  ~GumV8Platform ();

  void DisposeIsolate (v8::Isolate ** isolate);
  void ForgetIsolate (v8::Isolate * isolate);

  GumScriptScheduler * GetScheduler () const { return scheduler; }
  std::shared_ptr<GumV8Operation> ScheduleOnJSThread (std::function<void ()> f);
  std::shared_ptr<GumV8Operation> ScheduleOnJSThread (gint priority,
      std::function<void ()> f);
  std::shared_ptr<GumV8Operation> ScheduleOnJSThreadDelayed (
      guint delay_in_milliseconds, std::function<void ()> f);
  std::shared_ptr<GumV8Operation> ScheduleOnJSThreadDelayed (
      guint delay_in_milliseconds, gint priority, std::function<void ()> f);
  void PerformOnJSThread (std::function<void ()> f);
  void PerformOnJSThread (gint priority, std::function<void ()> f);
  std::shared_ptr<GumV8Operation> ScheduleOnThreadPool (
      std::function<void ()> f);
  std::shared_ptr<GumV8Operation> ScheduleOnThreadPoolDelayed (
      guint delay_in_milliseconds, std::function<void ()> f);

  v8::PageAllocator * GetPageAllocator () override;
  int NumberOfWorkerThreads () override;
  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner (
      v8::Isolate * isolate) override;
  void CallOnWorkerThread (std::unique_ptr<v8::Task> task) override;
  void CallDelayedOnWorkerThread (std::unique_ptr<v8::Task> task,
      double delay_in_seconds) override;
  bool IdleTasksEnabled (v8::Isolate * isolate) override;
  std::unique_ptr<v8::JobHandle> CreateJob (v8::TaskPriority priority,
      std::unique_ptr<v8::JobTask> job_task) override;
  double MonotonicallyIncreasingTime () override;
  double CurrentClockTimeMillis () override;
  v8::ThreadingBackend * GetThreadingBackend () override;
  v8::TracingController * GetTracingController () override;

  v8::ArrayBuffer::Allocator * GetArrayBufferAllocator () const;

private:
  void InitRuntime ();
  void Dispose ();
  void CancelPendingOperations ();
  void MaybeDisposeIsolate (v8::Isolate * isolate);
  std::unordered_set<std::shared_ptr<GumV8Operation>> GetPendingOperationsFor (
      v8::Isolate * isolate);
  void OnOperationRemoved (GumV8Operation * op);

  static gboolean PerformMainContextOperation (gpointer data);
  static void ReleaseMainContextOperation (gpointer data);
  static void ReleaseSynchronousMainContextOperation (gpointer data);
  static void PerformThreadPoolOperation (gpointer data);
  static void ReleaseThreadPoolOperation (gpointer data);
  static gboolean StartDelayedThreadPoolOperation (gpointer data);
  static void PerformDelayedThreadPoolOperation (gpointer data);
  static void ReleaseDelayedThreadPoolOperation (gpointer data);

  GMutex mutex;
  bool disposing;
  GumScriptScheduler * scheduler;
  std::unordered_set<v8::Isolate *> dying_isolates;
  std::unordered_set<std::shared_ptr<GumV8Operation>> js_ops;
  std::unordered_set<std::shared_ptr<GumV8Operation>> pool_ops;
  std::map<v8::Isolate *, std::shared_ptr<v8::TaskRunner>> foreground_runners;
  std::unique_ptr<v8::PageAllocator> page_allocator;
  std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator;
  std::unique_ptr<v8::ThreadingBackend> threading_backend;
  std::unique_ptr<v8::TracingController> tracing_controller;

  friend class GumV8MainContextOperation;
  friend class GumV8ThreadPoolOperation;
  friend class GumV8DelayedThreadPoolOperation;
  friend class GumV8PlatformLocker;
  friend class GumV8PlatformUnlocker;
};

class GumV8Operation
{
public:
  GumV8Operation ();
  GumV8Operation (const GumV8Operation &) = delete;
  GumV8Operation & operator= (const GumV8Operation &) = delete;
  virtual ~GumV8Operation () = default;

  void AnchorTo (v8::Isolate * i);
  bool IsAnchoredTo (v8::Isolate * i) const;

  virtual void Cancel () = 0;
  virtual void Await () = 0;

private:
  v8::Isolate * isolate;

  friend class GumV8Platform;
};

#endif
