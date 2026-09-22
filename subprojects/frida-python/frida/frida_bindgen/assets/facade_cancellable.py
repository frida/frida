def __enter__(self) -> "Cancellable":
    stack = getattr(_current_cancellable, "stack", None)
    if stack is None:
        stack = []
        _current_cancellable.stack = stack
    stack.append(self)
    return self

def __exit__(self, *exc: Any) -> Literal[False]:
    _current_cancellable.stack.pop()
    return False

@classmethod
def get_current(cls) -> "Cancellable":
    return _current_cancellable_get()

def raise_if_cancelled(self) -> None:
    if self._impl.is_cancelled():
        raise _frida.OperationCancelledError("operation was cancelled")

def get_pollfd(self) -> "CancellablePollFD":
    return CancellablePollFD(self._impl)


def _setup(self) -> None:
    self._cancel_handlers: Dict[int, Callable[..., Any]] = {}
    self._next_cancel_handler_id = 1


def connect(self, callback: Callable[..., Any]) -> int:
    if self._impl.is_cancelled():
        callback()
        return 0

    handler_id = self._next_cancel_handler_id
    self._next_cancel_handler_id = handler_id + 1

    def handler(*args: Any) -> None:
        callback()

    self._cancel_handlers[handler_id] = handler
    self._impl.on("cancelled", handler)
    return handler_id


def disconnect(self, handler_id: int) -> None:
    handler = self._cancel_handlers.pop(handler_id, None)
    if handler is not None:
        self._impl.off("cancelled", handler)
