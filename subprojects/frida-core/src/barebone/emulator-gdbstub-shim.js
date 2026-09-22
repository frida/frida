// Injected into the Android emulator's QEMU by the barebone backend to make its
// HVF-backed gdbstub usable: commit gdb register writes the resume path would
// otherwise clobber, emulate the breakpoints HVF cannot insert, and keep the
// vcpu's unknown exits from aborting QEMU. Synchronous exceptions the guest
// should see are reinjected into its own EL1 vector. The offsets are for one
// build (QEMU 2.12 fork, Runtime 13.3) and are verified before patching.

const HV_REG_PC = 31;
const HV_REG_CPSR = 34;

const ELR_EL1 = 0xc201;
const SPSR_EL1 = 0xc200;
const ESR_EL1 = 0xc290;
const VBAR_EL1 = 0xc600;
const MDSCR_EL1 = 0x8012;
const DBGBVR0_EL1 = 0x8004;
const DBGBCR0_EL1 = 0x8005;
const DBGBCR_ENABLE_EL0_EL1 = uint64('0x1e7');
const MDSCR_MDE = uint64('0xa000');
const PSTATE_EL1H_MASKED = uint64('0x3c5');

const CPU_STATE_DIRTY = 0x82b3;
const CPU_STATE_FD = 0x82b4;
const CPU_STATE_EXIT_REQUEST = 0x82a4;
const CPU_STATE_EXIT = 0x82c8;

const HV_EXIT_REASON_UNKNOWN = 3;
const HV_EXIT_REASON_CANCELED = 0;

const breakpointClasses = new Set([0x30, 0x31]);
const guestDebugClasses = new Set([0x3c, 0x32, 0x33, 0x34, 0x35]);

const cpuGdbWriteRegister = Module.getGlobalExportByName('aarch64_cpu_gdb_write_register');
const hvfPutRegisters = Module.getGlobalExportByName('hvf_put_registers');
const hvfGetRegisters = Module.getGlobalExportByName('hvf_get_registers');
const cpuBreakpointInsert = Module.getGlobalExportByName('cpu_breakpoint_insert');
const cpuBreakpointRemoveAll = Module.getGlobalExportByName('cpu_breakpoint_remove_all');
const hvfVcpuExec = Module.getGlobalExportByName('hvf_vcpu_exec');
const hvfHandleException = Module.getGlobalExportByName('hvf_handle_exception');

const slide = cpuBreakpointInsert.sub(0x100024a6c);
const reasonCompare = slide.add(0x10012bfe8);
const breakpointInvalidate = slide.add(0x100024b2c);

const setSysReg = new NativeFunction(Module.getGlobalExportByName('hv_vcpu_set_sys_reg'), 'int', ['uint64', 'uint32', 'uint64']);
const getSysReg = new NativeFunction(Module.getGlobalExportByName('hv_vcpu_get_sys_reg'), 'int', ['uint64', 'uint32', 'pointer']);
const setReg = new NativeFunction(Module.getGlobalExportByName('hv_vcpu_set_reg'), 'int', ['uint64', 'uint32', 'uint64']);
const getReg = new NativeFunction(Module.getGlobalExportByName('hv_vcpu_get_reg'), 'int', ['uint64', 'uint32', 'pointer']);
const setTrapDebugExceptions = new NativeFunction(Module.getGlobalExportByName('hv_vcpu_set_trap_debug_exceptions'), 'int', ['uint64', 'uint32']);
const setStopCpu = new NativeFunction(Module.getGlobalExportByName('gdb_set_stop_cpu'), 'void', ['pointer']);
const requestDebug = new NativeFunction(Module.getGlobalExportByName('qemu_system_debug_request'), 'void', []);

const scratch = Memory.alloc(8);

let vcpuFd = -1;
let hardwareBreakpoint = null;
let pendingRegisterCommit = null;

verifyBuild();
captureVcpuFd();
commitGdbRegisterWrites();
emulateHardwareBreakpoints();
rewriteUnknownVcpuExits();
handleDebugExceptions();
recv('allow-pipe-path', onAllowPipePath);
send({ type: 'armed' });

function verifyBuild() {
  const sites = [
    [cpuBreakpointInsert, 0xa9bc5ff8, 'cpu_breakpoint_insert'],
    [reasonCompare, 0x7100051f, 'reason-compare'],
    [breakpointInvalidate, 0xd10103ff, 'breakpoint_invalidate'],
  ];
  for (const [address, expected, name] of sites) {
    const found = address.readU32();
    if (found !== expected) {
      send({ type: 'shim-error', message: `unexpected instruction at ${name}: got 0x${found.toString(16)}, want 0x${expected.toString(16)}` });
      throw new Error('offset mismatch, refusing to patch');
    }
  }
}

function captureVcpuFd() {
  Interceptor.attach(hvfGetRegisters, {
    onEnter(args) {
      if (vcpuFd < 0)
        vcpuFd = args[0].add(CPU_STATE_FD).readU32();
    }
  });
}

function commitGdbRegisterWrites() {
  Interceptor.attach(cpuGdbWriteRegister, {
    onEnter(args) {
      this.cpuState = args[0];
      this.register = args[2].toInt32();
    },
    onLeave() {
      if (this.register >= 31) {
        this.cpuState.add(CPU_STATE_DIRTY).writeU8(1);
        pendingRegisterCommit = this.cpuState;
      }
    }
  });

  Interceptor.attach(hvfPutRegisters, {
    onLeave() {
      if (pendingRegisterCommit !== null) {
        pendingRegisterCommit.add(CPU_STATE_DIRTY).writeU8(0);
        pendingRegisterCommit = null;
      }
    }
  });
}

function emulateHardwareBreakpoints() {
  Interceptor.replace(breakpointInvalidate, new NativeCallback(() => {}, 'void', ['pointer', 'uint64']));

  Interceptor.attach(cpuBreakpointInsert, {
    onEnter(args) {
      hardwareBreakpoint = uint64(args[1].toString());
    }
  });

  Interceptor.attach(cpuBreakpointRemoveAll, {
    onEnter() {
      hardwareBreakpoint = null;
    }
  });

  Interceptor.attach(hvfVcpuExec, {
    onEnter() {
      programHardwareBreakpoint(hardwareBreakpoint !== null);
    }
  });
}

function rewriteUnknownVcpuExits() {
  // The reason is in x8 at the compare; context registers are NativePointer, so
  // read them with toUInt32().
  Interceptor.attach(reasonCompare, {
    onEnter() {
      if (this.context.x8.toUInt32() === HV_EXIT_REASON_UNKNOWN)
        this.context.x8 = ptr(HV_EXIT_REASON_CANCELED);
    }
  });
}

function handleDebugExceptions() {
  Interceptor.attach(hvfHandleException, {
    onEnter(args) {
      const cpuState = args[0];
      const syndrome = cpuState.add(CPU_STATE_EXIT).readPointer().add(8);
      const exceptionClass = syndrome.readU64().shr(26).and(0x3f).toNumber();

      if (breakpointClasses.has(exceptionClass))
        reportBreakpointStop(cpuState);
      else if (guestDebugClasses.has(exceptionClass))
        reinjectToGuest(syndrome.readU64());
      else
        return;

      cpuState.add(CPU_STATE_EXIT_REQUEST).writeU32(1);
      neutralizeSyndrome(syndrome);
    }
  });
}

function reportBreakpointStop(cpuState) {
  programHardwareBreakpoint(false);
  setStopCpu(cpuState);
  requestDebug();
}

function reinjectToGuest(syndrome) {
  if (vcpuFd < 0)
    return;

  const pc = readReg(HV_REG_PC);
  const cpsr = readReg(HV_REG_CPSR);
  const vbar = readSysReg(VBAR_EL1);

  setSysReg(vcpuFd, ELR_EL1, pc);
  setSysReg(vcpuFd, SPSR_EL1, cpsr);
  setSysReg(vcpuFd, ESR_EL1, syndrome.and(uint64('0xffffffff')));

  const fromEl0 = cpsr.and(uint64('0xf')).toNumber() === 0;
  setReg(vcpuFd, HV_REG_PC, vbar.add(fromEl0 ? 0x400 : 0x200));
  setReg(vcpuFd, HV_REG_CPSR, PSTATE_EL1H_MASKED);
}

function readReg(index) {
  if (getReg(vcpuFd, index, scratch) === 0)
    return scratch.readU64();
  return uint64(0);
}

function readSysReg(encoding) {
  if (getSysReg(vcpuFd, encoding, scratch) === 0)
    return scratch.readU64();
  return uint64(0);
}

function programHardwareBreakpoint(enable) {
  if (vcpuFd < 0)
    return;

  if (enable && hardwareBreakpoint !== null) {
    setSysReg(vcpuFd, DBGBVR0_EL1, hardwareBreakpoint);
    setSysReg(vcpuFd, DBGBCR0_EL1, DBGBCR_ENABLE_EL0_EL1);
    setSysReg(vcpuFd, MDSCR_EL1, MDSCR_MDE);
    setTrapDebugExceptions(vcpuFd, 1);
  } else {
    setSysReg(vcpuFd, DBGBCR0_EL1, uint64(0));
    setSysReg(vcpuFd, MDSCR_EL1, uint64(0));
    setTrapDebugExceptions(vcpuFd, 0);
  }
}

function neutralizeSyndrome(syndrome) {
  syndrome.writeU64(syndrome.readU64().and(uint64('0x03ffffff')).or(uint64('0x04000000')));
}

function onAllowPipePath(message) {
  const addAllowedPath = new NativeFunction(Module.getGlobalExportByName('android_unix_pipes_add_allowed_path'), 'void', ['pointer']);
  addAllowedPath(Memory.allocUtf8String(message.path));
}
