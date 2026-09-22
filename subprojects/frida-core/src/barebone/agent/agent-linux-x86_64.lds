SECTIONS {
  .text : {
    *(.text.entrypoint)
    *(.text*)
  }
  .rela.dyn : {
    _agent_relocs_start = .;
    *(.rela.dyn)
    _agent_relocs_end = .;
  }
  .rodata : {
    *(.rodata*)
  }
  .kernel_addrs : ALIGN(8) {
    . = ALIGN(8);
    __printk = .; . += 8;
    _printk = .; . += 8;
    _panic = .; . += 8;
    _kthread_create_on_node = .; . += 8;
    _wake_up_process = .; . += 8;
    _fget = .; . += 8;
    _kernel_read = .; . += 8;
    _kernel_write = .; . += 8;
    ___kmalloc_noprof = .; . += 8;
    _kmalloc = .; . += 8;
    _kfree = .; . += 8;
    _execmem_alloc = .; . += 8;
    _set_memory_rw = .; . += 8;
    _set_memory_ro = .; . += 8;
    _set_memory_x = .; . += 8;
    _set_memory_nx = .; . += 8;
    _modules = .; . += 8;
    _register_module_notifier = .; . += 8;
    _unregister_module_notifier = .; . += 8;
    _text_poke_kgdb = .; . += 8;
    _execmem_free = .; . += 8;
    _module_alloc = .; . += 8;
    _module_memfree = .; . += 8;
    ___init_waitqueue_head = .; . += 8;
    _prepare_to_wait_event = .; . += 8;
    _finish_wait = .; . += 8;
    ___wake_up = .; . += 8;
    _autoremove_wake_function = .; . += 8;
    _schedule_hrtimeout_range = .; . += 8;
    _pci_get_domain_bus_and_slot = .; . += 8;
    _pci_irq_vector = .; . += 8;
    _pci_dev_put = .; . += 8;
    _schedule = .; . += 8;
    _ktime_get_mono_fast_ns = .; . += 8;
    _ktime_get_real_ts64 = .; . += 8;
    _send_sig = .; . += 8;
    _free_irq = .; . += 8;
    _request_threaded_irq = .; . += 8;
    _vmap = .; . += 8;
    _page_offset_base = .; . += 8;
    _current_task = .; . += 8;
    _cpu_current_top_of_stack = .; . += 8;
    _register_die_notifier = .; . += 8;
    _unregister_die_notifier = .; . += 8;
    _ioremap = .; . += 8;
    ___default_kernel_pte_mask = .; . += 8;
    _init_task = .; . += 8;
    _tasklist_lock = .; . += 8;
    __raw_read_lock = .; . += 8;
    __raw_read_unlock = .; . += 8;
    __raw_read_lock_irqsave = .; . += 8;
    __raw_read_unlock_irqrestore = .; . += 8;
    __raw_write_lock = .; . += 8;
    __raw_write_unlock = .; . += 8;
    __raw_write_lock_irqsave = .; . += 8;
    __raw_write_unlock_irqrestore = .; . += 8;
    _detach_pid = .; . += 8;
    _free_pids = .; . += 8;
    _get_task_exe_file = .; . += 8;
    _file_path = .; . += 8;
    _fput = .; . += 8;
    _copy_from_kernel_nofault = .; . += 8;
    _get_task_mm = .; . += 8;
    _kthread_use_mm = .; . += 8;
    _kthread_unuse_mm = .; . += 8;
    _mmput = .; . += 8;
    _vm_mmap = .; . += 8;
    _vm_munmap = .; . += 8;
    _tracepoint_probe_register = .; . += 8;
    _tracepoint_probe_unregister = .; . += 8;
    ___tracepoint_sched_process_fork = .; . += 8;
    ___tracepoint_sched_process_exit = .; . += 8;
    ___tracepoint_sched_process_exec = .; . += 8;
    _vunmap = .; . += 8;
    __copy_to_user = .; . += 8;
    _down_read = .; . += 8;
    _up_read = .; . += 8;
    _get_user_pages_unlocked = .; . += 8;
    _get_user_pages_remote = .; . += 8;
    _do_futex = .; . += 8;
    _user_mode_thread = .; . += 8;
    ___start_BTF = .; . += 8;
    ___stop_BTF = .; . += 8;
    _call_usermodehelper_setup = .; . += 8;
    _call_usermodehelper_exec = .; . += 8;
    __copy_from_user = .; . += 8;
    _access_process_vm = .; . += 8;
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
  . = ALIGN(8);
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
