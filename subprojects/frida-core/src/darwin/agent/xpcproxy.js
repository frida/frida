const POSIX_SPAWN_START_SUSPENDED = 0x0080;

applyJailbreakQuirks();

Interceptor.attach(Process.getModuleByName('/usr/lib/system/libsystem_kernel.dylib').getExportByName('__posix_spawn'), {
  onEnter(args) {
    const attrs = args[2].add(Process.pointerSize).readPointer();

    let flags = attrs.readU16();
    flags |= POSIX_SPAWN_START_SUSPENDED;
    attrs.writeU16(flags);
  }
});

function applyJailbreakQuirks() {
  const bootstrapper = findSubstrateBootstrapper();
  if (bootstrapper !== null) {
    instrumentSubstrateBootstrapper(bootstrapper);
    return;
  }

  const jbdCallImpl = findJbdCallImpl();
  if (jbdCallImpl !== null) {
    sabotageJbdCall(jbdCallImpl);
    return;
  }

  const proxyer = findSubstrateProxyer();
  if (proxyer !== null)
    instrumentSubstrateExec(proxyer.exec);
}

function sabotageJbdCall(jbdCallImpl) {
  const retType = 'int';
  const argTypes = ['uint', 'uint', 'uint'];

  const jbdCall = new NativeFunction(jbdCallImpl, retType, argTypes);

  Interceptor.replace(jbdCall, new NativeCallback((port, command, pid) => {
    return 0;
  }, retType, argTypes));
}

function instrumentSubstrateBootstrapper(bootstrapper) {
  Interceptor.attach(Process.getModuleByName('/usr/lib/system/libdyld.dylib').getExportByName('dlopen'), {
    onEnter(args) {
      this.path = args[0].readUtf8String();
    },
    onLeave(retval) {
      if (!retval.isNull() && this.path === '/usr/lib/substrate/SubstrateInserter.dylib') {
        const inserter = Process.getModuleByName(this.path);
        const exec = resolveSubstrateExec(inserter.base, inserter.size);
        instrumentSubstrateExec(exec);
      }
    }
  });
}

function instrumentSubstrateExec(exec) {
  Interceptor.attach(exec, {
    onEnter(args) {
      const startSuspendedYup = ptr(1);
      args[2] = startSuspendedYup;
    }
  });
}

function findJbdCallImpl() {
  const impl = Module.findGlobalExportByName('jbd_call');
  if (impl !== null)
    return impl;

  const payload = Process.findModuleByName('/chimera/pspawn_payload.dylib');
  if (payload === null)
    return null;

  const matches = Memory.scanSync(payload.base, payload.size, 'ff 43 01 d1 f4 4f 03 a9 fd 7b 04 a9 fd 03 01 91');
  if (matches.length !== 1)
    throw new Error('Unsupported version of Chimera; please file a bug');

  return matches[0].address;
}

function findSubstrateBootstrapper() {
  if (Process.arch !== 'arm64')
    return null;

  return Process.findModuleByName('/usr/lib/substrate/SubstrateBootstrap.dylib');
}

function findSubstrateProxyer() {
  if (Process.arch !== 'arm64')
    return null;

  const proxyerDylibName = '50 72 6f 78 79 65 72 2e 74 2e 64 79 6c 69 62';

  const modules = new ModuleMap();
  const ranges = Process.enumerateRanges('r-x')
      .filter(r => !modules.has(r.base))
      .filter(r => (r.base.readU32() & 0xfffffffe) >>> 0 === 0xfeedface)
      .filter(r => Memory.scanSync(r.base, 2048, proxyerDylibName).length > 0);
  if (ranges.length === 0)
    return null;
  const proxyer = ranges[0];

  return {
    exec: resolveSubstrateExec(proxyer.base, proxyer.size)
  };
}

function resolveSubstrateExec(base, size) {
  const matches = Memory.scanSync(base, size, 'fd 7b bf a9 fd 03 00 91 f4 4f bf a9 ff c3 00 d1 f3 03 02 aa');
  if (matches.length !== 1) {
    throw new Error('Unsupported version of Substrate; please file a bug');
  }
  return matches[0].address;
}
