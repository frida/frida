import threading

from frida_tools.application import Reactor

import frida


class Application:
    def __init__(self):
        self._stop_requested = threading.Event()
        self._reactor = Reactor(run_until_return=lambda reactor: self._stop_requested.wait())

        self._device = frida.get_local_device()
        self._sessions = set()

        self._device.on("child-added", lambda child: self._reactor.schedule(lambda: self._on_child_added(child)))
        self._device.on("child-removed", lambda child: self._reactor.schedule(lambda: self._on_child_removed(child)))
        self._device.on("output", lambda pid, fd, data: self._reactor.schedule(lambda: self._on_output(pid, fd, data)))

    def run(self):
        self._reactor.schedule(lambda: self._start())
        self._reactor.run()

    def _start(self):
        argv = ["/bin/sh", "-c", "cat /etc/hosts"]
        env = {
            "BADGER": "badger-badger-badger",
            "SNAKE": "mushroom-mushroom",
        }
        print(f"✔ spawn(argv={argv})")
        pid = self._device.spawn(argv, env=env, stdio="pipe")
        self._instrument(pid)

    def _stop_if_idle(self):
        if len(self._sessions) == 0:
            self._stop_requested.set()

    def _instrument(self, pid):
        print(f"✔ attach(pid={pid})")
        session = self._device.attach(pid)
        session.on("detached", lambda reason: self._reactor.schedule(lambda: self._on_detached(pid, session, reason)))
        print("✔ enable_child_gating()")
        session.enable_child_gating()
        print("✔ create_script()")
        script = session.create_script("""\
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
        self._sessions.add(session)

    def _on_child_added(self, child):
        print(f"⚡ child_added: {child}")
        self._instrument(child.pid)

    def _on_child_removed(self, child):
        print(f"⚡ child_removed: {child}")

    def _on_output(self, pid, fd, data):
        print(f"⚡ output: pid={pid}, fd={fd}, data={repr(data)}")

    def _on_detached(self, pid, session, reason):
        print(f"⚡ detached: pid={pid}, reason='{reason}'")
        self._sessions.remove(session)
        self._reactor.schedule(self._stop_if_idle, delay=0.5)

    def _on_message(self, pid, message):
        print(f"⚡ message: pid={pid}, payload={message['payload']}")


app = Application()
app.run()
