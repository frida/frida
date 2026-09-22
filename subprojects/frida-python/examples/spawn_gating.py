import threading

from frida_tools.application import Reactor

import frida


class Application:
    def __init__(self):
        self._stop_requested = threading.Event()
        self._reactor = Reactor(run_until_return=lambda reactor: self._stop_requested.wait())

        self._device = frida.get_usb_device()
        self._sessions = set()
        self._sessions_lock = threading.Lock()

        self._device.on("spawn-added", lambda spawn: self._reactor.schedule(lambda: self._on_spawn_added(spawn)))
        self._device.on("spawn-removed", lambda spawn: self._reactor.schedule(lambda: self._on_spawn_removed(spawn)))

    def run(self):
        self._reactor.schedule(lambda: self._start())
        self._reactor.run()

    def _start(self):
        self._device.enable_spawn_gating()

    def _instrument(self, pid):
        print(f"✔ attach(pid={pid})")
        session = self._device.attach(pid)
        session.on("detached", lambda reason: self._reactor.schedule(lambda: self._on_detached(pid, session, reason)))
        print("✔ create_script()")
        script = session.create_script("""\
const puts = new NativeFunction(Module.getGlobalExportByName('puts'), 'int', ['pointer']);
puts(Memory.allocUtf8String('Hello from Frida agent'));
Interceptor.attach(Module.getGlobalExportByName('open'), {
  onEnter(args) {
    send({
      type: 'open',
      path: args[0].readUtf8String()
    });
  }
});
""")
        script.on("message", lambda message, data: self._reactor.schedule(lambda: self._on_message(pid, message)))
        print("✔ load()")
        script.load()
        print(f"✔ resume(pid={pid})")
        self._device.resume(pid)
        with self._sessions_lock:
            self._sessions.add(session)

    def _on_spawn_added(self, spawn):
        print(f"⚡ spawn_added: {spawn}")
        t = threading.Thread(target=self._handle_spawn, args=(spawn,))
        t.start()

    def _handle_spawn(self, spawn):
        if "/bin/ls" in spawn.identifier:
            self._instrument(spawn.pid)
        else:
            pid = spawn.pid
            print(f"✔ resume(pid={pid})")
            self._device.resume(pid)

    def _on_spawn_removed(self, spawn):
        print(f"⚡ spawn_removed: {spawn}")

    def _on_detached(self, pid, session, reason):
        print(f"⚡ detached: pid={pid}, reason='{reason}'")
        with self._sessions_lock:
            self._sessions.remove(session)

    def _on_message(self, pid, message):
        print(f"⚡ message: pid={pid}, payload={message['payload']}")


app = Application()
app.run()
