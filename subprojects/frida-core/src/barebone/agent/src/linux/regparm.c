extern void *_panic;

void frida_k_panic (void * a0)
{
  typedef void (* fn_t) (const char *, ...);

  ((fn_t) _panic) (a0);
}

extern void *_wake_up_process;

unsigned long frida_k_wake_up_process (void * a0)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (void *);

  return ((fn_t) _wake_up_process) (a0);
}

extern void *_fget;

void * frida_k_fget (unsigned long a0)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (unsigned long);

  return ((fn_t) _fget) (a0);
}

extern void *_kernel_read;

long frida_k_kernel_read (void * a0, void * a1, unsigned long a2, void * a3)
{
  typedef long (__attribute__((regparm(3))) * fn_t) (void *, void *, unsigned long, void *);

  return ((fn_t) _kernel_read) (a0, a1, a2, a3);
}

extern void *_kernel_write;

long frida_k_kernel_write (void * a0, void * a1, unsigned long a2, void * a3)
{
  typedef long (__attribute__((regparm(3))) * fn_t) (void *, void *, unsigned long, void *);

  return ((fn_t) _kernel_write) (a0, a1, a2, a3);
}

extern void *_kfree;

void frida_k_kfree (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _kfree) (a0);
}

extern void *___init_waitqueue_head;

void frida_k___init_waitqueue_head (void * a0, void * a1, void * a2)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *, void *, void *);

  ((fn_t) ___init_waitqueue_head) (a0, a1, a2);
}

extern void *_prepare_to_wait_event;

unsigned long frida_k_prepare_to_wait_event (void * a0, void * a1, unsigned long a2)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (void *, void *, unsigned long);

  return ((fn_t) _prepare_to_wait_event) (a0, a1, a2);
}

extern void *_finish_wait;

void frida_k_finish_wait (void * a0, void * a1)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *, void *);

  ((fn_t) _finish_wait) (a0, a1);
}

extern void *___wake_up;

void frida_k___wake_up (void * a0, unsigned long a1, unsigned long a2, void * a3)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *, unsigned long, unsigned long, void *);

  ((fn_t) ___wake_up) (a0, a1, a2, a3);
}

extern void *_schedule_hrtimeout_range;

unsigned long frida_k_schedule_hrtimeout_range (void * a0, unsigned long long a1, unsigned long a2)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (void *, unsigned long long, unsigned long);

  return ((fn_t) _schedule_hrtimeout_range) (a0, a1, a2);
}

extern void *_pci_get_domain_bus_and_slot;

void * frida_k_pci_get_domain_bus_and_slot (unsigned long a0, unsigned long a1, unsigned long a2)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (unsigned long, unsigned long, unsigned long);

  return ((fn_t) _pci_get_domain_bus_and_slot) (a0, a1, a2);
}

extern void *_pci_irq_vector;

unsigned long frida_k_pci_irq_vector (void * a0, unsigned long a1)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (void *, unsigned long);

  return ((fn_t) _pci_irq_vector) (a0, a1);
}

extern void *_pci_dev_put;

void frida_k_pci_dev_put (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _pci_dev_put) (a0);
}

extern void *_schedule;

void frida_k_schedule (void)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void);

  ((fn_t) _schedule) ();
}

extern void *_ktime_get_mono_fast_ns;

unsigned long long frida_k_ktime_get_mono_fast_ns (void)
{
  typedef unsigned long long (__attribute__((regparm(3))) * fn_t) (void);

  return ((fn_t) _ktime_get_mono_fast_ns) ();
}

extern void *_ktime_get_real_ts64;

void frida_k_ktime_get_real_ts64 (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _ktime_get_real_ts64) (a0);
}

extern void *_send_sig;

unsigned long frida_k_send_sig (unsigned long a0, void * a1, unsigned long a2)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (unsigned long, void *, unsigned long);

  return ((fn_t) _send_sig) (a0, a1, a2);
}

extern void *_free_irq;

void * frida_k_free_irq (unsigned long a0, void * a1)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (unsigned long, void *);

  return ((fn_t) _free_irq) (a0, a1);
}

extern void *_vmap;

void * frida_k_vmap (void * a0, unsigned long a1, unsigned long a2, unsigned long a3)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (void *, unsigned long, unsigned long, unsigned long);

  return ((fn_t) _vmap) (a0, a1, a2, a3);
}

extern void *_ioremap;

void * frida_k_ioremap (unsigned long a0, unsigned long a1)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (unsigned long, unsigned long);

  return ((fn_t) _ioremap) (a0, a1);
}

extern void *_get_task_exe_file;

void * frida_k_get_task_exe_file (void * a0)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (void *);

  return ((fn_t) _get_task_exe_file) (a0);
}

extern void *_file_path;

void * frida_k_file_path (void * a0, void * a1, unsigned long a2)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (void *, void *, unsigned long);

  return ((fn_t) _file_path) (a0, a1, a2);
}

extern void *_fput;

void frida_k_fput (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _fput) (a0);
}

extern void *_get_task_mm;

void * frida_k_get_task_mm (void * a0)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (void *);

  return ((fn_t) _get_task_mm) (a0);
}

extern void *_kthread_use_mm;

void frida_k_kthread_use_mm (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _kthread_use_mm) (a0);
}

extern void *_kthread_unuse_mm;

void frida_k_kthread_unuse_mm (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _kthread_unuse_mm) (a0);
}

extern void *_mmput;

void frida_k_mmput (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _mmput) (a0);
}

extern void *_vm_mmap;

unsigned long frida_k_vm_mmap (void * a0, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (void *, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);

  return ((fn_t) _vm_mmap) (a0, a1, a2, a3, a4, a5);
}

extern void *_vm_munmap;

unsigned long frida_k_vm_munmap (unsigned long a0, unsigned long a1)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (unsigned long, unsigned long);

  return ((fn_t) _vm_munmap) (a0, a1);
}

extern void *_vunmap;

void frida_k_vunmap (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _vunmap) (a0);
}

extern void *__copy_to_user;

unsigned long frida_k__copy_to_user (void * a0, void * a1, unsigned long a2)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (void *, void *, unsigned long);

  return ((fn_t) __copy_to_user) (a0, a1, a2);
}

extern void *_down_read;

void frida_k_down_read (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _down_read) (a0);
}

extern void *_up_read;

void frida_k_up_read (void * a0)
{
  typedef void (__attribute__((regparm(3))) * fn_t) (void *);

  ((fn_t) _up_read) (a0);
}

extern void *_get_user_pages_remote;

long frida_k_get_user_pages_remote (void * a0, unsigned long a1, unsigned long a2, unsigned long a3, void * a4, void * a5)
{
  typedef long (__attribute__((regparm(3))) * fn_t) (void *, unsigned long, unsigned long, unsigned long, void *, void *);

  return ((fn_t) _get_user_pages_remote) (a0, a1, a2, a3, a4, a5);
}

extern void *_do_futex;

long frida_k_do_futex (unsigned long a0, unsigned long a1, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5, unsigned long a6)
{
  typedef long (__attribute__((regparm(3))) * fn_t) (unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long, unsigned long);

  return ((fn_t) _do_futex) (a0, a1, a2, a3, a4, a5, a6);
}

extern void *_call_usermodehelper_exec;

unsigned long frida_k_call_usermodehelper_exec (void * a0, unsigned long a1)
{
  typedef unsigned long (__attribute__((regparm(3))) * fn_t) (void *, unsigned long);

  return ((fn_t) _call_usermodehelper_exec) (a0, a1);
}

extern void *__printk;
extern void *_printk;
extern void *___kmalloc_noprof;
extern void *_kmalloc;
extern void *_execmem_alloc;
extern void *_module_alloc;
extern void *_set_memory_rw;
extern void *_execmem_free;
extern void *_module_memfree;
extern void *__raw_read_lock;
extern void *__raw_read_unlock;
extern void *__raw_read_lock_irqsave;
extern void *__raw_read_unlock_irqrestore;
extern void *__raw_write_lock;
extern void *__raw_write_unlock;
extern void *__raw_write_lock_irqsave;
extern void *__raw_write_unlock_irqrestore;
extern void *_detach_pid;
extern void *_free_pids;
extern void *_tasklist_lock;

typedef void (* frida_say_t) (const char *, ...);
typedef void * (__attribute__((regparm(3))) * frida_take_t) (unsigned long, unsigned long);
typedef void * (__attribute__((regparm(3))) * frida_take_one_t) (unsigned long);
typedef void (__attribute__((regparm(3))) * frida_give_t) (void *);
typedef unsigned long (__attribute__((regparm(3))) * frida_widen_t) (unsigned long, unsigned long);
typedef void (__attribute__((regparm(3))) * frida_hold_t) (void *);
typedef unsigned long (__attribute__((regparm(3))) * frida_hold_flags_t) (void *);
typedef void (__attribute__((regparm(3))) * frida_release_flags_t) (void *, unsigned long);

void frida_k_log (const char * message)
{
  void *say = __printk ? __printk : _printk;

  ((frida_say_t) say) (message);
}

void * frida_k_alloc (unsigned long size, unsigned long flags)
{
  void *take = ___kmalloc_noprof ? ___kmalloc_noprof : _kmalloc;

  return ((frida_take_t) take) (size, flags);
}

void * frida_k_alloc_code (unsigned long kind, unsigned long size, unsigned long pages)
{
  void *code;

  if (_execmem_alloc)
    code = ((frida_take_t) _execmem_alloc) (kind, size);
  else
    code = ((frida_take_one_t) _module_alloc) (size);

  if (_set_memory_rw)
    ((frida_widen_t) _set_memory_rw) ((unsigned long) code, pages);

  return code;
}

void frida_k_free_code (void * code)
{
  void *give = _execmem_free ? _execmem_free : _module_memfree;

  ((frida_give_t) give) (code);
}

unsigned long frida_k_lock_tasklist (void)
{
  if (__raw_read_lock_irqsave && __raw_read_unlock_irqrestore)
    return ((frida_hold_flags_t) __raw_read_lock_irqsave) (_tasklist_lock);

  ((frida_hold_t) __raw_read_lock) (_tasklist_lock);

  return 0;
}

void frida_k_unlock_tasklist (unsigned long flags)
{
  if (__raw_read_lock_irqsave && __raw_read_unlock_irqrestore)
    ((frida_release_flags_t) __raw_read_unlock_irqrestore) (_tasklist_lock, flags);
  else
    ((frida_hold_t) __raw_read_unlock) (_tasklist_lock);
}

unsigned long frida_k_write_lock_tasklist (void)
{
  if (__raw_write_lock_irqsave && __raw_write_unlock_irqrestore)
    return ((frida_hold_flags_t) __raw_write_lock_irqsave) (_tasklist_lock);

  ((frida_hold_t) __raw_write_lock) (_tasklist_lock);

  return 0;
}

void frida_k_write_unlock_tasklist (unsigned long flags)
{
  if (__raw_write_lock_irqsave && __raw_write_unlock_irqrestore)
    ((frida_release_flags_t) __raw_write_unlock_irqrestore) (_tasklist_lock, flags);
  else
    ((frida_hold_t) __raw_write_unlock) (_tasklist_lock);
}

void frida_k_detach_pid (void * pids, void * task, int type)
{
  typedef void (__attribute__ ((regparm (3))) * fn_t) (void *, void *, int);

  ((fn_t) _detach_pid) (pids, task, type);
}

void frida_k_free_pids (void * pids)
{
  typedef void (__attribute__ ((regparm (3))) * fn_t) (void *);

  ((fn_t) _free_pids) (pids);
}

extern int frida_cb_thread (void * data);
extern int frida_cb_interrupt (int irq, void * cookie);
extern int frida_cb_user (void * argument);
extern void frida_cb_exec (void * data, void * task, int was, void * program);
extern void frida_cb_thread_appeared (void * data, void * parent, void * child);
extern void frida_cb_thread_left (void * data, void * task, _Bool group_dead);

__attribute__ ((regparm (3))) int
frida_kcb_thread (void * data)
{
  return frida_cb_thread (data);
}

__attribute__ ((regparm (3))) int
frida_kcb_interrupt (int irq, void * cookie)
{
  return frida_cb_interrupt (irq, cookie);
}

__attribute__ ((regparm (3))) int
frida_kcb_user (void * argument)
{
  return frida_cb_user (argument);
}

__attribute__ ((regparm (3))) void
frida_kcb_exec (void * data, void * task, int was, void * program)
{
  frida_cb_exec (data, task, was, program);
}

__attribute__ ((regparm (3))) void
frida_kcb_thread_appeared (void * data, void * parent, void * child)
{
  frida_cb_thread_appeared (data, parent, child);
}

__attribute__ ((regparm (3))) void
frida_kcb_thread_left (void * data, void * task, _Bool group_dead)
{
  frida_cb_thread_left (data, task, group_dead);
}

extern void *_request_threaded_irq;

int
frida_k_request_threaded_irq (unsigned int a0, void * a1, void * a2, unsigned long a3, const char * a4,
    void * a5)
{
  typedef int (__attribute__ ((regparm (3))) * fn_t) (unsigned int, void *, void *, unsigned long,
      const char *, void *);

  return ((fn_t) _request_threaded_irq) (a0, a1, a2, a3, a4, a5);
}

extern void *_copy_from_kernel_nofault;

long
frida_k_copy_from_kernel_nofault (void * a0, const void * a1, unsigned int a2)
{
  typedef long (__attribute__ ((regparm (3))) * fn_t) (void *, const void *, unsigned int);

  return ((fn_t) _copy_from_kernel_nofault) (a0, a1, a2);
}

extern void *_tracepoint_probe_register;
extern void *_tracepoint_probe_unregister;
extern void *_user_mode_thread;
extern void *_call_usermodehelper_setup;

int
frida_k_tracepoint_probe_register (void * a0, void * a1, void * a2)
{
  typedef int (__attribute__ ((regparm (3))) * fn_t) (void *, void *, void *);

  return ((fn_t) _tracepoint_probe_register) (a0, a1, a2);
}

int
frida_k_tracepoint_probe_unregister (void * a0, void * a1, void * a2)
{
  typedef int (__attribute__ ((regparm (3))) * fn_t) (void *, void *, void *);

  return ((fn_t) _tracepoint_probe_unregister) (a0, a1, a2);
}

void *
frida_k_user_mode_thread (void * a0, void * a1, unsigned long a2)
{
  typedef void * (__attribute__ ((regparm (3))) * fn_t) (void *, void *, unsigned long);

  return ((fn_t) _user_mode_thread) (a0, a1, a2);
}

void *
frida_k_call_usermodehelper_setup (const void * a0, const void * a1, const void * a2,
    unsigned int a3, void * a4, void * a5, void * a6)
{
  typedef void * (__attribute__ ((regparm (3))) * fn_t) (const void *, const void *, const void *,
      unsigned int, void *, void *, void *);

  return ((fn_t) _call_usermodehelper_setup) (a0, a1, a2, a3, a4, a5, a6);
}

extern int frida_cb_hold_spawn (void * info, void * credentials);
extern void frida_cb_release_words (void * info);

__attribute__ ((regparm (3))) int
frida_kcb_hold_spawn (void * info, void * credentials)
{
  return frida_cb_hold_spawn (info, credentials);
}

__attribute__ ((regparm (3))) void
frida_kcb_release_words (void * info)
{
  frida_cb_release_words (info);
}

extern void *_get_user_pages_unlocked;

long
frida_k_get_user_pages_unlocked (unsigned long a0, unsigned long a1, void * a2, int a3)
{
  typedef long (__attribute__ ((regparm (3))) * fn_t) (unsigned long, unsigned long, void *,
      unsigned int);

  return ((fn_t) _get_user_pages_unlocked) (a0, a1, a2, a3);
}

extern void *_text_poke_kgdb;

void * frida_k_text_poke (void * addr, const void * opcode, unsigned long len)
{
  typedef void * (__attribute__((regparm(3))) * fn_t) (void *, const void *, unsigned long);

  if (!_text_poke_kgdb)
    return 0;

  return ((fn_t) _text_poke_kgdb) (addr, opcode, len);
}

extern void *_register_module_notifier;

int frida_k_register_module_notifier (void * a0)
{
  typedef int (__attribute__ ((regparm (3))) * fn_t) (void *);

  return ((fn_t) _register_module_notifier) (a0);
}

extern void *_unregister_module_notifier;

int frida_k_unregister_module_notifier (void * a0)
{
  typedef int (__attribute__ ((regparm (3))) * fn_t) (void *);

  return ((fn_t) _unregister_module_notifier) (a0);
}

extern int frida_cb_module_state (void * nb, unsigned long action, void * data);

__attribute__ ((regparm (3))) int
frida_kcb_module_state (void * nb, unsigned long action, void * data)
{
  return frida_cb_module_state (nb, action, data);
}

extern void *_register_die_notifier;

int frida_k_register_die_notifier (void * a0)
{
  typedef int (__attribute__ ((regparm (3))) * fn_t) (void *);

  return ((fn_t) _register_die_notifier) (a0);
}

extern void *_unregister_die_notifier;

int frida_k_unregister_die_notifier (void * a0)
{
  typedef int (__attribute__ ((regparm (3))) * fn_t) (void *);

  return ((fn_t) _unregister_die_notifier) (a0);
}

extern int frida_cb_die (void * nb, unsigned long action, void * data);

__attribute__ ((regparm (3))) int
frida_kcb_die (void * nb, unsigned long action, void * data)
{
  return frida_cb_die (nb, action, data);
}
