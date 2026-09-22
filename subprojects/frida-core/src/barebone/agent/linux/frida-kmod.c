/*
 * Glue between the Rust agent and the Linux kernel it is loaded into.
 *
 * Everything the agent needs from the kernel lives here rather than in Rust,
 * because all of it is version- and config-dependent: struct layouts, inline helpers
 * that are really macros, the character-device and uaccess APIs, and which of
 * kallsyms' entry points happen to be exported this release. Compiling against the
 * target kernel's own headers is what keeps that knowledge correct.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fdtable.h>
#include <linux/fs_struct.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/kasan.h>
#include <linux/kfifo.h>
#include <linux/kprobes.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/nsproxy.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/seq_file.h>
#include <linux/set_memory.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/completion.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#if defined (__has_include)
# if __has_include (<linux/execmem.h>)
#  include <linux/execmem.h>
#  define FRIDA_HAVE_EXECMEM
# endif
#endif

#ifdef CONFIG_X86_64
# include <asm/ibt.h>
# include <asm/msr.h>
# include <asm/processor-flags.h>
# include <asm/segment.h>
#endif
#ifdef CONFIG_ARM64
# include <asm/cacheflush.h>
# include <asm/ptrace.h>
#endif

#ifdef CONFIG_X86_64
# define FRIDA_SYSCALL_NAME_PRCTL "__x64_sys_prctl"
# define FRIDA_SYSCALL_ARG0(regs) ((regs)->di)
# define FRIDA_SYSCALL_ARG1(regs) ((regs)->si)
# define FRIDA_SYSCALL_ARG2(regs) ((regs)->dx)
# define FRIDA_SYSCALL_ARG3(regs) ((regs)->r10)
# define FRIDA_SYSCALL_ARG4(regs) ((regs)->r8)
# if LINUX_VERSION_CODE >= KERNEL_VERSION (6, 16, 0)
#  define frida_write_fsbase(value) wrmsrq (MSR_FS_BASE, value)
# else
#  define frida_write_fsbase(value) wrmsrl (MSR_FS_BASE, value)
# endif
#endif
#ifdef CONFIG_ARM64
# define FRIDA_SYSCALL_NAME_PRCTL "__arm64_sys_prctl"
# define FRIDA_SYSCALL_ARG0(regs) ((regs)->regs[0])
# define FRIDA_SYSCALL_ARG1(regs) ((regs)->regs[1])
# define FRIDA_SYSCALL_ARG2(regs) ((regs)->regs[2])
# define FRIDA_SYSCALL_ARG3(regs) ((regs)->regs[3])
# define FRIDA_SYSCALL_ARG4(regs) ((regs)->regs[4])
#endif

#ifdef FRIDA_HAVE_FS_STRUCT_LOCK
# define frida_fs_lock(fs) spin_lock (&(fs)->lock)
# define frida_fs_unlock(fs) spin_unlock (&(fs)->lock)
#else
# define frida_fs_lock(fs) read_seqlock_excl (&(fs)->seq)
# define frida_fs_unlock(fs) read_sequnlock_excl (&(fs)->seq)
#endif

/* Mirrors GumPageProtection. */
#define FRIDA_PAGE_WRITE   (1 << 1)
#define FRIDA_PAGE_EXECUTE (1 << 2)

#define FRIDA_CONTROL_MAGIC 0x46524944
#define FRIDA_CONTROL_TOKEN 0x1d5f9e6b2c7a4038ULL

#define FRIDA_LINK_CAPACITY (1024 * 1024)

#define FRIDA_WAIT_FOREVER_SLICE_US (1000 * 1000)

typedef int (* FridaFoundSymbolFunc) (const char * name, u64 address, void * user_data);
typedef int (* FridaFoundModuleFunc) (const char * name, const char * version, u64 base, u64 size,
    void * user_data);
typedef void (* FridaModuleEventFunc) (int loaded, const char * name, const char * version, u64 base,
    u64 size, void * user_data);
typedef int (* FridaFoundModuleSymbolFunc) (const char * name, u64 address, u64 size, u8 kind,
    int is_global, void * user_data);
typedef int (* FridaFoundModuleExportFunc) (const char * name, u64 address, void * user_data);
typedef void (* FridaThreadEntry) (void * parameter, int wait_result);

typedef struct task_struct * (* FridaFindTaskByVpidFunc) (pid_t nr);
typedef struct mm_struct * (* FridaGetTaskMmFunc) (struct task_struct * task);
typedef void (* FridaMmputFunc) (struct mm_struct * mm);
typedef void (* FridaKthreadUseMmFunc) (struct mm_struct * mm);
typedef void (* FridaKthreadUnuseMmFunc) (struct mm_struct * mm);
typedef unsigned long (* FridaVmMmapFunc) (struct file * file, unsigned long addr, unsigned long len,
    unsigned long prot, unsigned long flag, unsigned long offset);
typedef int (* FridaVmMunmapFunc) (unsigned long start, size_t len);
typedef pid_t (* FridaUserModeThreadFunc) (int (* fn) (void *), void * arg, unsigned long flags);
typedef void (* FridaDetachPidFunc) (struct pid ** pids, struct task_struct * task, enum pid_type type);
typedef void (* FridaFreePidsFunc) (struct pid ** pids);
typedef void (* FridaTaskJoinGroupStopFunc) (struct task_struct * task);
typedef void (* FridaCleanupSighandFunc) (struct sighand_struct * sighand);
typedef void (* FridaKmemCacheFreeFunc) (struct kmem_cache * cache, void * object);
typedef void (* FridaPutFilesStructFunc) (struct files_struct * files);
typedef void (* FridaSwitchTaskNamespacesFunc) (struct task_struct * tsk, struct nsproxy * new);
typedef void (* FridaFreeFsStructFunc) (struct fs_struct * fs);
#ifdef FRIDA_HAVE_EXECMEM
typedef void * (* FridaExecmemAllocRwFunc) (enum execmem_type type, size_t size);
typedef void (* FridaExecmemFreeFunc) (void * ptr);
#endif

typedef struct _GumInterceptor GumInterceptor;

enum frida_cloak_op
{
  FRIDA_CLOAK_OP_ADD_THREAD = 1,
  FRIDA_CLOAK_OP_REMOVE_THREAD,
  FRIDA_CLOAK_OP_ADD_RANGE,
  FRIDA_CLOAK_OP_REMOVE_RANGE,
  FRIDA_CLOAK_OP_ADD_FD,
  FRIDA_CLOAK_OP_REMOVE_FD,
};

enum frida_control_op
{
  FRIDA_CONTROL_OP_PING = 32,
};

enum frida_process_op
{
  FRIDA_PROCESS_OP_ALLOC = 64,
  FRIDA_PROCESS_OP_FREE,
  FRIDA_PROCESS_OP_WRITE,
  FRIDA_PROCESS_OP_READ,
  FRIDA_PROCESS_OP_SPAWN,
  FRIDA_PROCESS_OP_CLOAK_THREAD,
  FRIDA_PROCESS_OP_CLOAK_RANGE,
};

struct frida_thread_ctx
{
  FridaThreadEntry entry;
  void * parameter;
};

struct frida_writable_alias
{
  struct list_head node;
  void * mapping;
  unsigned long first_page;
  unsigned int n_pages;
};

struct frida_symbol_visit_ctx
{
  FridaFoundSymbolFunc func;
  void * user_data;
};

struct frida_spawn_ctx
{
  u64 entry;
  u64 stack;
  u64 arg;
  u64 tls;
  struct task_struct * leader;
};

struct frida_sem_undo_list
{
  refcount_t refcnt;
};

struct frida_cloak_range
{
  unsigned long start;
  unsigned long end;
};

struct frida_fd_data
{
  fmode_t mode;
  unsigned int fd;
};

struct frida_cloak
{
  struct list_head node;
  pid_t tgid;
  struct mm_struct * mm;

  pid_t * threads;
  unsigned int n_threads;
  struct frida_cloak_range * ranges;
  unsigned int n_ranges;
  int * fds;
  unsigned int n_fds;
};

struct frida_alloc_args
{
  u64 size;
  u32 pid;
  u32 prot;
};

struct frida_free_args
{
  u64 address;
  u64 size;
  u32 pid;
};

struct frida_rw_args
{
  u64 address;
  u64 buffer;
  u64 size;
  u32 pid;
};

struct frida_spawn_args
{
  u64 entry;
  u64 stack;
  u64 arg;
  u64 tls;
  u32 pid;
};

struct frida_cloak_thread_args
{
  u32 tgid;
  u32 tid;
};

struct frida_cloak_range_args
{
  u64 base;
  u64 size;
  u32 tgid;
};

struct frida_xfer_job
{
  u32 pid;
  u64 address;
  void * buffer;
  u64 size;
};

struct frida_kthread_call
{
  long (* fn) (void * arg);
  void * arg;
  long result;
  struct completion done;
};

extern GumInterceptor * gum_interceptor_obtain (void);
extern void gum_interceptor_begin_transaction (GumInterceptor * self);
extern void gum_interceptor_end_transaction (GumInterceptor * self);
extern int gum_interceptor_replace (GumInterceptor * self, void * function_address,
    void * replacement_function, void * original_function, const void * options);
extern void gum_interceptor_revert (GumInterceptor * self, void * function_address);

int frida_agent_start (void);
void frida_agent_stop (void);

u64 frida_kmod_find_symbol (const char * name);
u64 frida_kmod_find_function (const char * name);
void * frida_kmod_remap_writable (u64 first_page, unsigned int n_pages);
void frida_kmod_unmap_writable (void * mapping);
unsigned int frida_kmod_wait_prepare (void);
void frida_kmod_wait_commit (unsigned int seq, s64 timeout_us);
void frida_kmod_wake (void);
void frida_kmod_yield (void);
void frida_kmod_install_hooks (void);

static void frida_resolve_kallsyms (void);
static void frida_resolve_kernel_range (void);
static void frida_resolve_process_ops (void);
static void frida_resolve_code_allocator (void);
static struct mm_struct * frida_grab_process_mm (int pid);
static struct task_struct * frida_grab_process_leader (int pid);
static int frida_spawn_trampoline (void * data);
static void frida_reparent_into_group (struct task_struct * leader);
static void frida_adopt_target_context (struct task_struct * leader);
static void frida_remove_cloak_hooks (void);
static int frida_show_map (struct seq_file * m, void * v);
static int frida_show_smap (struct seq_file * m, void * v);
static bool frida_proc_fill_cache (struct file * file, struct dir_context * ctx,
    const char * name, unsigned int len, void * instantiate, struct task_struct * task,
    const void * ptr);
static long frida_sys_prctl (const struct pt_regs * regs);
static int frida_proc_pid_status (struct seq_file * m, struct pid_namespace * ns,
    struct pid * pid, struct task_struct * task);
static int frida_do_task_stat (struct seq_file * m, struct pid_namespace * ns,
    struct pid * pid, struct task_struct * task, int whole);
static void frida_release_task (struct task_struct * p);
static int frida_cloak_count_threads (pid_t tgid);
static void frida_seq_replace_number (struct seq_file * m, size_t num_off, size_t num_len,
    int new_val);
static struct frida_cloak * frida_cloak_get (pid_t tgid);
static bool frida_reader_is_cloaked (void);
static bool frida_cloak_has_range (struct mm_struct * mm, unsigned long start, unsigned long end);
static bool frida_cloak_has_thread (pid_t tid);
static bool frida_cloak_has_fd (pid_t tgid, int fd);
static void frida_cloak_forget (pid_t tgid);
static struct frida_cloak * frida_cloak_find (pid_t tgid);
static void * frida_resolve_unexported (const char * name);
static void * frida_rewind_to_function_entry (void * address);
static void * frida_unseal_landing_pad (void * address);
static int frida_thread_trampoline (void * data);
static void frida_kmod_module_range (struct module * mod, u64 * base, u64 * size);
static struct module * frida_kmod_module_at (u64 base);
static const char * frida_kmod_symbol_name (const struct kernel_symbol * sym);
static u64 frida_kmod_symbol_value (const struct kernel_symbol * sym);
static int frida_kmod_on_module_state (struct notifier_block * nb, unsigned long action,
    void * data);
static int frida_dev_open (struct inode * inode, struct file * file);
static int frida_dev_release (struct inode * inode, struct file * file);
static ssize_t frida_dev_read (struct file * file, char __user * buffer, size_t size,
    loff_t * offset);
static ssize_t frida_dev_write (struct file * file, const char __user * buffer, size_t size,
    loff_t * offset);
static __poll_t frida_dev_poll (struct file * file, struct poll_table_struct * wait);
static struct page * frida_page_for_virtual (unsigned long address);
static bool frida_is_vmalloc_or_module_addr (const void * x);
static void frida_flush_icache_range (unsigned long start, unsigned long size);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (6, 4, 0)
static int frida_on_each_symbol (void * data, const char * name, unsigned long address);
#else
static int frida_on_each_symbol (void * data, const char * name, struct module * mod,
    unsigned long address);
#endif
static bool frida_can_sleep (void);
static void frida_spin_until_woken (unsigned int seq, s64 timeout_us);
static bool frida_should_wake (unsigned int seq);
static void frida_strip_module_suffix (char * rendered);

static long frida_prctl_alloc (const struct frida_alloc_args __user * uargs);
static long frida_prctl_free (const struct frida_free_args __user * uargs);
static long frida_prctl_write (const struct frida_rw_args __user * uargs);
static long frida_prctl_read (const struct frida_rw_args __user * uargs);
static long frida_prctl_spawn (const struct frida_spawn_args __user * uargs);
static long frida_prctl_cloak_thread (const struct frida_cloak_thread_args __user * uargs);
static long frida_prctl_cloak_range (const struct frida_cloak_range_args __user * uargs);
static long frida_call_on_kthread (long (* fn) (void * arg), void * arg);
static int frida_op_worker_fn (void * unused);
static long frida_do_alloc (void * data);
static long frida_do_free (void * data);
static long frida_do_write (void * data);
static long frida_do_read (void * data);
static long frida_do_spawn (void * data);

static DECLARE_WAIT_QUEUE_HEAD (frida_wq);
static atomic_t frida_wake_seq = ATOMIC_INIT (0);

static u64 frida_kernel_base;
static u64 frida_kernel_size;

static typeof (&kallsyms_lookup_name) frida_kallsyms_lookup_name_impl;
static typeof (&kallsyms_on_each_symbol) frida_kallsyms_on_each_symbol_impl;
static struct list_head * frida_modules;
static FridaModuleEventFunc frida_kmod_module_event_func;
static void * frida_kmod_module_event_data;
static struct notifier_block frida_kmod_module_notifier = {
  .notifier_call = frida_kmod_on_module_state,
};
static struct mutex * frida_module_mutex;
static typeof (&set_memory_ro) frida_set_memory_ro_impl;
static typeof (&set_memory_rw) frida_set_memory_rw_impl;
static typeof (&set_memory_x) frida_set_memory_x_impl;
static typeof (&set_memory_nx) frida_set_memory_nx_impl;

static FridaFindTaskByVpidFunc frida_find_task_by_vpid_impl;
static FridaGetTaskMmFunc frida_get_task_mm_impl;
static FridaMmputFunc frida_mmput_impl;
static FridaKthreadUseMmFunc frida_kthread_use_mm_impl;
static FridaKthreadUnuseMmFunc frida_kthread_unuse_mm_impl;
static FridaVmMmapFunc frida_vm_mmap_impl;
static FridaVmMunmapFunc frida_vm_munmap_impl;
static FridaUserModeThreadFunc frida_user_mode_thread_impl;
static FridaDetachPidFunc frida_detach_pid_impl;
static FridaFreePidsFunc frida_free_pids_impl;
static FridaTaskJoinGroupStopFunc frida_task_join_group_stop_impl;
static FridaCleanupSighandFunc frida_cleanup_sighand_impl;
static FridaKmemCacheFreeFunc frida_kmem_cache_free_impl;
static FridaPutFilesStructFunc frida_put_files_struct_impl;
static FridaSwitchTaskNamespacesFunc frida_switch_task_namespaces_impl;
static FridaFreeFsStructFunc frida_free_fs_struct_impl;
#ifdef FRIDA_HAVE_EXECMEM
static FridaExecmemAllocRwFunc frida_execmem_alloc_rw_impl;
static FridaExecmemFreeFunc frida_execmem_free_impl;
#endif
static struct kmem_cache ** frida_signal_cachep;
static rwlock_t * frida_tasklist_lock;

static struct task_struct * frida_op_worker;
static DECLARE_WAIT_QUEUE_HEAD (frida_op_wq);
static struct frida_kthread_call * frida_op_pending;
static DEFINE_MUTEX (frida_op_mutex);

static LIST_HEAD (frida_cloaks);
static DEFINE_SPINLOCK (frida_cloaks_lock);

static GumInterceptor * frida_interceptor;
static void * frida_show_map_addr;
static void * frida_show_smap_addr;
static void * frida_proc_fill_cache_addr;
static void * frida_sys_prctl_addr;
static void * frida_proc_pid_status_addr;
static void * frida_do_task_stat_addr;
static void * frida_release_task_addr;
static void * frida_proc_task_instantiate_addr;
static void * frida_proc_fd_instantiate_addr;
static void * frida_proc_fdinfo_instantiate_addr;
static int (* frida_show_map_orig) (struct seq_file * m, void * v);
static int (* frida_show_smap_orig) (struct seq_file * m, void * v);
static bool (* frida_proc_fill_cache_orig) (struct file * file, struct dir_context * ctx,
    const char * name, unsigned int len, void * instantiate, struct task_struct * task,
    const void * ptr);
static long (* frida_sys_prctl_orig) (const struct pt_regs * regs);
static int (* frida_proc_pid_status_orig) (struct seq_file * m, struct pid_namespace * ns,
    struct pid * pid, struct task_struct * task);
static int (* frida_do_task_stat_orig) (struct seq_file * m, struct pid_namespace * ns,
    struct pid * pid, struct task_struct * task, int whole);
static void (* frida_release_task_orig) (struct task_struct * p);

static LIST_HEAD (frida_writable_aliases);
static DEFINE_MUTEX (frida_writable_aliases_mutex);

static DECLARE_KFIFO_PTR (frida_to_client, u8);
static DECLARE_KFIFO_PTR (frida_from_client, u8);
static DEFINE_MUTEX (frida_link_mutex);
static DECLARE_WAIT_QUEUE_HEAD (frida_readable_wq);
static atomic_t frida_client_count = ATOMIC_INIT (0);
static bool frida_link_registered;

static const struct file_operations frida_dev_fops =
{
  .owner = THIS_MODULE,
  .open = frida_dev_open,
  .release = frida_dev_release,
  .read = frida_dev_read,
  .write = frida_dev_write,
  .poll = frida_dev_poll,
};

static struct miscdevice frida_dev =
{
  .minor = MISC_DYNAMIC_MINOR,
  .name = "frida",
  .fops = &frida_dev_fops,
  .mode = 0600,
};

static int __init
frida_kmod_init (void)
{
  frida_resolve_kallsyms ();
  frida_resolve_kernel_range ();
  frida_resolve_process_ops ();
  frida_resolve_code_allocator ();

  return frida_agent_start ();
}

static void __exit
frida_kmod_exit (void)
{
  frida_remove_cloak_hooks ();
  frida_agent_stop ();
}

module_init (frida_kmod_init);
module_exit (frida_kmod_exit);

MODULE_LICENSE ("GPL");
MODULE_DESCRIPTION ("Frida instrumentation agent");
MODULE_VERSION ("1.0.0");

static void
frida_resolve_kallsyms (void)
{
  frida_kallsyms_lookup_name_impl = frida_resolve_unexported ("kallsyms_lookup_name");
  frida_kallsyms_on_each_symbol_impl = frida_resolve_unexported ("kallsyms_on_each_symbol");

  if (frida_kallsyms_lookup_name_impl == NULL)
    {
      printk (KERN_WARNING "frida: kallsyms_lookup_name unavailable\n");
      return;
    }

  frida_modules = (struct list_head *) frida_kallsyms_lookup_name_impl ("modules");
  frida_module_mutex = (struct mutex *) frida_kallsyms_lookup_name_impl ("module_mutex");

  /*
   * GKI trims its export table to the KMI symbol list, which leaves set_memory_*()
   * callable but unexported. Being absent from Module.symvers only stops the module
   * loader from resolving them, so go through kallsyms like everything else here.
   */
  frida_set_memory_ro_impl = (void *) frida_kmod_find_function ("set_memory_ro");
  frida_set_memory_rw_impl = (void *) frida_kmod_find_function ("set_memory_rw");
  frida_set_memory_x_impl = (void *) frida_kmod_find_function ("set_memory_x");
  frida_set_memory_nx_impl = (void *) frida_kmod_find_function ("set_memory_nx");
}

static void
frida_resolve_kernel_range (void)
{
  u64 start, end;

  start = frida_kmod_find_symbol ("_stext");
  end = frida_kmod_find_symbol ("_end");
  if (start == 0 || end == 0)
    return;

  frida_kernel_base = start;
  frida_kernel_size = end - start;
}

static void
frida_resolve_process_ops (void)
{
  frida_find_task_by_vpid_impl = (FridaFindTaskByVpidFunc) frida_kmod_find_function ("find_task_by_vpid");
  frida_get_task_mm_impl = (FridaGetTaskMmFunc) frida_kmod_find_function ("get_task_mm");
  frida_mmput_impl = (FridaMmputFunc) frida_kmod_find_function ("mmput");
  frida_kthread_use_mm_impl = (FridaKthreadUseMmFunc) frida_kmod_find_function ("kthread_use_mm");
  frida_kthread_unuse_mm_impl = (FridaKthreadUnuseMmFunc) frida_kmod_find_function ("kthread_unuse_mm");
  frida_vm_mmap_impl = (FridaVmMmapFunc) frida_kmod_find_function ("vm_mmap");
  frida_vm_munmap_impl = (FridaVmMunmapFunc) frida_kmod_find_function ("vm_munmap");
  frida_user_mode_thread_impl = (FridaUserModeThreadFunc) frida_kmod_find_function ("user_mode_thread");
  frida_detach_pid_impl = (FridaDetachPidFunc) frida_kmod_find_function ("detach_pid");
  frida_free_pids_impl = (FridaFreePidsFunc) frida_kmod_find_function ("free_pids");
  frida_task_join_group_stop_impl = (FridaTaskJoinGroupStopFunc) frida_kmod_find_function ("task_join_group_stop");
  frida_cleanup_sighand_impl = (FridaCleanupSighandFunc) frida_kmod_find_function ("__cleanup_sighand");
  frida_kmem_cache_free_impl = (FridaKmemCacheFreeFunc) frida_kmod_find_function ("kmem_cache_free");
  frida_put_files_struct_impl = (FridaPutFilesStructFunc) frida_kmod_find_function ("put_files_struct");
  frida_switch_task_namespaces_impl = (FridaSwitchTaskNamespacesFunc) frida_kmod_find_function ("switch_task_namespaces");
  frida_free_fs_struct_impl = (FridaFreeFsStructFunc) frida_kmod_find_function ("free_fs_struct");
  frida_signal_cachep = (struct kmem_cache **) frida_kmod_find_symbol ("signal_cachep");
  frida_tasklist_lock = (rwlock_t *) frida_kmod_find_symbol ("tasklist_lock");
}

static void
frida_resolve_code_allocator (void)
{
#ifdef FRIDA_HAVE_EXECMEM
  FridaExecmemAllocRwFunc alloc_rw;
  FridaExecmemFreeFunc free;

  alloc_rw = (FridaExecmemAllocRwFunc) frida_kmod_find_function ("execmem_alloc_rw");
  free = (FridaExecmemFreeFunc) frida_kmod_find_function ("execmem_free");
  if (alloc_rw == NULL || free == NULL)
    return;

  frida_execmem_alloc_rw_impl = alloc_rw;
  frida_execmem_free_impl = free;
#endif
}

/*
 * kallsyms_lookup_name() and kallsyms_on_each_symbol() stopped being exported in
 * 5.7. A kprobe registered by symbol name resolves an address without needing an
 * export, which is the standard way back in.
 */
static void *
frida_resolve_unexported (const char * name)
{
  struct kprobe kp = { .symbol_name = name };
  void * address;

  if (register_kprobe (&kp) < 0)
    return NULL;
  address = kp.addr;
  unregister_kprobe (&kp);

  return frida_unseal_landing_pad (frida_rewind_to_function_entry (address));
}

static void *
frida_rewind_to_function_entry (void * address)
{
#ifdef HAS_KERNEL_IBT
  u32 * pad = (u32 *) ((u8 *) address - ENDBR_INSN_SIZE);

  if (__is_endbr (*pad))
    return pad;
#endif

  return address;
}

static void *
frida_unseal_landing_pad (void * address)
{
#ifdef HAS_KERNEL_IBT
  u32 * pad = address;
  unsigned long first_page;
  unsigned int n_pages;
  void * alias;

  if (*pad != gen_endbr_poison ())
    return address;

  first_page = (unsigned long) address & PAGE_MASK;
  n_pages = ((((unsigned long) address + ENDBR_INSN_SIZE - 1) & PAGE_MASK)
      - first_page) / PAGE_SIZE + 1;

  alias = frida_kmod_remap_writable (first_page, n_pages);
  if (alias == NULL)
    return address;

  *(u32 *) ((u8 *) alias + ((unsigned long) address - first_page)) = gen_endbr ();

  frida_kmod_unmap_writable (alias);
#endif

  return address;
}

void
frida_kmod_log (const char * message)
{
  printk (KERN_INFO "%s", message);
}

void
frida_kmod_panic (const char * message)
{
  panic ("%s", message);
}

int
frida_kmod_spawn_thread (FridaThreadEntry entry,
                         void * parameter)
{
  struct frida_thread_ctx * ctx;
  struct task_struct * task;

  ctx = kmalloc (sizeof (struct frida_thread_ctx), GFP_KERNEL);
  ctx->entry = entry;
  ctx->parameter = parameter;

  task = kthread_run (frida_thread_trampoline, ctx, "frida-agent");
  if (IS_ERR (task))
    {
      kfree (ctx);
      return PTR_ERR (task);
    }

  return 0;
}

static int __nocfi
frida_thread_trampoline (void * data)
{
  struct frida_thread_ctx ctx = *((struct frida_thread_ctx *) data);

  kfree (data);

  ctx.entry (ctx.parameter, 0);

  return 0;
}

u64
frida_kmod_process_alloc (int pid,
                          u64 size,
                          int prot)
{
  u64 address;
  struct mm_struct * mm;

  mm = frida_grab_process_mm (pid);
  if (mm == NULL)
    return 0;

  frida_kthread_use_mm_impl (mm);
  address = frida_vm_mmap_impl (NULL, 0, size, prot, MAP_ANONYMOUS | MAP_PRIVATE, 0);
  frida_kthread_unuse_mm_impl (mm);

  frida_mmput_impl (mm);

  return IS_ERR_VALUE (address) ? 0 : address;
}

int
frida_kmod_process_free (int pid,
                         u64 address,
                         u64 size)
{
  int result;
  struct mm_struct * mm;

  mm = frida_grab_process_mm (pid);
  if (mm == NULL)
    return -ESRCH;

  frida_kthread_use_mm_impl (mm);
  result = frida_vm_munmap_impl (address, size);
  frida_kthread_unuse_mm_impl (mm);

  frida_mmput_impl (mm);

  return result;
}

u64
frida_kmod_process_write (int pid,
                          u64 address,
                          const void * data,
                          u64 size)
{
  u64 written;
  struct mm_struct * mm;

  mm = frida_grab_process_mm (pid);
  if (mm == NULL)
    return 0;

  frida_kthread_use_mm_impl (mm);
  written = size - copy_to_user ((void __user *) (uintptr_t) address, data, size);
  frida_kthread_unuse_mm_impl (mm);

  frida_mmput_impl (mm);

  return written;
}

u64
frida_kmod_process_read (int pid,
                         u64 address,
                         void * data,
                         u64 size)
{
  u64 read;
  struct mm_struct * mm;

  mm = frida_grab_process_mm (pid);
  if (mm == NULL)
    return 0;

  frida_kthread_use_mm_impl (mm);
  read = size - copy_from_user (data, (const void __user *) (uintptr_t) address, size);
  frida_kthread_unuse_mm_impl (mm);

  frida_mmput_impl (mm);

  return read;
}

int
frida_kmod_process_spawn_thread (int pid,
                                 u64 entry,
                                 u64 stack,
                                 u64 arg,
                                 u64 tls)
{
  int tid;
  struct mm_struct * mm;
  struct task_struct * leader;
  struct frida_spawn_ctx * ctx;

  mm = frida_grab_process_mm (pid);
  if (mm == NULL)
    return -ESRCH;

  leader = frida_grab_process_leader (pid);
  if (leader == NULL)
  {
    frida_mmput_impl (mm);
    return -ESRCH;
  }

  ctx = kmalloc (sizeof (struct frida_spawn_ctx), GFP_KERNEL);
  ctx->entry = entry;
  ctx->stack = stack;
  ctx->arg = arg;
  ctx->tls = tls;
  ctx->leader = leader;

  frida_kthread_use_mm_impl (mm);
  tid = frida_user_mode_thread_impl (frida_spawn_trampoline, ctx, 0);
  frida_kthread_unuse_mm_impl (mm);

  frida_mmput_impl (mm);

  return tid;
}

int
frida_kmod_cloak_add_thread (int tgid, int tid)
{
  int result;
  struct frida_cloak * c;
  unsigned int i;
  pid_t * slot;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_get (tgid);
  if (c == NULL)
  {
    result = -ENOMEM;
    goto beach;
  }

  result = 0;

  for (i = 0; i != c->n_threads; i++)
  {
    if (c->threads[i] == tid)
      goto beach;
  }

  slot = krealloc (c->threads, (c->n_threads + 1) * sizeof (pid_t), GFP_ATOMIC);
  if (slot == NULL)
  {
    result = -ENOMEM;
    goto beach;
  }
  c->threads = slot;
  c->threads[c->n_threads++] = tid;

beach:
  spin_unlock (&frida_cloaks_lock);

  return result;
}

int
frida_kmod_cloak_remove_thread (int tgid, int tid)
{
  struct frida_cloak * c;
  unsigned int i;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_find (tgid);
  if (c != NULL)
  {
    for (i = 0; i != c->n_threads; i++)
    {
      if (c->threads[i] == tid)
      {
        c->threads[i] = c->threads[--c->n_threads];
        break;
      }
    }
  }

  spin_unlock (&frida_cloaks_lock);

  return 0;
}

int
frida_kmod_cloak_add_range (int tgid, u64 base, u64 size)
{
  int result;
  struct frida_cloak * c;
  struct frida_cloak_range * slot;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_get (tgid);
  if (c == NULL)
  {
    result = -ENOMEM;
    goto beach;
  }

  slot = krealloc (c->ranges, (c->n_ranges + 1) * sizeof (struct frida_cloak_range), GFP_ATOMIC);
  if (slot == NULL)
  {
    result = -ENOMEM;
    goto beach;
  }
  c->ranges = slot;
  c->ranges[c->n_ranges].start = base;
  c->ranges[c->n_ranges].end = base + size;
  c->n_ranges++;

  result = 0;

beach:
  spin_unlock (&frida_cloaks_lock);

  return result;
}

int
frida_kmod_cloak_remove_range (int tgid, u64 base, u64 size)
{
  struct frida_cloak * c;
  unsigned int i;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_find (tgid);
  if (c != NULL)
  {
    for (i = 0; i != c->n_ranges; i++)
    {
      if (c->ranges[i].start == base && c->ranges[i].end == base + size)
      {
        c->ranges[i] = c->ranges[--c->n_ranges];
        break;
      }
    }
  }

  spin_unlock (&frida_cloaks_lock);

  return 0;
}

int
frida_kmod_cloak_add_fd (int tgid, int fd)
{
  int result;
  struct frida_cloak * c;
  unsigned int i;
  int * slot;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_get (tgid);
  if (c == NULL)
  {
    result = -ENOMEM;
    goto beach;
  }

  result = 0;

  for (i = 0; i != c->n_fds; i++)
  {
    if (c->fds[i] == fd)
      goto beach;
  }

  slot = krealloc (c->fds, (c->n_fds + 1) * sizeof (int), GFP_ATOMIC);
  if (slot == NULL)
  {
    result = -ENOMEM;
    goto beach;
  }
  c->fds = slot;
  c->fds[c->n_fds++] = fd;

beach:
  spin_unlock (&frida_cloaks_lock);

  return result;
}

int
frida_kmod_cloak_remove_fd (int tgid, int fd)
{
  struct frida_cloak * c;
  unsigned int i;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_find (tgid);
  if (c != NULL)
  {
    for (i = 0; i != c->n_fds; i++)
    {
      if (c->fds[i] == fd)
      {
        c->fds[i] = c->fds[--c->n_fds];
        break;
      }
    }
  }

  spin_unlock (&frida_cloaks_lock);

  return 0;
}

static struct mm_struct *
frida_grab_process_mm (int pid)
{
  struct mm_struct * mm;
  struct task_struct * task;

  rcu_read_lock ();
  task = frida_find_task_by_vpid_impl (pid);
  mm = (task != NULL) ? frida_get_task_mm_impl (task) : NULL;
  rcu_read_unlock ();

  return mm;
}

static struct task_struct *
frida_grab_process_leader (int pid)
{
  struct task_struct * leader;
  struct task_struct * task;

  rcu_read_lock ();
  task = frida_find_task_by_vpid_impl (pid);
  leader = (task != NULL) ? task->group_leader : NULL;
  if (leader != NULL)
    get_task_struct (leader);
  rcu_read_unlock ();

  return leader;
}

static int
frida_spawn_trampoline (void * data)
{
  struct frida_spawn_ctx ctx = *(struct frida_spawn_ctx *) data;
  struct pt_regs * regs;

  regs = task_pt_regs (current);

  kfree (data);

  frida_reparent_into_group (ctx.leader);
  put_task_struct (ctx.leader);

  memset (regs, 0, sizeof (struct pt_regs));
#ifdef CONFIG_X86_64
  regs->ip = ctx.entry;
  regs->sp = ctx.stack;
  regs->di = ctx.arg;
  regs->cs = __USER_CS;
  regs->ss = __USER_DS;
  regs->flags = X86_EFLAGS_IF;
  regs->orig_ax = -1;

  current->thread.fsbase = ctx.tls;
  frida_write_fsbase (ctx.tls);
#endif
#ifdef CONFIG_ARM64
  regs->pc = ctx.entry;
  regs->sp = ctx.stack;
  regs->regs[0] = ctx.arg;
  regs->pstate = PSR_MODE_EL0t;

  current->thread.uw.tp_value = ctx.tls;
  write_sysreg (ctx.tls, tpidr_el0);
#endif

  return 0;
}

static void
frida_reparent_into_group (struct task_struct * leader)
{
  struct task_struct * child = current;
  struct signal_struct * old_signal = child->signal;
  struct sighand_struct * old_sighand = child->sighand;
  struct pid * freed[PIDTYPE_MAX] = { NULL };

  write_lock_irq (frida_tasklist_lock);

  frida_detach_pid_impl (freed, child, PIDTYPE_SID);
  frida_detach_pid_impl (freed, child, PIDTYPE_PGID);
  frida_detach_pid_impl (freed, child, PIDTYPE_TGID);

  list_del_rcu (&child->tasks);
  list_del_init (&child->sibling);
  list_del_rcu (&child->thread_node);

  child->real_parent = leader->real_parent;
  child->parent = leader->real_parent;
  child->group_leader = leader;
  child->tgid = leader->tgid;
  child->exit_signal = -1;
  child->signal = leader->signal;
  child->sighand = leader->sighand;

  refcount_inc (&leader->sighand->count);

  spin_lock (&leader->sighand->siglock);

  leader->signal->nr_threads++;
  leader->signal->quick_threads++;
  atomic_inc (&leader->signal->live);
  refcount_inc (&leader->signal->sigcnt);

  frida_task_join_group_stop_impl (child);

#ifdef FRIDA_HAVE_TASK_THREAD_GROUP
  list_add_tail_rcu (&child->thread_group, &leader->thread_group);
#endif
  list_add_tail_rcu (&child->thread_node, &leader->signal->thread_head);

  spin_unlock (&leader->sighand->siglock);

  write_unlock_irq (frida_tasklist_lock);

  frida_free_pids_impl (freed);

  frida_cleanup_sighand_impl (old_sighand);
  if (refcount_dec_and_test (&old_signal->sigcnt))
    frida_kmem_cache_free_impl (*frida_signal_cachep, old_signal);

  frida_adopt_target_context (leader);
}

/*
 * user_mode_thread hands the new thread the kernel worker's fd table and credentials. The loader
 * dlopen()s the agent through /proc/<tgid>/fd/<n>, and it must reach the target's fds and pass the
 * target's SELinux checks (the credentials carry the security context), so adopt them from the
 * leader — along with its fs, namespaces and SysV semaphore undo list, as a real CLONE_THREAD
 * sibling would share them.
 */
static void
frida_adopt_target_context (struct task_struct * leader)
{
  struct files_struct * old_files = current->files;
  struct files_struct * files = leader->files;
  struct fs_struct * old_fs = current->fs;
  struct fs_struct * fs = leader->fs;
  const struct cred * old_cred = current->cred;
  const struct cred * old_real_cred = current->real_cred;
  void * undo_list = leader->sysvsem.undo_list;
  int fs_dead;

  atomic_inc (&files->count);
  task_lock (current);
  current->files = files;
  task_unlock (current);
  frida_put_files_struct_impl (old_files);

  frida_fs_lock (fs);
  fs->users++;
  frida_fs_unlock (fs);
  task_lock (current);
  current->fs = fs;
  task_unlock (current);
  frida_fs_lock (old_fs);
  fs_dead = --old_fs->users == 0;
  frida_fs_unlock (old_fs);
  if (fs_dead)
    frida_free_fs_struct_impl (old_fs);

  current->real_cred = get_cred (leader->real_cred);
  rcu_assign_pointer (current->cred, get_cred (leader->cred));
  put_cred (old_cred);
  put_cred (old_real_cred);

  get_nsproxy (leader->nsproxy);
  frida_switch_task_namespaces_impl (current, leader->nsproxy);

  if (undo_list != NULL)
  {
    refcount_inc (&((struct frida_sem_undo_list *) undo_list)->refcnt);
    current->sysvsem.undo_list = undo_list;
  }
}

void
frida_kmod_install_hooks (void)
{
  frida_op_worker = kthread_run (frida_op_worker_fn, NULL, "frida-op");

  frida_interceptor = gum_interceptor_obtain ();

  frida_show_map_addr = (void *) frida_kmod_find_symbol ("show_map");
  frida_show_smap_addr = (void *) frida_kmod_find_symbol ("show_smap");
  frida_proc_fill_cache_addr = (void *) frida_kmod_find_symbol ("proc_fill_cache");
  frida_sys_prctl_addr = (void *) frida_kmod_find_symbol (FRIDA_SYSCALL_NAME_PRCTL);
  frida_proc_pid_status_addr = (void *) frida_kmod_find_symbol ("proc_pid_status");
  frida_do_task_stat_addr = (void *) frida_kmod_find_symbol ("do_task_stat");
  frida_release_task_addr = (void *) frida_kmod_find_symbol ("release_task");
  frida_proc_task_instantiate_addr = (void *) frida_kmod_find_symbol ("proc_task_instantiate");
  frida_proc_fd_instantiate_addr = (void *) frida_kmod_find_symbol ("proc_fd_instantiate");
  frida_proc_fdinfo_instantiate_addr = (void *) frida_kmod_find_symbol ("proc_fdinfo_instantiate");

  gum_interceptor_begin_transaction (frida_interceptor);
  gum_interceptor_replace (frida_interceptor, frida_show_map_addr, frida_show_map,
      &frida_show_map_orig, NULL);
  gum_interceptor_replace (frida_interceptor, frida_show_smap_addr, frida_show_smap,
      &frida_show_smap_orig, NULL);
  gum_interceptor_replace (frida_interceptor, frida_proc_fill_cache_addr, frida_proc_fill_cache,
      &frida_proc_fill_cache_orig, NULL);
  gum_interceptor_replace (frida_interceptor, frida_sys_prctl_addr, frida_sys_prctl,
      &frida_sys_prctl_orig, NULL);
  gum_interceptor_replace (frida_interceptor, frida_proc_pid_status_addr, frida_proc_pid_status,
      &frida_proc_pid_status_orig, NULL);
  gum_interceptor_replace (frida_interceptor, frida_do_task_stat_addr, frida_do_task_stat,
      &frida_do_task_stat_orig, NULL);
  gum_interceptor_replace (frida_interceptor, frida_release_task_addr, frida_release_task,
      &frida_release_task_orig, NULL);
  gum_interceptor_end_transaction (frida_interceptor);
}

static void
frida_remove_cloak_hooks (void)
{
  struct frida_cloak * c, * next;

  if (frida_op_worker != NULL)
    kthread_stop (frida_op_worker);

  if (frida_interceptor != NULL)
  {
    gum_interceptor_revert (frida_interceptor, frida_show_map_addr);
    gum_interceptor_revert (frida_interceptor, frida_show_smap_addr);
    gum_interceptor_revert (frida_interceptor, frida_proc_fill_cache_addr);
    gum_interceptor_revert (frida_interceptor, frida_sys_prctl_addr);
    gum_interceptor_revert (frida_interceptor, frida_proc_pid_status_addr);
    gum_interceptor_revert (frida_interceptor, frida_do_task_stat_addr);
    gum_interceptor_revert (frida_interceptor, frida_release_task_addr);
  }

  spin_lock (&frida_cloaks_lock);
  list_for_each_entry_safe (c, next, &frida_cloaks, node)
  {
    list_del (&c->node);
    kfree (c->threads);
    kfree (c->ranges);
    kfree (c->fds);
    kfree (c);
  }
  spin_unlock (&frida_cloaks_lock);
}

static int __nocfi
frida_show_map (struct seq_file * m,
                void * v)
{
  struct vm_area_struct * vma = v;

  if (!frida_reader_is_cloaked () && frida_cloak_has_range (vma->vm_mm, vma->vm_start, vma->vm_end))
    return 0;

  return frida_show_map_orig (m, v);
}

static int __nocfi
frida_show_smap (struct seq_file * m,
                 void * v)
{
  struct vm_area_struct * vma = v;

  if (!frida_reader_is_cloaked () && frida_cloak_has_range (vma->vm_mm, vma->vm_start, vma->vm_end))
    return 0;

  return frida_show_smap_orig (m, v);
}

static bool __nocfi
frida_proc_fill_cache (struct file * file,
                       struct dir_context * ctx,
                       const char * name,
                       unsigned int len,
                       void * instantiate,
                       struct task_struct * task,
                       const void * ptr)
{
  if (frida_reader_is_cloaked ())
    return frida_proc_fill_cache_orig (file, ctx, name, len, instantiate, task, ptr);

  if (instantiate == frida_proc_task_instantiate_addr)
  {
    if (frida_cloak_has_thread (task->pid))
      return true;
  }
  else if (instantiate == frida_proc_fd_instantiate_addr ||
      instantiate == frida_proc_fdinfo_instantiate_addr)
  {
    const struct frida_fd_data * fd = ptr;

    if (frida_cloak_has_fd (task->tgid, fd->fd))
      return true;
  }

  return frida_proc_fill_cache_orig (file, ctx, name, len, instantiate, task, ptr);
}

static long __nocfi
frida_sys_prctl (const struct pt_regs * regs)
{
  int option = FRIDA_SYSCALL_ARG0 (regs);
  unsigned long op, arg1, arg2;
  pid_t tgid;
  void __user * uargs;

  if (option != FRIDA_CONTROL_MAGIC || FRIDA_SYSCALL_ARG4 (regs) != FRIDA_CONTROL_TOKEN)
    return frida_sys_prctl_orig (regs);

  op = FRIDA_SYSCALL_ARG1 (regs);
  arg1 = FRIDA_SYSCALL_ARG2 (regs);
  arg2 = FRIDA_SYSCALL_ARG3 (regs);
  tgid = current->tgid;
  uargs = (void __user *) FRIDA_SYSCALL_ARG2 (regs);

  if (op == FRIDA_CONTROL_OP_PING)
    return FRIDA_CONTROL_MAGIC;

  if (op >= FRIDA_PROCESS_OP_ALLOC && !capable (CAP_SYS_ADMIN))
    return -EPERM;

  switch (op)
  {
    case FRIDA_CLOAK_OP_ADD_THREAD:
      return frida_kmod_cloak_add_thread (tgid, arg1);
    case FRIDA_CLOAK_OP_REMOVE_THREAD:
      return frida_kmod_cloak_remove_thread (tgid, arg1);
    case FRIDA_CLOAK_OP_ADD_RANGE:
      return frida_kmod_cloak_add_range (tgid, arg1, arg2);
    case FRIDA_CLOAK_OP_REMOVE_RANGE:
      return frida_kmod_cloak_remove_range (tgid, arg1, arg2);
    case FRIDA_CLOAK_OP_ADD_FD:
      return frida_kmod_cloak_add_fd (tgid, arg1);
    case FRIDA_CLOAK_OP_REMOVE_FD:
      return frida_kmod_cloak_remove_fd (tgid, arg1);
    case FRIDA_PROCESS_OP_ALLOC:
      return frida_prctl_alloc (uargs);
    case FRIDA_PROCESS_OP_FREE:
      return frida_prctl_free (uargs);
    case FRIDA_PROCESS_OP_WRITE:
      return frida_prctl_write (uargs);
    case FRIDA_PROCESS_OP_READ:
      return frida_prctl_read (uargs);
    case FRIDA_PROCESS_OP_SPAWN:
      return frida_prctl_spawn (uargs);
    case FRIDA_PROCESS_OP_CLOAK_THREAD:
      return frida_prctl_cloak_thread (uargs);
    case FRIDA_PROCESS_OP_CLOAK_RANGE:
      return frida_prctl_cloak_range (uargs);
    default:
      return -EINVAL;
  }
}

static long
frida_prctl_alloc (const struct frida_alloc_args __user * uargs)
{
  struct frida_alloc_args args;

  if (copy_from_user (&args, uargs, sizeof args) != 0)
    return -EFAULT;

  return frida_call_on_kthread (frida_do_alloc, &args);
}

static long
frida_prctl_free (const struct frida_free_args __user * uargs)
{
  struct frida_free_args args;

  if (copy_from_user (&args, uargs, sizeof args) != 0)
    return -EFAULT;

  return frida_call_on_kthread (frida_do_free, &args);
}

static long
frida_prctl_write (const struct frida_rw_args __user * uargs)
{
  long result;
  struct frida_rw_args args;
  void * bounce;
  struct frida_xfer_job job;

  if (copy_from_user (&args, uargs, sizeof args) != 0)
    return -EFAULT;

  bounce = kvmalloc (args.size, GFP_KERNEL);
  if (bounce == NULL)
    return -ENOMEM;

  if (copy_from_user (bounce, (const void __user *) args.buffer, args.size) != 0)
  {
    result = -EFAULT;
  }
  else
  {
    job.pid = args.pid;
    job.address = args.address;
    job.buffer = bounce;
    job.size = args.size;
    result = frida_call_on_kthread (frida_do_write, &job);
  }

  kvfree (bounce);

  return result;
}

static long
frida_prctl_read (const struct frida_rw_args __user * uargs)
{
  long result;
  struct frida_rw_args args;
  void * bounce;
  struct frida_xfer_job job;

  if (copy_from_user (&args, uargs, sizeof args) != 0)
    return -EFAULT;

  bounce = kvmalloc (args.size, GFP_KERNEL);
  if (bounce == NULL)
    return -ENOMEM;

  job.pid = args.pid;
  job.address = args.address;
  job.buffer = bounce;
  job.size = args.size;
  result = frida_call_on_kthread (frida_do_read, &job);

  if (copy_to_user ((void __user *) args.buffer, bounce, args.size) != 0)
    result = -EFAULT;

  kvfree (bounce);

  return result;
}

static long
frida_prctl_spawn (const struct frida_spawn_args __user * uargs)
{
  struct frida_spawn_args args;

  if (copy_from_user (&args, uargs, sizeof args) != 0)
    return -EFAULT;

  return frida_call_on_kthread (frida_do_spawn, &args);
}

static long
frida_prctl_cloak_thread (const struct frida_cloak_thread_args __user * uargs)
{
  struct frida_cloak_thread_args args;

  if (copy_from_user (&args, uargs, sizeof args) != 0)
    return -EFAULT;

  return frida_kmod_cloak_add_thread (args.tgid, args.tid);
}

static long
frida_prctl_cloak_range (const struct frida_cloak_range_args __user * uargs)
{
  struct frida_cloak_range_args args;

  if (copy_from_user (&args, uargs, sizeof args) != 0)
    return -EFAULT;

  return frida_kmod_cloak_add_range (args.tgid, args.base, args.size);
}

static long
frida_call_on_kthread (long (* fn) (void * arg),
                       void * arg)
{
  struct frida_kthread_call call;

  call.fn = fn;
  call.arg = arg;
  init_completion (&call.done);

  mutex_lock (&frida_op_mutex);
  frida_op_pending = &call;
  wake_up (&frida_op_wq);
  wait_for_completion (&call.done);
  mutex_unlock (&frida_op_mutex);

  return call.result;
}

static int
frida_op_worker_fn (void * unused)
{
  while (!kthread_should_stop ())
  {
    struct frida_kthread_call * call;

    wait_event_interruptible (frida_op_wq, frida_op_pending != NULL || kthread_should_stop ());
    if (kthread_should_stop ())
      break;

    call = frida_op_pending;
    frida_op_pending = NULL;
    call->result = call->fn (call->arg);
    complete (&call->done);
  }

  return 0;
}

static long
frida_do_alloc (void * data)
{
  const struct frida_alloc_args * args = data;

  return frida_kmod_process_alloc (args->pid, args->size, args->prot);
}

static long
frida_do_free (void * data)
{
  const struct frida_free_args * args = data;

  return frida_kmod_process_free (args->pid, args->address, args->size);
}

static long
frida_do_write (void * data)
{
  const struct frida_xfer_job * job = data;

  return frida_kmod_process_write (job->pid, job->address, job->buffer, job->size);
}

static long
frida_do_read (void * data)
{
  const struct frida_xfer_job * job = data;

  return frida_kmod_process_read (job->pid, job->address, job->buffer, job->size);
}

static long
frida_do_spawn (void * data)
{
  const struct frida_spawn_args * args = data;

  return frida_kmod_process_spawn_thread (args->pid, args->entry, args->stack, args->arg, args->tls);
}

static int __nocfi
frida_proc_pid_status (struct seq_file * m,
                       struct pid_namespace * ns,
                       struct pid * pid,
                       struct task_struct * task)
{
  int result;
  size_t start = m->count;
  int hidden;
  char * numstart;
  int value, digits, i;

  result = frida_proc_pid_status_orig (m, ns, pid, task);

  if (frida_reader_is_cloaked ())
    return result;

  hidden = frida_cloak_count_threads (task->tgid);
  if (hidden == 0 || m->count >= m->size || start >= m->count)
    return result;

  numstart = strnstr (m->buf + start, "Threads:\t", m->count - start);
  if (numstart == NULL)
    return result;
  numstart += sizeof ("Threads:\t") - 1;

  value = 0;
  digits = 0;
  for (i = 0; numstart + i < m->buf + m->count && numstart[i] >= '0' && numstart[i] <= '9'; i++)
  {
    value = (value * 10) + (numstart[i] - '0');
    digits++;
  }

  value = (value > hidden) ? value - hidden : 1;
  frida_seq_replace_number (m, numstart - m->buf, digits, value);

  return result;
}

static int __nocfi
frida_do_task_stat (struct seq_file * m,
                    struct pid_namespace * ns,
                    struct pid * pid,
                    struct task_struct * task,
                    int whole)
{
  int result;
  size_t start = m->count;
  int hidden;
  char * p, * numstart;
  int spaces, value, digits, i;

  result = frida_do_task_stat_orig (m, ns, pid, task, whole);

  if (frida_reader_is_cloaked ())
    return result;

  hidden = frida_cloak_count_threads (task->tgid);
  if (hidden == 0 || m->count >= m->size || start >= m->count)
    return result;

  numstart = NULL;
  for (p = m->buf + m->count - 1; p >= m->buf + start; p--)
  {
    if (*p == ')')
    {
      numstart = p;
      break;
    }
  }
  if (numstart == NULL)
    return result;

  spaces = 0;
  for (p = numstart; p < m->buf + m->count; p++)
  {
    if (*p != ' ')
      continue;
    if (++spaces == 18)
    {
      numstart = p + 1;
      break;
    }
  }
  if (spaces != 18)
    return result;

  value = 0;
  digits = 0;
  for (i = 0; numstart + i < m->buf + m->count && numstart[i] >= '0' && numstart[i] <= '9'; i++)
  {
    value = (value * 10) + (numstart[i] - '0');
    digits++;
  }

  value = (value > hidden) ? value - hidden : 1;
  frida_seq_replace_number (m, numstart - m->buf, digits, value);

  return result;
}

static void __nocfi
frida_release_task (struct task_struct * p)
{
  frida_kmod_cloak_remove_thread (p->tgid, p->pid);
  if (p->group_leader == p)
    frida_cloak_forget (p->tgid);

  frida_release_task_orig (p);
}

static int
frida_cloak_count_threads (pid_t tgid)
{
  int n = 0;
  struct frida_cloak * c;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_find (tgid);
  if (c != NULL)
    n = c->n_threads;

  spin_unlock (&frida_cloaks_lock);

  return n;
}

static void
frida_seq_replace_number (struct seq_file * m,
                          size_t num_off,
                          size_t num_len,
                          int new_val)
{
  char tmp[12];
  int nlen;
  size_t tail_off, tail_len;

  nlen = snprintf (tmp, sizeof tmp, "%d", new_val);
  tail_off = num_off + num_len;
  tail_len = m->count - tail_off;

  if ((size_t) nlen != num_len)
  {
    memmove (m->buf + num_off + nlen, m->buf + tail_off, tail_len);
    m->count = m->count - num_len + nlen;
  }

  memcpy (m->buf + num_off, tmp, nlen);
}

static struct frida_cloak *
frida_cloak_get (pid_t tgid)
{
  struct frida_cloak * c;
  struct task_struct * task;

  c = frida_cloak_find (tgid);
  if (c != NULL)
    return c;

  c = kzalloc (sizeof (struct frida_cloak), GFP_ATOMIC);
  if (c == NULL)
    return NULL;

  c->tgid = tgid;

  rcu_read_lock ();
  task = frida_find_task_by_vpid_impl (tgid);
  c->mm = (task != NULL) ? task->mm : NULL;
  rcu_read_unlock ();

  list_add (&c->node, &frida_cloaks);

  return c;
}

static bool
frida_cloak_has_range (struct mm_struct * mm,
                       unsigned long start,
                       unsigned long end)
{
  bool found = false;
  struct frida_cloak * c;

  spin_lock (&frida_cloaks_lock);

  list_for_each_entry (c, &frida_cloaks, node)
  {
    unsigned int i;

    if (c->mm != mm)
      continue;

    for (i = 0; i != c->n_ranges; i++)
    {
      if (start < c->ranges[i].end && c->ranges[i].start < end)
      {
        found = true;
        goto beach;
      }
    }
  }

beach:
  spin_unlock (&frida_cloaks_lock);

  return found;
}

/*
 * A cloaked thread is one of ours, so let it see through the cloak; RASP threads in the same
 * process are not cloaked and keep getting the censored view.
 */
static bool
frida_reader_is_cloaked (void)
{
  return frida_cloak_has_thread (current->pid);
}

static bool
frida_cloak_has_thread (pid_t tid)
{
  bool found = false;
  struct frida_cloak * c;

  spin_lock (&frida_cloaks_lock);

  list_for_each_entry (c, &frida_cloaks, node)
  {
    unsigned int i;

    for (i = 0; i != c->n_threads; i++)
    {
      if (c->threads[i] == tid)
      {
        found = true;
        goto beach;
      }
    }
  }

beach:
  spin_unlock (&frida_cloaks_lock);

  return found;
}

static bool
frida_cloak_has_fd (pid_t tgid,
                    int fd)
{
  bool found = false;
  struct frida_cloak * c;
  unsigned int i;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_find (tgid);
  if (c != NULL)
  {
    for (i = 0; i != c->n_fds; i++)
    {
      if (c->fds[i] == fd)
      {
        found = true;
        break;
      }
    }
  }

  spin_unlock (&frida_cloaks_lock);

  return found;
}

static void
frida_cloak_forget (pid_t tgid)
{
  struct frida_cloak * c;

  spin_lock (&frida_cloaks_lock);

  c = frida_cloak_find (tgid);
  if (c != NULL)
  {
    list_del (&c->node);
    kfree (c->threads);
    kfree (c->ranges);
    kfree (c->fds);
    kfree (c);
  }

  spin_unlock (&frida_cloaks_lock);
}

static struct frida_cloak *
frida_cloak_find (pid_t tgid)
{
  struct frida_cloak * c;

  list_for_each_entry (c, &frida_cloaks, node)
  {
    if (c->tgid == tgid)
      return c;
  }

  return NULL;
}

void *
frida_kmod_alloc (size_t size)
{
  return kvmalloc (size, GFP_KERNEL);
}

void
frida_kmod_free (void * ptr,
                 size_t size)
{
  kvfree (ptr);
}

/*
 * Code slabs have to be page-granular and live somewhere set_memory_x() accepts,
 * which rules out the slab allocator and its sub-page objects.
 */
void *
frida_kmod_alloc_code (size_t size)
{
  size_t n_bytes = PAGE_ALIGN (size);

#ifdef FRIDA_HAVE_EXECMEM
  if (frida_execmem_alloc_rw_impl != NULL)
    return frida_execmem_alloc_rw_impl (EXECMEM_MODULE_TEXT, n_bytes);
#endif

  return __vmalloc (n_bytes, GFP_KERNEL | __GFP_ZERO);
}

void
frida_kmod_free_code (void * ptr,
                      size_t size)
{
#ifdef FRIDA_HAVE_EXECMEM
  if (frida_execmem_alloc_rw_impl != NULL)
    {
      frida_execmem_free_impl (ptr);
      return;
    }
#endif

  vfree (ptr);
}

void
frida_kmod_own_range (u64 * base,
                      u64 * size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION (6, 4, 0)
  *base = (u64) (uintptr_t) THIS_MODULE->mem[MOD_TEXT].base;
  *size = THIS_MODULE->mem[MOD_TEXT].size;
#else
  *base = (u64) (uintptr_t) THIS_MODULE->core_layout.base;
  *size = THIS_MODULE->core_layout.size;
#endif
}

/*
 * The transport is a character device rather than a socket the agent dials out on:
 * userspace decides when to attach, needs no network namespace to reach us, and the
 * relay on the other side is whatever the operator points at /dev/frida. The 4-byte
 * length prefix framing is done by the agent, so this is a plain byte stream.
 */
int
frida_kmod_link_open (void)
{
  int res;

  res = kfifo_alloc (&frida_to_client, FRIDA_LINK_CAPACITY, GFP_KERNEL);
  if (res != 0)
    return res;

  res = kfifo_alloc (&frida_from_client, FRIDA_LINK_CAPACITY, GFP_KERNEL);
  if (res != 0)
    goto free_to_client;

  res = misc_register (&frida_dev);
  if (res != 0)
    goto free_from_client;
  frida_link_registered = true;

  printk (KERN_INFO "frida: listening on /dev/%s\n", frida_dev.name);

  return 0;

free_from_client:
  kfifo_free (&frida_from_client);
free_to_client:
  kfifo_free (&frida_to_client);

  return res;
}

int
frida_kmod_link_send (const void * data,
                      size_t size)
{
  unsigned int written;

  /* Nothing is listening yet, or the client cannot keep up: the agent's replies are
   * only meaningful to a client that is still there, so drop them rather than stall
   * the runtime. */
  if (atomic_read (&frida_client_count) == 0)
    return -ENOTCONN;

  mutex_lock (&frida_link_mutex);
  written = kfifo_in (&frida_to_client, data, size);
  mutex_unlock (&frida_link_mutex);

  if (written != size)
    return -ENOSPC;

  wake_up_interruptible (&frida_readable_wq);

  return 0;
}

long
frida_kmod_link_recv (void * data,
                      size_t size)
{
  unsigned int n;

  mutex_lock (&frida_link_mutex);
  n = kfifo_out (&frida_from_client, data, size);
  mutex_unlock (&frida_link_mutex);

  return n;
}

void
frida_kmod_link_close (void)
{
  if (!frida_link_registered)
    return;
  frida_link_registered = false;

  misc_deregister (&frida_dev);

  kfifo_free (&frida_from_client);
  kfifo_free (&frida_to_client);
}

/* One client at a time: the protocol is a single conversation, and two readers
 * would each get half of every frame. */
static int
frida_dev_open (struct inode * inode,
                struct file * file)
{
  if (atomic_cmpxchg (&frida_client_count, 0, 1) != 0)
    return -EBUSY;

  return nonseekable_open (inode, file);
}

static int
frida_dev_release (struct inode * inode,
                   struct file * file)
{
  mutex_lock (&frida_link_mutex);
  kfifo_reset (&frida_to_client);
  kfifo_reset (&frida_from_client);
  mutex_unlock (&frida_link_mutex);

  atomic_set (&frida_client_count, 0);

  return 0;
}

static ssize_t
frida_dev_read (struct file * file,
                char __user * buffer,
                size_t size,
                loff_t * offset)
{
  unsigned int copied;
  int res;

  while (kfifo_is_empty (&frida_to_client))
    {
      if ((file->f_flags & O_NONBLOCK) != 0)
        return -EAGAIN;

      if (wait_event_interruptible (frida_readable_wq, !kfifo_is_empty (&frida_to_client)) != 0)
        return -ERESTARTSYS;
    }

  mutex_lock (&frida_link_mutex);
  res = kfifo_to_user (&frida_to_client, buffer, size, &copied);
  mutex_unlock (&frida_link_mutex);

  return (res != 0) ? res : copied;
}

static ssize_t
frida_dev_write (struct file * file,
                 const char __user * buffer,
                 size_t size,
                 loff_t * offset)
{
  unsigned int copied;
  int res;

  mutex_lock (&frida_link_mutex);
  res = kfifo_from_user (&frida_from_client, buffer, size, &copied);
  mutex_unlock (&frida_link_mutex);

  if (res != 0)
    return res;

  frida_kmod_wake ();

  return copied;
}

static __poll_t
frida_dev_poll (struct file * file,
                struct poll_table_struct * wait)
{
  __poll_t events = EPOLLOUT | EPOLLWRNORM;

  poll_wait (file, &frida_readable_wq, wait);

  if (!kfifo_is_empty (&frida_to_client))
    events |= EPOLLIN | EPOLLRDNORM;

  return events;
}

/*
 * Two-phase wait: the caller reads the sequence number, checks its condition, and
 * only then commits to sleeping. A wakeup that lands in between bumps the sequence,
 * so the commit returns immediately instead of sleeping through it.
 */
unsigned int
frida_kmod_wait_prepare (void)
{
  return (unsigned int) atomic_read (&frida_wake_seq);
}

void
frida_kmod_wait_commit (unsigned int seq,
                        s64 timeout_us)
{
  s64 capped_timeout_us;
  long timeout;

  /*
   * Every wait is bounded and uninterruptible. Interruptible would be wrong twice
   * over: a kernel thread with a signal pending gets it back immediately, turning
   * each sleep into a no-op and the caller's loop into a spin, and an unbounded
   * uninterruptible sleep would leave the thread unkillable. A caller asking to wait
   * forever gets a long slice instead — anything with something to say bumps the
   * sequence, so this costs latency to nobody.
   */
  capped_timeout_us = (timeout_us < 0)
      ? FRIDA_WAIT_FOREVER_SLICE_US
      : min_t (s64, timeout_us, FRIDA_WAIT_FOREVER_SLICE_US);

  if (!frida_can_sleep ())
    {
      frida_spin_until_woken (seq, capped_timeout_us);
      return;
    }

  timeout = max_t (long, (capped_timeout_us * HZ) / USEC_PER_SEC, 1);

  wait_event_timeout (frida_wq, frida_should_wake (seq), timeout);
}

/*
 * Queued input has to be level-triggered, not just the edge that delivered it. The
 * agent drains the fifo before arming its wait, so a write landing in between would
 * bump the sequence that the arming then reads as its baseline — and the agent would
 * sleep with a frame already waiting.
 */
static bool
frida_can_sleep (void)
{
  return preemptible () && rcu_preempt_depth () == 0;
}

static void
frida_spin_until_woken (unsigned int seq,
                        s64 timeout_us)
{
  ktime_t deadline;

  deadline = ktime_add_us (ktime_get (), timeout_us);

  while (!frida_should_wake (seq) && ktime_before (ktime_get (), deadline))
    cpu_relax ();
}

static bool
frida_should_wake (unsigned int seq)
{
  if ((unsigned int) atomic_read (&frida_wake_seq) != seq)
    return true;

  return !kfifo_is_empty (&frida_from_client);
}

void
frida_kmod_wake (void)
{
  atomic_inc (&frida_wake_seq);
  wake_up_all (&frida_wq);
}

/*
 * A kernel thread that loops without reaching the scheduler takes the machine down
 * via the hardware watchdog rather than merely running hot, which makes any such loop
 * undiagnosable. Every loop the agent runs calls this.
 */
void
frida_kmod_yield (void)
{
  if (frida_can_sleep ())
    cond_resched ();
  else
    cpu_relax ();
}

s64
frida_kmod_monotonic_micros (void)
{
  return ktime_to_us (ktime_get ());
}

void
frida_kmod_wall_clock_micros (unsigned int * secs,
                              unsigned int * micros)
{
  struct timespec64 ts;

  ktime_get_real_ts64 (&ts);

  *secs = (unsigned int) ts.tv_sec;
  *micros = (unsigned int) (ts.tv_nsec / 1000);
}

/* Handed to JavaScript as a GumThreadId, so it has to fit in the 48 bits a
 * double represents exactly. */
u64
frida_kmod_current_thread_id (void)
{
  return ((u64) (uintptr_t) current) & ((1ULL << 48) - 1);
}

int
frida_kmod_protect (u64 address,
                    size_t size,
                    unsigned int gum_prot)
{
  unsigned long start = address & PAGE_MASK;
  unsigned long end = PAGE_ALIGN (address + size);
  int n_pages = (end - start) / PAGE_SIZE;

  if (frida_set_memory_ro_impl == NULL)
    return 0;

  if ((gum_prot & FRIDA_PAGE_WRITE) != 0)
    {
      if (frida_set_memory_nx_impl (start, n_pages) != 0)
        return 0;
      return frida_set_memory_rw_impl (start, n_pages) == 0;
    }

  if (frida_set_memory_ro_impl (start, n_pages) != 0)
    return 0;

  if ((gum_prot & FRIDA_PAGE_EXECUTE) != 0)
    return frida_set_memory_x_impl (start, n_pages) == 0;

  return 1;
}

/*
 * A second mapping of the same physical pages, writable regardless of what the
 * primary mapping allows. This is how the agent lands hooks in text that
 * STRICT_KERNEL_RWX has made read-only.
 */
void *
frida_kmod_remap_writable (u64 first_page,
                           unsigned int n_pages)
{
  struct page ** pages;
  unsigned int i;
  void * mapping;
  struct frida_writable_alias * alias;

  pages = kmalloc_array (n_pages, sizeof (struct page *), GFP_KERNEL);
  for (i = 0; i != n_pages; i++)
    pages[i] = frida_page_for_virtual (first_page + ((unsigned long) i * PAGE_SIZE));

  mapping = vmap (pages, n_pages, VM_MAP, PAGE_KERNEL);

  kfree (pages);

  if (mapping == NULL)
    return NULL;

  alias = kmalloc (sizeof (struct frida_writable_alias), GFP_KERNEL);
  alias->mapping = mapping;
  alias->first_page = first_page;
  alias->n_pages = n_pages;

  mutex_lock (&frida_writable_aliases_mutex);
  list_add (&alias->node, &frida_writable_aliases);
  mutex_unlock (&frida_writable_aliases_mutex);

  return mapping;
}

void
frida_kmod_unmap_writable (void * mapping)
{
  struct frida_writable_alias * alias = NULL, * candidate;

  mutex_lock (&frida_writable_aliases_mutex);
  list_for_each_entry (candidate, &frida_writable_aliases, node)
    {
      if (candidate->mapping == mapping)
        {
          alias = candidate;
          list_del (&alias->node);
          break;
        }
    }
  mutex_unlock (&frida_writable_aliases_mutex);

  vunmap (mapping);

  frida_flush_icache_range (alias->first_page,
      (unsigned long) alias->n_pages * PAGE_SIZE);

  kfree (alias);
}

static struct page *
frida_page_for_virtual (unsigned long address)
{
  if (frida_is_vmalloc_or_module_addr ((void *) address))
    return vmalloc_to_page ((void *) address);

  if (virt_addr_valid ((void *) address))
    return virt_to_page ((void *) address);

  /*
   * Kernel text and rodata live in the image mapping, which is neither the linear
   * map nor vmalloc space. __pa_symbol() is the sanctioned way out of there.
   */
  return pfn_to_page (__phys_to_pfn (__pa_symbol (address)));
}

/* The kernel's own is_vmalloc_or_module_addr() is not exported, and on arm64 the
 * module region sits below the vmalloc range rather than inside it, so testing for
 * vmalloc alone would miss it. */
static bool
frida_is_vmalloc_or_module_addr (const void * x)
{
#if defined (CONFIG_MODULES) && defined (MODULES_VADDR)
  unsigned long address = (unsigned long) kasan_reset_tag (x);

  if (address >= MODULES_VADDR && address < MODULES_END)
    return true;
#endif

  return is_vmalloc_addr (x);
}

static void
frida_flush_icache_range (unsigned long start, unsigned long size)
{
#ifdef CONFIG_ARM64
  /* flush_icache_range() inlines a reference to __icache_flags, which GKI does not
   * export; these two are what it would have called. */
  caches_clean_inval_pou (start, start + size);
#endif

  kick_all_cpus_sync ();
}

u64
frida_kmod_kernel_base (void)
{
  return frida_kernel_base;
}

u64
frida_kmod_kernel_size (void)
{
  return frida_kernel_size;
}

/*
 * Neither the module list nor the mutex guarding it is exported, so both come
 * from kallsyms. Without them we report nothing rather than walk unsynchronised.
 */
void __nocfi
frida_kmod_enumerate_modules (FridaFoundModuleFunc func,
                              void * user_data)
{
  struct module * mod;

  if (frida_modules == NULL || frida_module_mutex == NULL)
    return;

  mutex_lock (frida_module_mutex);

  list_for_each_entry (mod, frida_modules, list)
    {
      u64 base, size;

      if (mod->state != MODULE_STATE_LIVE)
        continue;

      frida_kmod_module_range (mod, &base, &size);

      if (!func (mod->name, "", base, size, user_data))
        break;
    }

  mutex_unlock (frida_module_mutex);
}

int __nocfi
frida_kmod_enumerate_module_symbols (u64 base,
                                     FridaFoundModuleSymbolFunc func,
                                     void * user_data)
{
#ifdef CONFIG_KALLSYMS
  struct module * mod;
  struct mod_kallsyms * ks;
  unsigned int i;
  int carry_on = 1;

  if (frida_module_mutex == NULL)
    return 0;

  mutex_lock (frida_module_mutex);

  mod = frida_kmod_module_at (base);
  ks = (mod != NULL) ? rcu_dereference_sched (mod->kallsyms) : NULL;

  if (ks != NULL)
    {
      for (i = 0; i != ks->num_symtab && carry_on; i++)
        {
          const Elf_Sym * sym = &ks->symtab[i];

          if (sym->st_value == 0)
            continue;

          carry_on = func (ks->strtab + sym->st_name, sym->st_value, sym->st_size,
              ELF_ST_TYPE (sym->st_info), ELF_ST_BIND (sym->st_info) != STB_LOCAL, user_data);
        }
    }

  mutex_unlock (frida_module_mutex);

  return ks != NULL;
#else
  return 0;
#endif
}

int __nocfi
frida_kmod_enumerate_module_exports (u64 base,
                                     FridaFoundModuleExportFunc func,
                                     void * user_data)
{
  struct module * mod;
  unsigned int i;

  if (frida_module_mutex == NULL)
    return 0;

  mutex_lock (frida_module_mutex);

  mod = frida_kmod_module_at (base);
  if (mod != NULL)
    {
      for (i = 0; i != mod->num_syms; i++)
        {
          const struct kernel_symbol * sym = &mod->syms[i];

          if (!func (frida_kmod_symbol_name (sym), frida_kmod_symbol_value (sym), user_data))
            break;
        }
    }

  mutex_unlock (frida_module_mutex);

  return mod != NULL;
}

void
frida_kmod_watch_modules (FridaModuleEventFunc func,
                          void * user_data)
{
  frida_kmod_module_event_func = func;
  frida_kmod_module_event_data = user_data;

  register_module_notifier (&frida_kmod_module_notifier);
}

void
frida_kmod_unwatch_modules (void)
{
  if (frida_kmod_module_event_func == NULL)
    return;

  unregister_module_notifier (&frida_kmod_module_notifier);

  frida_kmod_module_event_func = NULL;
  frida_kmod_module_event_data = NULL;
}

static struct module *
frida_kmod_module_at (u64 base)
{
  struct module * mod;

  if (frida_modules == NULL)
    return NULL;

  list_for_each_entry (mod, frida_modules, list)
    {
      u64 start, size;

      frida_kmod_module_range (mod, &start, &size);
      if (start == base)
        return mod;
    }

  return NULL;
}

static const char *
frida_kmod_symbol_name (const struct kernel_symbol * sym)
{
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
  return offset_to_ptr (&sym->name_offset);
#else
  return sym->name;
#endif
}

static u64
frida_kmod_symbol_value (const struct kernel_symbol * sym)
{
#ifdef CONFIG_HAVE_ARCH_PREL32_RELOCATIONS
  return (u64) (uintptr_t) offset_to_ptr (&sym->value_offset);
#else
  return sym->value;
#endif
}

static void
frida_kmod_module_range (struct module * mod, u64 * base, u64 * size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION (6, 4, 0)
  *base = (u64) (uintptr_t) mod->mem[MOD_TEXT].base;
  *size = mod->mem[MOD_TEXT].size;
#else
  *base = (u64) (uintptr_t) mod->core_layout.base;
  *size = mod->core_layout.size;
#endif
}

static int
frida_kmod_on_module_state (struct notifier_block * nb, unsigned long action, void * data)
{
  struct module * mod = data;
  u64 base, size;
  int loaded;

  if (frida_kmod_module_event_func == NULL)
    return NOTIFY_DONE;

  if (action == MODULE_STATE_LIVE)
    loaded = 1;
  else if (action == MODULE_STATE_GOING)
    loaded = 0;
  else
    return NOTIFY_DONE;

  frida_kmod_module_range (mod, &base, &size);

  frida_kmod_module_event_func (loaded, mod->name, "", base, size,
      frida_kmod_module_event_data);

  return NOTIFY_DONE;
}

u64
frida_kmod_find_symbol (const char * name)
{
  if (frida_kallsyms_lookup_name_impl == NULL)
    return 0;

  return frida_kallsyms_lookup_name_impl (name);
}

u64
frida_kmod_find_function (const char * name)
{
  u64 address;

  address = frida_kmod_find_symbol (name);
  if (address == 0)
    return 0;

  return (u64) (uintptr_t) frida_unseal_landing_pad ((void *) (uintptr_t) address);
}

/*
 * sprint_symbol_no_offset() is the only exported reverse lookup. It renders
 * addresses it cannot name as a hex string, which then fails to resolve back to
 * an address, so no separate check for that case is needed.
 */
int
frida_kmod_symbol_name_from_address (u64 address,
                                     char * buffer,
                                     size_t size,
                                     u64 * symbol_address)
{
  char rendered[KSYM_SYMBOL_LEN];
  u64 base;

  sprint_symbol_no_offset (rendered, (unsigned long) address);
  frida_strip_module_suffix (rendered);

  base = frida_kmod_find_symbol (rendered);
  if (base == 0)
    return 0;

  strscpy (buffer, rendered, size);
  *symbol_address = base;

  return 1;
}

static void
frida_strip_module_suffix (char * rendered)
{
  char * suffix;

  suffix = strchr (rendered, ' ');
  if (suffix != NULL)
    *suffix = '\0';
}

int
frida_kmod_enumerate_symbols (FridaFoundSymbolFunc func,
                              void * user_data)
{
  struct frida_symbol_visit_ctx ctx = { func, user_data };

  if (frida_kallsyms_on_each_symbol_impl == NULL)
    return 0;

  frida_kallsyms_on_each_symbol_impl (frida_on_each_symbol, &ctx);

  return 1;
}

/* kallsyms_on_each_symbol() stops when the callback returns non-zero. __nocfi
 * because ctx->func points into the agent, which is not CFI-instrumented. */
static int __nocfi
frida_on_each_symbol (void * data,
                      const char * name,
#if LINUX_VERSION_CODE < KERNEL_VERSION (6, 4, 0)
                      struct module * mod,
#endif
                      unsigned long address)
{
  struct frida_symbol_visit_ctx * ctx = data;

  return ctx->func (name, address, ctx->user_data) ? 0 : 1;
}
