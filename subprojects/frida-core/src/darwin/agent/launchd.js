const POSIX_SPAWN_START_SUSPENDED = 0x0080;
const SIGKILL = 9;

const { pointerSize } = Process;

const crashServices = new Set([
  'com.apple.ReportCrash',
  'com.apple.osanalytics.osanalyticshelper',
]);

const upcoming = new Set();
const reportCrashes = @REPORT_CRASHES@;
let gating = false;
const suspendedPids = new Set();

let pidsToIgnore = null;

const substrateInvocations = new Set();
const substratePidsPending = new Map();

rpc.exports = {
  dispose() {
    if (suspendedPids.size > 0) {
      const kill = new NativeFunction(Module.getGlobalExportByName('kill'), 'int', ['int', 'int']);
      for (const pid of suspendedPids) {
        kill(pid, SIGKILL);
      }
    }
  },
  prepareForLaunch(identifier) {
    upcoming.add(identifier);
  },
  cancelLaunch(identifier) {
    upcoming.delete(identifier);
  },
  enableSpawnGating() {
    gating = true;
  },
  disableSpawnGating() {
    gating = false;
  },
  claimProcess(pid) {
    suspendedPids.delete(pid);
  },
  unclaimProcess(pid) {
    suspendedPids.add(pid);
  },
};

applyJailbreakQuirks();

Interceptor.attach(Process.getModuleByName('/usr/lib/system/libsystem_kernel.dylib').getExportByName('__posix_spawn'), {
  onEnter(args) {
    const env = parseStringv(args[4]);
    const prewarm = isPrewarmLaunch(env);

    if (prewarm && !gating)
      return;

    const path = args[1].readUtf8String();

    let rawIdentifier;
    if (path === '/usr/libexec/xpcproxy') {
      rawIdentifier = args[3].add(pointerSize).readPointer().readUtf8String();
    } else {
      rawIdentifier = tryParseXpcServiceName(env);
      if (rawIdentifier === null)
        return;
    }

    let identifier, event;
    if (rawIdentifier.startsWith('UIKitApplication:')) {
      identifier = rawIdentifier.substring(17, rawIdentifier.indexOf('['));
      if (!prewarm && upcoming.has(identifier))
        event = 'launch:app';
      else if (gating)
        event = 'spawn';
      else
        return;
    } else if (gating || (reportCrashes && crashServices.has(rawIdentifier))) {
      identifier = rawIdentifier;
      event = 'spawn';
    } else {
      return;
    }

    const attrs = args[2].add(pointerSize).readPointer();

    let flags = attrs.readU16();
    flags |= POSIX_SPAWN_START_SUSPENDED;
    attrs.writeU16(flags);

    this.event = event;
    this.path = path;
    this.identifier = identifier;
    this.pidPtr = args[0];
  },
  onLeave(retval) {
    const { event } = this;
    if (event === undefined)
      return;

    const { path, identifier, pidPtr, threadId } = this;

    if (event === 'launch:app')
      upcoming.delete(identifier);

    if (retval.toInt32() < 0)
      return;

    const pid = pidPtr.readU32();

    suspendedPids.add(pid);

    if (pidsToIgnore !== null)
      pidsToIgnore.add(pid);

    if (substrateInvocations.has(threadId)) {
      substratePidsPending.set(pid, notifyFridaBackend);
    } else {
      notifyFridaBackend();
    }

    function notifyFridaBackend() {
      send([event, path, identifier, pid]);
    }
  }
});

function parseStringv(p) {
  const strings = [];

  if (p.isNull())
    return [];

  let cur = p;
  while (true) {
    const elementPtr = cur.readPointer();
    if (elementPtr.isNull())
      break;

    const element = elementPtr.readUtf8String();
    strings.push(element);

    cur = cur.add(pointerSize);
  }

  return strings;
}

function isPrewarmLaunch(env) {
  return env.some(candidate => candidate.startsWith('ActivePrewarm='));
}

function tryParseXpcServiceName(env) {
  const entry = env.find(candidate => candidate.startsWith('XPC_SERVICE_NAME='));
  if (entry === undefined)
    return null;
  return entry.substring(17);
}

function applyJailbreakQuirks() {
  const jbdCallImpl = findJbdCallImpl();
  if (jbdCallImpl !== null) {
    pidsToIgnore = new Set();
    sabotageJbdCallForOurPids(jbdCallImpl);
    return;
  }

  const launcher = findSubstrateLauncher();
  if (launcher !== null) {
    instrumentSubstrateLauncher(launcher);
    return;
  }

  const inserterResume = findInserterResume();
  if (inserterResume !== null) {
    pidsToIgnore = new Set();
    instrumentInserter(inserterResume);
    return;
  }

  const ellekitGate = findEllekitShouldEnableTweaks();
  if (ellekitGate !== null) {
    pidsToIgnore = new Set();
    instrumentEllekitInjector(ellekitGate);
  }
}

function sabotageJbdCallForOurPids(jbdCallImpl) {
  const retType = 'int';
  const argTypes = ['uint', 'uint', 'uint'];

  const jbdCall = new NativeFunction(jbdCallImpl, retType, argTypes);

  Interceptor.replace(jbdCall, new NativeCallback((port, command, pid) => {
    if (pidsToIgnore.delete(pid))
      return 0;

    return jbdCall(port, command, pid);
  }, retType, argTypes));
}

function instrumentSubstrateLauncher(launcher) {
  Interceptor.attach(launcher.handlePosixSpawn, {
    onEnter() {
      substrateInvocations.add(this.threadId);
    },
    onLeave() {
      substrateInvocations.delete(this.threadId);
    }
  });

  Interceptor.attach(launcher.workerCont, {
    onEnter(args) {
      const baton = args[0];
      const pid = baton.readS32();

      const notify = substratePidsPending.get(pid);
      if (notify !== undefined) {
        substratePidsPending.delete(pid);

        const startSuspendedPtr = baton.add(4);
        startSuspendedPtr.writeU8(1);

        this.notify = notify;
      }
    },
    onLeave(retval) {
      const notify = this.notify;
      if (notify !== undefined)
        notify();
    },
  });
}

function instrumentInserter(at) {
  const original = new NativeFunction(at, 'int', ['uint', 'uint', 'uint', 'uint']);
  Interceptor.replace(at, new NativeCallback((a0, pid, a2, a3) => {
    if (pidsToIgnore.delete(pid))
      return 0;

    return original(a0, pid, a2, a3);
  }, 'int', ['uint', 'uint', 'uint', 'uint']));
}

function instrumentEllekitInjector(shouldEnableTweaks) {
  const retType = 'int';
  const argTypes = ['uint'];
  const original = new NativeFunction(shouldEnableTweaks, retType, argTypes);
  Interceptor.replace(shouldEnableTweaks, new NativeCallback(pid => {
    if (pidsToIgnore.delete(pid))
      return 0;

    return original(pid);
  }, retType, argTypes));
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

function findSubstrateLauncher() {
  if (Process.arch !== 'arm64')
    return null;

  const imp = Process.mainModule.enumerateImports().find(imp => imp.name === 'posix_spawn');
  if (imp === undefined)
    return null;
  const impl = imp.slot.readPointer().strip();
  const header = findClosestMachHeader(impl);

  const launcherDylibName = stringToHexPattern('Launcher.t.dylib');
  const isSubstrate = Memory.scanSync(header, 2048, launcherDylibName).length > 0;
  if (!isSubstrate)
    return null;

  const atvLauncherDylibName = stringToHexPattern('build.atv/Launcher.t.dylib');
  const isATVSubstrate = Memory.scanSync(header, 2048, atvLauncherDylibName).length > 0;

  return {
    handlePosixSpawn: resolveFunction('handlePosixSpawn',
      isATVSubstrate
      ? 'fc 6f ba a9 fa 67 01 a9 f8 5f 02 a9 f6 57 03 a9 f4 4f 04 a9 fd 7b 05 a9 fd 43 01 91 ff 83 02 d1 e6 1f 00 f9'
      : 'fd 7b bf a9 fd 03 00 91 f4 4f bf a9 f6 57 bf a9 f8 5f bf a9 fa 67 bf a9 fc 6f bf a9 ff 43 04 d1'),
    workerCont: resolveFunction('workerCont',
      isATVSubstrate
      ? 'f8 5f bc a9 f6 57 01 a9 f4 4f 02 a9 fd 7b 03 a9 fd c3 00 91 ff 83 00 d1 f3 03 00 aa c3 fc ff 97 f4 03 00 aa'
      : 'fd 7b bf a9 fd 03 00 91 f4 4f bf a9 f6 57 bf a9 f8 5f bf a9 fa 67 bf a9 fc 6f bf a9 ff 43 01 d1'),
  };

  function resolveFunction(name, signature) {
    const matches = Memory.scanSync(header, 37056, signature);
    if (matches.length !== 1) {
      throw new Error(`Unsupported version of Substrate; please file a bug: ${name} matched ${matches.length} times`);
    }
    return matches[0].address;
  }
}

function stringToHexPattern(str) {
  return str.split('').map(o => o.charCodeAt(0).toString(16)).join(' ');
}

function findClosestMachHeader(address) {
  let cur = address.and(ptr(4095).not());
  while (true) {
    if ((cur.readU32() & 0xfffffffe) >>> 0 === 0xfeedface)
      return cur;
    cur = cur.sub(4096);
  }
}

function findInserterResume() {
  const candidates = Process.enumerateModules().filter(x => x.name === 'substitute-inserter.dylib');
  if (candidates.length !== 1)
    return null;

  const { base, size } = candidates[0];
  const signature = 'e0 03 00 91 e1 07 00 32 82 05 80 52 83 05 80 52 05 00 80 52';

  const matches = Memory.scanSync(base, size, signature);
  if (matches.length !== 1)
    return null;

  let cursor = matches[0].address.sub(4);
  const end = cursor.sub(1024);
  while (cursor.compare(end) >= 0) {
    try {
      const instr = Instruction.parse(cursor);
      if (instr.mnemonic.startsWith('ret'))
        return cursor.add(4).sign();
    } catch (e) {
    }
    cursor = cursor.sub(4);
  }

  return null;
}

function findEllekitShouldEnableTweaks() {
  const impl = Module.findGlobalExportByName('should_enable_tweaks');
  if (impl !== null)
    return impl;

  for (const name of ['libinjector.dylib', 'TweakLoader.dylib']) {
    const mod = Process.findModuleByName(name);
    if (mod !== null) {
      const exp = mod.findExportByName('should_enable_tweaks');
      if (exp !== null)
        return exp;
    }
  }

  return null;
}
