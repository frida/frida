import json
import sys

from frida_tools.application import Reactor

import frida


class Application:
    def __init__(self, nick):
        self._reactor = Reactor(run_until_return=self._process_input)

        token = {"nick": nick, "secret": "knock-knock"}
        self._device = frida.get_device_manager().add_remote_device("::1", token=json.dumps(token))

        self._bus = self._device.bus
        self._bus.on("message", lambda *args: self._reactor.schedule(lambda: self._on_bus_message(*args)))

        self._channel = None
        self._prompt = "> "

    def run(self):
        self._reactor.schedule(self._start)
        self._reactor.run()

    def _start(self):
        self._bus.attach()

    def _process_input(self, reactor):
        while True:
            sys.stdout.write("\r")
            try:
                text = input(self._prompt).strip()
            except:
                self._reactor.cancel_io()
                return
            sys.stdout.write("\033[1A\033[K")
            sys.stdout.flush()

            if len(text) == 0:
                self._print("Processes:", self._device.enumerate_processes())
                continue

            if text.startswith("/join "):
                if self._channel is not None:
                    self._bus.post({"type": "part", "channel": self._channel})
                channel = text[6:]
                self._channel = channel
                self._prompt = f"{channel} > "
                self._bus.post({"type": "join", "channel": channel})
                continue

            if text.startswith("/announce "):
                self._bus.post({"type": "announce", "text": text[10:]})
                continue

            if self._channel is not None:
                self._bus.post({"channel": self._channel, "type": "say", "text": text})
            else:
                self._print("*** Need to /join a channel first")

    def _on_bus_message(self, message, data):
        mtype = message["type"]
        if mtype == "welcome":
            self._print("*** Welcome! Available channels:", repr(message["channels"]))
        elif mtype == "membership":
            self._print("*** Joined", message["channel"])
            self._print(
                "- Members:\n\t"
                + "\n\t".join([f"{m['nick']} (connected from {m['address']})" for m in message["members"]])
            )
            for item in message["history"]:
                self._print(f"<{item['sender']}> {item['text']}")
        elif mtype == "join":
            user = message["user"]
            self._print(f"👋 {user['nick']} ({user['address']}) joined {message['channel']}")
        elif mtype == "part":
            user = message["user"]
            self._print(f"🚪 {user['nick']} ({user['address']}) left {message['channel']}")
        elif mtype == "chat":
            self._print(f"<{message['sender']}> {message['text']}")
        elif mtype == "announce":
            self._print(f"📣 <{message['sender']}> {message['text']}")
        else:
            self._print("Unhandled message:", message)

    def _print(self, *words):
        print("\r\033[K" + " ".join([str(word) for word in words]))
        sys.stdout.write(self._prompt)
        sys.stdout.flush()


if __name__ == "__main__":
    nick = sys.argv[1]
    app = Application(nick)
    app.run()
