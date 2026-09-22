#include "frida-linux-syscalls.h"

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define SIGSTOP 19
#define MAX_FILENAME 256

typedef struct _ExecveEvent ExecveEvent;

struct _ExecveEvent
{
  int pid;
  char command[MAX_FILENAME];
};

struct
{
  __uint (type, BPF_MAP_TYPE_RINGBUF);
  __uint (max_entries, 1 << 22);
}
events SEC (".maps");

struct trace_event_raw_sys_enter
{
  __u64 unused;
  long id;
  unsigned long args[6];
};

SEC ("tracepoint/raw_syscalls/sys_enter")
int
on_execve_enter (struct trace_event_raw_sys_enter * ctx)
{
  __s32 nr = (__s32) ctx->id;
  if (nr != FRIDA_LINUX_SYSCALL_EXECVE)
    return 0;

  ExecveEvent * e = bpf_ringbuf_reserve (&events, sizeof (ExecveEvent), 0);
  if (e == NULL)
    return 0;

  e->pid = bpf_get_current_pid_tgid () >> 32;
  const char * filename = (const char *) ctx->args[0];
  bpf_probe_read_user_str (e->command, sizeof (e->command), filename);

  bpf_ringbuf_submit (e, 0);

  bpf_send_signal (SIGSTOP);

  return 0;
}

char LICENSE[] SEC ("license") = "Dual BSD/GPL";
