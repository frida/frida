def _setup(self) -> None:
    self._authenticated_handlers: List[PortalServiceAuthenticatedCallback] = []
    self._message_handlers: List[PortalServiceMessageCallback] = []
    self._impl.on("authenticated", self._on_authenticated)
    self._impl.on("message", self._on_message)

def on(self, signal: str, callback: Callable[..., Any]) -> None:
    if signal == "authenticated":
        self._authenticated_handlers.append(callback)
    elif signal == "message":
        self._message_handlers.append(callback)
    else:
        self._impl.on(signal, _make_signal_handler(callback))

def off(self, signal: str, callback: Callable[..., Any]) -> None:
    if signal == "authenticated":
        self._authenticated_handlers.remove(callback)
    elif signal == "message":
        self._message_handlers.remove(callback)
    else:
        self._impl.off(signal, callback)

def _on_authenticated(self, connection_id: int, raw_session_info: str) -> None:
    session_info = json.loads(raw_session_info)
    for handler in list(self._authenticated_handlers):
        try:
            handler(connection_id, session_info)
        except Exception:
            traceback.print_exc()

def _on_message(self, connection_id: int, raw_message: str, data: Optional[bytes]) -> None:
    message = json.loads(raw_message)
    for handler in list(self._message_handlers):
        try:
            handler(connection_id, message, data)
        except Exception:
            traceback.print_exc()
