def _to_camel_case(name: str) -> str:
    result = ""
    capitalize = False
    for ch in name:
        if ch == "_":
            capitalize = True
        elif capitalize:
            result += ch.upper()
            capitalize = False
        else:
            result += ch
    return result


class RPCException(Exception):
    def __str__(self) -> str:
        return str(self.args[2]) if len(self.args) >= 3 else str(self.args[0])


class _ScriptExports:
    def __init__(self, script: "Script") -> None:
        self._script = script

    def __getattr__(self, name: str) -> Callable[..., Any]:
        script = self._script
        js_name = _to_camel_case(name)

        def method(*args: Any) -> Any:
            if args and isinstance(args[-1], bytes):
                params, data = list(args[:-1]), args[-1]
            else:
                params, data = list(args), None
            return script._rpc_request(["call", js_name, params], data)

        return method

    def __dir__(self) -> List[str]:
        return self._script._list_exports()


ScriptExportsSync = _ScriptExports
ScriptExports = ScriptExportsSync


@dataclasses.dataclass
class RPCResult:
    finished: bool = False
    value: Any = None
    error: Optional[Exception] = None


def make_rpc_call_request(js_name: str, args: Sequence[Any]) -> Tuple[List[Any], Optional[bytes]]:
    if args and isinstance(args[-1], bytes):
        raw_args = args[:-1]
        data = args[-1]
    else:
        raw_args = args
        data = None
    return (["call", js_name, raw_args], data)
