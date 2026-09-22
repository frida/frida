import hashlib
import hmac
import json
from pathlib import Path

from frida_tools.application import Reactor

import frida

ENABLE_CONTROL_INTERFACE = True


class Application:
    def __init__(self):
        self._reactor = Reactor(run_until_return=self._process_input)

        cluster_params = frida.EndpointParameters(
            address="unix:/Users/oleavr/src/cluster",
            certificate="/Users/oleavr/src/identity2.pem",
            authentication=("token", "wow-such-secret"),
        )

        if ENABLE_CONTROL_INTERFACE:
            www = Path(__file__).parent.resolve() / "web_client" / "dist"
            control_params = frida.EndpointParameters(
                address="::1", port=27042, authentication=("callback", self._authenticate), asset_root=www
            )
        else:
            control_params = None

        service = frida.PortalService(cluster_params, control_params)
        self._service = service
        self._device = service.device
        self._peers = {}
        self._nicks = set()
        self._channels = {}

        service.on("node-connected", lambda *args: self._reactor.schedule(lambda: self._on_node_connected(*args)))
        service.on("node-joined", lambda *args: self._reactor.schedule(lambda: self._on_node_joined(*args)))
        service.on("node-left", lambda *args: self._reactor.schedule(lambda: self._on_node_left(*args)))
        service.on("node-disconnected", lambda *args: self._reactor.schedule(lambda: self._on_node_disconnected(*args)))
        service.on(
            "controller-connected", lambda *args: self._reactor.schedule(lambda: self._on_controller_connected(*args))
        )
        service.on(
            "controller-disconnected",
            lambda *args: self._reactor.schedule(lambda: self._on_controller_disconnected(*args)),
        )
        service.on("authenticated", lambda *args: self._reactor.schedule(lambda: self._on_authenticated(*args)))
        service.on("subscribe", lambda *args: self._reactor.schedule(lambda: self._on_subscribe(*args)))
        service.on("message", lambda *args: self._reactor.schedule(lambda: self._on_message(*args)))

    def run(self):
        self._reactor.schedule(self._start)
        self._reactor.run()

    def _start(self):
        self._service.start()

        self._device.enable_spawn_gating()

    def _stop(self):
        self._service.stop()

    def _process_input(self, reactor):
        while True:
            try:
                command = input("Enter command: ").strip()
            except KeyboardInterrupt:
                self._reactor.cancel_io()
                return

            if len(command) == 0:
                print("Processes:", self._device.enumerate_processes())
                continue

            if command == "stop":
                self._reactor.schedule(self._stop)
                break

    def _authenticate(self, raw_token):
        try:
            token = json.loads(raw_token)
            nick = str(token["nick"])
            secret = token["secret"].encode("utf-8")
        except:
            raise ValueError("invalid request")

        provided = hashlib.sha1(secret).digest()
        expected = hashlib.sha1(b"knock-knock").digest()
        if not hmac.compare_digest(provided, expected):
            raise ValueError("get outta here")

        return {
            "nick": nick,
        }

    def _on_node_connected(self, connection_id, remote_address):
        print("on_node_connected()", connection_id, remote_address)

    def _on_node_joined(self, connection_id, application):
        print("on_node_joined()", connection_id, application)
        print("\ttags:", self._service.enumerate_tags(connection_id))

    def _on_node_left(self, connection_id, application):
        print("on_node_left()", connection_id, application)

    def _on_node_disconnected(self, connection_id, remote_address):
        print("on_node_disconnected()", connection_id, remote_address)

    def _on_controller_connected(self, connection_id, remote_address):
        print("on_controller_connected()", connection_id, remote_address)
        self._peers[connection_id] = Peer(connection_id, remote_address)

    def _on_controller_disconnected(self, connection_id, remote_address):
        print("on_controller_disconnected()", connection_id, remote_address)
        peer = self._peers.pop(connection_id)
        for channel in list(peer.memberships):
            channel.remove_member(peer)
        if peer.nick is not None:
            self._release_nick(peer.nick)

    def _on_authenticated(self, connection_id, session_info):
        print("on_authenticated()", connection_id, session_info)
        peer = self._peers.get(connection_id, None)
        if peer is None:
            return
        peer.nick = self._acquire_nick(session_info["nick"])

    def _on_subscribe(self, connection_id):
        print("on_subscribe()", connection_id)
        self._service.post(connection_id, {"type": "welcome", "channels": list(self._channels.keys())})

    def _on_message(self, connection_id, message, data):
        peer = self._peers[connection_id]

        mtype = message["type"]
        if mtype == "join":
            self._get_channel(message["channel"]).add_member(peer)
        elif mtype == "part":
            channel = self._channels.get(message["channel"], None)
            if channel is None:
                return
            channel.remove_member(peer)
        elif mtype == "say":
            channel = self._channels.get(message["channel"], None)
            if channel is None:
                return
            channel.post(message["text"], peer)
        elif mtype == "announce":
            self._service.broadcast({"type": "announce", "sender": peer.nick, "text": message["text"]})
        else:
            print("Unhandled message:", message)

    def _acquire_nick(self, requested):
        candidate = requested
        serial = 2
        while candidate in self._nicks:
            candidate = requested + str(serial)
            serial += 1

        nick = candidate
        self._nicks.add(nick)

        return nick

    def _release_nick(self, nick):
        self._nicks.remove(nick)

    def _get_channel(self, name):
        channel = self._channels.get(name, None)
        if channel is None:
            channel = Channel(name, self._service)
            self._channels[name] = channel
        return channel


class Peer:
    def __init__(self, connection_id, remote_address):
        self.nick = None
        self.connection_id = connection_id
        self.remote_address = remote_address
        self.memberships = set()

    def to_json(self):
        return {"nick": self.nick, "address": self.remote_address[0]}


class Channel:
    def __init__(self, name, service):
        self.name = name
        self.members = set()
        self.history = []

        self._service = service

    def add_member(self, peer):
        if self in peer.memberships:
            return

        peer.memberships.add(self)
        self.members.add(peer)

        self._service.narrowcast(self.name, {"type": "join", "channel": self.name, "user": peer.to_json()})
        self._service.tag(peer.connection_id, self.name)

        self._service.post(
            peer.connection_id,
            {
                "type": "membership",
                "channel": self.name,
                "members": [peer.to_json() for peer in self.members],
                "history": self.history,
            },
        )

    def remove_member(self, peer):
        if self not in peer.memberships:
            return

        peer.memberships.remove(self)
        self.members.remove(peer)

        self._service.untag(peer.connection_id, self.name)
        self._service.narrowcast(self.name, {"type": "part", "channel": self.name, "user": peer.to_json()})

    def post(self, text, peer):
        if self not in peer.memberships:
            return

        item = {"type": "chat", "sender": peer.nick, "text": text}

        self._service.narrowcast(self.name, item)

        history = self.history
        history.append(item)
        if len(history) == 20:
            history.pop(0)


if __name__ == "__main__":
    app = Application()
    app.run()
