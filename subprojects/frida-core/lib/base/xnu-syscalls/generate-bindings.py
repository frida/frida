#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple

MAX_ARGS = 9


@dataclass(frozen=True)
class SyscallArg:
    type: str
    name: str


@dataclass(frozen=True)
class SyscallSignature:
    name: str
    args: Tuple[SyscallArg, ...]


MACH_TRAP_INDEXED_CALL_RE = re.compile(r"/\*\s*(?P<idx>\d+)\s*\*/\s*MACH_TRAP\s*\(")
MACH_TRAPS_IFNDEF_KERNEL_RE = re.compile(r"^\s*#ifndef\s+KERNEL\b", re.MULTILINE)
MACH_TRAPS_ELSE_KERNEL_RE = re.compile(r"^\s*#else\b.*\bKERNEL\b", re.MULTILINE)

USER_EXTERN_PROTO_RE = re.compile(
    r"^\s*extern\s+(?P<ret>.+?)\s+(?P<fn>[A-Za-z_]\w*)\s*\((?P<args>.*?)\)\s*;",
    re.MULTILINE | re.DOTALL,
)
KERNEL_STRUCT_RE = re.compile(
    r"\bstruct\s+(?P<name>[A-Za-z_]\w*)\s*\{\s*(?P<body>.*?)\s*\}\s*;",
    re.DOTALL,
)
KERNEL_TRAP_EXTERN_RE = re.compile(
    r"^\s*(?:extern\s+)?(?P<ret>.+?)\s+(?P<fn>[A-Za-z_]\w*)\s*\(\s*struct\s+(?P<args_struct>[A-Za-z_]\w*)\s*\*\s*args\s*\)\s*;",
    re.MULTILINE | re.DOTALL,
)
PAD_ARG_RE = re.compile(
    r"\bPAD_ARG_\s*\(\s*(?P<ty>[^,]+?)\s*,\s*(?P<name>[^)]+?)\s*\)\s*;",
)
PLAIN_FIELD_RE = re.compile(r"^\s*(?P<ty>.+?)\s+(?P<name>[A-Za-z_]\w*)\s*;\s*$")

BSD_MASTER_ENTRY_RE = re.compile(
    r"^\s*(?P<nr>\d+)\s+(?P<audit>\S+)\s+(?P<files>\S+)\s+\{\s*(?P<proto>[^}]+?)\s*\}\s*(?:\{.*\})?\s*$"
)
BSD_MASTER_PROTO_RE = re.compile(
    r"^\s*(?P<ret>.+?)\s+(?P<fn>[A-Za-z_]\w*)\s*\((?P<args>.*)\)\s*(?P<attrs>.*)$",
    re.DOTALL,
)

LINE_COMMENT_RE = re.compile(r"//.*$")
BLOCK_COMMENT_RE = re.compile(r"/\*[^*]*\*+(?:[^/*][^*]*\*+)*/")

TRAILING_ARRAY_SUFFIX_RE = re.compile(r"\[[^\]]*\]\s*$")
POINTER_SPACING_RE = re.compile(r"\s*\*\s*")
PARAM_NAME_RE = re.compile(r"[A-Za-z_]\w*$")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--kernel-tree", required=True)
    ap.add_argument("--out-vala", required=True)
    ap.add_argument("--out-c", required=True)
    args = ap.parse_args()

    kernel_tree = Path(args.kernel_tree).resolve()
    if not kernel_tree.exists():
        raise SystemExit(f"--kernel-tree does not exist: {kernel_tree}")

    mach_numbers, mach_sigs = load_mach_traps(kernel_tree)
    bsd_numbers, bsd_sigs = load_bsd_syscalls_master(kernel_tree)

    vala = render_vala(
        mach_numbers=mach_numbers,
        bsd_numbers=bsd_numbers,
    )
    c_src = render_c_source(
        mach_numbers=mach_numbers,
        bsd_numbers=bsd_numbers,
        mach_sigs=mach_sigs,
        bsd_sigs=bsd_sigs,
    )

    out_vala = Path(args.out_vala)
    out_c = Path(args.out_c)
    out_vala.parent.mkdir(parents=True, exist_ok=True)
    out_c.parent.mkdir(parents=True, exist_ok=True)
    out_vala.write_text(vala, encoding="utf-8")
    out_c.write_text(c_src, encoding="utf-8")

    return 0


def load_mach_traps(
    kernel_tree: Path,
) -> Tuple[Dict[int, str], Dict[str, SyscallSignature]]:
    table_path = kernel_tree / "osfmk" / "kern" / "syscall_sw.c"
    if not table_path.exists():
        raise RuntimeError(f"Missing Mach trap table source: {table_path}")

    header_path = kernel_tree / "osfmk" / "mach" / "mach_traps.h"
    if not header_path.exists():
        raise RuntimeError(f"Missing Mach traps header: {header_path}")

    table_text = table_path.read_text(encoding="utf-8", errors="ignore")
    trap_routines = parse_mach_trap_table(table_text, table_path)

    header_text = header_path.read_text(encoding="utf-8", errors="ignore")
    userspace_text, kernel_text = split_mach_traps_header(header_text, header_path)

    userspace_protos = parse_userspace_mach_traps_prototypes(
        userspace_text, header_path
    )
    kernel_argmap = parse_kernel_mach_traps_args(kernel_text, header_path)

    numbers: Dict[int, str] = {}
    sigs: Dict[str, SyscallSignature] = {}

    for index, routine in trap_routines:
        if routine == "kern_invalid":
            continue

        nr = -index
        public_name = normalize_mach_name(routine)
        if public_name == "":
            raise RuntimeError(f"{table_path}: empty normalized name from {routine!r}")

        if nr in numbers:
            raise RuntimeError(f"{table_path}: duplicate mach trap number {nr}")
        if public_name in sigs:
            raise RuntimeError(
                f"{table_path}: normalized Mach name collision for {public_name!r}"
            )

        args = resolve_mach_trap_signature(
            routine=routine,
            userspace_protos=userspace_protos,
            kernel_argmap=kernel_argmap,
            table_path=table_path,
        )

        if len(args) > MAX_ARGS:
            raise RuntimeError(
                f"{table_path}: trap {routine!r} has {len(args)} args; max supported is {MAX_ARGS}"
            )

        numbers[nr] = public_name
        sigs[public_name] = SyscallSignature(name=public_name, args=tuple(args))

    return numbers, sigs


def parse_mach_trap_table(text: str, path: Path) -> List[Tuple[int, str]]:
    anchor = text.find("mach_trap_table")
    if anchor == -1:
        raise RuntimeError(f"{path}: mach_trap_table not found")

    brace = text.find("{", anchor)
    if brace == -1:
        raise RuntimeError(f"{path}: mach_trap_table '{{' not found")

    payload, _ = extract_brace_payload(text, brace, path)

    by_index: Dict[int, str] = {}

    off = 0
    while True:
        m = MACH_TRAP_INDEXED_CALL_RE.search(payload, off)
        if m is None:
            break

        idx = int(m.group("idx"), 10)
        open_paren = m.end(0) - 1
        args_blob, end_off = extract_parentheses_payload(payload, open_paren, path)

        parts = [
            a.strip() for a in split_top_level_commas(args_blob) if a.strip() != ""
        ]
        if len(parts) < 1:
            raise RuntimeError(f"{path}: bad MACH_TRAP() entry: {args_blob!r}")

        routine = parts[0]

        prev = by_index.get(idx)
        if prev is None:
            by_index[idx] = routine
        else:
            if prev == routine:
                pass
            elif prev == "kern_invalid" and routine != "kern_invalid":
                by_index[idx] = routine
            elif routine == "kern_invalid" and prev != "kern_invalid":
                pass
            else:
                raise RuntimeError(
                    f"{path}: conflicting MACH_TRAP entries for index {idx}: {prev!r} vs {routine!r}"
                )

        off = end_off

    return sorted(by_index.items(), key=lambda kv: kv[0])


def split_mach_traps_header(text: str, path: Path) -> Tuple[str, str]:
    m_start = MACH_TRAPS_IFNDEF_KERNEL_RE.search(text)
    if m_start is None:
        raise RuntimeError(f"{path}: missing '#ifndef KERNEL'")

    m_else = MACH_TRAPS_ELSE_KERNEL_RE.search(text, m_start.end())
    if m_else is None:
        raise RuntimeError(f"{path}: missing '#else ... KERNEL ...'")

    userspace = text[m_start.end():m_else.start()]
    kernel = text[m_else.end():]
    return userspace, kernel


def parse_userspace_mach_traps_prototypes(
    userspace_text: str, path: Path
) -> Dict[str, Tuple[SyscallArg, ...]]:
    protos: Dict[str, Tuple[SyscallArg, ...]] = {}

    for m in USER_EXTERN_PROTO_RE.finditer(userspace_text):
        fn = m.group("fn").strip()
        args_blob = m.group("args").strip()
        ret_blob = strip_c_comments(m.group("ret")).strip()
        if ret_blob == "":
            raise RuntimeError(f"{path}: empty return type for {fn!r}")

        line_no = line_number_at_offset(userspace_text, m.start())

        args = parse_c_param_list(args_blob, path, line_no)
        cur = tuple(args)

        prev = protos.get(fn)
        if prev is not None and prev != cur:
            raise RuntimeError(f"{path}:{line_no}: conflicting prototype for {fn!r}")

        protos[fn] = cur

    return protos


def parse_kernel_mach_traps_args(
    kernel_text: str, path: Path
) -> Dict[str, Tuple[SyscallArg, ...]]:
    text = strip_c_comments(kernel_text)

    structs: Dict[str, str] = {}
    for m in KERNEL_STRUCT_RE.finditer(text):
        name = m.group("name").strip()
        body = m.group("body")
        structs[name] = body

    fn_to_struct: Dict[str, str] = {}
    for m in KERNEL_TRAP_EXTERN_RE.finditer(text):
        fn = m.group("fn").strip()
        args_struct = m.group("args_struct").strip()
        fn_to_struct[fn] = args_struct

    out: Dict[str, Tuple[SyscallArg, ...]] = {}

    for fn, args_struct in fn_to_struct.items():
        body = structs.get(args_struct)
        if body is None:
            continue

        args = parse_mach_args_struct_body(body, path)
        out[fn] = tuple(args)

    return out


def parse_mach_args_struct_body(body: str, path: Path) -> List[SyscallArg]:
    args: List[SyscallArg] = []

    body = strip_c_comments(body)
    for raw_ln in body.splitlines():
        ln = raw_ln.strip()
        if ln == "" or ln.startswith("#"):
            continue
        if "PAD_ARG_8" in ln:
            continue

        m = PAD_ARG_RE.search(ln)
        if m is not None:
            ty = normalize_whitespace(m.group("ty").strip())
            name = normalize_whitespace(m.group("name").strip())
            if ty == "" or name == "":
                raise RuntimeError(f"{path}: bad PAD_ARG_ in args struct: {raw_ln!r}")
            args.append(SyscallArg(type=ty, name=name))
            continue

        m = PLAIN_FIELD_RE.match(ln)
        if m is not None:
            ty = normalize_whitespace(m.group("ty").strip())
            name = m.group("name").strip()
            if ty == "" or name == "":
                raise RuntimeError(f"{path}: bad field in args struct: {raw_ln!r}")
            args.append(SyscallArg(type=ty, name=name))
            continue

    if len(args) == 1 and args[0].name == "dummy":
        return []

    return args


def resolve_mach_trap_signature(
    *,
    routine: str,
    userspace_protos: Dict[str, Tuple[SyscallArg, ...]],
    kernel_argmap: Dict[str, Tuple[SyscallArg, ...]],
    table_path: Path,
) -> List[SyscallArg]:
    def get_userspace(name: str) -> Optional[Tuple[SyscallArg, ...]]:
        return userspace_protos.get(name)

    def get_kernel(name: str) -> Optional[Tuple[SyscallArg, ...]]:
        return kernel_argmap.get(name)

    proto_args = get_userspace(routine)

    if proto_args is None and routine.endswith("_trap"):
        proto_args = get_userspace(routine[: -len("_trap")])

    if proto_args is not None:
        return list(proto_args)

    proto_args = get_kernel(routine)
    if proto_args is None and routine.endswith("_trap"):
        proto_args = get_kernel(routine[: -len("_trap")])

    if proto_args is None:
        raise RuntimeError(
            f"{table_path}: missing userspace prototype for {routine!r}, and no kernel extern(struct *_args *args) mapping found"
        )

    return list(proto_args)


def normalize_mach_name(routine: str) -> str:
    if routine.startswith("_"):
        routine = routine[1:]
    if routine.startswith("kernelrpc_"):
        routine = routine[len("kernelrpc_"):]
    if routine.endswith("_trap"):
        routine = routine[: -len("_trap")]
    return routine


def load_bsd_syscalls_master(
    kernel_tree: Path,
) -> Tuple[Dict[int, str], Dict[str, SyscallSignature]]:
    path = kernel_tree / "bsd" / "kern" / "syscalls.master"
    if not path.exists():
        raise RuntimeError(f"Missing BSD master file: {path}")

    numbers: Dict[int, str] = {}
    sigs: Dict[str, SyscallSignature] = {}

    lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()

    stack: List[bool] = []
    active = True

    for line_no, raw in enumerate(lines, start=1):
        line = raw.rstrip("\n")
        stripped = line.lstrip()

        if (
            stripped.startswith("#if ")
            or stripped.startswith("#ifdef ")
            or stripped.startswith("#ifndef ")
        ):
            stack.append(active)
            continue

        if stripped.startswith("#else"):
            if not stack:
                raise RuntimeError(f"{path}:{line_no}: #else without #if")
            active = False
            continue

        if stripped.startswith("#endif"):
            if not stack:
                raise RuntimeError(f"{path}:{line_no}: #endif without #if")
            active = stack.pop()
            continue

        if not active:
            continue

        m = BSD_MASTER_ENTRY_RE.match(line)
        if m is None:
            continue

        nr = int(m.group("nr"), 10)
        proto_blob = strip_c_comments(m.group("proto")).strip()

        mp = BSD_MASTER_PROTO_RE.match(proto_blob)
        if mp is None:
            raise RuntimeError(
                f"{path}:{line_no}: cannot parse prototype: {proto_blob!r}"
            )

        fn = mp.group("fn").strip()
        args_blob = mp.group("args").strip()

        if fn in {"nosys", "enosys"}:
            continue

        public_name = normalize_bsd_name(fn)
        if public_name == "":
            raise RuntimeError(f"{path}:{line_no}: empty normalized name from {fn!r}")

        if nr in numbers:
            raise RuntimeError(f"{path}:{line_no}: duplicate syscall number {nr}")
        if public_name in sigs:
            raise RuntimeError(
                f"{path}:{line_no}: duplicate syscall name {public_name!r}"
            )

        args = parse_c_param_list(args_blob, path, line_no)

        if len(args) > MAX_ARGS:
            raise RuntimeError(
                f"{path}:{line_no}: syscall {public_name!r} has {len(args)} args; max supported is {MAX_ARGS}"
            )

        numbers[nr] = public_name
        sigs[public_name] = SyscallSignature(name=public_name, args=tuple(args))

    return numbers, sigs


def normalize_bsd_name(fn: str) -> str:
    if fn.startswith("sys_"):
        return fn[len("sys_"):]
    if fn.startswith("__"):
        return fn[2:]
    return fn


def strip_c_comments(s: str) -> str:
    s = LINE_COMMENT_RE.sub("", s)
    while True:
        new_s = BLOCK_COMMENT_RE.sub("", s)
        if new_s == s:
            break
        s = new_s
    return s


def split_top_level_commas(text: str) -> List[str]:
    parts: List[str] = []
    buf: List[str] = []
    depth = 0
    in_string = False
    string_quote = ""

    cursor = 0
    while cursor < len(text):
        ch = text[cursor]

        if in_string:
            buf.append(ch)
            if ch == "\\" and cursor + 1 < len(text):
                buf.append(text[cursor + 1])
                cursor += 2
                continue
            if ch == string_quote:
                in_string = False
            cursor += 1
            continue

        if ch in ("'", '"'):
            in_string = True
            string_quote = ch
            buf.append(ch)
            cursor += 1
            continue

        if ch == "(":
            depth += 1
        elif ch == ")":
            depth = max(0, depth - 1)

        if ch == "," and depth == 0:
            parts.append("".join(buf).strip())
            buf = []
            cursor += 1
            continue

        buf.append(ch)
        cursor += 1

    parts.append("".join(buf).strip())
    return parts


def parse_c_param_list(args_blob: str, path: Path, line_no: int) -> List[SyscallArg]:
    raw = strip_c_comments(args_blob).strip()
    if raw == "" or raw == "void":
        return []

    raw_params = [p.strip() for p in split_top_level_commas(raw) if p.strip() != ""]
    if len(raw_params) == 1 and raw_params[0] == "void":
        return []

    args: List[SyscallArg] = []
    for idx, p in enumerate(raw_params):
        ty, name = split_param_decl(p, path, line_no)
        if name is None:
            name = f"arg{idx}"
        if ty == "user_addr_t" and name in {"attrname", "link", "path"}:
            ty = "char *"
        args.append(SyscallArg(type=ty, name=name))
    return args


def split_param_decl(decl: str, path: Path, line_no: int) -> Tuple[str, Optional[str]]:
    d = strip_c_comments(decl).strip()

    if d == "" or d == "void":
        raise RuntimeError(f"{path}:{line_no}: bad parameter decl: {decl!r}")

    tokens = _tokenize_decl(d)
    ty_tokens, name = _split_type_and_name(tokens)

    ty = _format_type(ty_tokens)
    if not ty:
        raise RuntimeError(
            f"{path}:{line_no}: cannot parse parameter type from {decl!r}"
        )

    return ty, _camel_to_snake(name) if name else None


def _tokenize_decl(s: str) -> list[str]:
    out = []
    for c in s:
        out.append(" * " if c == "*" else c)
    return "".join(out).split()


def _split_type_and_name(tokens: list[str]) -> Tuple[list[str], Optional[str]]:
    if not tokens:
        return [], None

    last = tokens[-1]

    if last == "*":
        return tokens, None

    stars, remainder = _split_leading_stars(last)
    if stars:
        if _is_identifier(remainder):
            return tokens[:-1] + ["*"] * stars, remainder
        return tokens, None

    if _is_identifier(last):
        return tokens[:-1], last

    return tokens, None


def _format_type(tokens: list[str]) -> str:
    formatted: list[str] = []
    i = 0
    while i < len(tokens):
        if tokens[i] != "*":
            formatted.append(tokens[i])
            i += 1
            continue

        j = i
        while j < len(tokens) and tokens[j] == "*":
            j += 1
        formatted.append("*" * (j - i))
        i = j

    return " ".join(formatted).strip()


def _split_leading_stars(token: str) -> Tuple[int, str]:
    i = 0
    while i < len(token) and token[i] == "*":
        i += 1
    return i, token[i:]


def _is_identifier(s: str) -> bool:
    if not s:
        return False
    if not (s[0].isalpha() or s[0] == "_"):
        return False
    return all(c.isalnum() or c == "_" for c in s)


def _camel_to_snake(name: str) -> str:
    out = []
    for i, c in enumerate(name):
        if (
            c.isupper()
            and i > 0
            and (name[i - 1].islower() or (i + 1 < len(name) and name[i + 1].islower()))
        ):
            out.append("_")
        out.append(c.lower())
    return "".join(out)


def normalize_whitespace(s: str) -> str:
    return " ".join(s.split())


def line_number_at_offset(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def extract_parentheses_payload(
    text: str, open_paren_offset: int, path: Path
) -> Tuple[str, int]:
    if (
        open_paren_offset < 0
        or open_paren_offset >= len(text)
        or text[open_paren_offset] != "("
    ):
        raise RuntimeError(f"{path}: expected '(' at offset {open_paren_offset}")

    depth = 0
    cursor = open_paren_offset
    start = open_paren_offset + 1

    in_string = False
    string_quote = ""

    while cursor < len(text):
        ch = text[cursor]

        if in_string:
            if ch == "\\":
                cursor += 2
                continue
            if ch == string_quote:
                in_string = False
            cursor += 1
            continue

        if ch in ("'", '"'):
            in_string = True
            string_quote = ch
            cursor += 1
            continue

        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return text[start:cursor], cursor + 1

        cursor += 1

    raise RuntimeError(f"{path}: unterminated parentheses payload")


def extract_brace_payload(
    text: str, open_brace_offset: int, path: Path
) -> Tuple[str, int]:
    if (
        open_brace_offset < 0
        or open_brace_offset >= len(text)
        or text[open_brace_offset] != "{"
    ):
        raise RuntimeError(f"{path}: expected '{{' at offset {open_brace_offset}")

    depth = 0
    cursor = open_brace_offset
    start = open_brace_offset + 1

    in_string = False
    string_quote = ""

    while cursor < len(text):
        ch = text[cursor]

        if in_string:
            if ch == "\\":
                cursor += 2
                continue
            if ch == string_quote:
                in_string = False
            cursor += 1
            continue

        if ch in ("'", '"'):
            in_string = True
            string_quote = ch
            cursor += 1
            continue

        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return text[start:cursor], cursor + 1

        cursor += 1

    raise RuntimeError(f"{path}: unterminated brace payload")


def render_vala(*, mach_numbers: Dict[int, str], bsd_numbers: Dict[int, str]) -> str:
    out: List[str] = []

    out += [
        "// AUTO-GENERATED FILE. DO NOT EDIT.\n",
        "\n",
        "namespace Frida {\n",
        "\tpublic extern unowned XnuSyscallSignature[] get_xnu_mach_traps ();\n",
        "\tpublic extern unowned XnuSyscallSignature[] get_xnu_bsd_syscalls ();\n",
        "\n",
        "\tpublic struct XnuSyscallSignature {\n",
        "\t\tpublic int nr;\n",
        "\t\tpublic string name;\n",
        "\t\tpublic uint8 nargs;\n",
        f"\t\tpublic XnuSyscallArg args[{MAX_ARGS}];\n",
        "\t}\n",
        "\n",
        "\tpublic struct XnuSyscallArg {\n",
        "\t\tpublic string? type;\n",
        "\t\tpublic string? name;\n",
        "\t}\n",
        "\n",
        "\tpublic enum XnuMachTrap {\n",
    ]

    for nr, name in sorted(mach_numbers.items(), key=lambda kv: kv[0], reverse=True):
        out += [f"\t\t{name.upper()} = {nr},\n"]

    out += [
        "\t}\n",
        "\n",
        "\tpublic enum XnuBsdSyscall {\n",
    ]

    for nr, name in sorted(bsd_numbers.items(), key=lambda kv: kv[0]):
        out += [f"\t\t{name.upper()} = {nr},\n"]

    out += [
        "\t}\n",
        "}\n",
    ]

    return "".join(out)


def render_c_source(
    *,
    mach_numbers: Dict[int, str],
    bsd_numbers: Dict[int, str],
    mach_sigs: Dict[str, SyscallSignature],
    bsd_sigs: Dict[str, SyscallSignature],
) -> str:
    out: List[str] = []

    out += [
        "/* AUTO-GENERATED FILE. DO NOT EDIT. */\n",
        "\n",
        '#include "frida-base.h"\n',
        "\n",
    ]

    emit_c_signature_table(
        out,
        c_var="frida_xnu_mach_traps",
        numbers=mach_numbers,
        sigs=mach_sigs,
        reverse=True,
    )

    emit_c_signature_table(
        out,
        c_var="frida_xnu_bsd_syscalls",
        numbers=bsd_numbers,
        sigs=bsd_sigs,
    )

    emit_c_table_getter(
        out, c_func="frida_get_xnu_mach_traps", c_var="frida_xnu_mach_traps"
    )
    out += ["\n"]
    emit_c_table_getter(
        out, c_func="frida_get_xnu_bsd_syscalls", c_var="frida_xnu_bsd_syscalls"
    )

    return "".join(out)


def emit_c_signature_table(
    out: List[str],
    *,
    c_var: str,
    numbers: Dict[int, str],
    sigs: Dict[str, SyscallSignature],
    reverse: bool = False,
) -> None:
    out += [
        f"static const FridaXnuSyscallSignature {c_var}[] =\n",
        "{\n",
    ]

    for nr, name in sorted(numbers.items(), key=lambda kv: kv[0], reverse=reverse):
        sig = sigs[name]

        args_parts: List[str] = []
        for a in sig.args:
            args_parts += [f"{{ {c_escape(a.type)}, {c_escape(a.name)} }}"]

        args_blob = ", ".join(args_parts)

        if args_blob != "":
            out += [
                f"  {{ {nr}, {c_escape(name)}, {len(sig.args)}, {{ {args_blob} }} }},\n"
            ]
        else:
            out += [f"  {{ {nr}, {c_escape(name)}, 0, {{}} }},\n"]

    out += [
        "};\n",
        "\n",
    ]


def emit_c_table_getter(out: List[str], *, c_func: str, c_var: str) -> None:
    out += [
        "FridaXnuSyscallSignature *\n",
        f"{c_func} (int * len)\n",
        "{\n",
        "  if (len != NULL)\n",
        f"    *len = G_N_ELEMENTS ({c_var});\n",
        f"  return (FridaXnuSyscallSignature *) {c_var};\n",
        "}\n",
    ]


def c_escape(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


if __name__ == "__main__":
    raise SystemExit(main())
