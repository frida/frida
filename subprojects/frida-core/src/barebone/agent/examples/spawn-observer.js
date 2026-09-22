const _copyinstr = new NativeFunction(
    DebugSymbol.getFunctionByName('copyinstr'),
  'int',
  ['pointer', 'pointer', 'size_t', 'pointer']);
const { pointerSize } = Process;

Interceptor.attach(DebugSymbol.getFunctionByName('__mac_execve'), {
  onEnter(args) {
    const fname = copyinstr(args[1].readPointer());
    console.log(`>>> __mac_execve() fname=${fname}`);
  },
  onLeave(retval) {
    console.log(`<<< __mac_execve() => ${retval.toInt32()}`);
  }
});

Interceptor.attach(DebugSymbol.getFunctionByName('posix_spawn'), {
  onEnter(args) {
    const path = copyinstr(args[1].add(pointerSize).readPointer());
    console.log(`>>> posix_spawn() path=${path}`);
  },
  onLeave(retval) {
    console.log(`<<< posix_spawn() => ${retval.toInt32()}`);
  }
});

function copyinstr(userAddr) {
  const bufSize = 4096;
  const buf = Memory.alloc(bufSize);
  const actual = Memory.alloc(pointerSize);
  if (_copyinstr(userAddr, buf, bufSize, actual) !== 0)
    throw new Error('copyinstr failed');
  return buf.readUtf8String();
}
