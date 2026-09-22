#!/usr/bin/env python3
"""Build a Frida devkit as an Apple XCFramework."""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from collections import OrderedDict
from pathlib import Path

KITS = {
    "gum": {
        "kit_id": "frida-gum",
        "meson_name": "gum",
        "framework_name": "FridaGum",
    },
    "gumjs": {
        "kit_id": "frida-gumjs",
        "meson_name": "gumjs",
        "framework_name": "FridaGumJS",
    },
}

PROJECT_DIR = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(
        description="Build a Frida devkit as an Apple XCFramework.",
        epilog="""\
examples:
  %(prog)s ios-arm64-simulator ios-x86_64-simulator
  %(prog)s ios-arm64 ios-arm64-simulator ios-x86_64-simulator
  %(prog)s macos-arm64 macos-x86_64
  %(prog)s --kit gumjs ios-arm64 ios-arm64-simulator""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "targets",
        nargs="+",
        metavar="TARGET",
        help="Frida host triple (e.g. ios-arm64, ios-arm64-simulator, macos-arm64)",
    )
    parser.add_argument(
        "--kit",
        choices=KITS.keys(),
        default="gum",
        help="devkit to build (default: gum)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="output .xcframework path (default: build/<Name>.xcframework)",
    )
    parser.add_argument(
        "--strip",
        action="store_true",
        default=True,
        dest="strip",
        help="strip debug symbols (default)",
    )
    parser.add_argument(
        "--no-strip",
        action="store_false",
        dest="strip",
        help="keep debug symbols",
    )
    args = parser.parse_args()

    kit = KITS[args.kit]
    lib_filename = f"lib{kit['kit_id']}.a"
    header_filename = f"{kit['kit_id']}.h"

    output = args.output
    if output is None:
        output = PROJECT_DIR / "build" / f"{kit['framework_name']}.xcframework"

    groups = group_targets(args.targets)

    print(f"==> Building {kit['kit_id']} devkit for: {' '.join(args.targets)}")

    for target in args.targets:
        build_target(target, kit["meson_name"])

    staging_dir = Path(tempfile.mkdtemp())
    try:
        xcframework_args = []

        for group_name, targets in groups.items():
            group_dir = staging_dir / group_name
            headers_dir = group_dir / "Headers"
            headers_dir.mkdir(parents=True)

            libs = []
            header_src = None
            for target in targets:
                devkit_dir = PROJECT_DIR / f"build/{target}" / kit["meson_name"] / "devkit"
                lib = devkit_dir / lib_filename
                if not lib.exists():
                    sys.exit(f"ERROR: Expected artifact not found: {lib}")
                libs.append(lib)

                if header_src is None:
                    header_src = devkit_dir / header_filename
                    if not header_src.exists():
                        sys.exit(f"ERROR: Expected header not found: {header_src}")

            fat_lib = group_dir / lib_filename
            if len(libs) > 1:
                print(f"==> [{group_name}] Creating fat binary with lipo...")
                run(["lipo", "-create"] + [str(l) for l in libs] + ["-output", str(fat_lib)])
            else:
                shutil.copy2(libs[0], fat_lib)

            if args.strip:
                print(f"==> [{group_name}] Stripping debug symbols...")
                run(["strip", "-S", str(fat_lib)])

            shutil.copy2(header_src, headers_dir)

            xcframework_args += ["-library", str(fat_lib), "-headers", str(headers_dir)]

        print("==> Creating XCFramework...")
        output.parent.mkdir(parents=True, exist_ok=True)
        if output.exists():
            shutil.rmtree(output)

        run(["xcodebuild", "-create-xcframework"] + xcframework_args + ["-output", str(output)])

        print(f"==> Done: {output}")

    finally:
        shutil.rmtree(staging_dir, ignore_errors=True)


def group_targets(targets):
    """Group targets by SDK variant for lipo.

    Targets sharing the same platform variant (e.g. ios-simulator) are
    combined into a single fat binary. Each group becomes a separate
    -library entry in the XCFramework.
    """
    groups = OrderedDict()
    for target in targets:
        if target.endswith("-simulator"):
            # ios-arm64-simulator -> ios-simulator
            parts = target.split("-")
            group = f"{parts[0]}-simulator"
        else:
            # ios-arm64 -> ios, macos-arm64 -> macos
            group = target.rsplit("-", 1)[0]
        groups.setdefault(group, []).append(target)
    return groups


def build_target(target, devkit_meson_name):
    build_dir = PROJECT_DIR / f"build/{target}"

    if (build_dir / "build.ninja").exists():
        print(f"==> [{target}] Already configured, rebuilding...")
    else:
        print(f"==> [{target}] Configuring...")
        build_dir.mkdir(parents=True, exist_ok=True)
        run(
            [
                str(PROJECT_DIR / "configure"),
                "--host", target,
                "--with-devkits", devkit_meson_name,
                "--disable-tests",
            ],
            cwd=build_dir,
        )

    print(f"==> [{target}] Building...")
    run(["make", "-C", str(build_dir)])


def run(cmd, **kwargs):
    result = subprocess.run(cmd, **kwargs)
    if result.returncode != 0:
        sys.exit(result.returncode)


if __name__ == "__main__":
    main()
