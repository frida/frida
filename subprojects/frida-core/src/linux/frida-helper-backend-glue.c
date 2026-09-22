#include "frida-helper-backend.h"

#include <sys/syscall.h>

gboolean
_frida_syscall_satisfies (gint syscall_id, FridaLinuxSyscallMask mask)
{
  switch (syscall_id)
  {
    case __NR_restart_syscall:
      return (mask & FRIDA_LINUX_SYSCALL_MASK_RESTART) != 0;
    case __NR_ioctl:
      return (mask & FRIDA_LINUX_SYSCALL_MASK_IOCTL) != 0;
    case __NR_read:
    case __NR_readv:
      return (mask & FRIDA_LINUX_SYSCALL_MASK_READ) != 0;
#ifdef __NR_select
    case __NR_select:
#endif
#ifdef __NR__newselect
    case __NR__newselect:
#endif
#ifdef __NR_pselect6
    case __NR_pselect6:
#endif
#ifdef __NR_pselect6_time64
    case __NR_pselect6_time64:
#endif
#ifdef __NR_poll
    case __NR_poll:
#endif
#ifdef __NR_ppoll
    case __NR_ppoll:
#endif
#ifdef __NR_ppoll_time64
    case __NR_ppoll_time64:
#endif
#ifdef __NR_epoll_wait
    case __NR_epoll_wait:
#endif
#ifdef __NR_epoll_pwait
    case __NR_epoll_pwait:
#endif
#ifdef __NR_epoll_pwait2
    case __NR_epoll_pwait2:
#endif
      return (mask & FRIDA_LINUX_SYSCALL_MASK_POLL_LIKE) != 0;
#ifdef __NR_wait4
    case __NR_wait4:
#endif
#ifdef __NR_waitpid
    case __NR_waitpid:
#endif
    case __NR_waitid:
      return (mask & FRIDA_LINUX_SYSCALL_MASK_WAIT) != 0;
    case __NR_rt_sigtimedwait:
#ifdef __NR_rt_sigtimedwait_time64
    case __NR_rt_sigtimedwait_time64:
#endif
      return (mask & FRIDA_LINUX_SYSCALL_MASK_SIGWAIT) != 0;
    case __NR_futex:
      return (mask & FRIDA_LINUX_SYSCALL_MASK_FUTEX) != 0;
#ifdef __NR_accept
    case __NR_accept:
#endif
#ifdef __NR_accept4
    case __NR_accept4:
#endif
      return (mask & FRIDA_LINUX_SYSCALL_MASK_ACCEPT) != 0;
#ifdef __NR_recv
    case __NR_recv:
#endif
#ifdef __NR_recvfrom
    case __NR_recvfrom:
#endif
#ifdef __NR_recvmsg
    case __NR_recvmsg:
#endif
#ifdef __NR_recvmmsg
    case __NR_recvmmsg:
#endif
#ifdef __NR_recvmmsg_time64
    case __NR_recvmmsg_time64:
#endif
      return (mask & FRIDA_LINUX_SYSCALL_MASK_RECV) != 0;
    default:
      break;
  }

  return FALSE;
}
