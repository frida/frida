class _Implementation:
    def _frida_dispatch(self, name: str, args: List[Any], completion: Any) -> None:
        def run() -> None:
            try:
                result = _unwrap(getattr(self, name)(*[_wrap(a) for a in args]))
                error = None
            except Exception as e:
                result = None
                error = e
            _frida._complete_request(completion, result, error)

        threading.Thread(target=run, daemon=True).start()
