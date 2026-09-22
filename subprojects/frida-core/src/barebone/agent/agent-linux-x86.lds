SECTIONS {
  .text : {
    *(.text.entrypoint)
    *(.text*)
  }
  .rel.dyn : {
    _agent_relocs_start = .;
    *(.rel.dyn)
    _agent_relocs_end = .;
  }
  .rodata : {
    *(.rodata*)
  }
  .kernel_addrs : ALIGN(4) {
    . = ALIGN(4);
    __printk = .; . += 4;
    _printk = .; . += 4;
    _panic = .; . += 4;
    _kthread_create_on_node = .; . += 4;
    _wake_up_process = .; . += 4;
    _fget = .; . += 4;
    _kernel_read = .; . += 4;
    _kernel_write = .; . += 4;
    ___kmalloc_noprof = .; . += 4;
    _kmalloc = .; . += 4;
    _kfree = .; . += 4;
    _execmem_alloc = .; . += 4;
    _set_memory_rw = .; . += 4;
    _set_memory_ro = .; . += 4;
    _set_memory_x = .; . += 4;
    _set_memory_nx = .; . += 4;
    _modules = .; . += 4;
    _register_module_notifier = .; . += 4;
    _unregister_module_notifier = .; . += 4;
    _text_poke_kgdb = .; . += 4;
    _execmem_free = .; . += 4;
    _module_alloc = .; . += 4;
    _module_memfree = .; . += 4;
    ___init_waitqueue_head = .; . += 4;
    _prepare_to_wait_event = .; . += 4;
    _finish_wait = .; . += 4;
    ___wake_up = .; . += 4;
    _autoremove_wake_function = .; . += 4;
    _schedule_hrtimeout_range = .; . += 4;
    _pci_get_domain_bus_and_slot = .; . += 4;
    _pci_irq_vector = .; . += 4;
    _pci_dev_put = .; . += 4;
    _schedule = .; . += 4;
    _ktime_get_mono_fast_ns = .; . += 4;
    _ktime_get_real_ts64 = .; . += 4;
    _send_sig = .; . += 4;
    _free_irq = .; . += 4;
    _request_threaded_irq = .; . += 4;
    _vmap = .; . += 4;
    _current_task = .; . += 4;
    _ioremap = .; . += 4;
    ___default_kernel_pte_mask = .; . += 4;
    _init_task = .; . += 4;
    _tasklist_lock = .; . += 4;
    __raw_read_lock = .; . += 4;
    __raw_read_unlock = .; . += 4;
    __raw_read_lock_irqsave = .; . += 4;
    __raw_read_unlock_irqrestore = .; . += 4;
    __raw_write_lock = .; . += 4;
    __raw_write_unlock = .; . += 4;
    __raw_write_lock_irqsave = .; . += 4;
    __raw_write_unlock_irqrestore = .; . += 4;
    _detach_pid = .; . += 4;
    _free_pids = .; . += 4;
    _get_task_exe_file = .; . += 4;
    _file_path = .; . += 4;
    _fput = .; . += 4;
    _copy_from_kernel_nofault = .; . += 4;
    _get_task_mm = .; . += 4;
    _kthread_use_mm = .; . += 4;
    _kthread_unuse_mm = .; . += 4;
    _mmput = .; . += 4;
    _vm_mmap = .; . += 4;
    _vm_munmap = .; . += 4;
    _tracepoint_probe_register = .; . += 4;
    _tracepoint_probe_unregister = .; . += 4;
    ___tracepoint_sched_process_fork = .; . += 4;
    ___tracepoint_sched_process_exit = .; . += 4;
    ___tracepoint_sched_process_exec = .; . += 4;
    _vunmap = .; . += 4;
    __copy_to_user = .; . += 4;
    _down_read = .; . += 4;
    _up_read = .; . += 4;
    _get_user_pages_unlocked = .; . += 4;
    _get_user_pages_remote = .; . += 4;
    _do_futex = .; . += 4;
    _user_mode_thread = .; . += 4;
    ___start_BTF = .; . += 4;
    ___stop_BTF = .; . += 4;
    _call_usermodehelper_setup = .; . += 4;
    _call_usermodehelper_exec = .; . += 4;
    __copy_from_user = .; . += 4;
    _access_process_vm = .; . += 4;
    _register_die_notifier = .; . += 4;
    _unregister_die_notifier = .; . += 4;
  }
  /* Everything above is only ever read, so a process being instrumented can be shown the very
     same pages; everything below it each copy needs its own of. */
  . = ALIGN(4096);
  _agent_private_start = .;
  .data.rel.ro : {
    *(.data.rel.ro*)
  }
  .got : {
    *(.got*)
  }
  .init_array : {
    _agent_init_start = .;
    *(.init_array*)
    _agent_init_end = .;
  }
  .data : {
    *(.data*)
  }
  .bss : {
    *(.bss*)
  }
  . = ALIGN(4);
  _heap_start = .;
}

VERSION {
  {
    global:
      _start;

    local:
      *;
  };
}
