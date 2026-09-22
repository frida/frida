const libsystem = Process.getModuleByName('libSystem.B.dylib');
const sleep = libsystem.getExportByName((Process.arch === 'ia32') ? 'sleep$UNIX2003' : 'sleep');
const exit = new NativeFunction(libsystem.getExportByName('exit'), 'void', ['int']);

rpc.exports = {
  init() {
    try {
      Interceptor.attach(sleep, {
        onEnter() {
          exit(123);
        }
      });
    } catch (e) {
      console.error(e.message);
    }
  }
};
