SECTIONS {
  /* The large code model gives every section an l-prefixed name of its own, so that
     the compiler can reach it with a 64-bit displacement rather than assume 2GB. */
  .text : {
    *(.text.entrypoint)
    *(.ltext.entrypoint)
    *(.text*)
    *(.ltext*)
  }
  .rela.dyn : {
    _agent_relocs_start = .;
    *(.rela.dyn)
    _agent_relocs_end = .;
  }
  .rodata : {
    *(.rodata*)
    *(.lrodata*)
  }
  .kernel_addrs : ALIGN(8) {
    . = ALIGN(8);
    _ExAllocatePoolWithTag = .; . += 8;
    _ExFreePoolWithTag = .; . += 8;
    _PsCreateSystemThread = .; . += 8;
    _PsGetCurrentThreadId = .; . += 8;
    _KeInitializeEvent = .; . += 8;
    _KeWaitForSingleObject = .; . += 8;
    _KeSetEvent = .; . += 8;
    _KeInitializeDpc = .; . += 8;
    _PsSetCreateThreadNotifyRoutine = .; . += 8;
    _PsRemoveCreateThreadNotifyRoutine = .; . += 8;
    _KeInsertQueueDpc = .; . += 8;
    _ZwYieldExecution = .; . += 8;
    _KeExpandKernelStackAndCalloutEx = .; . += 8;
    _KeBugCheckEx = .; . += 8;
    _DbgPrint = .; . += 8;
    _MmGetVirtualForPhysical = .; . += 8;
    _MmMapIoSpace = .; . += 8;
    _MmGetPhysicalAddress = .; . += 8;
    _HalGetInterruptVector = .; . += 8;
    _IoConnectInterrupt = .; . += 8;
    _IoDisconnectInterrupt = .; . += 8;
    _PsActiveProcessHead = .; . += 8;
    _PsInitialSystemProcess = .; . += 8;
    _PsGetProcessId = .; . += 8;
    _PsGetProcessImageFileName = .; . += 8;
    _PsGetProcessPeb = .; . += 8;
    _PsGetCurrentThread = .; . += 8;
    _PsGetThreadProcess = .; . += 8;
    _PsGetThreadId = .; . += 8;
    _PsGetThreadTeb = .; . += 8;
    _PsGetContextThread = .; . += 8;
    _PsSetContextThread = .; . += 8;
    _KeStackAttachProcess = .; . += 8;
    _KeUnstackDetachProcess = .; . += 8;
    _ZwCreateFile = .; . += 8;
    _ZwOpenKey = .; . += 8;
    _ZwEnumerateKey = .; . += 8;
    _ZwQueryValueKey = .; . += 8;
    _ZwReadFile = .; . += 8;
    _ZwWriteFile = .; . += 8;
    _ZwEnumerateValueKey = .; . += 8;
    _ZwDeviceIoControlFile = .; . += 8;
    _ZwClose = .; . += 8;
    _ZwCreateEvent = .; . += 8;
    _ExAllocatePool2 = .; . += 8;
    _MmCopyVirtualMemory = .; . += 8;
    _KeIpiGenericCall = .; . += 8;
    _MmIsAddressValid = .; . += 8;
    _ZwClearEvent = .; . += 8;
    _ZwWaitForSingleObject = .; . += 8;
    _ZwAllocateVirtualMemory = .; . += 8;
    _ZwFreeVirtualMemory = .; . += 8;
    _ZwCreateThread = .; . += 8;
    _PsProcessType = .; . += 8;
    _ObReferenceObjectByHandle = .; . += 8;
    _ObOpenObjectByPointer = .; . += 8;
    _ExEventObjectType = .; . += 8;
    _IoAllocateMdl = .; . += 8;
    _IoFreeMdl = .; . += 8;
    _MmBuildMdlForNonPagedPool = .; . += 8;
    _MmMapLockedPagesSpecifyCache = .; . += 8;
    _MmUnmapLockedPages = .; . += 8;
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
    *(.ldata*)
  }
  .bss : {
    *(.bss*)
    *(.lbss*)
  }
  . = ALIGN(8);
  _heap_start = .;
}

VERSION {
  {
    global:
      _start;
      frida_winnt_user_main;
      frida_winnt_user_bootstrap;

    local:
      *;
  };
}
