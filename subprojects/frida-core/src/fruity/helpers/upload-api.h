#ifndef __FRIDA_UPLOAD_API_H__
#define __FRIDA_UPLOAD_API_H__

#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <TargetConditionals.h>
#if TARGET_OS_OSX
# include <mach/mach_vm.h>
#endif

typedef struct _FridaUploadApi FridaUploadApi;

struct _FridaUploadApi
{
  int (* socket) (int domain, int type, int protocol);
  int (* setsockopt) (int socket, int level, int option_name, const void * option_value, socklen_t option_len);
  int (* bind) (int socket, const struct sockaddr * address, socklen_t address_len);
  int (* listen) (int socket, int backlog);
  int (* getsockname) (int socket, struct sockaddr * restrict address, socklen_t * restrict address_len);
  int (* accept) (int socket, struct sockaddr * restrict address, socklen_t * restrict address_len);
  int (* close) (int fd);
  ssize_t (* read) (int fd, void * buf, size_t n);
  ssize_t (* write) (int fd, const void *buf, size_t n);
  int (* getpagesize) (void);
  int (* mprotect) (void * addr, size_t len, int prot);
  void (* sys_icache_invalidate) (void * start, size_t len);
  void (* sys_dcache_flush) (void * start, size_t len);
  mach_port_t (* _mach_task_self) (void);
  kern_return_t (* mach_vm_allocate) (vm_map_t target, mach_vm_address_t * address, mach_vm_size_t size, int flags);
  kern_return_t (* mach_vm_deallocate) (vm_map_t target, mach_vm_address_t address, mach_vm_size_t size);
  kern_return_t (* vm_remap) (vm_map_t target_task, vm_address_t * target_address, vm_size_t size, vm_address_t mask, int flags,
      vm_map_t src_task, vm_address_t src_address, boolean_t copy, vm_prot_t * cur_protection, vm_prot_t * max_protection,
      vm_inherit_t inheritance);
  void * (* dlopen) (const char * path, int mode);
  void * (* dlsym) (void * handle, const char * symbol);

  int * (* get_errno_storage) (void);
};

#define FRIDA_UPLOAD_API_INIT { \
      socket, \
      setsockopt, \
      bind, \
      listen, \
      getsockname, \
      accept, \
      close, \
      read, \
      write, \
      getpagesize, \
      mprotect, \
      sys_icache_invalidate, \
      sys_dcache_flush, \
      dlsym (RTLD_DEFAULT, "mach_task_self"), \
      mach_vm_allocate, \
      mach_vm_deallocate, \
      vm_remap, \
      dlopen, \
      dlsym, \
      __error, \
    }

#define FRIDA_RX_BUFFER_SIZE (1024 * 1024)

#endif
