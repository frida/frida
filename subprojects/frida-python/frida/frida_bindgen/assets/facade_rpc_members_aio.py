def _setup(self) -> None:
    self.exports = _ScriptExports(self)
    self._message_handlers: List[ScriptMessageCallback] = []
    self._log_handler: Callable[[str, str], None] = self.default_log_handler
    self._pending: Dict[int, Callable[[Any, Optional[Exception]], None]] = {}
    self._next_request_id = 1
    self._loop = asyncio.get_event_loop()
    self._impl.on("message", self._on_message)
    self._impl.on("destroyed", self._on_destroyed)

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

def get_log_handler(self) -> Callable[[str, str], None]:
    return self._log_handler

def set_log_handler(self, handler: Callable[[str, str], None]) -> None:
    self._log_handler = handler

def default_log_handler(self, level: str, text: str) -> None:
    if level == "info":
        print(text, file=sys.stdout)
    else:
        print(text, file=sys.stderr)

async def list_exports(self) -> List[str]:
    return await self._rpc_request(["list"])

def _rpc_request(self, args: Any, data: Optional[bytes] = None) -> "asyncio.Future[Any]":
    loop = asyncio.get_running_loop()
    future: "asyncio.Future[Any]" = loop.create_future()

    def complete(value: Any, error: Optional[Exception]) -> None:
        if future.done():
            return
        if error is not None:
            future.set_exception(error)
        else:
            future.set_result(value)

    request_id = self._next_request_id
    self._next_request_id += 1
    self._pending[request_id] = lambda value, error: _dispatch(loop, complete, value, error)

    if self._impl.is_destroyed():
        self._on_destroyed()
    else:
        self.post(["frida:rpc", request_id] + list(args), data)

    return future

def _on_message(self, raw_message: str, data: Optional[bytes]) -> None:
    message = json.loads(raw_message)
    mtype = message["type"]
    payload = message.get("payload")
    if mtype == "log":
        _dispatch(self._loop, self._log_handler, message["level"], payload)
    elif mtype == "send" and isinstance(payload, list) and payload[:1] == ["frida:rpc"]:
        self._on_rpc_message(payload[1], payload[2], payload[3:], data)
    else:
        for handler in list(self._message_handlers):
            _dispatch(self._loop, self._deliver_message, handler, message, data)

def _deliver_message(self, handler: ScriptMessageCallback, message: ScriptMessage, data: Optional[bytes]) -> None:
    try:
        handler(message, data)
    except Exception:
        traceback.print_exc()

def _on_destroyed(self) -> None:
    while True:
        pending_ids = list(self._pending.keys())
        complete = self._pending.pop(pending_ids[0]) if pending_ids else None
        if complete is None:
            break
        complete(None, _frida.InvalidOperationError("script has been destroyed"))

def _on_rpc_message(self, request_id: int, operation: str, params: List[Any], data: Optional[Any]) -> None:
    if operation not in ("ok", "error"):
        return
    complete = self._pending.pop(request_id, None)
    if complete is None:
        return
    if operation == "error":
        complete(None, RPCException(*params[0:3]))
    elif data is not None:
        complete((params[1], data) if len(params) > 1 else data, None)
    else:
        complete(params[0], None)
