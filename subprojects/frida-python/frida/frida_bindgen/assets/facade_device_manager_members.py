def get_device_matching(self, predicate: Callable[["Device"], bool], timeout: Union[int, float] = 0) -> "Device":
    deadline = time.monotonic() + timeout
    while True:
        for device in self.enumerate_devices():
            if predicate(device):
                return device
        if time.monotonic() >= deadline:
            raise _frida.InvalidArgumentError("no matching device found")
        time.sleep(0.05)


def get_device(self, id: Optional[str], timeout: Union[int, float] = 0) -> "Device":
    return self.get_device_matching(lambda d: d.id == id, timeout)


def get_local_device(self) -> "Device":
    return self.get_device_matching(lambda d: d.type == "local", 0)


def get_remote_device(self) -> "Device":
    return self.get_device_matching(lambda d: d.type == "remote", 0)


def get_usb_device(self, timeout: Union[int, float] = 0) -> "Device":
    return self.get_device_matching(lambda d: d.type == "usb", timeout)
