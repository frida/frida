from __future__ import annotations

from typing import Mapping

from .model import (
    ConstructorCustomizations,
    CustomCode,
    Customizations,
    EnumerationCustomizations,
    EnumerationMemberCustomizations,
    MethodCustomizations,
    ModuleFunction,
    ObjectTypeCustomizations,
    PropertyCustomizations,
    SignalCustomizations,
    TypeCustomizations,
)

_RESOLVE_PID = ("pid = self._pid_of(target)", "pid = await self._pid_of(target)")


def load_customizations() -> Customizations:
    type_customizations: Mapping[str, TypeCustomizations] = {
        "DeviceManager": ObjectTypeCustomizations(
            custom_code=CustomCode(
                members=(
                    "facade_device_manager_members.py",
                    "facade_device_manager_members_aio.py",
                ),
                helpers=("facade_toplevel.py", "facade_toplevel_aio.py"),
                module_functions=(
                    ModuleFunction(
                        py_name="get_device_manager",
                        c_symbol="PyFrida_get_device_manager",
                        asset="module_get_device_manager.c",
                    ),
                ),
            ),
        ),
        "Bus": ObjectTypeCustomizations(
            provides_signals=True,
            custom_code=CustomCode(
                members=("facade_bus.py", "facade_bus_aio.py"),
            ),
            methods={
                "post": MethodCustomizations(
                    param_typings=["message: Any", "data: Optional[bytes] = None"],
                    custom_logic="json = _to_json(message)",
                ),
            },
        ),
        "PortalService": ObjectTypeCustomizations(
            provides_signals=True,
            custom_code=CustomCode(
                members=("facade_portal.py", "facade_portal_aio.py"),
            ),
            methods={
                "post": MethodCustomizations(
                    param_typings=["connection_id: int", "message: Any", "data: Optional[bytes] = None"],
                    custom_logic="json = _to_json(message)",
                ),
                "narrowcast": MethodCustomizations(
                    param_typings=["tag: str", "message: Any", "data: Optional[bytes] = None"],
                    custom_logic="json = _to_json(message)",
                ),
                "broadcast": MethodCustomizations(
                    param_typings=["message: Any", "data: Optional[bytes] = None"],
                    custom_logic="json = _to_json(message)",
                ),
            },
        ),
        "Device": ObjectTypeCustomizations(
            methods={
                "spawn": MethodCustomizations(
                    param_typings=[
                        "program: Union[str, List[Union[str, bytes]]]",
                        "argv: Optional[List[Union[str, bytes]]] = None",
                        "envp: Optional[Mapping[str, str]] = None",
                        "env: Optional[Mapping[str, str]] = None",
                        "cwd: Optional[str] = None",
                        "stdio: Optional[str] = None",
                        "**aux: Any",
                    ],
                    custom_logic="""\
if not isinstance(program, str):
    args = list(program)
    argv = args if len(args) > 1 else None
    first = args[0]
    program = first.decode() if isinstance(first, bytes) else first

options = _frida.SpawnOptions()
if argv is not None:
    options.argv = [arg.decode() if isinstance(arg, bytes) else arg for arg in argv]
if envp is not None:
    options.envp = _to_envp(envp)
if env is not None:
    options.env = _to_envp(env)
if cwd is not None:
    options.cwd = cwd
if stdio is not None:
    options.stdio = stdio
if aux:
    options.aux = aux
""",
                ),
                "is_lost": MethodCustomizations(as_property=True),
                "resume": MethodCustomizations(param_typings=["target: ProcessTarget"], custom_logic=_RESOLVE_PID),
                "kill": MethodCustomizations(param_typings=["target: ProcessTarget"], custom_logic=_RESOLVE_PID),
                "input": MethodCustomizations(
                    param_typings=["target: ProcessTarget", "data: bytes"], custom_logic=_RESOLVE_PID
                ),
                "inject_library_file": MethodCustomizations(
                    param_typings=["target: ProcessTarget", "path: str", "entrypoint: str", "data: str"],
                    custom_logic=_RESOLVE_PID,
                ),
                "inject_library_blob": MethodCustomizations(
                    param_typings=["target: ProcessTarget", "blob: bytes", "entrypoint: str", "data: str"],
                    custom_logic=_RESOLVE_PID,
                ),
                "attach": MethodCustomizations(
                    param_typings=["target: ProcessTarget", "**kwargs: Any"],
                    custom_logic=_RESOLVE_PID,
                ),
            },
            custom_code=CustomCode(
                members=("facade_device_members.py", "facade_device_members_aio.py"),
            ),
        ),
        "Script": ObjectTypeCustomizations(
            provides_signals=True,
            custom_code=CustomCode(
                members=("facade_rpc_members.py", "facade_rpc_members_aio.py"),
                helpers=("facade_rpc_helpers.py", "facade_rpc_helpers_aio.py"),
            ),
            methods={
                "post": MethodCustomizations(
                    param_typings=["message: Any", "data: Optional[bytes] = None"],
                    custom_logic="json = _to_json(message)",
                ),
                "enable_debugger": MethodCustomizations(param_typings=["port: int = 0"]),
                "is_destroyed": MethodCustomizations(as_property=True),
            },
        ),
        "Session": ObjectTypeCustomizations(
            methods={
                "is_detached": MethodCustomizations(as_property=True),
                "create_script": MethodCustomizations(
                    param_typings=[
                        "source: str",
                        "name: Optional[str] = None",
                        "snapshot: Optional[bytes] = None",
                        "runtime: Optional[str] = None",
                    ],
                    custom_logic="""\
options = _frida.ScriptOptions()
if name is not None:
    options.name = name
if snapshot is not None:
    options.snapshot = snapshot
if runtime is not None:
    options.runtime = runtime""",
                ),
                "create_script_from_bytes": MethodCustomizations(
                    param_typings=[
                        "data: bytes",
                        "name: Optional[str] = None",
                        "snapshot: Optional[bytes] = None",
                        "runtime: Optional[str] = None",
                    ],
                    custom_logic="""\
bytes = data
options = _frida.ScriptOptions()
if name is not None:
    options.name = name
if snapshot is not None:
    options.snapshot = snapshot
if runtime is not None:
    options.runtime = runtime""",
                ),
                "compile_script": MethodCustomizations(
                    param_typings=["source: str", "name: Optional[str] = None", "runtime: Optional[str] = None"],
                    custom_logic="""\
options = _frida.ScriptOptions()
if name is not None:
    options.name = name
if runtime is not None:
    options.runtime = runtime""",
                ),
                "snapshot_script": MethodCustomizations(
                    param_typings=[
                        "embed_script: str",
                        "warmup_script: Optional[str] = None",
                        "runtime: Optional[str] = None",
                    ],
                    custom_logic="""\
options = _frida.SnapshotOptions()
if warmup_script is not None:
    options.warmup_script = warmup_script
if runtime is not None:
    options.runtime = runtime""",
                ),
            },
        ),
        "Service": ObjectTypeCustomizations(
            methods={
                "is_closed": MethodCustomizations(as_property=True),
            },
        ),
        "EndpointParameters": ObjectTypeCustomizations(
            custom_constructor="codegen_endpoint_parameters.c",
            constructor=ConstructorCustomizations(
                param_typings=[
                    "address: Optional[str] = None",
                    "port: Optional[int] = None",
                    "certificate: Optional[str] = None",
                    "origin: Optional[str] = None",
                    "authentication: Optional[Tuple[str, Any]] = None",
                    "asset_root: Optional[str] = None",
                    "request_handler: Optional[Callable[..., Any]] = None",
                ],
                custom_logic="""\
auth_service = None
if authentication is not None:
    if isinstance(authentication, tuple):
        scheme, data = authentication
        if scheme == "token":
            auth_service = _frida.StaticAuthenticationService(data)
        elif scheme == "callback":
            if not callable(data):
                raise ValueError("authentication data must be callable for the callback scheme")
            service = AuthenticationService.__new__(AuthenticationService)
            service.authenticate = lambda token: json.dumps(data(token))  # type: ignore[attr-defined]
            AuthenticationService.__init__(service)
            auth_service = service._impl
        else:
            raise ValueError("invalid authentication scheme")
    else:
        auth_service = _unwrap(authentication)

kwargs: Dict[str, Any] = {}
if address is not None:
    kwargs["address"] = address
if port is not None:
    kwargs["port"] = port
if certificate is not None:
    kwargs["certificate"] = certificate
if origin is not None:
    kwargs["origin"] = origin
if auth_service is not None:
    kwargs["auth_service"] = auth_service
if asset_root is not None:
    kwargs["asset_root"] = str(asset_root)

self._impl = _frida.EndpointParameters(**kwargs)

if request_handler is not None:
    self._impl.request_handler = _unwrap(request_handler)
""",
            ),
        ),
        "RelayKind": EnumerationCustomizations(
            members={
                "turn_udp": EnumerationMemberCustomizations(js_name="TurnUDP"),
                "turn_tcp": EnumerationMemberCustomizations(js_name="TurnTCP"),
                "turn_tls": EnumerationMemberCustomizations(js_name="TurnTLS"),
            },
        ),
        "ScriptRuntime": EnumerationCustomizations(
            members={
                "qjs": EnumerationMemberCustomizations(js_name="QJS"),
            },
        ),
        "ControlService": ObjectTypeCustomizations(
            methods={
                "get_endpoint_params": MethodCustomizations(drop=True),
            },
            properties={
                "endpoint-params": PropertyCustomizations(drop=True),
            },
        ),
        "Injector": ObjectTypeCustomizations(drop=True),
        "RpcClient": ObjectTypeCustomizations(drop=True),
        "RpcPeer": ObjectTypeCustomizations(drop=True),
        "Cancellable": ObjectTypeCustomizations(
            custom_code=CustomCode(
                members=("facade_cancellable.py", "facade_cancellable_aio.py"),
                helpers=("facade_cancellable_helpers.py", "facade_cancellable_helpers_aio.py"),
            ),
            methods={
                "is_cancelled": MethodCustomizations(as_property=True),
                "make_pollfd": MethodCustomizations(drop=True),
                "source_new": MethodCustomizations(drop=True),
            },
        ),
        "IOStream": ObjectTypeCustomizations(
            custom_code=CustomCode(
                members=("facade_iostream.py", "facade_iostream_aio.py"),
            ),
            methods={
                "close": MethodCustomizations(drop=True),
                "close_async": MethodCustomizations(suppress_facade=True),
                "splice_async": MethodCustomizations(drop=True),
                "has_pending": MethodCustomizations(drop=True),
                "set_pending": MethodCustomizations(drop=True),
                "clear_pending": MethodCustomizations(drop=True),
            },
        ),
        "InputStream": ObjectTypeCustomizations(
            methods={
                "close": MethodCustomizations(drop=True),
                "read": MethodCustomizations(drop=True),
                "read_async": MethodCustomizations(drop=True),
                "read_all": MethodCustomizations(drop=True),
                "read_all_async": MethodCustomizations(drop=True),
                "read_bytes": MethodCustomizations(drop=True),
                "skip": MethodCustomizations(drop=True),
                "is_closed": MethodCustomizations(drop=True),
                "has_pending": MethodCustomizations(drop=True),
                "set_pending": MethodCustomizations(drop=True),
                "clear_pending": MethodCustomizations(drop=True),
            },
        ),
        "OutputStream": ObjectTypeCustomizations(
            methods={
                "close": MethodCustomizations(drop=True),
                "flush": MethodCustomizations(drop=True),
                "write": MethodCustomizations(drop=True),
                "write_async": MethodCustomizations(drop=True),
                "write_all": MethodCustomizations(drop=True),
                "write_all_async": MethodCustomizations(drop=True),
                "write_bytes": MethodCustomizations(drop=True),
                "writev": MethodCustomizations(drop=True),
                "writev_async": MethodCustomizations(drop=True),
                "writev_all": MethodCustomizations(drop=True),
                "writev_all_async": MethodCustomizations(drop=True),
                "splice": MethodCustomizations(drop=True),
                "is_closing": MethodCustomizations(drop=True),
                "is_closed": MethodCustomizations(drop=True),
                "has_pending": MethodCustomizations(drop=True),
                "set_pending": MethodCustomizations(drop=True),
                "clear_pending": MethodCustomizations(drop=True),
            },
        ),
        "UnixSocketAddress": ObjectTypeCustomizations(
            methods={
                "get_path_len": MethodCustomizations(drop=True),
                "get_is_abstract": MethodCustomizations(drop=True),
            },
            properties={
                "abstract": PropertyCustomizations(drop=True),
                "path-as-array": PropertyCustomizations(drop=True),
            },
        ),
        "SocketAddress": ObjectTypeCustomizations(
            constructor=ConstructorCustomizations(drop=True),
            methods={
                "to_native": MethodCustomizations(drop=True),
            },
        ),
        "SocketConnectable": ObjectTypeCustomizations(drop_abstract_base=True),
        "InetAddress": ObjectTypeCustomizations(
            properties={
                "bytes": PropertyCustomizations(drop=True),
            },
        ),
        "Object": ObjectTypeCustomizations(
            methods={
                "is_floating": MethodCustomizations(drop=True),
                "ref": MethodCustomizations(drop=True),
                "ref_sink": MethodCustomizations(drop=True),
                "unref": MethodCustomizations(drop=True),
                "getv": MethodCustomizations(drop=True),
                "get_property": MethodCustomizations(drop=True),
                "set_property": MethodCustomizations(drop=True),
                "notify_by_pspec": MethodCustomizations(drop=True),
                "freeze_notify": MethodCustomizations(drop=True),
                "thaw_notify": MethodCustomizations(drop=True),
                "bind_property": MethodCustomizations(drop=True),
                "bind_property_full": MethodCustomizations(drop=True),
                "bind_property_with_closures": MethodCustomizations(drop=True),
                "force_floating": MethodCustomizations(drop=True),
                "get_data": MethodCustomizations(drop=True),
                "get_qdata": MethodCustomizations(drop=True),
                "run_dispose": MethodCustomizations(drop=True),
                "set_data": MethodCustomizations(drop=True),
                "steal_data": MethodCustomizations(drop=True),
                "steal_qdata": MethodCustomizations(drop=True),
                "watch_closure": MethodCustomizations(drop=True),
            },
            signals={
                "notify": SignalCustomizations(drop=True),
            },
        ),
    }

    return Customizations(
        type_customizations=type_customizations,
        facade_preludes=("facade_types.py",),
        facade_epilogues=("facade_epilogue.py",),
    )
