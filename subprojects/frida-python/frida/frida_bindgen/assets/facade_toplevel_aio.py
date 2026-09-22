__version__ = _frida.__version__


def get_device_manager() -> "DeviceManager":
    return _wrap(_frida.get_device_manager())


async def get_device(id: str, timeout: Union[int, float] = 0) -> "Device":
    return await get_device_manager().get_device_by_id(id, int(timeout * 1000))


async def get_device_matching(predicate: Callable[["Device"], bool], timeout: Union[int, float] = 0) -> "Device":
    return await get_device_manager().get_device_matching(predicate, timeout)


async def get_local_device() -> "Device":
    return await get_device_manager().get_device_by_type("local", 0)


async def get_remote_device() -> "Device":
    return await get_device_manager().get_device_by_type("remote", 0)


async def get_usb_device(timeout: Union[int, float] = 0) -> "Device":
    return await get_device_manager().get_device_by_type("usb", int(timeout * 1000))


async def enumerate_devices() -> List["Device"]:
    return await get_device_manager().enumerate_devices()


async def query_system_parameters() -> Dict[str, Any]:
    return await (await get_local_device()).query_system_parameters()


async def spawn(
    program: Union[str, List[Union[str, bytes]]],
    argv: Optional[List[Union[str, bytes]]] = None,
    envp: Optional[Dict[str, str]] = None,
    env: Optional[Dict[str, str]] = None,
    cwd: Optional[str] = None,
    stdio: Optional[str] = None,
    **aux: Any,
) -> int:
    device = await get_local_device()
    return await device.spawn(
        program, argv=argv, envp=envp, env=env, cwd=cwd, stdio=stdio, **aux
    )


async def resume(target: ProcessTarget) -> None:
    return await (await get_local_device()).resume(target)


async def kill(target: ProcessTarget) -> None:
    return await (await get_local_device()).kill(target)


async def attach(target: ProcessTarget, **kwargs: Any) -> "Session":
    return await (await get_local_device()).attach(target, **kwargs)


async def inject_library_file(target: ProcessTarget, path: str, entrypoint: str, data: str) -> int:
    return await (await get_local_device()).inject_library_file(target, path, entrypoint, data)


async def inject_library_blob(target: ProcessTarget, blob: bytes, entrypoint: str, data: str) -> int:
    return await (await get_local_device()).inject_library_blob(target, blob, entrypoint, data)


async def shutdown() -> None:
    await get_device_manager().close()


def make_auth_callback(callback: Callable[[str], Any]) -> Callable[[Any], str]:
    def authenticate(token: str) -> str:
        session_info = callback(token)
        return json.dumps(session_info)

    return authenticate
