from frida_tools.application import Reactor

import frida


class Application:
    def __init__(self):
        self._reactor = Reactor(run_until_return=self._process_input)

        self._device = None
        self._session = None

    def run(self):
        self._reactor.schedule(lambda: self._start())
        self._reactor.run()

    def _start(self):
        device = frida.get_remote_device()
        self._device = device

        session = self._device.attach("hello2", persist_timeout=30)
        self._session = session
        session.on("detached", lambda *args: self._reactor.schedule(lambda: self._on_detached(*args)))

        script = session.create_script("""
let _puts = null;

Interceptor.attach(DebugSymbol.getFunctionByName('f'), {
  onEnter(args) {
    const n = args[0].toInt32();
    send(n);
  }
});

rpc.exports.dispose = () => {
  puts('Script unloaded');
};

let serial = 1;
setInterval(() => {
  puts(`Agent still here! serial=${serial++}`);
}, 5000);

function puts(s) {
  if (_puts === null) {
    _puts = new NativeFunction(Module.getExportByName(null, 'puts'), 'int', ['pointer']);
  }
  _puts(Memory.allocUtf8String(s));
}
""")
        self._script = script
        script.on("message", lambda *args: self._reactor.schedule(lambda: self._on_message(*args)))
        script.load()

    def _process_input(self, reactor):
        while True:
            try:
                command = input("> ").strip()
            except:
                self._reactor.cancel_io()
                return

            if command == "resume":
                try:
                    self._session.resume()
                except Exception as e:
                    print("Failed to resume:", e)
            else:
                print("Unknown command")

    def _on_detached(self, reason, crash):
        print(f"⚡ detached: reason={reason}, crash={crash}")

    def _on_message(self, message, data):
        print(f"⚡ message: {message}")


app = Application()
app.run()
