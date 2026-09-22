#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from __future__ import annotations

import argparse
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, FrozenSet, List, Optional, Tuple


@dataclass(frozen=True)
class SyscallTableSpec:
    relpath: Path
    allowed_tags: Optional[FrozenSet[str]] = None
    nr_base: int = 0


@dataclass(frozen=True)
class SyscallArg:
    type: str
    name: str


@dataclass(frozen=True)
class SyscallSignature:
    name: str
    big_endian_args: Tuple[
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
    ]
    little_endian_args: Tuple[
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
        Optional[SyscallArg],
    ]

    @property
    def nargs(self) -> int:
        count = 0
        for arg in self.big_endian_args:
            if arg is None:
                break
            count += 1
        return count

    def is_endian_sensitive(self) -> bool:
        return self.big_endian_args != self.little_endian_args


@dataclass(frozen=True)
class SignatureIndex:
    native: Dict[str, SyscallSignature]
    compat: Dict[str, SyscallSignature]
    syscall_impls: Dict[str, Tuple[str, Optional[str]]]
    prototypes: Dict[str, Tuple[SyscallArg, ...]]


@dataclass(frozen=True)
class AbiEnvironment:
    abi: str
    arch: str
    bits_per_long: int
    compat_enabled: bool
    defines: Dict[str, Optional[int]]


SYSCALL_DEFINE_INVOCATION_RE = re.compile(
    r"(SYSCALL_DEFINE|COMPAT_SYSCALL_DEFINE)([0-6])\s*\("
)
SYSCALL_PROTO_RE = re.compile(
    r"^\s*asmlinkage\s+long\s+(?P<fn>[A-Za-z_]\w*)\s*\((?P<args>[^;]*?)\)\s*;",
    re.MULTILINE,
)
FUNCTION_LIKE_CALL_RE = re.compile(
    r"^(?P<name>[A-Za-z_]\w*)\s*\(\s*(?P<args>.*)\s*\)\s*$"
)
PARAM_NAME_RE = re.compile(r"[A-Za-z_]\w*$")
WHITESPACE_RE = re.compile(r"\s+")
USER_QUAL_RE = re.compile(r"(?<!\w)__user(?!\w)")
DEFINE_DIRECTIVE_RE = re.compile(r"^\s*#\s*(?P<kw>\w+)\b(?P<rest>.*)$")
LINE_COMMENT_RE = re.compile(r"//.*$")
BLOCK_COMMENT_RE = re.compile(r"/\*[^*]*\*+(?:[^/*][^*]*\*+)*/")

POINTER_SPACING_RE = re.compile(r"\s*\*\s*")
BITS_PER_LONG_IDENT_RE = re.compile(r"\bBITS_PER_LONG\b")
BITS_PER_LONG2_IDENT_RE = re.compile(r"\b__BITS_PER_LONG\b")
LEADING_DECIMAL_RE = re.compile(r"^([0-9]+)\b")
TRAILING_ARRAY_SUFFIX_RE = re.compile(r"\[[^\]]*\]\s*$")

SYSCALL_TABLES: Dict[str, SyscallTableSpec] = {
    "x86": SyscallTableSpec(
        relpath=Path("arch") / "x86" / "entry" / "syscalls" / "syscall_32.tbl",
        allowed_tags=frozenset({"i386"}),
    ),
    "x86_64": SyscallTableSpec(
        relpath=Path("arch") / "x86" / "entry" / "syscalls" / "syscall_64.tbl",
        allowed_tags=frozenset({"common", "64"}),
    ),
    "arm": SyscallTableSpec(
        relpath=Path("arch") / "arm" / "tools" / "syscall.tbl",
        allowed_tags=frozenset({"common", "oabi", "eabi"}),
    ),
    "arm64": SyscallTableSpec(
        relpath=Path("arch") / "arm64" / "tools" / "syscall_64.tbl",
        allowed_tags=frozenset({"common", "64", "rlimit"}),
    ),
    "mips": SyscallTableSpec(
        relpath=Path("arch") / "mips" / "kernel" / "syscalls" / "syscall_o32.tbl",
        nr_base=4000,
    ),
    "mips64": SyscallTableSpec(
        relpath=Path("arch") / "mips" / "kernel" / "syscalls" / "syscall_n64.tbl",
        nr_base=5000,
    ),
    "s390x": SyscallTableSpec(
        relpath=Path("arch") / "s390" / "kernel" / "syscalls" / "syscall.tbl",
    ),
}

LINUX_ARCH: Dict[str, str] = {
    "x86_64": "x86",
    "mips64": "mips",
    "s390x": "s390",
}

ABI_BITS_PER_LONG: Dict[str, int] = {
    "x86": 32,
    "arm": 32,
    "mips": 32,
    "x86_64": 64,
    "arm64": 64,
    "mips64": 64,
    "s390x": 64,
}

HARDCODED_CPP_CONSTANTS: Dict[str, Optional[int]] = {
    "ACCT_VERSION": 3,
    "CONFIG_CACHESTAT_SYSCALL": None,
    "CONFIG_LOG_BUF_SHIFT": 18,
    "CONFIG_MODULE_UNLOAD": None,
    "CONFIG_MULTIUSER": None,
    "CONFIG_PGTABLE_LEVELS": 3,
    "CONFIG_PRINTK": None,
    "CONFIG_SYSFS_SYSCALL": None,
    "HZ": 1000,
    "MSEC_PER_SEC": 1000,
    "NSEC_PER_SEC": 1000000000,
    "PAGE_SIZE": 4096,
    "SIGEV_NONE": 1,
    "SIGEV_SIGNAL": 0,
    "SIGEV_THREAD": 2,
    "SIGEV_THREAD_ID": 4,
    "THREAD_SIZE": 65536,
    "TICK_NSEC": 1000000,
    "USEC_PER_SEC": 1000000,
    "USER_HZ": 100,
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--abi", required=True)
    parser.add_argument("--kernel-tree", required=True)
    parser.add_argument("--out-vala", required=True)
    parser.add_argument("--out-c", required=True)
    args = parser.parse_args()

    kernel_tree = Path(args.kernel_tree).resolve()
    if not kernel_tree.exists():
        raise SystemExit(f"--kernel-tree does not exist: {kernel_tree}")

    abi = args.abi
    if abi not in SYSCALL_TABLES:
        known = ", ".join(SYSCALL_TABLES.keys())
        raise SystemExit(f"Unknown ABI {abi!r}. Known: {known}")

    native_numbers = load_syscall_numbers(kernel_tree, abi)
    compat32_numbers = load_compat32_numbers(kernel_tree, abi)

    native_impls = load_syscall_impls(kernel_tree, abi)
    compat_impls = load_compat32_impls(kernel_tree, abi)

    compat_enabled = compat32_numbers is not None
    env = build_abi_environment(kernel_tree, abi, compat_enabled=compat_enabled)

    prototypes = load_syscall_prototypes(kernel_tree, env)

    parsed = load_syscall_signatures(
        kernel_tree=kernel_tree,
        env=env,
        syscall_impls=native_impls,
        prototypes=prototypes,
    )

    signatures_native = parsed
    signatures_compat = (
        SignatureIndex(
            native=parsed.native,
            compat=parsed.compat,
            syscall_impls=compat_impls,
            prototypes=parsed.prototypes,
        )
        if compat32_numbers is not None and compat_impls is not None
        else None
    )

    vala = render_vala(
        native_numbers=native_numbers,
        compat32_numbers=compat32_numbers,
        signatures_native=signatures_native,
        signatures_compat=signatures_compat,
    )

    out_vala = Path(args.out_vala)
    out_c = Path(args.out_c)

    out_vala.parent.mkdir(parents=True, exist_ok=True)
    out_c.parent.mkdir(parents=True, exist_ok=True)

    out_vala.write_text(vala, encoding="utf-8")

    c_c = render_c_source(
        native_numbers=native_numbers,
        compat32_numbers=compat32_numbers,
        signatures_native=signatures_native,
        signatures_compat=signatures_compat,
    )

    out_c.write_text(c_c, encoding="utf-8")

    return 0


def load_syscall_impls(
    kernel_tree: Path, abi: str
) -> Dict[str, Tuple[str, Optional[str]]]:
    return load_syscall_impls_from_location(kernel_tree, SYSCALL_TABLES[abi])


def load_compat32_impls(
    kernel_tree: Path, abi: str
) -> Optional[Dict[str, Tuple[str, Optional[str]]]]:
    if abi == "x86_64":
        return load_syscall_impls(kernel_tree, "x86")

    if abi == "arm64":
        table = SyscallTableSpec(
            relpath=Path("arch") / "arm64" / "tools" / "syscall_32.tbl",
            allowed_tags=frozenset({"common", "32"}),
        )
        return load_syscall_impls_from_location(kernel_tree, table)

    return None


def load_syscall_impls_from_location(
    kernel_tree: Path,
    table: SyscallTableSpec,
) -> Dict[str, Tuple[str, Optional[str]]]:
    table_path = (kernel_tree / table.relpath).resolve()
    if not table_path.exists():
        raise RuntimeError(f"Missing syscall table file: {table_path}")

    impls: Dict[str, Tuple[str, Optional[str]]] = {}

    for line_no, raw_line in enumerate(
        table_path.read_text(encoding="utf-8").splitlines(),
        start=1,
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        line = line.split("#", 1)[0].strip()
        if not line:
            continue

        cols = WHITESPACE_RE.split(line)
        if len(cols) < 3:
            raise RuntimeError(f"{table_path}:{line_no}: too few columns: {raw_line!r}")

        number_token, tag, name = cols[0:3]
        if not number_token.isdigit():
            raise RuntimeError(
                f"{table_path}:{line_no}: invalid syscall number token: {number_token!r}"
            )

        if table.allowed_tags is not None and tag not in table.allowed_tags:
            continue

        entry: Optional[str] = cols[3] if len(cols) >= 4 else None
        compat_entry: Optional[str] = cols[4] if len(cols) >= 5 else None

        prev = impls.get(name)
        cur = (entry, compat_entry)
        if prev is not None and prev != cur:
            raise RuntimeError(
                f"{table_path}:{line_no}: conflicting impl for {name!r}: {prev!r} vs {cur!r}"
            )

        impls[name] = cur

    if not impls:
        raise RuntimeError(f"{table_path}: parsed 0 syscall impls")

    return impls


def load_compat32_numbers(kernel_tree: Path, abi: str) -> Optional[Dict[int, str]]:
    if abi == "x86_64":
        return load_syscall_numbers(kernel_tree, "x86")

    if abi == "arm64":
        table = SyscallTableSpec(
            relpath=Path("arch") / "arm64" / "tools" / "syscall_32.tbl",
            allowed_tags=frozenset({"common", "32"}),
        )
        return load_syscall_numbers_from_location(kernel_tree, table)

    return None


def load_syscall_numbers(kernel_tree: Path, abi: str) -> Dict[int, str]:
    return load_syscall_numbers_from_location(kernel_tree, SYSCALL_TABLES[abi])


def load_syscall_numbers_from_location(
    kernel_tree: Path,
    table: SyscallTableSpec,
) -> Dict[int, str]:
    table_path = (kernel_tree / table.relpath).resolve()
    if not table_path.exists():
        raise RuntimeError(f"Missing syscall table file: {table_path}")

    syscalls: Dict[int, str] = {}
    for line_no, raw_line in enumerate(
        table_path.read_text(encoding="utf-8").splitlines(),
        start=1,
    ):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        line = line.split("#", 1)[0].strip()
        if not line:
            continue

        cols = WHITESPACE_RE.split(line)
        if len(cols) < 3:
            raise RuntimeError(f"{table_path}:{line_no}: too few columns: {raw_line!r}")

        number_token, tag, name = cols[0], cols[1], cols[2]
        if not number_token.isdigit():
            raise RuntimeError(
                f"{table_path}:{line_no}: invalid syscall number token: {number_token!r}"
            )

        if table.allowed_tags is not None and tag not in table.allowed_tags:
            continue

        number = table.nr_base + int(number_token, 10)
        if number in syscalls:
            raise RuntimeError(
                f"{table_path}:{line_no}: duplicate syscall number {number}"
            )

        syscalls[number] = name

    if not syscalls:
        raise RuntimeError(f"{table_path}: parsed 0 syscalls")

    return syscalls


def build_abi_environment(
    kernel_tree: Path,
    abi: str,
    compat_enabled: bool,
) -> AbiEnvironment:
    arch = LINUX_ARCH.get(abi, abi)
    bits_per_long = ABI_BITS_PER_LONG[abi]

    defines: Dict[str, Optional[int]] = dict()
    defines["BITS_PER_LONG"] = bits_per_long
    defines["__BITS_PER_LONG"] = bits_per_long
    defines[f"CONFIG_{bits_per_long}BIT"] = None
    if compat_enabled:
        defines["CONFIG_COMPAT"] = None
    if bits_per_long == 32 or compat_enabled:
        defines["CONFIG_COMPAT_32BIT_TIME"] = None
    if abi in {"x86_64", "arm64"}:
        defines["CONFIG_ARCH_HAS_PKEYS"] = None
    if abi in {"x86", "arm"}:
        defines["CONFIG_OLD_SIGACTION"] = None
    if abi in {"x86_64", "arm64"} and compat_enabled:
        defines["CONFIG_COMPAT_OLD_SIGACTION"] = None
    if (abi in {"x86_64", "arm64"} and compat_enabled) or abi in {"x86", "arm"}:
        defines["CONFIG_OLD_SIGSUSPEND3"] = None

    arch_defines = collect_arch_defines(kernel_tree, arch, dict(defines), bits_per_long)
    defines.update(arch_defines)

    return AbiEnvironment(
        abi=abi,
        arch=arch,
        bits_per_long=bits_per_long,
        compat_enabled=compat_enabled,
        defines=defines,
    )


def collect_arch_defines(
    kernel_tree: Path,
    arch: str,
    initial_defines: Dict[str, Optional[int]],
    bits_per_long: int,
) -> Dict[str, Optional[int]]:
    unistd_h = kernel_tree / "arch" / arch / "include" / "asm" / "unistd.h"
    if not unistd_h.exists():
        raise RuntimeError(f"Missing arch unistd.h: {unistd_h}")

    text = unistd_h.read_text(encoding="utf-8", errors="ignore")
    pp = Preprocessor(
        text=text,
        rel_path=unistd_h.relative_to(kernel_tree),
        initial_defines=initial_defines,
        bits_per_long=bits_per_long,
    )
    return pp.run_and_collect_defines()


def ensure_git_checkout(kernel_tree: Path) -> None:
    subprocess.run(
        ["git", "-C", str(kernel_tree), "rev-parse", "--is-inside-work-tree"],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def load_syscall_signatures(
    kernel_tree: Path,
    env: AbiEnvironment,
    syscall_impls: Dict[str, Tuple[str, Optional[str]]],
    prototypes: Dict[str, Tuple[SyscallArg, ...]],
) -> SignatureIndex:
    ensure_git_checkout(kernel_tree)

    pathspecs = compute_grep_pathspecs(kernel_tree, env.arch)
    grep = subprocess.run(
        [
            "git",
            "-C",
            str(kernel_tree),
            "grep",
            "-n",
            "-I",
            "-e",
            "SYSCALL_DEFINE[0-6]",
            "-e",
            "COMPAT_SYSCALL_DEFINE[0-6]",
            "--",
            *pathspecs,
        ],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )

    native: Dict[str, SyscallSignature] = {}
    compat: Dict[str, SyscallSignature] = {}

    per_file_hits: Dict[Path, List[int]] = {}
    for hit in grep.stdout.splitlines():
        rel_path_s, line_s, _ = hit.split(":", 2)
        rel_path = Path(rel_path_s)
        per_file_hits.setdefault(rel_path, []).append(int(line_s))

    for rel_path, hit_lines in per_file_hits.items():
        if rel_path.suffix not in {".c", ".h", ".S", ".s", ".inc"}:
            continue

        source_path = kernel_tree / rel_path
        text = source_path.read_text(encoding="utf-8", errors="ignore")

        pp = Preprocessor(
            text=text,
            rel_path=rel_path,
            initial_defines=dict(env.defines),
            bits_per_long=env.bits_per_long,
        )
        active_spans = pp.compute_active_spans()

        for start_line in sorted(set(hit_lines)):
            sigs = parse_signatures_from_line(
                text=text,
                start_line=start_line,
                rel_path=rel_path,
                active_spans=active_spans,
            )
            for macro_prefix, signature in sigs:
                if macro_prefix == "SYSCALL_DEFINE":
                    native[signature.name] = signature
                else:
                    compat[signature.name] = signature

    if not native and not compat:
        raise RuntimeError("parsed 0 syscall signatures")

    return SignatureIndex(
        native=native,
        compat=compat,
        syscall_impls=syscall_impls,
        prototypes=prototypes,
    )


def compute_grep_pathspecs(kernel_tree: Path, wanted_arch: str) -> List[str]:
    arch_dir = kernel_tree / "arch"
    if not arch_dir.is_dir():
        raise RuntimeError(f"Missing arch directory: {arch_dir}")

    arch_names = {p.name for p in arch_dir.iterdir() if p.is_dir()}
    if wanted_arch not in arch_names:
        raise RuntimeError(f"Missing arch/{wanted_arch} in {kernel_tree}")

    excluded = sorted(arch_names - {wanted_arch})
    specs: List[str] = ["."]
    specs.extend([f":(exclude)arch/{name}" for name in excluded])
    specs.extend(
        [
            ":(exclude)tools",
            ":(exclude)scripts",
            ":(exclude)Documentation",
        ]
    )
    return specs


def load_syscall_prototypes(
    kernel_tree: Path,
    env: AbiEnvironment,
) -> Dict[str, Tuple[SyscallArg, ...]]:
    paths = [
        kernel_tree / "include" / "linux" / "syscalls.h",
        kernel_tree / "include" / "linux" / "compat.h",
    ]

    protos: Dict[str, Tuple[SyscallArg, ...]] = {}

    for path in paths:
        if not path.exists():
            raise RuntimeError(f"Missing syscall prototype header: {path}")

        text = path.read_text(encoding="utf-8", errors="ignore")

        pp = Preprocessor(
            text=text,
            rel_path=path.relative_to(kernel_tree),
            initial_defines=dict(env.defines),
            bits_per_long=env.bits_per_long,
        )
        active_spans = pp.compute_active_spans()

        for m in SYSCALL_PROTO_RE.finditer(text):
            if not is_offset_in_spans(m.start(), active_spans):
                continue

            fn = m.group("fn")
            args_blob = m.group("args").strip()
            line_no = line_number_at_offset(text, m.start())

            raw_params = [
                p.strip() for p in split_top_level_commas(args_blob) if p.strip()
            ]
            if len(raw_params) == 1 and raw_params[0] == "void":
                raw_params = []

            args: List[SyscallArg] = []
            for idx, p in enumerate(raw_params):
                ty, name = split_param_decl(p, path.relative_to(kernel_tree), line_no)
                if name is None:
                    name = f"arg{idx}"
                args.append(SyscallArg(type=ty, name=name))

            cur = tuple(args)
            prev = protos.get(fn)
            if prev is not None and prev != cur:
                raise RuntimeError(
                    f"{path}:{line_no}: conflicting prototype for {fn!r}"
                )
            protos[fn] = cur

    if not protos:
        raise RuntimeError("parsed 0 prototypes")

    return protos


def split_param_decl(
    decl: str,
    rel_path: Path,
    line_no: int,
) -> Tuple[str, Optional[str]]:
    d = strip_c_comments(decl).strip()
    if d == "" or d == "void":
        raise RuntimeError(f"{rel_path}:{line_no}: bad parameter decl: {decl!r}")

    d = TRAILING_ARRAY_SUFFIX_RE.sub("", d).strip()
    d = POINTER_SPACING_RE.sub(" * ", d)
    d = normalize_whitespace(d)

    parts = d.split(" ")
    if not parts:
        raise RuntimeError(f"{rel_path}:{line_no}: bad parameter decl: {decl!r}")

    if len(parts) == 1:
        ty = normalize_type(parts[0])
        if ty == "":
            raise RuntimeError(
                f"{rel_path}:{line_no}: cannot parse parameter type from {decl!r}"
            )
        return ty, None

    last = parts[-1]

    if last == "*":
        ty = normalize_type(d)
        if ty == "":
            raise RuntimeError(
                f"{rel_path}:{line_no}: cannot parse parameter type from {decl!r}"
            )
        return ty, None

    if last.startswith("*"):
        name = last[1:]
        if not PARAM_NAME_RE.match(name):
            ty = normalize_type(d)
            if ty == "":
                raise RuntimeError(
                    f"{rel_path}:{line_no}: cannot parse parameter type from {decl!r}"
                )
            return ty, None
        ty = " ".join(parts[:-1] + ["*"]).strip()
        ty = normalize_type(ty)
        if ty == "":
            raise RuntimeError(
                f"{rel_path}:{line_no}: cannot parse parameter type from {decl!r}"
            )
        return ty, name

    if PARAM_NAME_RE.match(last):
        ty = " ".join(parts[:-1]).strip()
        ty = normalize_type(ty)
        if ty == "":
            raise RuntimeError(
                f"{rel_path}:{line_no}: cannot parse parameter type from {decl!r}"
            )
        return ty, last

    ty = normalize_type(d)
    if ty == "":
        raise RuntimeError(
            f"{rel_path}:{line_no}: cannot parse parameter type from {decl!r}"
        )
    return ty, None


def parse_signatures_from_line(
    text: str,
    start_line: int,
    rel_path: Path,
    active_spans: List[Tuple[int, int]],
) -> List[Tuple[str, SyscallSignature]]:
    lines = text.splitlines(keepends=True)
    if start_line < 1 or start_line > len(lines):
        raise RuntimeError(f"{rel_path}:{start_line}: line out of range")

    start_offset = sum(len(lines[i]) for i in range(start_line - 1))
    out: List[Tuple[str, SyscallSignature]] = []

    offset = start_offset
    while offset < len(text):
        found = find_next_syscall_define_invocation(text, offset)
        if found is None:
            break

        macro_offset, macro_prefix, nargs, open_paren_offset = found

        if not is_offset_in_spans(macro_offset, active_spans):
            offset = open_paren_offset + 1
            continue

        if is_inside_block_comment(text, macro_offset):
            offset = macro_offset + 1
            continue

        if is_line_commented_out(text, macro_offset):
            offset = macro_offset + 1
            continue

        line_start = text.rfind("\n", 0, macro_offset) + 1
        line_end = text.find("\n", macro_offset)
        if line_end == -1:
            line_end = len(text)
        line_text = text[line_start:line_end].strip()
        if line_text.startswith("#define"):
            offset = open_paren_offset + 1
            continue

        payload, end_offset = extract_parentheses_payload(
            text=text,
            open_paren_offset=open_paren_offset,
            rel_path=rel_path,
        )
        signature = parse_macro_payload(
            text=text,
            payload=payload,
            nargs=nargs,
            rel_path=rel_path,
            macro_offset=macro_offset,
            macro_prefix=macro_prefix,
        )
        out.append((macro_prefix, signature))
        offset = end_offset

    return out


def is_offset_in_spans(offset: int, spans: List[Tuple[int, int]]) -> bool:
    for start, end in spans:
        if start <= offset < end:
            return True
    return False


def find_next_syscall_define_invocation(
    text: str,
    start_offset: int,
) -> Optional[Tuple[int, str, int, int]]:
    match = SYSCALL_DEFINE_INVOCATION_RE.search(text, start_offset)
    if match is None:
        return None

    macro_offset = match.start(1)
    macro_prefix = match.group(1)
    nargs = int(match.group(2), 10)
    open_paren_offset = match.end(0) - 1
    return macro_offset, macro_prefix, nargs, open_paren_offset


def strip_c_comments(s: str) -> str:
    s = LINE_COMMENT_RE.sub("", s)
    while True:
        new_s = BLOCK_COMMENT_RE.sub("", s)
        if new_s == s:
            break
        s = new_s
    return s.strip()


def is_inside_block_comment(text: str, offset: int) -> bool:
    last_open = text.rfind("/*", 0, offset)
    if last_open == -1:
        return False
    last_close = text.rfind("*/", 0, offset)
    return last_close < last_open


def is_line_commented_out(text: str, offset: int) -> bool:
    line_start = text.rfind("\n", 0, offset) + 1
    return text.find("//", line_start, offset) != -1


def extract_parentheses_payload(
    text: str,
    open_paren_offset: int,
    rel_path: Path,
) -> Tuple[str, int]:
    if text[open_paren_offset] != "(":
        raise RuntimeError(
            f"{rel_path}:{line_number_at_offset(text, open_paren_offset)}: expected '('"
        )

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

    raise RuntimeError(
        f"{rel_path}:{line_number_at_offset(text, open_paren_offset)}: unterminated macro invocation"
    )


def parse_macro_payload(
    text: str,
    payload: str,
    nargs: int,
    rel_path: Path,
    macro_offset: int,
    macro_prefix: str,
) -> SyscallSignature:
    line_no = line_number_at_offset(text, macro_offset)

    tokens = split_top_level_commas(payload)
    if not tokens:
        raise RuntimeError(f"{rel_path}:{line_no}: empty macro payload")

    name = tokens[0].strip()
    if not name or any(c.isspace() for c in name):
        raise RuntimeError(
            f"{rel_path}:{line_no}: invalid syscall name token: {tokens[0]!r}"
        )

    remainder = [t.strip() for t in tokens[1:] if t.strip() != ""]

    if nargs == 0:
        args = (None, None, None, None, None, None)
        return SyscallSignature(
            name=name,
            big_endian_args=args,
            little_endian_args=args,
        )

    be_pairs = parse_pairs_with_emulated_macros(
        remainder, True, rel_path, line_no, name
    )
    le_pairs = parse_pairs_with_emulated_macros(
        remainder, False, rel_path, line_no, name
    )

    if len(be_pairs) != nargs:
        raise RuntimeError(
            f"{rel_path}:{line_no}: expected {nargs} args for {name!r} (BIG_ENDIAN), "
            f"got {len(be_pairs)}: {be_pairs!r}"
        )
    if len(le_pairs) != nargs:
        raise RuntimeError(
            f"{rel_path}:{line_no}: expected {nargs} args for {name!r} (LITTLE_ENDIAN), "
            f"got {len(le_pairs)}: {le_pairs!r}"
        )

    be_args = pad_args([SyscallArg(type=t, name=n) for t, n in be_pairs])
    le_args = pad_args([SyscallArg(type=t, name=n) for t, n in le_pairs])

    return SyscallSignature(
        name=name,
        big_endian_args=be_args,
        little_endian_args=le_args,
    )


def parse_pairs_with_emulated_macros(
    tokens: List[str],
    big_endian: bool,
    rel_path: Path,
    line_no: int,
    syscall_name: str,
) -> List[Tuple[str, str]]:
    out: List[Tuple[str, str]] = []
    i = 0
    while i < len(tokens):
        token = tokens[i]
        call = parse_function_like_call(token)
        if call is None:
            if i + 1 >= len(tokens):
                raise RuntimeError(
                    f"{rel_path}:{line_no}: trailing token {token!r} in arg list of {syscall_name!r}"
                )
            out.append((normalize_type(tokens[i]), normalize_whitespace(tokens[i + 1])))
            i += 2
            continue

        macro_name, actuals = call
        if len(actuals) != 1:
            raise RuntimeError(
                f"{rel_path}:{line_no}: {macro_name} expects 1 arg, got {len(actuals)} in {syscall_name!r}"
            )

        base = normalize_whitespace(actuals[0])
        if base == "":
            raise RuntimeError(
                f"{rel_path}:{line_no}: empty {macro_name} argument in {syscall_name!r}"
            )

        expanded = expand_emulated_macro(macro_name, base, big_endian)
        if expanded is None:
            raise RuntimeError(
                f"{rel_path}:{line_no}: unsupported macro {macro_name!r} in arg list of {syscall_name!r}"
            )

        out.extend(expanded)
        i += 1

    return out


def expand_u32_pair(base: str, big_endian: bool) -> List[Tuple[str, str]]:
    if big_endian:
        return [("u32", f"{base}_hi"), ("u32", f"{base}_lo")]
    return [("u32", f"{base}_lo"), ("u32", f"{base}_hi")]


def expand_emulated_macro(
    macro_name: str,
    base: str,
    big_endian: bool,
) -> Optional[List[Tuple[str, str]]]:
    if macro_name in {"arg_u32p", "SC_ARG64", "compat_arg_u64_dual"}:
        return expand_u32_pair(base, big_endian)
    return None


def parse_function_like_call(token: str) -> Optional[Tuple[str, List[str]]]:
    match = FUNCTION_LIKE_CALL_RE.match(token)
    if match is None:
        return None

    name = match.group("name")
    raw_args = match.group("args")
    args = [a.strip() for a in split_top_level_commas(raw_args) if a.strip() != ""]
    return name, args


def pad_args(args: List[SyscallArg]) -> Tuple[Optional[SyscallArg], ...]:
    padded: List[Optional[SyscallArg]] = [None] * 6
    for i, a in enumerate(args):
        if i >= 6:
            raise RuntimeError("nargs > 6 is unsupported")
        padded[i] = a
    return tuple(padded)


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


def normalize_type(s: str) -> str:
    stripped = USER_QUAL_RE.sub("", s)
    stripped = normalize_whitespace(stripped)
    return stripped


def normalize_whitespace(s: str) -> str:
    return " ".join(s.split())


def line_number_at_offset(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def render_vala(
    native_numbers: Dict[int, str],
    compat32_numbers: Optional[Dict[int, str]],
    signatures_native: SignatureIndex,
    signatures_compat: Optional[SignatureIndex],
) -> str:
    out: List[str] = []

    out.append("// AUTO-GENERATED FILE. DO NOT EDIT.\n\n")
    out.append("namespace Frida {\n")

    out.append("\tpublic enum LinuxSyscall {\n")
    for number, name in sorted(native_numbers.items(), key=lambda kv: kv[0]):
        sig = resolve_signature(signatures_native, name, prefer="native")
        if sig is None:
            continue
        out.append(f"\t\t{name.upper()} = {number},\n")
    out.append("\t}\n")

    if compat32_numbers is not None and signatures_compat is not None:
        out.append("\n\tpublic enum LinuxCompat32Syscall {\n")
        for number, name in sorted(compat32_numbers.items(), key=lambda kv: kv[0]):
            sig = resolve_signature(signatures_compat, name, prefer="compat")
            if sig is None:
                continue
            out.append(f"\t\t{name.upper()} = {number},\n")
        out.append("\t}\n")

    out.append("}\n")

    return "".join(out)


def resolve_signature(
    signatures: SignatureIndex,
    name: str,
    prefer: str,
) -> Optional[SyscallSignature]:
    direct = _lookup_signature_by_basename(signatures, name, prefer)
    if direct is not None:
        return direct

    impl = signatures.syscall_impls.get(name)
    if impl is None:
        return None

    entry, compat_entry = impl
    handler = (
        compat_entry if (prefer == "compat" and compat_entry is not None) else entry
    )
    if handler is None:
        return None

    proto = signatures.prototypes.get(handler)
    if proto is not None:
        padded = pad_args(list(proto))
        return SyscallSignature(
            name=name,
            big_endian_args=padded,
            little_endian_args=padded,
        )

    base = _strip_syscall_handler_prefix(handler)
    sig2 = _lookup_signature_by_basename(signatures, base, prefer)
    if sig2 is None:
        return None

    return SyscallSignature(
        name=name,
        big_endian_args=sig2.big_endian_args,
        little_endian_args=sig2.little_endian_args,
    )


def _strip_syscall_handler_prefix(handler: str) -> str:
    for p in ("compat_sys_", "sys_", "__se_sys_"):
        if handler.startswith(p):
            return handler[len(p):]
    return handler


def _lookup_signature_by_basename(
    signatures: SignatureIndex,
    base: str,
    prefer: str,
) -> Optional[SyscallSignature]:
    if prefer == "native":
        return signatures.native.get(base) or signatures.compat.get(base)
    else:
        return signatures.compat.get(base) or signatures.native.get(base)


def render_c_source(
    native_numbers: Dict[int, str],
    compat32_numbers: Optional[Dict[int, str]],
    signatures_native: SignatureIndex,
    signatures_compat: Optional[SignatureIndex],
) -> str:
    out: List[str] = [
        "/* AUTO-GENERATED FILE. DO NOT EDIT. */\n\n",
        '#include "frida-base.h"\n\n',
    ]

    _emit_c_signature_table(
        out,
        c_var="frida_syscall_signatures",
        numbers=native_numbers,
        signatures=signatures_native,
        prefer="native",
    )
    if compat32_numbers is not None:
        _emit_c_signature_table(
            out,
            c_var="frida_compat32_syscall_signatures",
            numbers=compat32_numbers,
            signatures=signatures_compat,
            prefer="compat",
        )

    _emit_c_signature_table_getter(
        out,
        c_func="frida_get_syscall_signatures",
        c_var="frida_syscall_signatures",
    )
    out.append("\n")
    _emit_c_signature_table_getter(
        out,
        c_func="frida_get_compat32_syscall_signatures",
        c_var="frida_compat32_syscall_signatures" if compat32_numbers is not None else None,
    )

    return "".join(out)


def _emit_c_signature_table(
    out: List[str],
    *,
    c_var: str,
    numbers: Dict[int, str],
    signatures: SignatureIndex,
    prefer: str,
) -> None:
    out += [
        f"static const FridaLinuxSyscallSignature {c_var}[] =\n",
        "{\n",
    ]

    for nr, name in sorted(numbers.items(), key=lambda kv: kv[0]):
        sig = resolve_signature(signatures, name, prefer=prefer)
        if sig is None:
            continue

        if sig.is_endian_sensitive():
            be_init = _emit_c_signature_initializer(
                nr, name, sig.big_endian_args, sig.nargs
            )
            le_init = _emit_c_signature_initializer(
                nr, name, sig.little_endian_args, sig.nargs
            )
            out.append("#if G_BYTE_ORDER == G_BIG_ENDIAN\n")
            out.append(f"  {be_init},\n")
            out.append("#else\n")
            out.append(f"  {le_init},\n")
            out.append("#endif\n")
        else:
            init = _emit_c_signature_initializer(nr, name, sig.big_endian_args, sig.nargs)
            out.append(f"  {init},\n")

    out.append("};\n\n")


def _emit_c_signature_initializer(
    nr: int,
    name: str,
    args: Tuple[Optional[SyscallArg], ...],
    nargs: int,
) -> str:
    inits: List[str] = []
    for a in args:
        if a is None:
            break
        inits.append(f"{{ {_c_escape_string(a.type)}, {_c_escape_string(a.name)} }}")
    inits_code = "{ " + ", ".join(inits) + " }" if inits else "{}"
    return (
        "{ "
        f"{nr}, "
        f"{_c_escape_string(name)}, "
        f"{nargs}, "
        f"{inits_code}"
        " }"
    )


def _emit_c_signature_table_getter(
    out: List[str],
    *,
    c_func: str,
    c_var: Optional[str],
) -> None:
    out += [
        "FridaLinuxSyscallSignature *\n",
        f"{c_func} (int * len)\n",
        "{\n",
    ]
    if c_var is not None:
        out += [
            "  if (len != NULL)\n",
            f"    *len = G_N_ELEMENTS ({c_var});\n",
            f"  return (FridaLinuxSyscallSignature *) {c_var};\n",
        ]
    else:
        out += [
            "  if (len != NULL)\n",
            "    *len = 0;\n",
            "  return NULL;\n",
        ]
    out.append("}\n")


def _c_escape_string(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


class Preprocessor:
    def __init__(
        self,
        text: str,
        rel_path: Path,
        initial_defines: Dict[str, Optional[int]],
        bits_per_long: int,
    ) -> None:
        self._text = text
        self._rel_path = rel_path
        self._defines = initial_defines
        for k, v in HARDCODED_CPP_CONSTANTS.items():
            if k not in self._defines:
                self._defines[k] = v
        self._bits_per_long = bits_per_long

    def run_and_collect_defines(self) -> Dict[str, Optional[int]]:
        self.compute_active_spans()
        return self._defines

    def compute_active_spans(self) -> List[Tuple[int, int]]:
        spans: List[Tuple[int, int]] = []
        stack: List[_IfFrame] = []

        offset = 0
        span_start = 0
        active = True

        lines = self._text.splitlines(keepends=True)
        i = 0
        while i < len(lines):
            raw_line = lines[i]
            line = raw_line.rstrip("\n")
            m = DEFINE_DIRECTIVE_RE.match(line)
            if m is not None:
                kw = m.group("kw")
                rest = strip_c_comments(m.group("rest"))

                if kw in {"if", "elif"}:
                    rest, consumed = self._gather_continued_directive_rest(
                        lines=lines,
                        start_index=i,
                        initial_rest=rest,
                    )
                    rest = strip_c_comments(rest)
                    i += consumed

                if kw in {"if", "ifdef", "ifndef", "elif", "else", "endif"}:
                    if active and offset > span_start:
                        spans.append((span_start, offset))

                    stack, active = self._apply_conditional(
                        stack=stack,
                        kw=kw,
                        rest=rest,
                    )
                    span_start = offset + len(raw_line)
                elif active and kw in {"define", "undef"}:
                    self._apply_define_like(kw, rest)

            offset += len(raw_line)
            i += 1

        if active and offset > span_start:
            spans.append((span_start, offset))

        return spans

    def _gather_continued_directive_rest(
        self,
        lines: List[str],
        start_index: int,
        initial_rest: str,
    ) -> Tuple[str, int]:
        rest = initial_rest
        consumed = 0

        idx = start_index
        while True:
            raw_line = lines[idx]
            consumed += 1

            stripped = raw_line.rstrip("\n").rstrip()
            if not stripped.endswith("\\"):
                break

            rest = rest.rstrip()
            if rest.endswith("\\"):
                rest = rest[:-1].rstrip()

            idx += 1
            if idx >= len(lines):
                raise RuntimeError(f"{self._rel_path}: unterminated line continuation")

            next_line = lines[idx].rstrip("\n")
            rest = f"{rest} {next_line.strip()}"

        return rest, consumed - 1

    def _apply_define_like(self, kw: str, rest: str) -> None:
        rest = strip_c_comments(rest)

        parts = rest.split(None, 1)
        if not parts or not parts[0]:
            raise RuntimeError(f"{self._rel_path}: malformed #{kw}")

        name = parts[0]
        if kw == "undef":
            self._defines.pop(name, None)
            return

        value: Optional[int] = None
        if len(parts) == 2:
            tail = parts[1].strip()
            m = LEADING_DECIMAL_RE.match(tail)
            if m is not None:
                value = int(m.group(1), 10)

        self._defines[name] = value

    def _apply_conditional(
        self,
        stack: List["_IfFrame"],
        kw: str,
        rest: str,
    ) -> Tuple[List["_IfFrame"], bool]:
        if kw == "if":
            parent_active = stack[-1].active if stack else True
            cond = self._eval_expr(rest) if parent_active else False
            active = parent_active and cond
            stack.append(
                _IfFrame(parent_active=parent_active, any_taken=active, active=active)
            )
            return stack, self._compute_active(stack)

        if kw == "ifdef":
            name = rest.split()[0] if rest else ""
            if not name:
                raise RuntimeError(f"{self._rel_path}: malformed #ifdef")
            cond = name in self._defines
            parent_active = stack[-1].active if stack else True
            active = parent_active and cond
            stack.append(
                _IfFrame(parent_active=parent_active, any_taken=active, active=active)
            )
            return stack, self._compute_active(stack)

        if kw == "ifndef":
            name = rest.split()[0] if rest else ""
            if not name:
                raise RuntimeError(f"{self._rel_path}: malformed #ifndef")
            cond = name not in self._defines
            parent_active = stack[-1].active if stack else True
            active = parent_active and cond
            stack.append(
                _IfFrame(parent_active=parent_active, any_taken=active, active=active)
            )
            return stack, self._compute_active(stack)

        if kw == "elif":
            if not stack:
                raise RuntimeError(f"{self._rel_path}: #elif without #if")
            top = stack[-1]

            if not top.parent_active:
                stack[-1] = _IfFrame(
                    parent_active=False, any_taken=top.any_taken, active=False
                )
                return stack, self._compute_active(stack)

            if top.any_taken:
                stack[-1] = _IfFrame(parent_active=True, any_taken=True, active=False)
                return stack, self._compute_active(stack)

            cond = self._eval_expr(rest)
            active = cond
            stack[-1] = _IfFrame(parent_active=True, any_taken=active, active=active)
            return stack, self._compute_active(stack)

        if kw == "else":
            if not stack:
                raise RuntimeError(f"{self._rel_path}: #else without #if")
            top = stack[-1]
            if not top.parent_active:
                stack[-1] = _IfFrame(
                    parent_active=False, any_taken=top.any_taken, active=False
                )
                return stack, self._compute_active(stack)
            active = not top.any_taken
            stack[-1] = _IfFrame(parent_active=True, any_taken=True, active=active)
            return stack, self._compute_active(stack)

        if kw == "endif":
            if not stack:
                raise RuntimeError(f"{self._rel_path}: #endif without #if")
            stack.pop()
            return stack, self._compute_active(stack)

        raise RuntimeError(f"{self._rel_path}: unsupported directive #{kw}")

    def _compute_active(self, stack: List["_IfFrame"]) -> bool:
        return all(frame.active for frame in stack) if stack else True

    def _eval_expr(self, expr: str) -> bool:
        expr_s = expr.strip()
        if expr_s == "":
            raise RuntimeError(f"{self._rel_path}: empty #if expression")

        expr_s = self._replace_bits_per_long(expr_s)

        tokens = _tokenize_expr(expr_s, self._rel_path)
        parser = _ExprParser(tokens, self._rel_path, self._defines)
        return parser.parse()

    def _replace_bits_per_long(self, expr: str) -> str:
        expr = BITS_PER_LONG_IDENT_RE.sub(str(self._bits_per_long), expr)
        expr = BITS_PER_LONG2_IDENT_RE.sub(str(self._bits_per_long), expr)
        return expr


@dataclass(frozen=True)
class _IfFrame:
    parent_active: bool
    any_taken: bool
    active: bool


@dataclass(frozen=True)
class _Token:
    kind: str
    value: str


def _tokenize_expr(expr: str, rel_path: Path) -> List[_Token]:
    tokens: List[_Token] = []
    i = 0
    while i < len(expr):
        ch = expr[i]
        if ch.isspace():
            i += 1
            continue

        if expr.startswith("&&", i):
            tokens.append(_Token("and", "&&"))
            i += 2
            continue
        if expr.startswith("||", i):
            tokens.append(_Token("or", "||"))
            i += 2
            continue
        if expr.startswith(">=", i):
            tokens.append(_Token("ge", ">="))
            i += 2
            continue
        if expr.startswith("<=", i):
            tokens.append(_Token("le", "<="))
            i += 2
            continue
        if expr.startswith("==", i):
            tokens.append(_Token("eq", "=="))
            i += 2
            continue
        if expr.startswith("!=", i):
            tokens.append(_Token("ne", "!="))
            i += 2
            continue
        if expr.startswith("<<", i):
            tokens.append(_Token("shl", "<<"))
            i += 2
            continue
        if expr.startswith(">>", i):
            tokens.append(_Token("shr", ">>"))
            i += 2
            continue

        if ch == ">":
            tokens.append(_Token("gt", ">"))
            i += 1
            continue
        if ch == "<":
            tokens.append(_Token("lt", "<"))
            i += 1
            continue
        if ch == "!":
            tokens.append(_Token("not", "!"))
            i += 1
            continue
        if ch == "~":
            tokens.append(_Token("bnot", "~"))
            i += 1
            continue
        if ch == "&":
            tokens.append(_Token("band", "&"))
            i += 1
            continue
        if ch == "|":
            tokens.append(_Token("bor", "|"))
            i += 1
            continue
        if ch == "^":
            tokens.append(_Token("bxor", "^"))
            i += 1
            continue
        if ch == "+":
            tokens.append(_Token("plus", "+"))
            i += 1
            continue
        if ch == "-":
            tokens.append(_Token("minus", "-"))
            i += 1
            continue
        if ch == "*":
            tokens.append(_Token("mul", "*"))
            i += 1
            continue
        if ch == "/":
            tokens.append(_Token("div", "/"))
            i += 1
            continue
        if ch == "%":
            tokens.append(_Token("mod", "%"))
            i += 1
            continue

        if ch == "(":
            tokens.append(_Token("lparen", "("))
            i += 1
            continue
        if ch == ")":
            tokens.append(_Token("rparen", ")"))
            i += 1
            continue

        if ch.isdigit():
            j = i + 1

            if ch == "0" and j < len(expr) and expr[j] in ("x", "X"):
                j += 1
                k = j
                while k < len(expr) and (
                    expr[k].isdigit() or ("a" <= expr[k].lower() <= "f")
                ):
                    k += 1
                if k == j:
                    raise RuntimeError(
                        f"{rel_path}: invalid hex literal in expression: {expr[i:]!r}"
                    )
                tokens.append(_Token("num", expr[i:k]))
                i = k
                continue

            while j < len(expr) and expr[j].isdigit():
                j += 1
            tokens.append(_Token("num", expr[i:j]))
            i = j
            continue

        if ch == "_" or ch.isalpha():
            j = i + 1
            while j < len(expr) and (expr[j] == "_" or expr[j].isalnum()):
                j += 1
            tokens.append(_Token("ident", expr[i:j]))
            i = j
            continue

        raise RuntimeError(f"{rel_path}: unsupported token in expression: {expr[i:]!r}")

    tokens.append(_Token("eof", ""))
    return tokens


class _ExprParser:
    def __init__(
        self, tokens: List[_Token], rel_path: Path, defines: Dict[str, Optional[int]]
    ) -> None:
        self._tokens = tokens
        self._i = 0
        self._rel_path = rel_path
        self._defines = defines

    def parse(self) -> bool:
        value = self._parse_logical_or()
        if self._peek().kind != "eof":
            raise RuntimeError(f"{self._rel_path}: trailing tokens in #if expression")
        return value != 0

    def _peek(self) -> _Token:
        return self._tokens[self._i]

    def _pop(self) -> _Token:
        tok = self._tokens[self._i]
        self._i += 1
        return tok

    def _expect(self, kind: str) -> _Token:
        tok = self._peek()
        if tok.kind != kind:
            raise RuntimeError(
                f"{self._rel_path}: expected {kind}, got {tok.kind} ({tok.value!r})"
            )
        return self._pop()

    def _parse_defined_operator(self) -> int:
        tok = self._peek()
        if tok.kind == "lparen":
            self._pop()
            ident = self._peek()
            if ident.kind != "ident":
                raise RuntimeError(f"{self._rel_path}: malformed defined()")
            macro = ident.value
            self._pop()
            self._expect("rparen")
        else:
            ident = self._peek()
            if ident.kind != "ident":
                raise RuntimeError(f"{self._rel_path}: malformed defined operator")
            macro = ident.value
            self._pop()
        return 1 if macro in self._defines else 0

    def _parse_primary(self) -> int:
        tok = self._peek()

        if tok.kind == "ident":
            self._pop()
            name = tok.value

            if name == "defined":
                return self._parse_defined_operator()

            if name in {"IS_ENABLED", "IS_MODULE"}:
                self._skip_paren_payload(name)
                return 1

            if name not in self._defines:
                raise RuntimeError(
                    f"{self._rel_path}: unknown identifier {name!r} in #if expression"
                )
            v = self._defines[name]
            if v is None:
                raise RuntimeError(
                    f"{self._rel_path}: identifier {name!r} has non-numeric or unknown value in #if expression"
                )
            return v

        if tok.kind == "num":
            self._pop()
            return int(tok.value, 0)

        if tok.kind == "lparen":
            self._pop()
            value = self._parse_logical_or()
            self._expect("rparen")
            return value

        raise RuntimeError(
            f"{self._rel_path}: unsupported #if expression token: {tok.value!r}"
        )

    def _skip_paren_payload(self, func_name: str) -> None:
        if self._peek().kind != "lparen":
            raise RuntimeError(
                f"{self._rel_path}: malformed {func_name}() in #if expression"
            )

        self._pop()
        depth = 1
        while depth != 0:
            tok = self._peek()
            if tok.kind == "eof":
                raise RuntimeError(
                    f"{self._rel_path}: unterminated {func_name}() in #if expression"
                )
            self._pop()
            if tok.kind == "lparen":
                depth += 1
            elif tok.kind == "rparen":
                depth -= 1

    def _parse_logical_or(self) -> int:
        left = self._parse_logical_and()
        while self._peek().kind == "or":
            self._pop()
            right = self._parse_logical_and()
            left = 1 if (left != 0 or right != 0) else 0
        return left

    def _parse_logical_and(self) -> int:
        left = self._parse_bitor()
        while self._peek().kind == "and":
            self._pop()
            right = self._parse_bitor()
            left = 1 if (left != 0 and right != 0) else 0
        return left

    def _parse_bitor(self) -> int:
        left = self._parse_bitxor()
        while self._peek().kind == "bor":
            self._pop()
            right = self._parse_bitxor()
            left = left | right
        return left

    def _parse_bitxor(self) -> int:
        left = self._parse_bitand()
        while self._peek().kind == "bxor":
            self._pop()
            right = self._parse_bitand()
            left = left ^ right
        return left

    def _parse_bitand(self) -> int:
        left = self._parse_cmp()
        while self._peek().kind == "band":
            self._pop()
            right = self._parse_cmp()
            left = left & right
        return left

    def _parse_cmp(self) -> int:
        left = self._parse_shift()
        k = self._peek().kind
        if k in {"eq", "ne", "gt", "lt", "ge", "le"}:
            op = self._pop().kind
            right = self._parse_shift()
            if op == "eq":
                return 1 if left == right else 0
            if op == "ne":
                return 1 if left != right else 0
            if op == "gt":
                return 1 if left > right else 0
            if op == "lt":
                return 1 if left < right else 0
            if op == "ge":
                return 1 if left >= right else 0
            return 1 if left <= right else 0
        return left

    def _parse_shift(self) -> int:
        left = self._parse_add()
        while self._peek().kind in {"shl", "shr"}:
            op = self._pop().kind
            right = self._parse_add()
            if right < 0:
                raise RuntimeError(
                    f"{self._rel_path}: negative shift in #if expression"
                )
            left = (left << right) if op == "shl" else (left >> right)
        return left

    def _parse_add(self) -> int:
        left = self._parse_mul()
        while self._peek().kind in {"plus", "minus"}:
            op = self._pop().kind
            right = self._parse_mul()
            left = (left + right) if op == "plus" else (left - right)
        return left

    def _parse_mul(self) -> int:
        left = self._parse_unary()
        while self._peek().kind in {"mul", "div", "mod"}:
            op = self._pop().kind
            right = self._parse_unary()
            if op == "mul":
                left = left * right
            elif op == "div":
                if right == 0:
                    raise RuntimeError(f"{self._rel_path}: division by zero in #if")
                left = int(left / right)
            else:
                if right == 0:
                    raise RuntimeError(f"{self._rel_path}: modulo by zero in #if")
                left = left % right
        return left

    def _parse_unary(self) -> int:
        k = self._peek().kind
        if k == "not":
            self._pop()
            v = self._parse_unary()
            return 1 if v == 0 else 0
        if k == "bnot":
            self._pop()
            v = self._parse_unary()
            return ~v
        if k == "plus":
            self._pop()
            return +self._parse_unary()
        if k == "minus":
            self._pop()
            return -self._parse_unary()
        return self._parse_primary()


if __name__ == "__main__":
    raise SystemExit(main())
