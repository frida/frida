"""Rendering of JavaScript values inspected by the REPL agent.

The agent encodes each evaluated value into a bounded tagged tree, so that
serializing cyclic or very large objects can never hang. This module
pretty-prints that tree into prompt_toolkit formatted text, which the caller
downsamples to the terminal's color depth.
"""

from typing import Any, List, Sequence, Tuple

from prompt_toolkit.formatted_text import FormattedText

# Tags mirror ValueTag in agents/repl/agent.ts.
NUMBER = 0
STRING = 1
OBJECT = 2
ARRAY = 3
NATIVE_POINTER = 4
NULL = 5
BOOLEAN = 6
BYTES = 7
FUNCTION = 8
ERROR = 9
UNDEFINED = 10
BIGINT = 11
SYMBOL = 12
DATE = 13
REGEXP = 14
MAP = 15
SET = 16
PROMISE = 17
WEAKMAP = 18
WEAKSET = 19
DEPTH_LIMIT = 20
CIRCULAR = 21

_CYAN = "fg:#64d2ff"
_MINT = "fg:#66d4cf"
_KEY = "fg:#32b64f"
_ORANGE = "fg:#ff9f0a"
_PURPLE = "fg:#bf5af2"
_RED = "fg:#ff453a"
_BLUE = "fg:#0a84ff"
_GRAY = "fg:#98989d"

_MAX_ITEMS = 100

_HEX_BYTES_PER_ROW = 16

_CONTAINER_NAMES = {OBJECT: "Object", ARRAY: "Array", MAP: "Map", SET: "Set"}

Node = Sequence[Any]
Fragment = Tuple[str, str]


def render(tree: Node, blob: Sequence[int]) -> FormattedText:
    out: List[Fragment] = []
    _append(tree, 0, out, blob)
    return FormattedText(out)


def render_hexdump(data: bytes) -> FormattedText:
    out: List[Fragment] = []
    for offset in range(0, len(data), _HEX_BYTES_PER_ROW):
        if offset > 0:
            out.append(("", "\n"))
        _hex_row(out, offset, data[offset:offset + _HEX_BYTES_PER_ROW])
    return FormattedText(out)


def is_undefined(tree: Node) -> bool:
    return tree[0] == UNDEFINED


def to_string_list(tree: Node) -> List[str]:
    return [element[1] for element in tree[2]]


def _append(node: Node, level: int, out: List[Fragment], blob: Sequence[int]) -> None:
    tag = node[0]

    if tag == NUMBER:
        out.append((_CYAN, _format_number(node[1])))

    elif tag == STRING:
        out.append((_MINT, '"%s"' % node[1]))

    elif tag == OBJECT:
        _append_object(node, level, out, blob)

    elif tag == ARRAY:
        _append_array(node, level, out, blob)

    elif tag == NATIVE_POINTER:
        out.append((_ORANGE, node[1]))

    elif tag == NULL:
        out.append((_ORANGE, "null"))

    elif tag == BOOLEAN:
        out.append((_ORANGE, "true" if node[1] else "false"))

    elif tag == BYTES:
        _append_bytes(node, level, out, blob)

    elif tag == FUNCTION:
        out.append((_PURPLE, node[1]))

    elif tag == ERROR:
        _append_error(node, level, out)

    elif tag == UNDEFINED:
        out.append((_ORANGE, "undefined"))

    elif tag == BIGINT:
        out.append((_CYAN, node[1] + "n"))

    elif tag == SYMBOL:
        out.append((_PURPLE, node[1]))

    elif tag == DATE:
        out.append((_BLUE, "Date("))
        out.append((_MINT, node[1]))
        out.append((_BLUE, ")"))

    elif tag == REGEXP:
        _append_regexp(node, out)

    elif tag == MAP:
        _append_map(node, level, out, blob)

    elif tag == SET:
        _append_set(node, level, out, blob)

    elif tag == PROMISE:
        out.append((_PURPLE, "Promise"))

    elif tag == WEAKMAP:
        out.append((_PURPLE, "WeakMap"))

    elif tag == WEAKSET:
        out.append((_PURPLE, "WeakSet"))

    elif tag == DEPTH_LIMIT:
        out.append((_ORANGE, "%s<depth limit reached>" % _CONTAINER_NAMES[node[1]]))

    elif tag == CIRCULAR:
        out.append((_ORANGE, "⟳ circular *%d" % node[1]))


def _append_object(node: Node, level: int, out: List[Fragment], blob: Sequence[int]) -> None:
    properties = node[2]
    if not properties:
        out.append((_CYAN, "{}"))
        return

    out.append((_CYAN, "{"))
    out.append(("", "\n"))
    shown, hidden = _capped(properties)
    for index, (key, value) in enumerate(shown):
        _indent(level + 1, out)
        out.append((_KEY, key[1]))
        out.append(("", ": "))
        _append(value, level + 1, out, blob)
        if _has_more(index, shown, hidden):
            out.append(("", ","))
        out.append(("", "\n"))
    _append_overflow(hidden, level + 1, out)
    _indent(level, out)
    out.append((_CYAN, "}"))


def _append_array(node: Node, level: int, out: List[Fragment], blob: Sequence[int]) -> None:
    elements = node[2]
    if not elements:
        out.append(("", "[]"))
        return

    out.append(("", "["))
    out.append(("", "\n"))
    shown, hidden = _capped(elements)
    for index, element in enumerate(shown):
        _indent(level + 1, out)
        _append(element, level + 1, out, blob)
        if _has_more(index, shown, hidden):
            out.append(("", ","))
        out.append(("", "\n"))
    _append_overflow(hidden, level + 1, out)
    _indent(level, out)
    out.append(("", "]"))


def _append_bytes(node: Node, level: int, out: List[Fragment], blob: Sequence[int]) -> None:
    offset, length, kind = node[1], node[2], node[3]
    out.append((_MINT, "Bytes("))
    out.append((_CYAN, kind))
    out.append((_MINT, "[%d])" % length))
    data = bytes(blob[offset:offset + length])
    for start in range(0, len(data), _HEX_BYTES_PER_ROW):
        out.append(("", "\n"))
        _indent(level + 1, out)
        _hex_row(out, start, data[start:start + _HEX_BYTES_PER_ROW])


def _append_error(node: Node, level: int, out: List[Fragment]) -> None:
    name, message, stack = node[1], node[2], node[3]
    out.append((_RED, name if not message else "%s: %s" % (name, message)))
    if not stack:
        return
    for line in stack.split("\n"):
        out.append(("", "\n"))
        _indent(level + 1, out)
        out.append((_GRAY, line))


def _append_regexp(node: Node, out: List[Fragment]) -> None:
    pattern, flags = node[1], node[2]
    out.append((_PURPLE, "/"))
    out.append((_MINT, pattern))
    out.append((_PURPLE, "/"))
    if flags:
        out.append((_PURPLE, flags))


def _append_map(node: Node, level: int, out: List[Fragment], blob: Sequence[int]) -> None:
    entries = node[2]
    if not entries:
        out.append((_CYAN, "Map{}"))
        return

    out.append((_CYAN, "Map{"))
    out.append(("", "\n"))
    shown, hidden = _capped(entries)
    for index, (key, value) in enumerate(shown):
        _indent(level + 1, out)
        _append(key, level + 1, out, blob)
        out.append(("", " => "))
        _append(value, level + 1, out, blob)
        if _has_more(index, shown, hidden):
            out.append(("", ","))
        out.append(("", "\n"))
    _append_overflow(hidden, level + 1, out)
    _indent(level, out)
    out.append((_CYAN, "}"))


def _append_set(node: Node, level: int, out: List[Fragment], blob: Sequence[int]) -> None:
    elements = node[2]
    if not elements:
        out.append((_CYAN, "Set[]"))
        return

    out.append((_CYAN, "Set["))
    out.append(("", "\n"))
    shown, hidden = _capped(elements)
    for element in shown:
        _indent(level + 1, out)
        out.append((_CYAN, "• "))
        _append(element, level + 1, out, blob)
        out.append(("", "\n"))
    _append_overflow(hidden, level + 1, out)
    _indent(level, out)
    out.append((_CYAN, "]"))


def _capped(items: Sequence[Any]) -> Tuple[Sequence[Any], int]:
    shown = items[:_MAX_ITEMS]
    return shown, len(items) - len(shown)


def _has_more(index: int, shown: Sequence[Any], hidden: int) -> bool:
    return index < len(shown) - 1 or hidden > 0


def _append_overflow(hidden: int, level: int, out: List[Fragment]) -> None:
    if not hidden:
        return
    _indent(level, out)
    out.append((_ORANGE, "… %d more" % hidden))
    out.append(("", "\n"))


def _hex_row(out: List[Fragment], offset: int, chunk: bytes) -> None:
    out.append((_KEY, "%08X" % offset))
    out.append(("", "  "))
    for index, byte in enumerate(chunk):
        if index > 0:
            out.append(("", " "))
        out.append((_byte_color(byte), "%02X" % byte))
    out.append(("", "   " * (_HEX_BYTES_PER_ROW - len(chunk)) + "  "))
    for byte in chunk:
        out.append((_GRAY, _ascii_glyph(byte)))


def _byte_color(byte: int) -> str:
    if byte == 0x00:
        return _GRAY
    if 0x20 <= byte <= 0x7E:
        return _MINT
    if byte <= 0x1F or byte == 0x7F:
        return _ORANGE
    return _CYAN


def _ascii_glyph(byte: int) -> str:
    return chr(byte) if 0x20 <= byte <= 0x7E else "."


def _format_number(value: Any) -> str:
    if value is None:
        return "null"
    if isinstance(value, float):
        return str(int(value)) if value.is_integer() else repr(value)
    return str(value)


def _indent(level: int, out: List[Fragment]) -> None:
    if level > 0:
        out.append(("", "  " * level))
