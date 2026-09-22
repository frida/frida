import os
import socket
import subprocess
import sys
import unittest
from pathlib import Path


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


REPO = Path(__file__).resolve().parent.parent
BINDGEN_CORE = REPO / "frida-bindgen"
GIRDIR = REPO / "build" / "subprojects" / "frida-core" / "src" / "api"
OUTDIR = REPO / "build" / "bindgen-out"
BUILT_EXTENSION = REPO / "build" / "frida" / "_frida.abi3.so"
CERTIFICATE = """\
-----BEGIN CERTIFICATE-----
MIIDGzCCAgOgAwIBAgIUEoCZX+FRWNTzcvvrCE2chFvE0EgwDQYJKoZIhvcNAQEL
BQAwHDEaMBgGA1UEAwwRZnJpZGEtcHl0aG9uLXRlc3QwIBcNMjYwNzMwMjIyNjAy
WhgPMjEyNjA3MDYyMjI2MDJaMBwxGjAYBgNVBAMMEWZyaWRhLXB5dGhvbi10ZXN0
MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAtKTIUbHLhkqRKdKqXuMc
u7q8OZD2eKdykp4Qvv+I5JcmNW7nlW+58zTCLaqUCsSrojDb8T2qvtRJbaJYAofE
P7bhb5ROOVwsyOjhvP7rZ4zRXIDAUlB7zE15yNkWL+Rh4l3wRxMOMIB2HQlurtDk
PIqPOITwRvrADKZt/73iMASwrkkqKnl/F3rLT0JQrd907sBdGGZNbJM9L4TUBT9t
MBBl6qXtDEFyBfeoqlnrLUTN/Jwyka+/6n6eZCN8g2g5tS1Lc7LFjX4iBG9ibgaH
hrNSTM+haLImFS3dtXFr4rOCAHnDYYvkCY1nuZOXannCvZowv0xczqpGLbj5IYJK
MQIDAQABo1MwUTAdBgNVHQ4EFgQUi83cV5yMTdWaVeRpgNO3vyeitMMwHwYDVR0j
BBgwFoAUi83cV5yMTdWaVeRpgNO3vyeitMMwDwYDVR0TAQH/BAUwAwEB/zANBgkq
hkiG9w0BAQsFAAOCAQEADScSFuasB4mgOb/kktq3O09rf/uUIa6OEjdFRec/i2+E
ZAUd5onsWQTHm4yhjqQwqVBte58tTqZB7S1HMCy5sEgEVUFnTrMUqtrf/WAnsWA6
SdjNr7h/aRnLHHysLkbl0Zo4fOIWIYSJpLn39e0OzcNBjEHN5U9HE/m/2LFku1Kb
KMTYXrhBKvVPpZLxZyoLz00EM7euaNx3juAgfQdSDfKGahf5CqqMJ/VmITISDJDR
jtN3wcv0/nZA5x/b73QCj0jqIEWqguYf4GLWitxGsSdT2dBEpT88Kz3Ibwb8uRT+
Ls3eRsdoqxiOT0E7Z44O8gjbqsYOr/jdMShwEytDwQ==
-----END CERTIFICATE-----
"""


@unittest.skipUnless(
    (GIRDIR / "Frida-1.0.gir").exists(),
    "requires a configured build tree with generated .gir files",
)
class TestBindgen(unittest.TestCase):
    def test_generates_all_outputs(self):
        OUTDIR.mkdir(exist_ok=True)
        subprocess.run(
            [
                sys.executable,
                "-m",
                "frida_bindgen",
                f"--frida-gir={GIRDIR / 'Frida-1.0.gir'}",
                f"--glib-gir={GIRDIR / 'GLib-2.0.gir'}",
                f"--gobject-gir={GIRDIR / 'GObject-2.0.gir'}",
                f"--gio-gir={GIRDIR / 'Gio-2.0.gir'}",
                f"--output-py={OUTDIR / '__init__.py'}",
                f"--output-aio={OUTDIR / 'aio.py'}",
                f"--output-pyi={OUTDIR / '_frida.pyi'}",
                f"--output-c={OUTDIR / 'extension.c'}",
            ],
            check=True,
            env={"PYTHONPATH": os.pathsep.join([str(REPO / "frida"), str(BINDGEN_CORE)])},
        )
        for name in ("__init__.py", "aio.py", "_frida.pyi", "extension.c"):
            self.assertGreater((OUTDIR / name).stat().st_size, 0)

    def test_generated_python_compiles(self):
        for name in ("__init__.py", "aio.py"):
            path = OUTDIR / name
            if not path.exists():
                self.skipTest("run test_generates_all_outputs first")
            compile(path.read_text(), str(path), "exec")


@unittest.skipUnless(
    BUILT_EXTENSION.exists(),
    "requires a built _frida extension (run `make`)",
)
class TestExtension(unittest.TestCase):
    def setUp(self):
        sys.path.insert(0, str(BUILT_EXTENSION.parent))
        import _frida

        self._frida = _frida

    def tearDown(self):
        sys.path.remove(str(BUILT_EXTENSION.parent))

    def test_exposes_version(self):
        self.assertRegex(self._frida.__version__, r"\d+\.\d+")

    def test_constructs_object_with_string_and_enum_parameters(self):
        relay = self._frida.Relay("1.2.3.4:5", "user", "pass", "turn-udp")
        self.assertIsInstance(relay, self._frida.Relay)

    def test_constructs_object_without_parameters(self):
        self.assertIsInstance(self._frida.DeviceManager(), self._frida.DeviceManager)

    def test_rejects_invalid_enum_value(self):
        with self.assertRaises(ValueError):
            self._frida.Relay("1.2.3.4:5", "user", "pass", "bogus-kind")

    def test_reads_back_string_and_enum_properties(self):
        relay = self._frida.Relay("1.2.3.4:5", "bob", "secret", "turn-tcp")
        self.assertEqual(relay.address, "1.2.3.4:5")
        self.assertEqual(relay.username, "bob")
        self.assertEqual(relay.password, "secret")
        self.assertEqual(relay.kind, "turn-tcp")

    def test_invokes_synchronous_methods(self):
        cancellable = self._frida.Cancellable()
        self.assertFalse(cancellable.is_cancelled())
        cancellable.cancel()
        self.assertTrue(cancellable.is_cancelled())

    def test_registers_frida_exceptions(self):
        for name in (
            "ServerNotRunningError",
            "ProcessNotFoundError",
            "InvalidArgumentError",
            "TransportError",
            "OperationCancelledError",
        ):
            exception = getattr(self._frida, name)
            self.assertTrue(issubclass(exception, Exception))
            self.assertEqual(exception.__module__, "frida")

    def test_throwing_method_raises_mapped_exception(self):
        cancellable = self._frida.Cancellable()
        cancellable.cancel()
        with self.assertRaises(self._frida.OperationCancelledError):
            cancellable.set_error_if_cancelled()

    def test_instantiates_handle_only_types(self):
        # Types with no .gir constructor are created by the marshalling layer
        # and must accept a bare, argument-free construction.
        self.assertIsInstance(self._frida.Device(), self._frida.Device)
        self.assertIsInstance(self._frida.Session(), self._frida.Session)

    def test_exposes_object_typed_property(self):
        self.assertTrue(hasattr(self._frida.IOStream, "input_stream"))
        self.assertTrue(hasattr(self._frida.IOStream, "output_stream"))

    def test_string_array_property_round_trips(self):
        options = self._frida.SpawnOptions()
        self.assertIsNone(options.argv)
        options.argv = ["/bin/ls", "-la"]
        self.assertEqual(options.argv, ["/bin/ls", "-la"])
        options.argv = None
        self.assertIsNone(options.argv)

    def test_certificate_property_accepts_pem_and_path(self):
        import tempfile

        options = self._frida.RemoteDeviceOptions()
        self.assertIsNone(options.certificate)

        options.certificate = CERTIFICATE
        self.assertEqual(options.certificate, CERTIFICATE)

        with tempfile.NamedTemporaryFile("w", suffix=".pem") as f:
            f.write(CERTIFICATE)
            f.flush()
            options.certificate = f.name
        self.assertEqual(options.certificate, CERTIFICATE)

        options.certificate = None
        self.assertIsNone(options.certificate)

    def test_certificate_property_rejects_missing_file(self):
        options = self._frida.RemoteDeviceOptions()
        with self.assertRaises(self._frida.InvalidArgumentError):
            options.certificate = "/nonexistent.pem"

    def test_vardict_property_round_trips(self):
        options = self._frida.SpawnOptions()
        options.aux = {"name": "value", "count": 42, "flag": True}
        self.assertEqual(options.aux, {"name": "value", "count": 42, "flag": True})

    def test_variant_marshalling_types_and_casts(self):
        options = self._frida.SpawnOptions()
        options.aux = {
            "int": 42,
            "uint64": ("uint64", 0xFFFFFFFFFFFFFFFF),
            "float": 3.5,
            "string": "hi",
            "bool": True,
            "bytes": b"\x00\x01\x02",
            "list": [1, 2, 3],
            "nested": {"x": 1, "y": "z"},
            "tuple": ("a", 1),
        }
        aux = options.aux
        self.assertEqual(aux["int"], 42)
        # the ("uint64", ...) cast preserves the value; as int64 it would be -1
        self.assertEqual(aux["uint64"], 0xFFFFFFFFFFFFFFFF)
        self.assertEqual(aux["float"], 3.5)
        self.assertEqual(aux["string"], "hi")
        self.assertIs(aux["bool"], True)
        self.assertEqual(aux["bytes"], b"\x00\x01\x02")
        self.assertEqual(aux["list"], [1, 2, 3])
        self.assertEqual(aux["nested"], {"x": 1, "y": "z"})
        self.assertEqual(aux["tuple"], ("a", 1))

    def test_async_method_delivers_result_to_callback(self):
        import threading

        manager = self._frida.DeviceManager()
        done = threading.Event()
        box = {}

        def on_complete(result, error):
            box["result"] = result
            box["error"] = error
            done.set()

        manager.close(on_complete)
        self.assertTrue(done.wait(10), "callback was not invoked")
        self.assertIsNone(box["error"])
        self.assertIsNone(box["result"])

    def test_raising_async_callback_does_not_corrupt_thread(self):
        import contextlib
        import io
        import threading

        raised = threading.Event()

        def bad_callback(result, error):
            raised.set()
            raise RuntimeError("handler blew up")

        with contextlib.redirect_stderr(io.StringIO()):
            self._frida.DeviceManager().close(bad_callback)
            self.assertTrue(raised.wait(10))

        # A subsequent operation must still complete cleanly.
        box = self._await_async(lambda cb: self._frida.DeviceManager().close(cb))
        self.assertIsNone(box["error"])

    def _await_async(self, invoke):
        import threading

        done = threading.Event()
        box = {}

        def on_complete(result, error):
            box["result"] = result
            box["error"] = error
            done.set()

        invoke(on_complete)
        self.assertTrue(done.wait(10), "callback was not invoked")
        return box

    def test_async_method_passes_parameters_and_maps_errors(self):
        manager = self._frida.DeviceManager()
        box = self._await_async(lambda cb: manager.get_device_by_id("no-such-device", 1, cb))
        self.assertIsNone(box["result"])
        self.assertIsInstance(box["error"], self._frida.InvalidArgumentError)

    def test_async_method_honors_cancellable(self):
        manager = self._frida.DeviceManager()
        cancellable = self._frida.Cancellable()
        cancellable.cancel()
        box = self._await_async(lambda cb: manager.get_device_by_id("anything", 5000, cb, cancellable))
        self.assertIsInstance(box["error"], self._frida.OperationCancelledError)


@unittest.skipUnless(
    (REPO / "build" / "frida" / "__init__.py").exists(),
    "requires a built facade (run `make`)",
)
class TestFacade(unittest.TestCase):
    def setUp(self):
        sys.path.insert(0, str(REPO / "build"))
        self._purge_frida()
        import frida

        self.frida = frida

    def tearDown(self):
        sys.path.remove(str(REPO / "build"))
        self._purge_frida()

    @staticmethod
    def _purge_frida():
        # The source tree's frida/ is a namespace package the stale test_core
        # imports; drop it so the built facade under build/ is what loads.
        for name in [n for n in sys.modules if n == "frida" or n.startswith("frida.")]:
            del sys.modules[name]

    def test_reexports_exceptions(self):
        self.assertTrue(issubclass(self.frida.ServerNotRunningError, Exception))

    def test_synchronous_method_blocks(self):
        self.frida.DeviceManager().close()

    def test_synchronous_method_raises_mapped_error(self):
        with self.assertRaises(self.frida.InvalidArgumentError):
            self.frida.DeviceManager().get_device_by_id("no-such-device", 1)

    def test_aio_awaitable_completes(self):
        import asyncio

        import frida.aio

        asyncio.run(frida.aio.DeviceManager().close())

    def test_aio_awaitable_raises_mapped_error(self):
        import asyncio

        import frida.aio

        async def scenario():
            with self.assertRaises(self.frida.InvalidArgumentError):
                await frida.aio.DeviceManager().get_device_by_id("no-such-device", 1)

        asyncio.run(scenario())

    def test_aio_cancellation_propagates(self):
        import asyncio

        import frida.aio

        async def scenario():
            manager = frida.aio.DeviceManager()
            task = asyncio.ensure_future(manager.get_device_by_id("nope", 60000))
            await asyncio.sleep(0.1)
            task.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await task

        asyncio.run(scenario())

    def test_unreferenced_interface_impl_is_collected(self):
        import gc
        import weakref

        class MyAuth(self.frida.AuthenticationService):
            def authenticate(self, token):
                return "{}"

        refs = [weakref.ref(MyAuth()) for _ in range(3)]
        gc.collect()
        self.assertTrue(all(ref() is None for ref in refs))

    def test_interface_impl_survives_gc_while_referenced_by_core(self):
        import gc

        class MyAuth(self.frida.AuthenticationService):
            def authenticate(self, token):
                return "{}"

        params = self.frida.EndpointParameters(address="127.0.0.1", port=0, authentication=MyAuth())
        gc.collect()
        gc.collect()
        self.assertIsNotNone(params._impl.auth_service)

    def test_interface_can_be_implemented(self):
        import json

        class MyAuth(self.frida.AuthenticationService):
            def authenticate(self, token):
                return json.dumps({"token": token})

        service = MyAuth()
        self.assertIsInstance(service, self.frida.AuthenticationService)
        self.assertEqual(type(service._impl).__name__, "AuthenticationService")

    def test_endpoint_parameters_authentication_schemes(self):
        self.frida.EndpointParameters(address="::1", authentication=("token", "secret"))
        self.frida.EndpointParameters(address="::1", authentication=("callback", lambda token: {"token": token}))

        class MyAuth(self.frida.AuthenticationService):
            def authenticate(self, token):
                return "{}"

        self.frida.EndpointParameters(address="::1", authentication=MyAuth())
        with self.assertRaises(ValueError):
            self.frida.EndpointParameters(authentication=("bogus", "x"))

    def test_object_constructor_parameters(self):
        cluster = self.frida.EndpointParameters(address="127.0.0.1", port=0)
        service = self.frida.PortalService(cluster, None)
        self.assertIsInstance(service.device, self.frida.Device)

    def test_implemented_interface_receives_dispatch(self):
        frida = self.frida
        calls = []

        def authenticate(token):
            calls.append(token)
            if token != "secret":
                raise ValueError("wrong token")
            return "{}"

        control_port = free_port()
        control = frida.EndpointParameters(
            address="127.0.0.1", port=control_port, authentication=("callback", authenticate)
        )
        cluster = frida.EndpointParameters(address="127.0.0.1", port=free_port())
        service = frida.PortalService(cluster, control)
        service.start()
        try:
            with self.assertRaises(frida.InvalidArgumentError):
                frida.DeviceManager().add_remote_device(
                    f"127.0.0.1:{control_port}", token="wrong"
                ).enumerate_processes()
            frida.DeviceManager().add_remote_device(f"127.0.0.1:{control_port}", token="secret").enumerate_processes()
            self.assertEqual(calls, ["wrong", "secret"])
        finally:
            service.stop()

    def test_aio_implemented_interface_receives_dispatch(self):
        import asyncio

        import frida.aio

        calls = []

        async def scenario():
            class MyAuth(frida.aio.AuthenticationService):
                async def authenticate(self, token):
                    calls.append(token)
                    await asyncio.sleep(0)
                    if token != "secret":
                        raise ValueError("wrong token")
                    return "{}"

            control_port = free_port()
            control = frida.aio.EndpointParameters(address="127.0.0.1", port=control_port, authentication=MyAuth())
            cluster = frida.aio.EndpointParameters(address="127.0.0.1", port=free_port())
            service = frida.aio.PortalService(cluster, control)
            await service.start()
            try:
                with self.assertRaises(self.frida.InvalidArgumentError):
                    manager = frida.aio.DeviceManager()
                    device = await manager.add_remote_device(f"127.0.0.1:{control_port}", token="wrong")
                    await device.enumerate_processes()
                manager = frida.aio.DeviceManager()
                device = await manager.add_remote_device(f"127.0.0.1:{control_port}", token="secret")
                await device.enumerate_processes()
                self.assertEqual(calls, ["wrong", "secret"])
            finally:
                await service.stop()

        asyncio.run(scenario())

    def test_web_request_handler_serves_response(self):
        import urllib.request

        frida = self.frida
        seen = []

        class Handler(frida.WebRequestHandler):
            def handle_request(self, request):
                seen.append(request.path)
                return frida.WebResponse(200, b"pong")

        control_port = free_port()
        control = frida.EndpointParameters(address="127.0.0.1", port=control_port, request_handler=Handler())
        cluster = frida.EndpointParameters(address="127.0.0.1", port=free_port())
        service = frida.PortalService(cluster, control)
        service.start()
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{control_port}/ping") as response:
                status = response.status
                body = response.read()
        finally:
            service.stop()

        self.assertEqual(status, 200)
        self.assertEqual(body, b"pong")
        self.assertEqual(seen, ["/ping"])

    def test_web_request_handler_returning_none_yields_not_found(self):
        import urllib.error
        import urllib.request

        frida = self.frida

        class Handler(frida.WebRequestHandler):
            def handle_request(self, request):
                return None

        control_port = free_port()
        control = frida.EndpointParameters(address="127.0.0.1", port=control_port, request_handler=Handler())
        cluster = frida.EndpointParameters(address="127.0.0.1", port=free_port())
        service = frida.PortalService(cluster, control)
        service.start()
        try:
            with self.assertRaises(urllib.error.HTTPError) as raised:
                urllib.request.urlopen(f"http://127.0.0.1:{control_port}/ping")
            self.assertEqual(raised.exception.code, 404)
        finally:
            service.stop()

    def test_web_request_handler_error_propagates(self):
        import urllib.error
        import urllib.request

        frida = self.frida

        class Handler(frida.WebRequestHandler):
            def handle_request(self, request):
                raise ValueError("boom")

        control_port = free_port()
        control = frida.EndpointParameters(address="127.0.0.1", port=control_port, request_handler=Handler())
        cluster = frida.EndpointParameters(address="127.0.0.1", port=free_port())
        service = frida.PortalService(cluster, control)
        service.start()
        try:
            with self.assertRaises(urllib.error.HTTPError) as raised:
                urllib.request.urlopen(f"http://127.0.0.1:{control_port}/ping")
            self.assertEqual(raised.exception.code, 500)
            self.assertEqual(raised.exception.read(), b"boom")
        finally:
            service.stop()

    @unittest.skipIf(sys.platform == "win32", "requires a POSIX shell")
    def test_spawn_accepts_program_as_list(self):
        import os
        import tempfile
        import time

        device = self.frida.get_local_device()
        fd, path = tempfile.mkstemp()
        os.close(fd)
        try:
            pid = device.spawn(["/bin/sh", "-c", f'printf hi > "{path}"'])
            device.resume(pid)

            deadline = time.time() + 5
            content = ""
            while time.time() < deadline:
                with open(path) as f:
                    content = f.read()
                if content:
                    break
                time.sleep(0.05)
        finally:
            os.unlink(path)

        self.assertEqual(content, "hi")

    @unittest.skipIf(sys.platform == "win32", "requires a POSIX shell")
    def test_spawn_accepts_env_as_dict(self):
        import os
        import tempfile
        import time

        device = self.frida.get_local_device()
        fd, path = tempfile.mkstemp()
        os.close(fd)
        try:
            pid = device.spawn(
                "/bin/sh",
                argv=["/bin/sh", "-c", f'printf %s "$FRIDA_ENVP_TEST" > "{path}"'],
                env={"FRIDA_ENVP_TEST": "envp-as-dict"},
            )
            device.resume(pid)

            deadline = time.time() + 5
            content = ""
            while time.time() < deadline:
                with open(path) as f:
                    content = f.read()
                if content:
                    break
                time.sleep(0.05)
        finally:
            os.unlink(path)

        self.assertEqual(content, "envp-as-dict")

    @unittest.skipIf(sys.platform == "win32", "requires a POSIX shell")
    def test_spawn_accepts_envp_and_bytes_argv(self):
        import os
        import tempfile
        import time

        device = self.frida.get_local_device()
        fd, path = tempfile.mkstemp()
        os.close(fd)
        try:
            pid = device.spawn(
                [b"/bin/sh", b"-c", f'printf %s "$FRIDA_ENVP2" > "{path}"'],
                envp={"FRIDA_ENVP2": "envp-full"},
            )
            device.resume(pid)

            deadline = time.time() + 5
            content = ""
            while time.time() < deadline:
                with open(path) as f:
                    content = f.read()
                if content:
                    break
                time.sleep(0.05)
        finally:
            os.unlink(path)

        self.assertEqual(content, "envp-full")

    def test_toplevel_convenience_functions(self):
        frida = self.frida
        self.assertRegex(frida.__version__, r"\d+\.\d+")
        for name in (
            "spawn",
            "resume",
            "kill",
            "attach",
            "inject_library_file",
            "inject_library_blob",
            "enumerate_devices",
            "get_device_matching",
            "shutdown",
            "query_system_parameters",
        ):
            self.assertTrue(callable(getattr(frida, name)), name)
        self.assertIn("local", [d.id for d in frida.enumerate_devices()])
        self.assertEqual(frida.get_device_matching(lambda d: d.type == "local").id, "local")

    @unittest.skipIf(sys.platform == "win32", "requires a POSIX shell")
    def test_toplevel_attach_and_query(self):
        frida = self.frida
        pid = frida.spawn(["/bin/sh", "-c", "sleep 30"])
        try:
            session = frida.attach(pid)
            session.detach()
        finally:
            frida.kill(pid)
        params = frida.query_system_parameters()
        self.assertIn("arch", params)

    def test_device_type_property(self):
        device = self.frida.get_local_device()
        self.assertEqual(device.type, "local")

    def test_bool_accessors_are_properties(self):
        device = self.frida.get_local_device()
        self.assertIs(device.is_lost, False)
        session = device.attach(0)
        self.assertIs(session.is_detached, False)
        session.detach()
        self.assertIs(session.is_detached, True)

    def test_device_get_process_by_name(self):
        device = self.frida.get_local_device()
        with self.assertRaises(self.frida.ProcessNotFoundError):
            device.get_process("frida-nonexistent-process-zzz")

    @unittest.skipIf(sys.platform == "win32", "requires a POSIX shell")
    def test_attach_accepts_pid(self):
        device = self.frida.get_local_device()
        pid = device.spawn(["/bin/sh", "-c", "sleep 30"])
        try:
            session = device.attach(pid)
            session.detach()
        finally:
            device.kill(pid)

    def test_facade_repr(self):
        device = self.frida.get_local_device()
        self.assertRegex(repr(device), r"Device\(id='local'")
        process = self.frida.get_local_device().enumerate_processes()[0]
        self.assertRegex(repr(process), r"Process\(pid=\d+")

    def test_create_script_positional_name(self):
        session = self.frida.get_local_device().attach(0)
        try:
            script = session.create_script("rpc.exports={ping:()=>1};", "my-script")
            script.load()
            self.assertEqual(script.exports_sync.ping(), 1)
        finally:
            session.detach()

    def test_create_script_from_bytes_positional_name(self):
        session = self.frida.get_local_device().attach(0)
        try:
            bytecode = session.compile_script("rpc.exports={ping:()=>1};", "n")
            script = session.create_script_from_bytes(bytecode, "my-script")
            script.load()
            self.assertEqual(script.exports_sync.ping(), 1)
        finally:
            session.detach()

    def test_attach_accepts_linker_notifier_offsets(self):
        session = self.frida.get_local_device().attach(0, linker_notifier_offsets=[0x100])
        session.detach()

    def test_enable_debugger_defaults_to_any_port(self):
        import inspect

        for script in [self.frida.Script, self.frida.aio.Script]:
            self.assertEqual(inspect.signature(script.enable_debugger).parameters["port"].default, 0)

    def test_file_monitor_reports_changes(self):
        import os
        import tempfile
        import time

        directory = tempfile.mkdtemp()
        target = os.path.join(directory, "watched.txt")
        monitor = self.frida.FileMonitor(target)
        events = []
        monitor.on("change", lambda *args: events.append(args))
        monitor.enable()
        try:
            with open(target, "w") as f:
                f.write("hi")
            deadline = time.time() + 3
            while not events and time.time() < deadline:
                time.sleep(0.05)
        finally:
            monitor.disable()
        self.assertTrue(events)
        self.assertEqual(events[0][0], target)

    def test_frida_core_alias(self):
        import frida.core

        self.assertIs(frida.core.Session, self.frida.Session)
        self.assertIs(frida.core.Device, self.frida.Device)

    def test_device_bus_post_accepts_dict(self):
        bus = self.frida.get_local_device().bus
        self.assertTrue(callable(bus.post))
        bus.post({"type": "ping"})

    def test_script_options_snapshot_setter(self):
        options = self._new_script_options()
        options.snapshot = b"\x00\x01\x02"
        self.assertEqual(options.snapshot, b"\x00\x01\x02")

    def _new_script_options(self):
        from frida import _frida

        return _frida.ScriptOptions()

    def test_iostream_exposes_old_api(self):
        for name in ("is_closed", "close", "read", "read_all", "write", "write_all"):
            self.assertTrue(hasattr(self.frida.IOStream, name))

    def test_get_local_device_returns_wrapped_device(self):
        device = self.frida.get_local_device()
        self.assertIsInstance(device, self.frida.Device)
        self.assertEqual(device.id, "local")

    def test_device_manager_is_a_singleton(self):
        import frida._frida as _frida
        import frida.aio

        underlying = _frida.get_device_manager()
        self.assertIs(_frida.get_device_manager(), underlying)
        self.assertIs(self.frida.get_device_manager()._impl, underlying)
        self.assertIs(frida.aio.get_device_manager()._impl, underlying)

    def test_aio_get_local_device(self):
        import asyncio

        import frida.aio

        device = asyncio.run(frida.aio.get_local_device())
        self.assertIsInstance(device, frida.aio.Device)
        self.assertEqual(device.id, "local")

    def test_method_with_options_parameter_returns_object(self):
        import os

        device = self.frida.get_local_device()
        process = device.get_process_by_pid(os.getpid())
        self.assertIsInstance(process, self.frida.Process)
        self.assertEqual(process.pid, os.getpid())

    def test_method_with_options_parameter_maps_errors(self):
        device = self.frida.get_local_device()
        with self.assertRaises(self.frida.ProcessNotFoundError):
            device.attach(999999)

    def test_list_return_marshals_to_wrapped_objects(self):
        devices = self.frida.get_device_manager().enumerate_devices()
        self.assertIsInstance(devices, list)
        self.assertTrue(all(isinstance(d, self.frida.Device) for d in devices))
        self.assertIn("local", [d.id for d in devices])

    def test_vardict_return_marshals_to_dict(self):
        params = self.frida.get_local_device().query_system_parameters()
        self.assertIsInstance(params, dict)
        self.assertIn("arch", params)
        self.assertIsInstance(params["os"], dict)

    def test_signal_handler_is_invoked(self):
        cancellable = self.frida.Cancellable()
        fired = []
        cancellable.on("cancelled", lambda: fired.append(True))
        cancellable.cancel()
        self.assertEqual(fired, [True])

    def test_removed_signal_handler_is_not_invoked(self):
        cancellable = self.frida.Cancellable()
        hits = []

        def handler():
            hits.append(True)

        cancellable.on("cancelled", handler)
        cancellable.off("cancelled", handler)
        cancellable.cancel()
        self.assertEqual(hits, [])

    def test_custom_base_type_still_exposes_signals(self):
        device = self.frida.get_local_device()
        callback = lambda *args: None
        device.on("spawn-added", callback)
        device.off("spawn-added", callback)

    def test_unknown_signal_name_raises(self):
        with self.assertRaises(ValueError):
            self.frida.get_device_manager().on("nonexistent", lambda: None)

    def test_signal_handler_can_receive_the_emitter(self):
        cancellable = self.frida.Cancellable()
        received = []
        cancellable.on("cancelled", lambda sender: received.append(sender))
        cancellable.cancel()
        self.assertEqual(len(received), 1)
        self.assertIsInstance(received[0], self.frida.Cancellable)

    def test_signal_handler_with_too_many_arguments_raises(self):
        cancellable = self.frida.Cancellable()
        with self.assertRaises(TypeError):
            cancellable.on("cancelled", lambda a, b, c: None)

    def test_cancellable_is_cancelled_is_a_property(self):
        cancellable = self.frida.Cancellable()
        self.assertFalse(cancellable.is_cancelled)
        cancellable.cancel()
        self.assertTrue(cancellable.is_cancelled)

    def test_cancellable_pollfd_round_trips(self):
        cancellable = self.frida.Cancellable()
        pollfd = cancellable.get_pollfd()
        with pollfd as fd:
            self.assertIsInstance(fd, int)
        pollfd.release()
        pollfd.release()

    def test_rpc_round_trip(self):
        session = self.frida.get_local_device().attach(0)
        try:
            script = session.create_script("rpc.exports = { add: (a, b) => a + b, echo: (x) => x };")
            script.load()
            self.assertEqual(sorted(dir(script.exports_sync)), ["add", "echo"])
            self.assertEqual(script.exports_sync.add(3, 4), 7)
            self.assertEqual(script.exports_sync.echo("hello"), "hello")
        finally:
            session.detach()

    def test_script_rpc_error_propagates(self):
        session = self.frida.get_local_device().attach(0)
        try:
            script = session.create_script("rpc.exports = { boom: () => { throw new Error('kaboom'); } };")
            script.load()
            with self.assertRaises(self.frida.RPCException):
                script.exports_sync.boom()
        finally:
            session.detach()

    def test_script_log_handler_receives_console_log(self):
        session = self.frida.get_local_device().attach(0)
        try:
            script = session.create_script("console.log('hello from script');")
            logs = []
            script.set_log_handler(lambda level, text: logs.append((level, text)))
            script.load()
            self.assertIn(("info", "hello from script"), logs)
        finally:
            session.detach()

    def test_script_list_exports(self):
        session = self.frida.get_local_device().attach(0)
        try:
            script = session.create_script("rpc.exports = { a: () => 1, b: () => 2 };")
            script.load()
            self.assertEqual(sorted(script.list_exports_sync()), ["a", "b"])
        finally:
            session.detach()

    def test_script_message_round_trip(self):
        session = self.frida.get_local_device().attach(0)
        try:
            script = session.create_script("send({ hi: 1 });")
            received = []
            script.on("message", lambda message, data: received.append(message))
            script.load()
            self.assertEqual(received[0]["type"], "send")
            self.assertEqual(received[0]["payload"], {"hi": 1})
        finally:
            session.detach()

    def test_script_rpc_raises_after_destroy(self):
        session = self.frida.get_local_device().attach(0)
        try:
            script = session.create_script("rpc.exports = { ping: () => 1 };")
            script.load()
            script.unload()
            with self.assertRaises(self.frida.InvalidOperationError):
                script.exports_sync.ping()
        finally:
            session.detach()

    def test_rpc_exception_str_surfaces_message(self):
        self.assertEqual(str(self.frida.RPCException("type", "name", "boom")), "boom")
        self.assertEqual(str(self.frida.RPCException("boom")), "boom")

    def test_sync_cancellable_context_cancels_blocking_call(self):
        import threading

        device = self.frida.get_local_device()
        cancellable = self.frida.Cancellable()
        threading.Timer(0.2, cancellable.cancel).start()
        with self.assertRaises(self.frida.OperationCancelledError):
            with cancellable:
                device.get_process_by_name("frida-nonexistent-zzz", timeout=5000)

    def test_aio_cancellable_context_cancels_awaitable(self):
        import asyncio

        import frida.aio

        async def scenario():
            device = await frida.aio.get_local_device()
            cancellable = frida.aio.Cancellable()

            async def cancel_soon():
                await asyncio.sleep(0.2)
                cancellable.cancel()

            asyncio.ensure_future(cancel_soon())
            with self.assertRaises(self.frida.OperationCancelledError):
                with cancellable:
                    await device.get_process_by_name("frida-nonexistent-zzz", timeout=5000)

        asyncio.run(scenario())

    def test_attach_accepts_options_as_kwargs(self):
        session = self.frida.get_local_device().attach(0, realm="native", persist_timeout=0)
        session.detach()

    def test_invalid_option_is_reported(self):
        with self.assertRaises(AttributeError):
            self.frida.get_local_device().attach(0, no_such_option=123)

    def test_compile_script_returns_bytes(self):
        session = self.frida.get_local_device().attach(0)
        try:
            data = session.compile_script("const x = 42;")
            self.assertIsInstance(data, bytes)
            self.assertGreater(len(data), 0)
        finally:
            session.detach()

    def test_aio_rpc_round_trip(self):
        import asyncio

        import frida.aio

        async def scenario():
            device = await frida.aio.get_local_device()
            session = await device.attach(0)
            try:
                script = await session.create_script("rpc.exports = { add: (a, b) => a + b };")
                await script.load()
                self.assertEqual(await script.exports.add(3, 4), 7)
            finally:
                await session.detach()

        asyncio.run(scenario())


if __name__ == "__main__":
    unittest.main()
