__version__ = _frida.__version__


def get_device_manager() -> "DeviceManager":
    return _wrap(_frida.get_device_manager())


def get_device(id: str, timeout: Union[int, float] = 0) -> "Device":
    return get_device_manager().get_device_by_id(id, int(timeout * 1000))


def get_device_matching(predicate: Callable[["Device"], bool], timeout: Union[int, float] = 0) -> "Device":
    return get_device_manager().get_device_matching(predicate, timeout)


def get_local_device() -> "Device":
    return get_device_manager().get_device_by_type("local", 0)


def get_remote_device() -> "Device":
    return get_device_manager().get_device_by_type("remote", 0)


def get_usb_device(timeout: Union[int, float] = 0) -> "Device":
    return get_device_manager().get_device_by_type("usb", int(timeout * 1000))


def enumerate_devices() -> List["Device"]:
    return get_device_manager().enumerate_devices()


def query_system_parameters() -> Dict[str, Any]:
    return get_local_device().query_system_parameters()


def spawn(
    program: Union[str, List[Union[str, bytes]]],
    argv: Optional[List[Union[str, bytes]]] = None,
    envp: Optional[Dict[str, str]] = None,
    env: Optional[Dict[str, str]] = None,
    cwd: Optional[str] = None,
    stdio: Optional[str] = None,
    **aux: Any,
) -> int:
    return get_local_device().spawn(
        program, argv=argv, envp=envp, env=env, cwd=cwd, stdio=stdio, **aux
    )


def resume(target: ProcessTarget) -> None:
    return get_local_device().resume(target)


def kill(target: ProcessTarget) -> None:
    return get_local_device().kill(target)


def attach(target: ProcessTarget, **kwargs: Any) -> "Session":
    return get_local_device().attach(target, **kwargs)


def inject_library_file(target: ProcessTarget, path: str, entrypoint: str, data: str) -> int:
    return get_local_device().inject_library_file(target, path, entrypoint, data)


def inject_library_blob(target: ProcessTarget, blob: bytes, entrypoint: str, data: str) -> int:
    return get_local_device().inject_library_blob(target, blob, entrypoint, data)


def shutdown() -> None:
    get_device_manager().close()


def make_auth_callback(callback: Callable[[str], Any]) -> Callable[[Any], str]:
    def authenticate(token: str) -> str:
        session_info = callback(token)
        return json.dumps(session_info)

    return authenticate
