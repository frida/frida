def _setup(self) -> None:
    self._message_handlers: List[BusMessageCallback] = []
    self._loop = asyncio.get_event_loop()
    self._impl.on("message", self._on_message)

def on(self, signal: str, callback: Callable[..., Any]) -> None:
    if signal == "message":
        self._message_handlers.append(callback)
    else:
        self._impl.on(signal, _make_signal_handler(callback))

def off(self, signal: str, callback: Callable[..., Any]) -> None:
    if signal == "message":
        self._message_handlers.remove(callback)
    else:
        self._impl.off(signal, callback)

def _on_message(self, raw_message: str, data: Optional[bytes]) -> None:
    message = json.loads(raw_message)
    for handler in list(self._message_handlers):
        _dispatch(self._loop, self._deliver_message, handler, message, data)

def _deliver_message(self, handler: BusMessageCallback, message: Mapping[Any, Any], data: Optional[bytes]) -> None:
    try:
        handler(message, data)
    except Exception:
        traceback.print_exc()
