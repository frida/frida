async def get_device_matching(self, predicate: Callable[["Device"], bool], timeout: Union[int, float] = 0) -> "Device":
    deadline = time.monotonic() + timeout
    while True:
        for device in await self.enumerate_devices():
            if predicate(device):
                return device
        if time.monotonic() >= deadline:
            raise _frida.InvalidArgumentError("no matching device found")
        await asyncio.sleep(0.05)


async def get_device(self, id: Optional[str], timeout: Union[int, float] = 0) -> "Device":
    return await self.get_device_matching(lambda d: d.id == id, timeout)


async def get_local_device(self) -> "Device":
    return await self.get_device_matching(lambda d: d.type == "local", 0)


async def get_remote_device(self) -> "Device":
    return await self.get_device_matching(lambda d: d.type == "remote", 0)


async def get_usb_device(self, timeout: Union[int, float] = 0) -> "Device":
    return await self.get_device_matching(lambda d: d.type == "usb", timeout)
