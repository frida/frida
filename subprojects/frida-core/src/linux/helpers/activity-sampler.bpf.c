#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#define MAX_DEPTH 16

struct
{
  __uint (type, BPF_MAP_TYPE_ARRAY);
  __uint (max_entries, 1);
  __type (key, __u32);
  __type (value, __u32);
}
target_tgid SEC (".maps");

struct
{
  __uint (type, BPF_MAP_TYPE_RINGBUF);
  __uint (max_entries, 1 << 22);
}
events SEC (".maps");

typedef struct _SampleEvent SampleEvent;

struct _SampleEvent
{
  __u64 time_ns;
  __u32 tgid;
  __u32 tid;
  __s32 stack_err;
  __u32 depth;
  __u64 ips[MAX_DEPTH];
};

SEC ("perf_event")
int
on_perf_event (struct bpf_perf_event_data * ctx)
{
  __u32 k0 = 0;

  __u32 * target = bpf_map_lookup_elem (&target_tgid, &k0);
  if (target == NULL)
    return 0;

  __u64 pid_tgid = bpf_get_current_pid_tgid ();
  __u32 tid  = (__u32) pid_tgid;
  __u32 tgid = pid_tgid >> 32;
  if (tgid != *target)
    return 0;

  SampleEvent * e = bpf_ringbuf_reserve (&events, sizeof (SampleEvent), 0);
  if (e == NULL)
    return 0;

  e->time_ns = bpf_ktime_get_ns ();
  e->tgid = tgid;
  e->tid = tid;

  int nbytes = bpf_get_stack (ctx, e->ips, sizeof (e->ips), BPF_F_USER_STACK);
  if (nbytes < 0)
  {
    e->stack_err = nbytes;
    e->depth = 0;
    bpf_ringbuf_submit (e, 0);
    return 0;
  }

  e->stack_err = 0;
  e->depth = (__u32) (nbytes / (int) sizeof (__u64));

  bpf_ringbuf_submit (e, 0);
  return 0;
}

char LICENSE[] SEC ("license") = "Dual BSD/GPL";
