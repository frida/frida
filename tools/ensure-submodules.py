#!/usr/bin/env python3

import subprocess
import sys
from pathlib import Path

SOURCE_ROOT = Path(__file__).resolve().parent.parent
UPDATE_FLAGS = ["--init", "--depth", "1"]


def main(argv: list[str]):
    names = argv[1:]
    if not names:
        names = ["frida-gum", "frida-core"]
    paths_to_check = [Path("subprojects") / name for name in names]

    try:
        releng = SOURCE_ROOT / "releng"
        if not (releng / "meson" / "meson.py").exists():
            print(f"Fetching releng...", flush=True)
            run(["git", "submodule", "update", *UPDATE_FLAGS, releng.name], cwd=SOURCE_ROOT)
            run(["git", "submodule", "update", *UPDATE_FLAGS], cwd=releng)

        for relpath in paths_to_check:
            if not (SOURCE_ROOT / relpath / "meson.build").exists():
                print(f"Fetching {relpath.name}...", flush=True)
                run(["git", "submodule", "update", *UPDATE_FLAGS, relpath], cwd=SOURCE_ROOT)
    except Exception as e:
        print(e, file=sys.stderr)
        if isinstance(e, subprocess.CalledProcessError):
            for label, data in [("Output", e.output), ("Stderr", e.stderr)]:
                if data:
                    print(f"{label}:\n\t| " + "\n\t| ".join(data.strip().split("\n")), file=sys.stderr)
        sys.exit(1)


def run(argv: list[str], **kwargs) -> subprocess.CompletedProcess:
    return subprocess.run(argv, capture_output=True, encoding="utf-8", check=True, **kwargs)


if __name__ == "__main__":
    main(sys.argv)
