async def close(self) -> None:
    await _invoke(self._impl.close_async, (0,))

async def read(self, count: int) -> bytes:
    return await _invoke(self._impl.input_stream.read_bytes_async, (count, 0))

async def read_all(self, count: int) -> bytes:
    chunks = []
    remaining = count
    while remaining > 0:
        chunk = await _invoke(self._impl.input_stream.read_bytes_async, (remaining, 0))
        if not chunk:
            break
        chunks.append(chunk)
        remaining -= len(chunk)
    return b"".join(chunks)

async def write(self, data: bytes) -> int:
    return await _invoke(self._impl.output_stream.write_bytes_async, (data, 0))

async def write_all(self, data: bytes) -> None:
    view = memoryview(data)
    while len(view) != 0:
        written = await _invoke(self._impl.output_stream.write_bytes_async, (bytes(view), 0))
        view = view[written:]
