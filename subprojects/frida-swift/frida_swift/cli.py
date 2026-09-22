from __future__ import annotations

import argparse
from io import StringIO
from pathlib import Path

from . import codegen
from .customization import load_customizations
from .loader import compute_model


def main() -> None:
    run(build_arguments())


def build_arguments() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Generate the Swift bindings for Frida.")
    p.add_argument("--frida-gir", required=True, type=Path)
    p.add_argument("--glib-gir", required=True, type=Path)
    p.add_argument("--gobject-gir", required=True, type=Path)
    p.add_argument("--gio-gir", required=True, type=Path)
    p.add_argument("--output-swift", required=True, type=Path)
    return p.parse_args()


def run(args: argparse.Namespace) -> None:
    customizations = load_customizations()
    model = compute_model(
        args.frida_gir, args.glib_gir, args.gobject_gir, args.gio_gir, customizations
    )

    swift = codegen.generate_swift(model)

    with OutputFile(args.output_swift) as f:
        f.write(swift)


class OutputFile:
    def __init__(self, output_path: Path):
        self._output_path = output_path
        self._io = StringIO()

    def __enter__(self):
        return self._io

    def __exit__(self, *exc):
        result = self._io.getvalue()
        if self._output_path.exists():
            if self._output_path.read_text(encoding="utf-8") == result:
                return False
        self._output_path.write_text(result, encoding="utf-8")
        return False
