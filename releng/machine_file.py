#!/usr/bin/env python3

import argparse
from configparser import ConfigParser
from pathlib import Path
import sys
from typing import Dict, List, Union


class CommandError(Exception):
    pass


def main():
    parser = argparse.ArgumentParser(description="Tool for working with Meson machine files.")
    subparsers = parser.add_subparsers()

    command = subparsers.add_parser("to-env", help="generate a legacy build environment from a machine file")
    command.add_argument("machine_file", help="machine file")
    command.add_argument("--flavor", help="environment flavor", default="c", choices=["c", "cpp"])
    command.set_defaults(func=lambda args: to_env(Path(args.machine_file), args.flavor))

    args = parser.parse_args()
    if 'func' in args:
        try:
            args.func(args)
        except CommandError as e:
            print(e, file=sys.stderr)
            sys.exit(1)
    else:
        parser.print_usage(file=sys.stderr)
        sys.exit(1)


def to_env(mfile: Path, flavor: Union["c", "cpp"]):
    config = load(mfile)

    if flavor == "c":
        entries = [
            ("CC", "c"),
            ("CFLAGS", "c_args"),
            ("LDFLAGS", "c_link_args"),
        ]
    else:
        entries = [
            ("CXX", "cpp"),
            ("CXXFLAGS", "cpp_args"),
            ("LDFLAGS", "cpp_link_args"),
        ]
    entries += [
        ("AR", "ar"),
        ("NM", "nm"),
        ("RANLIB", "ranlib"),
        ("STRIP", "strip"),
        ("READELF", "readelf"),
        ("OBJCOPY", "objcopy"),
        ("OBJDUMP", "objdump"),
        ("PKG_CONFIG", "pkgconfig"),
    ]
    for envvar_name, property_name in entries:
        value = config.get(property_name, None)
        if value is None:
            continue
        encoded_value = " ".join(value)
        print(f"export {envvar_name}=\"{encoded_value}\"")


def load(mfile: Path) -> Dict[str, Union[str, List[str]]]:
    config = ConfigParser()
    config.read(mfile)

    hidden_constants = {
        "true": True,
        "false": False,
    }

    items = {}
    for name, raw_value in config.items("constants"):
        items[name] = eval(raw_value, hidden_constants, items)

    for section_name, section in config.items():
        if section_name in ("DEFAULT", "constants"):
            continue
        for name, raw_value in section.items():
            value = eval(raw_value, hidden_constants, items)
            if section_name == "binaries" and isinstance(value, str):
                value = [value]
            items[name] = value

    return items


if __name__ == "__main__":
    main()
