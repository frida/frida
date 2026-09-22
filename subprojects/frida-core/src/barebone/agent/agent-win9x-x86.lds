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
    _Get_Cur_VM_Handle = .; . += 4;
    _Get_Sys_VM_Handle = .; . += 4;
    _Schedule_Global_Event = .; . += 4;
    _Create_Semaphore = .; . += 4;
    _Wait_Semaphore = .; . += 4;
    _Signal_Semaphore = .; . += 4;
    _Get_Next_VM_Handle = .; . += 4;
    _Release_Time_Slice = .; . += 4;
    _Set_Global_Time_Out = .; . += 4;
    _Cancel_Time_Out = .; . += 4;
    _Get_System_Time = .; . += 4;
    __HeapAllocate = .; . += 4;
    __HeapFree = .; . += 4;
    __PageAllocate = .; . += 4;
    __PageFree = .; . += 4;
    __CopyPageTable = .; . += 4;
    __MapPhysToLinear = .; . += 4;
    _VMM_Add_DDB = .; . += 4;
    _VMM_Remove_DDB = .; . += 4;
    __RegOpenKey = .; . += 4;
    __RegCloseKey = .; . += 4;
    __RegEnumKey = .; . += 4;
    __RegQueryValueEx = .; . += 4;
    __PageReserve = .; . += 4;
    __PageCommit = .; . += 4;
    _Hook_VMM_Fault = .; . += 4;
    _Unhook_VMM_Fault = .; . += 4;
    _Fatal_Error_Handler = .; . += 4;
    _Get_Cur_Thread_Handle = .; . += 4;

    _Get_Initial_Thread_Handle = .; . += 4;
    _Get_Next_Thread_Handle = .; . += 4;
    __Debug_Printf_Service = .; . += 4;
    __ContextSwitch = .; . += 4;
    __PageModifyPermissions = .; . += 4;
    __GetCurrentContext = .; . += 4;
    _VPICD_Virtualize_IRQ = .; . += 4;
    _VPICD_Force_Default_Behavior = .; . += 4;
    _VPICD_Phys_EOI = .; . += 4;
    _VPICD_Physically_Unmask = .; . += 4;
    __VWIN32_QueueUserApc = .; . += 4;
    __VWIN32_Get_Thread_Context = .; . += 4;
    __VWIN32_Set_Thread_Context = .; . += 4;
    __VWIN32_CreateRing0Thread = .; . += 4;
    _IFSMgr_Ring0_FileIO = .; . += 4;
    _KERNEL32_Base = .; . += 4;
    _KERNEL32_ProcessIdObfuscator = .; . += 4;
    _KERNEL32_ModuleTable = .; . += 4;
    __PageCommitPhys = .; . += 4;
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
      frida_win9x_user_main;

    local:
      *;
  };
}
