#include "frida-payload.h"

#ifdef HAVE_DARWIN

#include <gum/gumdarwin.h>

void
_frida_thread_suspend_monitor_remove_cloaked_threads (task_inspect_t task, thread_act_array_t * threads, mach_msg_type_number_t * count)
{
  guint i, o;
  thread_act_array_t old_threads = *threads;
  gsize page_size, old_size, new_size, pages_before, pages_after;

  if (task != mach_task_self () || *count == 0)
    return;

  for (i = 0, o = 0; i != *count; i++)
  {
    thread_t thread = old_threads[i];

    if (gum_cloak_has_thread (thread))
      mach_port_deallocate (task, thread);
    else
      old_threads[o++] = thread;
  }
  g_assert (o > 0);

  page_size = getpagesize ();
  old_size = *count * sizeof (thread_t);
  new_size = o * sizeof (thread_t);
  pages_before = GUM_ALIGN_SIZE (old_size, page_size) / page_size;
  pages_after = GUM_ALIGN_SIZE (new_size, page_size) / page_size;

  if (pages_before != pages_after)
  {
    thread_act_array_t new_threads;

    mach_vm_allocate (task, (mach_vm_address_t *) &new_threads, new_size, VM_FLAGS_ANYWHERE);
    mach_vm_copy (task, (mach_vm_address_t) old_threads, new_size, (mach_vm_address_t) new_threads);

    *threads = new_threads;
    *count = o;

    mach_vm_deallocate (task, (mach_vm_address_t) old_threads, old_size);
  }
  else
  {
    *count = o;
  }
}

#endif
