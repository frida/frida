from __future__ import annotations

import argparse
import asyncio
import binascii
import codecs
import email.utils
import gzip
import http
import mimetypes
import re
import shlex
import subprocess
import threading
from collections import OrderedDict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Generator, List, Mapping, Optional, Set
from zipfile import ZipFile

import frida
import websockets.asyncio.server
import websockets.datastructures
import websockets.exceptions
import websockets.http11

from frida_tools.reactor import Reactor

MANPAGE_CONTROL_CHARS = re.compile(r"\.[a-zA-Z]*(\s|$)|\s?\"")
MANPAGE_FUNCTION_PROTOTYPE = re.compile(r"([a-zA-Z_]\w+)\(([^\)]+)")


def main() -> None:
    import json
    import traceback

    from colorama import Fore, Style

    from frida_tools.application import ConsoleApplication, await_ctrl_c

    class TracerApplication(ConsoleApplication, UI):
        def __init__(self) -> None:
            super().__init__(await_ctrl_c)
            self._handlers = OrderedDict()
            self._ui_zip = ZipFile(Path(__file__).parent / "tracer_ui.zip", "r")
            self._ui_socket_handlers: Set[UISocketHandler] = set()
            self._ui_worker = None
            self._asyncio_loop = None
            self._palette = ["cyan", "magenta", "yellow", "green", "red", "blue"]
            self._next_color = 0
            self._style_by_thread_id = {}
            self._last_event_tid = -1

        def _add_options(self, parser: argparse.ArgumentParser) -> None:
            pb = TracerProfileBuilder()
            parser.add_argument(
                "-I", "--include-module", help="include MODULE", metavar="MODULE", type=pb.include_modules
            )
            parser.add_argument(
                "-X", "--exclude-module", help="exclude MODULE", metavar="MODULE", type=pb.exclude_modules
            )
            parser.add_argument(
                "-i", "--include", help="include [MODULE!]FUNCTION", metavar="FUNCTION", type=pb.include
            )
            parser.add_argument(
                "-x", "--exclude", help="exclude [MODULE!]FUNCTION", metavar="FUNCTION", type=pb.exclude
            )
            parser.add_argument(
                "-a", "--add", help="add MODULE!OFFSET", metavar="MODULE!OFFSET", type=pb.include_relative_address
            )
            parser.add_argument("-T", "--include-imports", help="include program's imports", type=pb.include_imports)
            parser.add_argument(
                "-t",
                "--include-module-imports",
                help="include MODULE imports",
                metavar="MODULE",
                type=pb.include_imports,
            )
            parser.add_argument(
                "-m",
                "--include-objc-method",
                help="include OBJC_METHOD",
                metavar="OBJC_METHOD",
                type=pb.include_objc_method,
            )
            parser.add_argument(
                "-M",
                "--exclude-objc-method",
                help="exclude OBJC_METHOD",
                metavar="OBJC_METHOD",
                type=pb.exclude_objc_method,
            )
            parser.add_argument(
                "-y",
                "--include-swift-func",
                help="include SWIFT_FUNC",
                metavar="SWIFT_FUNC",
                type=pb.include_swift_func,
            )
            parser.add_argument(
                "-Y",
                "--exclude-swift-func",
                help="exclude SWIFT_FUNC",
                metavar="SWIFT_FUNC",
                type=pb.exclude_swift_func,
            )
            parser.add_argument(
                "-j",
                "--include-java-method",
                help="include JAVA_METHOD",
                metavar="JAVA_METHOD",
                type=pb.include_java_method,
            )
            parser.add_argument(
                "-J",
                "--exclude-java-method",
                help="exclude JAVA_METHOD",
                metavar="JAVA_METHOD",
                type=pb.exclude_java_method,
            )
            parser.add_argument(
                "-s",
                "--include-debug-symbol",
                help="include DEBUG_SYMBOL",
                metavar="DEBUG_SYMBOL",
                type=pb.include_debug_symbol,
            )
            parser.add_argument(
                "-q", "--quiet", help="do not format output messages", action="store_true", default=False
            )
            parser.add_argument(
                "-d",
                "--decorate",
                help="add module name to generated onEnter log statement",
                action="store_true",
                default=False,
            )
            parser.add_argument(
                "-S",
                "--init-session",
                help="path to JavaScript file used to initialize the session",
                metavar="PATH",
                action="append",
                default=[],
            )
            parser.add_argument(
                "-P",
                "--parameters",
                help="parameters as JSON, exposed as a global named 'parameters'",
                metavar="PARAMETERS_JSON",
            )
            parser.add_argument("-o", "--output", help="dump messages to file", metavar="OUTPUT")
            parser.add_argument(
                "--ui-host", help="the host to serve the UI on (default localhost)", default="localhost"
            )
            parser.add_argument("--ui-port", help="the TCP port to serve the UI on")
            parser.add_argument(
                "--ui-allow-origin",
                help="allow browser requests from ORIGIN; may be specified multiple times",
                metavar="ORIGIN",
                action="append",
                default=[],
            )
            self._profile_builder = pb

        def _usage(self) -> str:
            return "%(prog)s [options] target"

        def _initialize(self, parser: argparse.ArgumentParser, options: argparse.Namespace, args: List[str]) -> None:
            self._repo: Optional[FileRepository] = None
            self._tracer: Optional[Tracer] = None
            self._profile = self._profile_builder.build()
            self._quiet: bool = options.quiet
            self._decorate: bool = options.decorate
            self._output: Optional[OutputFile] = None
            self._output_path: str = options.output
            self._ui_host: Optional[str] = options.ui_host
            self._ui_port: Optional[int] = options.ui_port
            self._ui_allowed_origins: List[str] = options.ui_allow_origin

            self._init_scripts = []
            for path in options.init_session:
                with codecs.open(path, "rb", "utf-8") as f:
                    source = f.read()
                self._init_scripts.append(InitScript(path, source))

            if options.parameters is not None:
                try:
                    params = json.loads(options.parameters)
                except Exception as e:
                    raise ValueError(f"failed to parse parameters argument as JSON: {e}")
                if not isinstance(params, dict):
                    raise ValueError("failed to parse parameters argument as JSON: not an object")
                self._parameters = params
            else:
                self._parameters = {}

        def _needs_target(self) -> bool:
            return True

        def _start(self) -> None:
            if self._ui_worker is None:
                worker = threading.Thread(target=self._run_ui_server, name="ui-server", daemon=True)
                worker.start()
                self._ui_worker = worker

            if self._output_path is not None:
                self._output = OutputFile(self._output_path)

            stage = "early" if self._target[0] == "file" else "late"

            self._repo = FileRepository(self._reactor, self._decorate)
            self._tracer = Tracer(
                self._reactor,
                self._repo,
                self._profile,
                self._init_scripts,
                log_handler=self._log,
            )
            try:
                self._tracer.start_trace(self._session, stage, self._parameters, self._runtime, self)
            except Exception as e:
                self._update_status(f"Failed to start tracing: {e}")
                self._exit(1)
                return

        def _stop(self) -> None:
            self._tracer.stop()
            self._tracer = None
            if self._output is not None:
                self._output.close()
            self._output = None

            self._handlers.clear()
            self._next_color = 0
            self._style_by_thread_id.clear()
            self._last_event_tid = -1

        def on_script_created(self, script: frida.core.Script) -> None:
            self._on_script_created(script)

        def on_trace_progress(self, status: str, *params) -> None:
            if status == "initializing":
                self._update_status("Instrumenting...")
            elif status == "initialized":
                self._resume()
            elif status == "started":
                (count,) = params
                if count == 1:
                    plural = ""
                else:
                    plural = "s"
                self._update_status(
                    f"Started tracing {count} function{plural}. Web UI available at http://localhost:{self._ui_port}/"
                )
                self._print(
                    "Tip: Luma, the official Frida app, has frida-trace built in — with a Monaco "
                    "handler editor and instruction-level tracing you can diff across runs. "
                    "https://luma.frida.re/"
                )

        def on_trace_warning(self, message: str) -> None:
            self._print(Fore.RED + Style.BRIGHT + "Warning" + Style.RESET_ALL + ": " + message)

        def on_trace_error(self, message: str) -> None:
            self._print(Fore.RED + Style.BRIGHT + "Error" + Style.RESET_ALL + ": " + message)
            self._exit(1)

        def on_trace_events(self, raw_events) -> None:
            events = [
                (target_id, timestamp, thread_id, depth, caller, backtrace, message, self._get_style(thread_id))
                for target_id, timestamp, thread_id, depth, caller, backtrace, message in raw_events
            ]
            self._asyncio_loop.call_soon_threadsafe(
                lambda: self._asyncio_loop.create_task(self._broadcast_trace_events(events))
            )

            no_attributes = Style.RESET_ALL
            for target_id, timestamp, thread_id, depth, caller, backtrace, message, style in events:
                if self._output is not None:
                    self._output.append(message + "\n")
                elif self._quiet:
                    self._print(message)
                else:
                    indent = depth * "   | "
                    attributes = getattr(Fore, style[0].upper())
                    if len(style) > 1:
                        attributes += getattr(Style, style[1].upper())
                    if thread_id != self._last_event_tid:
                        self._print("%s           /* TID 0x%x */%s" % (attributes, thread_id, Style.RESET_ALL))
                        self._last_event_tid = thread_id
                    self._print("%6d ms  %s%s%s%s" % (timestamp, attributes, indent, message, no_attributes))

        def on_trace_handler_create(self, target: TraceTarget, handler: str, source: Path) -> None:
            self._register_handler(target, source)
            if self._quiet:
                return
            self._print(f'{target}: Auto-generated handler at "{source}"')

        def on_trace_handler_load(self, target: TraceTarget, handler: str, source: Path) -> None:
            self._register_handler(target, source)
            if self._quiet:
                return
            self._print(f'{target}: Loaded handler at "{source}"')

        def _register_handler(self, target: TraceTarget, source: str) -> None:
            config = {"muted": False, "capture_backtraces": False}
            self._handlers[target.identifier] = (target, source, config)

        def _get_style(self, thread_id):
            style = self._style_by_thread_id.get(thread_id, None)
            if style is None:
                color = self._next_color
                self._next_color += 1
                style = [self._palette[color % len(self._palette)]]
                if (1 + int(color / len(self._palette))) % 2 == 0:
                    style.append("bright")
                self._style_by_thread_id[thread_id] = style
            return style

        def _run_ui_server(self):
            asyncio.run(self._handle_ui_requests())

        async def _handle_ui_requests(self):
            self._asyncio_loop = asyncio.get_running_loop()
            async with websockets.asyncio.server.serve(
                self._handle_websocket_connection,
                self._ui_host,
                self._ui_port,
                process_request=self._handle_asset_request,
            ) as server:
                self._ui_port = server.sockets[0].getsockname()[1]
                await asyncio.get_running_loop().create_future()

        async def _handle_websocket_connection(self, websocket: websockets.asyncio.server.ServerConnection):
            if self._tracer is None:
                return

            handler = UISocketHandler(self, websocket)
            self._ui_socket_handlers.add(handler)
            try:
                await handler.process_messages()
            except:
                traceback.print_exc()
                # pass
            finally:
                self._ui_socket_handlers.remove(handler)

        async def _broadcast_trace_events(self, events):
            for handler in self._ui_socket_handlers:
                await handler.post(
                    {
                        "type": "events:add",
                        "events": events,
                    }
                )

        def _handle_asset_request(
            self, connection: websockets.asyncio.server.ServerConnection, request: websockets.asyncio.server.Request
        ):
            origin = request.headers.get("Origin")
            if origin is not None and origin not in self._compute_allowed_ui_origins():
                self._print(
                    Fore.RED
                    + Style.BRIGHT
                    + "Warning"
                    + Style.RESET_ALL
                    + f": Cross-origin request from {origin} denied"
                )
                return connection.respond(http.HTTPStatus.FORBIDDEN, "Cross-origin request denied\n")

            connhdr = request.headers.get("Connection")
            if connhdr is not None:
                directives = [d.strip().lower() for d in connhdr.split(",")]
                if "upgrade" in directives:
                    return

            raw_path = request.path.split("?", maxsplit=1)[0]

            filename = raw_path[1:]
            if filename == "":
                filename = "index.html"

            try:
                body = self._ui_zip.read(filename)
            except KeyError:
                return connection.respond(http.HTTPStatus.NOT_FOUND, "File not found\n")

            status = http.HTTPStatus(http.HTTPStatus.OK)

            content_type, content_encoding = mimetypes.guess_type(filename)
            if content_type is None:
                content_type = "application/octet-stream"

            headers = websockets.datastructures.Headers(
                [
                    ("Connection", "close"),
                    ("Content-Length", str(len(body))),
                    ("Content-Type", content_type),
                    ("Date", email.utils.formatdate(usegmt=True)),
                ]
            )
            if content_encoding is not None:
                headers.update({"Content-Encoding": content_encoding})

            response = websockets.http11.Response(status.value, status.phrase, headers, body)
            connection.protocol.handshake_exc = websockets.exceptions.InvalidStatus(response)

            return response

        def _compute_allowed_ui_origins(self):
            return compute_allowed_ui_origins(self._ui_host, self._ui_port, self._ui_allowed_origins)

    class UISocketHandler:
        def __init__(self, app: TracerApplication, socket: websockets.asyncio.server.ServerConnection) -> None:
            self.app = app
            self.socket = socket

        async def process_messages(self) -> None:
            app = self.app

            await self.post(
                {
                    "type": "tracer:sync",
                    "spawned_program": app._spawned_argv[0] if app._spawned_argv is not None else None,
                    "process": app._tracer.process,
                    "handlers": [self._handler_entry_to_json(entry) for entry in app._handlers.values()],
                }
            )

            while True:
                request = json.loads(await self.socket.recv())
                request_id = request.get("id")

                try:
                    handle_request = getattr(self, "_on_" + request["type"].replace(":", "_").replace("-", "_"), None)
                    if handle_request is None:
                        raise NameError("unsupported request type")
                    result = await handle_request(request["payload"])
                except Exception as e:
                    if request_id is not None:
                        await self.post(
                            {
                                "type": "request:error",
                                "id": request_id,
                                "payload": {
                                    "message": str(e),
                                    "stack": traceback.format_exc(),
                                },
                            }
                        )
                    continue

                if request_id is not None:
                    await self.post({"type": "request:result", "id": request_id, "payload": result})

        async def post(self, message: dict) -> None:
            await self.socket.send(json.dumps(message))

        async def _on_tracer_respawn(self, _: dict) -> None:
            self.app._reactor.schedule(self.app._respawn)

        async def _on_handler_load(self, payload: dict) -> None:
            target, source, config = self.app._handlers[payload["id"]]
            return {"code": self.app._repo.ensure_handler(target), "config": config}

        async def _on_handler_save(self, payload: dict) -> None:
            target, _, _ = self.app._handlers[payload["id"]]
            self.app._repo.update_handler(target, payload["code"])

        async def _on_handler_configure(self, payload: dict) -> None:
            identifier = payload["id"]
            _, _, config = self.app._handlers[identifier]
            for k, v in payload["parameters"].items():
                config[k] = v
            self.app._tracer.update_handler_config(identifier, config)

        async def _on_targets_stage(self, payload: dict) -> None:
            profile = TracerProfile(list(map(tuple, payload["profile"]["spec"])))
            items = self.app._tracer.stage_targets(profile)
            return {
                "items": items,
            }

        async def _on_targets_commit(self, payload: dict) -> None:
            result = self.app._tracer.commit_targets(payload["id"])
            target_ids = result["ids"]

            await self.post(
                {
                    "type": "handlers:add",
                    "handlers": [
                        self._handler_entry_to_json(self.app._handlers[target_id]) for target_id in target_ids
                    ],
                }
            )

            return result

        async def _on_memory_read(self, payload: dict) -> None:
            data = self.app._tracer.read_memory(payload["address"], payload["size"])
            return list(data) if data is not None else None

        async def _on_symbols_resolve_addresses(self, payload: dict) -> None:
            names = self.app._tracer.resolve_addresses(payload["addresses"])
            return {"names": names}

        @staticmethod
        def _handler_entry_to_json(entry: tuple[str, str, str]) -> dict:
            target, _source, config = entry
            return {**target.to_json(), "config": config}

    app = TracerApplication()
    app.run()


def compute_allowed_ui_origins(
    ui_host: Optional[str], ui_port: Optional[int], extra_allowed_origins: Optional[List[str]] = None
) -> List[str]:
    if ui_host is None or ui_port is None:
        return list(extra_allowed_origins or [])

    origins = []
    for host in compute_ui_origin_hosts(ui_host):
        for port in (ui_port, ui_port + 1):
            origins.append(f"http://{host}:{port}")

    if extra_allowed_origins is not None:
        origins.extend(extra_allowed_origins)

    return list(OrderedDict.fromkeys(origins))


def compute_ui_origin_hosts(ui_host: str) -> List[str]:
    if ui_host in ("0.0.0.0", "::", ""):
        return [ui_host, "localhost", "127.0.0.1"]

    return [ui_host]


class TracerProfileBuilder:
    def __init__(self) -> None:
        self._spec = []

    def include_modules(self, *module_name_globs: str) -> "TracerProfileBuilder":
        for m in module_name_globs:
            self._spec.append(("include", "module", m))
        return self

    def exclude_modules(self, *module_name_globs: str) -> "TracerProfileBuilder":
        for m in module_name_globs:
            self._spec.append(("exclude", "module", m))
        return self

    def include(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("include", "function", f))
        return self

    def exclude(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("exclude", "function", f))
        return self

    def include_relative_address(self, *address_rel_offsets: str) -> "TracerProfileBuilder":
        for f in address_rel_offsets:
            self._spec.append(("include", "relative-function", f))
        return self

    def include_imports(self, *module_name_globs: str) -> "TracerProfileBuilder":
        for m in module_name_globs:
            self._spec.append(("include", "imports", m))
        return self

    def include_objc_method(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("include", "objc-method", f))
        return self

    def exclude_objc_method(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("exclude", "objc-method", f))
        return self

    def include_swift_func(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("include", "swift-func", f))
        return self

    def exclude_swift_func(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("exclude", "swift-func", f))
        return self

    def include_java_method(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("include", "java-method", f))
        return self

    def exclude_java_method(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("exclude", "java-method", f))
        return self

    def include_debug_symbol(self, *function_name_globs: str) -> "TracerProfileBuilder":
        for f in function_name_globs:
            self._spec.append(("include", "debug-symbol", f))
        return self

    def build(self) -> "TracerProfile":
        return TracerProfile(self._spec)


class TracerProfile:
    def __init__(self, spec) -> None:
        self.spec = spec


class Tracer:
    def __init__(
        self,
        reactor: Reactor,
        repository: "Repository",
        profile: TracerProfile,
        init_scripts=[],
        log_handler: Callable[[str, str], None] = None,
    ) -> None:
        self.main_module = None
        self._reactor = reactor
        self._repository = repository
        self._profile = profile
        self._script: Optional[frida.core.Script] = None
        self._message_handler = None
        self._agent = None
        self._init_scripts = init_scripts
        self._log_handler = log_handler

    def start_trace(self, session: frida.core.Session, stage, parameters, runtime, ui: UI) -> None:
        def on_create(*args) -> None:
            ui.on_trace_handler_create(*args)

        self._repository.on_create(on_create)

        def on_load(*args) -> None:
            ui.on_trace_handler_load(*args)

        self._repository.on_load(on_load)

        def on_update(target, handler, source) -> None:
            self._agent.update_handler_code(target.identifier, target.display_name, handler)

        self._repository.on_update(on_update)

        def on_message(message: Mapping[Any, Any], data: Any) -> None:
            if ui.try_handle_bridge_request(message, self._script):
                return
            self._reactor.schedule(lambda: self._on_message(message, data, ui))

        self._message_handler = on_message

        ui.on_trace_progress("initializing")
        data_dir = Path(__file__).parent
        source = (
            (data_dir / "tracer_agent.js").read_text(encoding="utf-8").replace("/agent.js", "/frida/tracer/agent.js", 1)
        )
        script = session.create_script(name="tracer", source=source, runtime=runtime)

        self._script = script
        script.set_log_handler(self._log_handler)
        script.on("message", self._message_handler)
        ui.on_script_created(script)
        script.load()

        self._agent = script.exports_sync

        raw_init_scripts = [{"filename": script.filename, "source": script.source} for script in self._init_scripts]
        self.process = self._agent.init(stage, parameters, raw_init_scripts, self._profile.spec)

    def stop(self) -> None:
        self._repository.close()

        if self._script is not None:
            self._script.off("message", self._message_handler)
            try:
                self._script.unload()
            except:
                pass
            self._script = None

    def update_handler_config(self, identifier: int, config: dict) -> None:
        return self._agent.update_handler_config(identifier, config)

    def stage_targets(self, profile: TracerProfile) -> List:
        return self._agent.stage_targets(profile.spec)

    def commit_targets(self, identifier: Optional[int]) -> dict:
        return self._agent.commit_targets(identifier)

    def read_memory(self, address: str, size: int) -> bytes:
        return self._agent.read_memory(address, size)

    def resolve_addresses(self, addresses: List[str]) -> List[str]:
        return self._agent.resolve_addresses(addresses)

    def _on_message(self, message, data, ui) -> None:
        handled = False

        if message["type"] == "send":
            try:
                payload = message["payload"]
                mtype = payload["type"]
                params = (mtype, payload, data, ui)
            except:
                # As user scripts may use send() we need to be prepared for this.
                params = None
            if params is not None:
                handled = self._try_handle_message(*params)

        if not handled:
            print(message)

    def _try_handle_message(self, mtype, params, data, ui) -> False:
        if mtype == "events:add":
            events = [
                (target_id, timestamp, thread_id, depth, caller, backtrace, message)
                for target_id, timestamp, thread_id, depth, caller, backtrace, message in params["events"]
            ]
            ui.on_trace_events(events)
            return True

        if mtype == "handlers:get":
            flavor = params["flavor"]
            base_id = params["baseId"]

            scripts = []
            response = {"type": f"reply:{base_id}", "scripts": scripts}

            repo = self._repository
            next_id = base_id
            for scope in params["scopes"]:
                scope_name = scope["name"]
                addresses = scope.get("addresses")
                i = 0
                for member_name in scope["members"]:
                    if isinstance(member_name, list):
                        name, display_name = member_name
                    else:
                        name = member_name
                        display_name = member_name
                    address = int(addresses[i], 16) if addresses is not None else None
                    target = TraceTarget(next_id, flavor, scope_name, name, display_name, address)
                    next_id += 1
                    handler = repo.ensure_handler(target)
                    scripts.append(handler)
                    i += 1

            self._script.post(response)

            return True

        if mtype == "agent:initialized":
            ui.on_trace_progress("initialized")
            return True

        if mtype == "agent:started":
            self._repository.commit_handlers()
            ui.on_trace_progress("started", params["count"])
            return True

        if mtype == "agent:warning":
            ui.on_trace_warning(params["message"])
            return True

        if mtype == "agent:error":
            ui.on_trace_error(params["message"])
            return True

        return False


@dataclass
class TraceTarget:
    identifier: int
    flavor: str
    scope: str
    name: str
    display_name: str
    address: Optional[int]

    def to_json(self) -> dict:
        return {
            "id": self.identifier,
            "flavor": self.flavor,
            "scope": self.scope,
            "display_name": self.display_name,
            "address": hex(self.address) if self.address is not None else None,
        }

    def __str__(self) -> str:
        return self.display_name


class Repository:
    def __init__(self) -> None:
        self._on_create_callback: Optional[Callable[[TraceTarget, str, str], None]] = None
        self._on_load_callback: Optional[Callable[[TraceTarget, str, str], None]] = None
        self._on_update_callback: Optional[Callable[[TraceTarget, str, str], None]] = None
        self._decorate = False
        self._manpages = None

    def close(self) -> None:
        self._on_create_callback = None
        self._on_load_callback = None
        self._on_update_callback = None

    def ensure_handler(self, target: TraceTarget):
        raise NotImplementedError("not implemented")

    def commit_handlers(self) -> None:
        pass

    def on_create(self, callback: Callable[[TraceTarget, str, str], None]) -> None:
        self._on_create_callback = callback

    def on_load(self, callback: Callable[[TraceTarget, str, str], None]) -> None:
        self._on_load_callback = callback

    def on_update(self, callback: Callable[[TraceTarget, str, str], None]) -> None:
        self._on_update_callback = callback

    def _notify_create(self, target: TraceTarget, handler: str, source: str) -> None:
        if self._on_create_callback is not None:
            self._on_create_callback(target, handler, source)

    def _notify_load(self, target: TraceTarget, handler: str, source: str) -> None:
        if self._on_load_callback is not None:
            self._on_load_callback(target, handler, source)

    def _notify_update(self, target: TraceTarget, handler: str, source: str) -> None:
        if self._on_update_callback is not None:
            self._on_update_callback(target, handler, source)

    def _create_stub_handler(self, target: TraceTarget, decorate: bool) -> str:
        if target.flavor == "insn":
            return self._create_stub_instruction_handler(target, decorate)
        if target.flavor == "java":
            return self._create_stub_java_handler(target, decorate)
        return self._create_stub_native_handler(target, decorate)

    def _create_stub_instruction_handler(self, target: TraceTarget, decorate: bool) -> str:
        return """\
/*
 * Auto-generated by Frida.
 *
 * For full API reference, see: https://frida.re/docs/javascript-api/
 */

defineHandler(function (log, args, state) {
  log(`%(display_name)s hit! sp=${this.context.sp}`);
});
""" % {"display_name": target.display_name}

    def _create_stub_native_handler(self, target: TraceTarget, decorate: bool) -> str:
        if target.flavor == "objc":
            log_str = self._create_objc_logging_code(target)
        elif target.flavor == "swift":
            log_str = self._create_swift_logging_code(target, decorate)
        else:
            log_str = self._create_cstyle_logging_code(target, decorate)

        return """\
/*
 * Auto-generated by Frida. Please modify to match the signature of %(display_name)s.
 * This stub is currently auto-generated from manpages when available.
 *
 * For full API reference, see: https://frida.re/docs/javascript-api/
 */

defineHandler({
  onEnter(log, args, state) {
    log(%(log_str)s);
  },

  onLeave(log, retval, state) {
  }
});
""" % {
            "display_name": target.display_name,
            "log_str": log_str,
        }

    def _create_cstyle_logging_code(self, target: TraceTarget, decorate: bool) -> str:
        if decorate:
            module_string = f" [{Path(target.scope).name}]"
        else:
            module_string = ""

        args = self._generate_cstyle_argument_logging_code(target)
        if len(args) == 0:
            code = "'%(name)s()%(module_string)s'" % {"name": target.name, "module_string": module_string}
        else:
            code = "`%(name)s(%(args)s)%(module_string)s`" % {
                "name": target.name,
                "args": ", ".join(args),
                "module_string": module_string,
            }

        return code

    def _create_objc_logging_code(self, target: TraceTarget) -> str:
        state = {"index": 2}

        def objc_arg(m):
            index = state["index"]
            r = ":${args[%d]} " % index
            state["index"] = index + 1
            return r

        code = "`" + re.sub(r":", objc_arg, target.display_name) + "`"
        if code.endswith("} ]`"):
            code = code[:-3] + "]`"

        return code

    def _create_swift_logging_code(self, target: TraceTarget, decorate: bool) -> str:
        if decorate:
            module_string = f" [{Path(target.scope).name}]"
        else:
            module_string = ""
        return "'%(name)s()%(module_string)s'" % {"name": target.name, "module_string": module_string}

    def _create_stub_java_handler(self, target: TraceTarget, decorate) -> str:
        return """\
/*
 * Auto-generated by Frida. Please modify to match the signature of %(display_name)s.
 *
 * For full API reference, see: https://frida.re/docs/javascript-api/
 */

defineHandler({
  /**
   * Called synchronously when about to call %(display_name)s.
   *
   * @this {object} - The Java class or instance.
   * @param {function} log - Call this function with a string to be presented to the user.
   * @param {array} args - Java method arguments.
   * @param {object} state - Object allowing you to keep state across function calls.
   */
  onEnter(log, args, state) {
    log(`%(display_name)s(${args.map(JSON.stringify).join(', ')})`);
  },

  /**
   * Called synchronously when about to return from %(display_name)s.
   *
   * See onEnter for details.
   *
   * @this {object} - The Java class or instance.
   * @param {function} log - Call this function with a string to be presented to the user.
   * @param {NativePointer} retval - Return value.
   * @param {object} state - Object allowing you to keep state across function calls.
   */
  onLeave(log, retval, state) {
    if (retval !== undefined) {
      log(`<= ${JSON.stringify(retval)}`);
    }
  }
});
""" % {"display_name": target.display_name}

    def _generate_cstyle_argument_logging_code(self, target: TraceTarget) -> List[str]:
        if self._manpages is None:
            self._manpages = {}
            try:
                manroots = [
                    Path(d)
                    for d in subprocess.run(["manpath"], stdout=subprocess.PIPE, encoding="utf-8", check=True)
                    .stdout.strip()
                    .split(":")
                ]
                for section in (2, 3):
                    for manroot in manroots:
                        mandir = manroot / f"man{section}"
                        if not mandir.exists():
                            continue
                        raw_section = str(section)
                        for entry in mandir.iterdir():
                            tokens = entry.name.split(".")
                            if len(tokens) < 2:
                                continue
                            if not tokens[1].startswith(raw_section):
                                continue
                            name = tokens[0]
                            if name in self._manpages:
                                continue
                            self._manpages[name] = (entry, section)
            except:
                return []

        man_entry = self._manpages.get(target.name)
        if man_entry is None:
            return []
        man_location, man_section = man_entry

        try:
            args = []
            cfunc = next(f for f in self._read_manpage(man_location) if f.name == target.name)
            for arg in cfunc.arguments:
                if arg == "void":
                    continue
                if arg.startswith("..."):
                    args.append("...")
                    continue

                tokens = arg.split(" ")

                arg_type = "".join(tokens[:-1])

                arg_name = tokens[-1]
                if arg_name.startswith("*"):
                    arg_type += "*"
                    arg_name = arg_name[1:]
                elif arg_name.endswith("]"):
                    arg_type += "*"
                    arg_name = arg_name[: arg_name.index("[")]

                read_ops = ""
                annotate_pre = ""
                annotate_post = ""

                if arg_type.endswith("*restrict"):
                    arg_type = arg_type[:-8]
                if arg_type in ("char*", "constchar*"):
                    read_ops = ".readUtf8String()"
                    annotate_pre = '"'
                    annotate_post = '"'

                arg_index = len(args)

                args.append(
                    "%(arg_name)s=%(annotate_pre)s${args[%(arg_index)s]%(read_ops)s}%(annotate_post)s"
                    % {
                        "arg_name": arg_name,
                        "arg_index": arg_index,
                        "read_ops": read_ops,
                        "annotate_pre": annotate_pre,
                        "annotate_post": annotate_post,
                    }
                )
            return args
        except Exception:
            return []

    def _read_manpage(self, man_location: Path) -> Generator[CFuncSpec]:
        if man_location.suffix == ".gz":
            man_file = gzip.open(man_location, "rt", encoding="utf-8", errors="replace")
        else:
            man_file = open(man_location, "r", encoding="utf-8", errors="replace")
        with man_file:
            man_data = man_file.read()

        manpage_format = "gnu"
        synopsis_lines = []
        found_synopsis = False
        in_multiline = False
        for raw_line in man_data.split("\n"):
            line = raw_line.strip()
            if line.startswith(".so "):
                redirected_location = man_location.parent.parent / Path(line[4:])
                if not redirected_location.exists():
                    redirected_location = redirected_location.parent / (redirected_location.name + ".gz")
                yield from self._read_manpage(redirected_location)
                return
            if not found_synopsis and "SYNOPSIS" in line:
                found_synopsis = True
                continue
            elif found_synopsis and line.endswith("DESCRIPTION"):
                break
            elif not found_synopsis:
                continue
            if line.startswith(".Fn ") or line.startswith(".Fo "):
                manpage_format = "bsd"
            escaped_newline = line.endswith("\\")
            if escaped_newline:
                line = line[:-1]
            if in_multiline:
                synopsis_lines[-1] += line
            else:
                synopsis_lines.append(line)
            in_multiline = escaped_newline

        if manpage_format == "gnu":
            raw_synopsis = "\n".join(synopsis_lines)
            synopsis = (
                MANPAGE_CONTROL_CHARS.sub("", raw_synopsis).replace("\n", " ").replace(" [", "[").replace(" ]", "]")
            )

            for m in MANPAGE_FUNCTION_PROTOTYPE.finditer(synopsis):
                name = m.group(1)
                signature = m.group(2)
                args = [a.strip() for a in signature.split(",")]
                yield CFuncSpec(name, args)
        else:
            name = None
            args = None
            for line in synopsis_lines:
                tokens = line.split(" ", maxsplit=1)
                directive = tokens[0]
                data = tokens[1] if len(tokens) == 2 else None
                if directive == ".Fn":
                    argv = shlex.split(data)
                    yield CFuncSpec(argv[0], argv[1:])
                elif directive == ".Fo":
                    name = data
                    args = []
                elif directive == ".Fa":
                    args.append(shlex.split(data)[0])
                elif directive == ".Fc":
                    yield CFuncSpec(name, args)


@dataclass
class CFuncSpec:
    name: str
    arguments: List[str]


class MemoryRepository(Repository):
    def __init__(self) -> None:
        super().__init__()
        self._handlers = {}

    def ensure_handler(self, target: TraceTarget) -> str:
        handler = self._handlers.get(target)
        if handler is None:
            handler = self._create_stub_handler(target, False)
            self._handlers[target] = handler
            self._notify_create(target, handler, "memory")
        else:
            self._notify_load(target, handler, "memory")
        return handler


class FileRepository(Repository):
    def __init__(self, reactor: Reactor, decorate: bool) -> None:
        super().__init__()
        self._reactor = reactor
        self._handler_by_id = {}
        self._handler_by_file = {}
        self._changed_files = set()
        self._last_change_id = 0
        self._repo_dir = Path.cwd() / "__handlers__"
        self._repo_monitors = {}
        self._decorate = decorate

    def close(self) -> None:
        for monitor in self._repo_monitors.values():
            try:
                monitor.disable()
            except:
                pass
        self._repo_monitors.clear()

        super().close()

    def ensure_handler(self, target: TraceTarget) -> str:
        entry = self._handler_by_id.get(target.identifier)
        if entry is not None:
            target, handler, handler_file = entry
            return handler

        handler = None

        scope = target.scope
        if len(scope) > 0:
            handler_file = self._repo_dir / to_filename(Path(scope).name) / to_handler_filename(target.name)
        else:
            handler_file = self._repo_dir / to_handler_filename(target.name)

        if handler_file.is_file():
            handler = self._load_handler(handler_file)
            self._notify_load(target, handler, handler_file)

        if handler is None:
            handler = self._create_stub_handler(target, self._decorate)
            handler_dir = handler_file.parent
            handler_dir.mkdir(parents=True, exist_ok=True)
            handler_file.write_text(handler, encoding="utf-8")
            self._notify_create(target, handler, handler_file)

        entry = (target, handler, handler_file)
        self._handler_by_id[target.identifier] = entry
        self._handler_by_file[handler_file] = entry

        self._ensure_monitor(handler_file)

        return handler

    def _load_handler(self, file: Path) -> None:
        handler = file.read_text(encoding="utf-8")
        if "defineHandler" not in handler:
            handler = self._migrate_handler(handler)
            file.write_text(handler, encoding="utf-8")
        return handler

    @staticmethod
    def _migrate_handler(handler: str) -> str:
        try:
            start = handler.index("{")
            end = handler.rindex("}")
        except ValueError:
            return handler
        preamble = handler[:start]
        definition = handler[start : end + 1]
        postamble = handler[end + 1 :]
        return "".join([preamble, "defineHandler(", definition, ");", postamble])

    def update_handler(self, target: TraceTarget, handler: str) -> None:
        _, _, handler_file = self._handler_by_id.get(target.identifier)
        entry = (target, handler, handler_file)
        self._handler_by_id[target.identifier] = entry
        self._handler_by_file[handler_file] = entry
        self._notify_update(target, handler, handler_file)

        handler_file.write_text(handler, encoding="utf-8")

    def _ensure_monitor(self, handler_file: Path) -> None:
        handler_dir = handler_file.parent
        monitor = self._repo_monitors.get(handler_dir)
        if monitor is None:
            monitor = frida.FileMonitor(str(handler_dir))
            monitor.on("change", self._on_change)
            self._repo_monitors[handler_dir] = monitor

    def commit_handlers(self) -> None:
        for monitor in self._repo_monitors.values():
            monitor.enable()

    def _on_change(self, raw_changed_file: str, other_file: str, event_type: str) -> None:
        changed_file = Path(raw_changed_file)
        if changed_file not in self._handler_by_file or event_type == "changes-done-hint":
            return
        self._changed_files.add(changed_file)
        self._last_change_id += 1
        change_id = self._last_change_id
        self._reactor.schedule(lambda: self._sync_handlers(change_id), delay=0.05)

    def _sync_handlers(self, change_id) -> None:
        if change_id != self._last_change_id:
            return
        changes = self._changed_files.copy()
        self._changed_files.clear()
        for changed_handler_file in changes:
            target, old_handler, handler_file = self._handler_by_file[changed_handler_file]
            new_handler = self._load_handler(handler_file)
            if new_handler != old_handler:
                entry = (target, new_handler, handler_file)
                self._handler_by_id[target.identifier] = entry
                self._handler_by_file[handler_file] = entry
                self._notify_update(target, new_handler, handler_file)


class InitScript:
    def __init__(self, filename, source) -> None:
        self.filename = filename
        self.source = source


class OutputFile:
    def __init__(self, filename: str) -> None:
        self._fd = codecs.open(filename, "wb", "utf-8")

    def close(self) -> None:
        self._fd.close()

    def append(self, message: str) -> None:
        self._fd.write(message)
        self._fd.flush()


class UI:
    def on_script_created(self, script: frida.core.Script) -> None:
        pass

    def on_trace_progress(self, status: str) -> None:
        pass

    def on_trace_warning(self, message: str):
        pass

    def on_trace_error(self, message: str) -> None:
        pass

    def on_trace_events(self, events) -> None:
        pass

    def on_trace_handler_create(self, target: TraceTarget, handler: str, source: Path) -> None:
        pass

    def on_trace_handler_load(self, target: TraceTarget, handler: str, source: Path) -> None:
        pass

    def try_handle_bridge_request(self, message: Mapping[Any, Any], script: frida.core.Script) -> bool:
        pass


def to_filename(name: str) -> str:
    result = ""
    for c in name:
        if c.isalnum() or c == ".":
            result += c
        else:
            result += "_"
    return result


def to_handler_filename(name: str) -> str:
    full_filename = to_filename(name)
    if len(full_filename) <= 41:
        return full_filename + ".js"
    crc = binascii.crc32(full_filename.encode())
    return full_filename[0:32] + "_%08x.js" % crc


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
