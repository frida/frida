#!/usr/bin/env python3

import argparse
from collections.abc import Iterable, Mapping
from dataclasses import dataclass
from pathlib import Path
import re
import subprocess


RAW_ADDRESS_PATTERN = re.compile(r"\b(0x[0-9a-f]+)\b")
SYMBOL_PATTERN = re.compile(r"(.+ )\((.+):\d+\)")

@dataclass
class DeclaredModule:
    path: Path
    start: int
    end: int

    def __hash__(self):
        return self.path.__hash__()

PendingAddresses = Mapping[DeclaredModule, set[int]]


def main():
    parser = argparse.ArgumentParser(description="Symbolicate stack traces.")
    parser.add_argument("--input", dest="input", required=True,
                        help="the file to symbolicate")
    parser.add_argument("--output", dest="output", required=True,
                        help="where the symbolicated file will be written")
    parser.add_argument("--declare-module", dest="modules", required=True, action="append",
                        help="declare a module at path:base")
    args = parser.parse_args()

    modules = []
    for mod in args.modules:
        raw_path, raw_base = mod.split(":", maxsplit=1)
        path = Path(raw_path)
        base = int(raw_base, 16)
        size = compute_module_size(path)
        modules.append(DeclaredModule(path, base, base + size))

    with Path(args.input).open(encoding="utf-8") as input_file:
        addresses = compute_pending_addresses(input_file, modules)

    symbols = symbolicate_pending_addresses(addresses)

    def symbolicate(m):
        raw_address = m.group(1)
        address = int(raw_address, 16)

        name = symbols.get(address, None)
        if name is not None:
            return name

        return raw_address

    with Path(args.input).open(encoding="utf-8") as input_file, \
            Path(args.output).open("w", encoding="utf-8") as output_file:
        for line_raw in input_file:
            line_symbolicated = RAW_ADDRESS_PATTERN.sub(symbolicate, line_raw)
            output_file.write(line_symbolicated)


def compute_pending_addresses(data: Iterable[str], modules: Iterable[DeclaredModule]) -> PendingAddresses:
    addresses = {}
    for raw_line in data:
        for match in RAW_ADDRESS_PATTERN.finditer(raw_line):
            address = int(match.group(1), 16)
            module = find_declared_module_by_address(address, modules)
            if module is not None:
                pending = addresses.get(module, None)
                if pending is None:
                    pending = set()
                    addresses[module] = pending
                pending.add(address)
    return addresses


def symbolicate_pending_addresses(addresses: PendingAddresses) -> Mapping[int, str]:
    symbols = {}
    for module, pending in addresses.items():
        pending = list(pending)
        pending.sort()
        query = subprocess.run([
                "atos",
                "-o", module.path,
                "-l", hex(module.start),
            ] + [hex(address) for address in pending],
            capture_output=True,
            encoding="utf-8",
            check=True)
        results = [normalize_symbol(line) for line in query.stdout.split("\n")]
        symbols.update(dict(zip(pending, results)))
    return symbols


def normalize_symbol(symbol):
    return SYMBOL_PATTERN.sub(lambda m: "".join([m.group(1), "(", m.group(2), ")"]), symbol)


def find_declared_module_by_address(address, modules):
    for m in modules:
        if address >= m.start and address < m.end:
            return m
    return None


def compute_module_size(path: Path) -> int:
    for raw_line in subprocess.run(["otool", "-l", path], capture_output=True, encoding="utf-8").stdout.split("\n"):
        line = raw_line.lstrip()
        if line.startswith("vmsize"):
            tokens = line.split(" ", maxsplit=1)
            return int(tokens[1], 16)
    assert False


if __name__ == "__main__":
    main()
