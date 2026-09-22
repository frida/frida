#!/usr/bin/env python3

import argparse
from enum import Enum
import hashlib
from pathlib import Path
import subprocess
import sys
import textwrap
from typing import Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))
from releng import devkit, env, machine_spec

DepSymbolScope = devkit.DepSymbolScope


def main():
    raw_args: List[str] = []
    ool_optvals: Dict[str, List[str]] = {}
    pending_raw_args = sys.argv[1:]
    while len(pending_raw_args) > 0:
        cur = pending_raw_args.pop(0)
        if cur == ">>>":
            ool_hash = hashlib.sha256()
            ool_strv = []
            while True:
                cur = pending_raw_args.pop(0)
                if cur == "<<<":
                    break
                ool_hash.update(cur.encode("utf-8"))
                ool_strv.append(cur)
            val_id = "ool:" + ool_hash.hexdigest()
            ool_optvals[val_id] = ool_strv
            raw_args.append(val_id)
        else:
            raw_args.append(cur)

    parser = argparse.ArgumentParser(formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("kit")
    parser.add_argument("machine",
                        type=machine_spec.MachineSpec.parse)
    parser.add_argument("outdir",
                        type=Path)
    parser.add_argument("-t", "--thin",
                        help="build without cross-arch support",
                        action="store_const",
                        dest="flavor",
                        const="_thin",
                        default="")
    parser.add_argument("--dep-symbol-scope",
                        metavar="S",
                        type=DepSymbolScope,
                        choices=list(DepSymbolScope),
                        default=DepSymbolScope.PREFIXED.value,
                        help=textwrap.dedent("""\
                            how to scope symbols from third-party archives:
                             - prefixed: add the '_frida_' prefix
                             - original: keep symbols exactly as upstream
                            """))
    parser.add_argument("--cc",
                        help="C compiler to use",
                        type=lambda v: parse_array_option_value(v, ool_optvals))
    machine_options = dict.fromkeys(["c_args", "lib", "libtool", "ar", "nm", "objcopy", "pkg_config", "pkg_config_path"])
    for name in machine_options.keys():
        pretty_name = name.replace("_", "-")
        parser.add_argument("--" + pretty_name,
                            help=f"the {pretty_name} to use",
                            type=lambda v: parse_array_option_value(v, ool_optvals))

    options = parser.parse_args(raw_args)

    kit = options.kit
    machine = options.machine
    outdir = options.outdir.resolve()
    flavor = options.flavor

    cc = options.cc
    if cc is not None:
        meson_config = {"c": cc}
        for k, v in vars(options).items():
            if k in machine_options and v is not None:
                name = "pkg-config" if k == "pkg_config" else k
                meson_config[name] = v
    else:
        build_dir = REPO_ROOT / "build"

        if flavor == "":
            fat_machine_file = env.query_machine_file_path(machine, flavor, build_dir)
            if not fat_machine_file.exists() \
                    and env.query_machine_file_path(machine, "_thin", build_dir).exists():
                flavor = "_thin"

        meson_config = env.load_meson_config(machine, flavor, build_dir)
        assert meson_config is not None

    try:
        app = devkit.CompilerApplication(kit, machine, meson_config, outdir, options.dep_symbol_scope)
        app.run()
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        if e.output:
            print("Stdout:", e.output, file=sys.stderr)
        if e.stderr:
            print("Stderr:", e.stderr, file=sys.stderr)
        sys.exit(1)


def parse_array_option_value(val: str, ool_optvals: Dict[str, List[str]]) -> Optional[List[str]]:
    if val == "":
        return None
    if val.startswith("ool:"):
        ool_val = ool_optvals.get(val)
        if ool_val is not None:
            return ool_val
    return [val]


if __name__ == "__main__":
    main()
