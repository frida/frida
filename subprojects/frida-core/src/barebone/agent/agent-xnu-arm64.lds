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
    _panic = .; . += 8;
    _IOLog = .; . += 8;
    _kalloc = .; . += 8;
    _proc_iterate = .; . += 8;
    _proc_find = .; . += 8;
    _proc_rele = .; . += 8;
    _proc_pid = .; . += 8;
    _proc_best_name = .; . += 8;
    _proc_task = .; . += 8;
    _current_task = .; . += 8;
    _current_thread = .; . += 8;
    _get_task_map = .; . += 8;
    _mach_vm_allocate = .; . += 8;
    _mach_vm_deallocate = .; . += 8;
    _mach_vm_protect = .; . += 8;
    _mach_vm_remap = .; . += 8;
    _vm_map_copyin = .; . += 8;
    _vm_map_copyout = .; . += 8;
    _thread_create = .; . += 8;
    _thread_set_state = .; . += 8;
    _thread_resume = .; . += 8;
    _thread_terminate = .; . += 8;
    _task_suspend = .; . += 8;
    _task_resume = .; . += 8;
    _task_clear_return_wait = .; . += 8;
    _get_bsdtask_info = .; . += 8;
    _thread_suspend = .; . += 8;
    _kfree = .; . += 8;
    _kernel_thread_start = .; . += 8;
    _assert_wait = .; . += 8;
    _assert_wait_timeout = .; . += 8;
    _thread_block = .; . += 8;
    _thread_wakeup = .; . += 8;
    _thread_wakeup_prim = .; . += 8;
    _mach_absolute_time = .; . += 8;
    _absolutetime_to_nanoseconds = .; . += 8;
    _clock_get_calendar_microtime = .; . += 8;
    _ml_io_map = .; . += 8;
    _ml_vtophys = .; . += 8;
    _ml_static_vtop = .; . += 8;
    __ZN9IOService11getPlatformEv = .; . += 8;
    __ZN9IOServiceC2Ev = .; . += 8;
    __ZN8OSSymbol17withCStringNoCopyEPKc = .; . += 8;
    __ZN6OSData9withBytesEPKvj = .; . += 8;
    _kalloc_data = .; . += 8;
    _kfree_data = .; . += 8;
    _wakeup = .; . += 8;
    _sock_socket = .; . += 8;
    _sock_connect = .; . += 8;
    _sock_send = .; . += 8;
    _sock_receive = .; . += 8;
    _sock_close = .; . += 8;
    _sysent = .; . += 8;
    _task_restartable_ranges_register = .; . += 8;
    _task_threads = .; . += 8;
    _mach_vm_region = .; . += 8;
    _mach_vm_region_recurse = .; . += 8;
    _copyin = .; . += 8;
    _copyout = .; . += 8;
    _ulock_wake = .; . += 8;
    _task_info = .; . += 8;
    _convert_port_to_thread = .; . += 8;
    _convert_port_to_thread_read = .; . += 8;
    _convert_port_to_thread_inspect = .; . += 8;
  }
  . = ALIGN(16384);
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
