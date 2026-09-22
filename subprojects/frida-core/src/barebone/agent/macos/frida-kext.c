
#include <libkern/libkern.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>
#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/uio.h>
#include <kern/locks.h>
#include <kern/thread.h>
#include <ptrauth.h>

extern int frida_agent_start (uint64_t own_base, uint64_t own_size);
extern void frida_agent_stop (void);
extern void frida_agent_wake (void);

#define FRIDA_KERNEL_ADDRS(X) \
  X (panic) \
  X (kalloc) X (kfree) X (kalloc_data) X (kfree_data) \
  X (proc_iterate) X (proc_find) X (proc_rele) X (proc_pid) X (proc_best_name) X (proc_task) \
  X (current_task) X (current_thread) X (get_task_map) X (get_bsdtask_info) \
  X (mach_vm_allocate) X (mach_vm_deallocate) X (mach_vm_protect) X (mach_vm_remap) \
  X (kernel_map) X (kernel_pmap) X (pmap_find_phys) X (ml_static_ptovirt) \
  X (mach_vm_region) X (mach_vm_region_recurse) \
  X (vm_map_copyin) X (vm_map_copyout) X (vm_map_wire_external) \
  X (thread_create) X (thread_set_state) X (thread_resume) X (thread_suspend) \
  X (thread_terminate) X (kernel_thread_start) \
  X (task_suspend) X (task_resume) X (task_clear_return_wait) X (task_threads) X (task_info) \
  X (task_restartable_ranges_register) \
  X (assert_wait) X (assert_wait_timeout) X (thread_block) X (thread_wakeup) \
  X (thread_wakeup_prim) X (wakeup) X (ulock_wake) \
  X (mach_absolute_time) X (absolutetime_to_nanoseconds) X (clock_get_calendar_microtime) \
  X (ml_io_map) X (ml_vtophys) X (ml_static_vtop) \
  X (copyin) X (copyout) X (sysent) X (IOLog) \
  X (convert_port_to_thread) X (convert_port_to_thread_read) \
  X (convert_port_to_thread_inspect) \
  X (sock_socket) X (sock_connect) X (sock_send) X (sock_receive) X (sock_close)

#define FRIDA_DEFINE_SLOT(name) void * _##name;
FRIDA_KERNEL_ADDRS (FRIDA_DEFINE_SLOT)
#undef FRIDA_DEFINE_SLOT

typedef struct _FridaKernelAddr FridaKernelAddr;

struct _FridaKernelAddr
{
  const char * name;
  void ** slot;
};

static const FridaKernelAddr frida_kernel_addrs[] = {
#define FRIDA_LIST_SLOT(name) { #name, &_##name },
  FRIDA_KERNEL_ADDRS (FRIDA_LIST_SLOT)
#undef FRIDA_LIST_SLOT
};

extern void * frida_kext_init_start __asm ("section$start$__DATA$__mod_init_func");
extern void * frida_kext_init_end __asm ("section$end$__DATA$__mod_init_func");

const uintptr_t frida_agent_init_start = (uintptr_t) &frida_kext_init_start;
const uintptr_t frida_agent_init_end = (uintptr_t) &frida_kext_init_end;

const uintptr_t frida_agent_private_start = 0;
const uintptr_t frida_agent_heap_start = 0;
const uintptr_t frida_agent_relocs_start = 0;
const uintptr_t frida_agent_relocs_end = 0;

typedef int (* FridaProcCallout) (proc_t process, void * argument);

const unsigned frida_agent_disc_thread_continue =
    ptrauth_type_discriminator (thread_continue_t);
const unsigned frida_agent_disc_proc_callout =
    ptrauth_type_discriminator (FridaProcCallout);

_Static_assert (ptrauth_type_discriminator (thread_continue_t) == 0xd507,
    "the agent signs a thread's entry with 0xd507");


int
getentropy (void * buffer, size_t size)
{
  read_random (buffer, (u_int) size);

  return 0;
}

void
sys_icache_invalidate (void * start, size_t size)
{
  flush_dcache ((vm_offset_t) start, (unsigned) size, FALSE);
  invalidate_icache ((vm_offset_t) start, (unsigned) size, FALSE);
}

#define FRIDA_IOC_SET_ADDR _IOW ('F', 1, FridaAddrRequest)
#define FRIDA_IOC_START _IO ('F', 2)
#define FRIDA_IOC_GET_IMAGE _IOR ('F', 3, FridaImageInfo)

typedef struct _FridaAddrRequest FridaAddrRequest;
typedef struct _FridaImageInfo FridaImageInfo;

struct _FridaAddrRequest
{
  char name[64];
  uint64_t address;
};

struct _FridaImageInfo
{
  uint64_t base;
  uint64_t size;
};

#define FRIDA_LINK_ROOM (1024 * 1024)

typedef struct _FridaLink FridaLink;

struct _FridaLink
{
  lck_mtx_t * lock;
  bool open;

  uint8_t * to_host;
  size_t to_host_len;

  uint8_t * from_host;
  size_t from_host_len;
};

static FridaLink frida_link;
static lck_grp_t * frida_lock_group;

static int frida_dev_open (dev_t dev, int flags, int devtype, struct proc * p);
static int frida_dev_close (dev_t dev, int flags, int devtype, struct proc * p);
static int frida_dev_read (dev_t dev, struct uio * uio, int ioflag);
static int frida_dev_write (dev_t dev, struct uio * uio, int ioflag);
static int frida_dev_ioctl (dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc * p);

static struct cdevsw frida_cdevsw = {
  .d_open = frida_dev_open,
  .d_close = frida_dev_close,
  .d_read = frida_dev_read,
  .d_write = frida_dev_write,
  .d_ioctl = frida_dev_ioctl,
  .d_stop = eno_stop,
  .d_reset = eno_reset,
  .d_ttys = NULL,
  .d_select = eno_select,
  .d_mmap = eno_mmap,
  .d_strategy = eno_strat,
  .d_reserved_1 = eno_getc,
  .d_reserved_2 = eno_putc,
  .d_type = 0
};

static int frida_major = -1;
static void * frida_node = NULL;
static bool frida_running = false;
static uint64_t frida_own_base = 0;
static uint64_t frida_own_size = 0;

kern_return_t
frida_kext_start (kmod_info_t * ki, void * d)
{
  frida_own_base = ki->address;
  frida_own_size = ki->size;

  frida_lock_group = lck_grp_alloc_init ("frida", LCK_GRP_ATTR_NULL);
  if (frida_lock_group == NULL)
    return KERN_FAILURE;
  frida_link.lock = lck_mtx_alloc_init (frida_lock_group, LCK_ATTR_NULL);

  frida_major = cdevsw_add (-1, &frida_cdevsw);
  if (frida_major == -1)
    return KERN_FAILURE;

  frida_node = devfs_make_node (makedev (frida_major, 0), DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0600,
      "frida");
  if (frida_node == NULL)
  {
    cdevsw_remove (frida_major, &frida_cdevsw);
    frida_major = -1;
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

kern_return_t
frida_kext_stop (kmod_info_t * ki, void * d)
{
  if (frida_running)
  {
    frida_agent_stop ();
    frida_running = false;
  }

  if (frida_node != NULL)
  {
    devfs_remove (frida_node);
    frida_node = NULL;
  }

  if (frida_major != -1)
  {
    cdevsw_remove (frida_major, &frida_cdevsw);
    frida_major = -1;
  }

  return KERN_SUCCESS;
}

KMOD_EXPLICIT_DECL (re.frida.agent, "1.0", frida_kext_start, frida_kext_stop)

static int
frida_dev_open (dev_t dev, int flags, int devtype, struct proc * p)
{
  int result = 0;

  lck_mtx_lock (frida_link.lock);

  if (frida_link.open)
  {
    result = EBUSY;
  }
  else
  {
    frida_link.to_host = _MALLOC (FRIDA_LINK_ROOM, M_TEMP, M_WAITOK);
    frida_link.from_host = _MALLOC (FRIDA_LINK_ROOM, M_TEMP, M_WAITOK);
    if (frida_link.to_host != NULL && frida_link.from_host != NULL)
    {
      frida_link.to_host_len = 0;
      frida_link.from_host_len = 0;
      frida_link.open = true;
    }
    else
    {
      result = ENOMEM;
    }
  }

  lck_mtx_unlock (frida_link.lock);

  return result;
}

static int
frida_dev_close (dev_t dev, int flags, int devtype, struct proc * p)
{
  lck_mtx_lock (frida_link.lock);

  frida_link.open = false;
  if (frida_link.to_host != NULL)
  {
    _FREE (frida_link.to_host, M_TEMP);
    frida_link.to_host = NULL;
  }
  if (frida_link.from_host != NULL)
  {
    _FREE (frida_link.from_host, M_TEMP);
    frida_link.from_host = NULL;
  }

  wakeup (&frida_link);

  lck_mtx_unlock (frida_link.lock);

  return 0;
}

static int
frida_dev_read (dev_t dev, struct uio * uio, int ioflag)
{
  int result = 0;

  lck_mtx_lock (frida_link.lock);

  while (frida_link.open && frida_link.to_host_len == 0)
  {
    if (msleep (&frida_link, frida_link.lock, PCATCH, "frida", NULL) != 0)
    {
      lck_mtx_unlock (frida_link.lock);
      return EINTR;
    }
  }

  if (frida_link.to_host_len != 0)
  {
    size_t n = MIN ((size_t) uio_resid (uio), frida_link.to_host_len);

    result = uiomove ((const char *) frida_link.to_host, (int) n, uio);
    if (result == 0)
    {
      memmove (frida_link.to_host, frida_link.to_host + n, frida_link.to_host_len - n);
      frida_link.to_host_len -= n;
    }
  }

  lck_mtx_unlock (frida_link.lock);

  return result;
}

static int
frida_dev_write (dev_t dev, struct uio * uio, int ioflag)
{
  size_t n;
  int result;

  lck_mtx_lock (frida_link.lock);

  n = MIN ((size_t) uio_resid (uio), FRIDA_LINK_ROOM - frida_link.from_host_len);
  if (n == 0)
  {
    lck_mtx_unlock (frida_link.lock);
    return ENOBUFS;
  }

  result = uiomove ((char *) frida_link.from_host + frida_link.from_host_len, (int) n, uio);
  if (result == 0)
    frida_link.from_host_len += n;

  lck_mtx_unlock (frida_link.lock);

  if (result == 0 && frida_running)
    frida_agent_wake ();

  return result;
}

int
frida_kmod_link_open (void)
{
  return frida_link.open ? 0 : -1;
}

void
frida_kmod_link_close (void)
{
}

int
frida_kmod_link_send (const void * data, size_t size)
{
  lck_mtx_lock (frida_link.lock);

  if (!frida_link.open || frida_link.to_host_len + size > FRIDA_LINK_ROOM)
  {
    lck_mtx_unlock (frida_link.lock);
    return -1;
  }

  memcpy (frida_link.to_host + frida_link.to_host_len, data, size);
  frida_link.to_host_len += size;

  wakeup (&frida_link);

  lck_mtx_unlock (frida_link.lock);

  return 0;
}

bool
frida_kmod_link_pending (void)
{
  bool pending;

  lck_mtx_lock (frida_link.lock);

  pending = frida_link.from_host_len != 0;

  lck_mtx_unlock (frida_link.lock);

  return pending;
}

ssize_t
frida_kmod_link_recv (void * data, size_t size)
{
  size_t n;

  lck_mtx_lock (frida_link.lock);

  n = MIN (size, frida_link.from_host_len);
  if (n != 0)
  {
    memcpy (data, frida_link.from_host, n);
    memmove (frida_link.from_host, frida_link.from_host + n, frida_link.from_host_len - n);
    frida_link.from_host_len -= n;
  }

  lck_mtx_unlock (frida_link.lock);

  return (ssize_t) n;
}

static int
frida_dev_ioctl (dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc * p)
{
  switch (cmd)
  {
    case FRIDA_IOC_SET_ADDR:
    {
      FridaAddrRequest * r = (FridaAddrRequest *) data;
      size_t i;

      r->name[sizeof (r->name) - 1] = '\0';

      for (i = 0; i != sizeof (frida_kernel_addrs) / sizeof (frida_kernel_addrs[0]); i++)
      {
        if (strncmp (frida_kernel_addrs[i].name, r->name, sizeof (r->name)) == 0)
        {
          *frida_kernel_addrs[i].slot = (void *) (uintptr_t) r->address;
          return 0;
        }
      }

      return ENOENT;
    }
    case FRIDA_IOC_GET_IMAGE:
    {
      FridaImageInfo * info = (FridaImageInfo *) data;

      info->base = frida_own_base;
      info->size = frida_own_size;

      return 0;
    }
    case FRIDA_IOC_START:
    {
      if (frida_running)
        return EBUSY;

      if (frida_agent_start (frida_own_base, frida_own_size) != 0)
        return ENOMEM;

      frida_running = true;

      return 0;
    }
    default:
      return ENOTTY;
  }
}
