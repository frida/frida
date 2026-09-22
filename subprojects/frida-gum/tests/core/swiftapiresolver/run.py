import frida
from frida_tools.application import Reactor
from pathlib import Path
import subprocess
import sys
import threading
import time


class Controller:
    def __init__(self):
        self._stop_requested = threading.Event()
        self._reactor = Reactor(run_until_return=lambda reactor: self._stop_requested.wait())

        runner_src_dir = Path(__file__).parent
        self._runner_js = runner_src_dir / "runner.js"
        self._runner_dylib = runner_src_dir.parent.parent.parent.parent / "build" / "tmp-macos-arm64" / "frida-gum" / "tests" / "core" / "swiftapiresolver" / "libtestswiftapiresolver.dylib"

        self._device = None
        self._session = None
        self._script = None

    def run(self):
        self._reactor.schedule(lambda: self._start())
        self._reactor.run()

    def _start(self):
        device = frida.get_remote_device()
        self._device = device

        session = device.attach("Xcode")
        session.on("detached", lambda reason: self._reactor.schedule(lambda: self._on_detached(reason)))
        self._session = session

        script = session.create_script(self._runner_js.read_text(encoding="utf-8"))
        script.on("message", lambda message, data: self._reactor.schedule(lambda: self._on_message(message, data)))
        script.load()
        self._script = script

        script.post({ "type": "start" }, self._runner_dylib.read_bytes())

        worker = threading.Thread(target=self._run_tests)
        worker.start()

    def _run_tests(self):
        print("Running...")
        t1 = time.time()
        num_matches = self._script.exports_sync.run("functions:*!*")
        t2 = time.time()
        duration = int((t2 - t1) * 1000)
        print(f"Got {num_matches} matches in {duration} ms.")
        self._stop_requested.set()

    def _on_detached(self, reason):
        print(f"⚡ detached: reason='{reason}'")
        self._script = None
        self._session = None
        self._stop_requested.set()

    def _on_message(self, message, data):
        handled = False
        if message["type"] == "send":
            payload = message["payload"]
            if payload["type"] == "ready":
                self._on_ready(payload["symbols"])
                handled = True
        if not handled:
            print(f"⚡ message: payload={message['payload']}")

    def _on_ready(self, symbols):
        for line in subprocess.run(["nm", self._runner_dylib], capture_output=True, encoding="utf-8").stdout.split("\n"):
            if line.endswith(" T _init"):
                tokens = line.split(" ")
                init_rva = int(tokens[0], 16)
                runner_base = int(symbols["init"], 16) - init_rva
                print(f"Runner is loaded at 0x{runner_base:x}")


controller = Controller()
controller.run()
