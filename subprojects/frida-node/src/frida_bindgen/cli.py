from __future__ import annotations

import argparse
from io import StringIO
from pathlib import Path

from . import codegen
from .customization import load_customizations
from .loader import compute_model


def main():
    run(build_arguments())


def build_arguments() -> argparse.Namespace:
    p = argparse.ArgumentParser(
        description="Generate TypeScript and Node-API bindings for Frida."
    )
    p.add_argument(
        "--frida-gir",
        required=True,
        type=Path,
        help="Path to the Frida .gir file.",
    )
    p.add_argument(
        "--glib-gir",
        required=True,
        type=Path,
        help="Path to the GLib .gir file.",
    )
    p.add_argument(
        "--gobject-gir",
        required=True,
        type=Path,
        help="Path to the GObject .gir file.",
    )
    p.add_argument(
        "--gio-gir",
        required=True,
        type=Path,
        help="Path to the GIO .gir file.",
    )
    p.add_argument(
        "--output-ts",
        required=True,
        type=Path,
        help="Path to the output TypeScript file.",
    )
    p.add_argument(
        "--output-dts",
        required=True,
        type=Path,
        help="Path to the output TypeScript declaration file.",
    )
    p.add_argument(
        "--output-c",
        required=True,
        type=Path,
        help="Path to the output C file for N-API bindings.",
    )
    return p.parse_args()


def run(args: argparse.Namespace) -> None:
    customizations = load_customizations()
    model = compute_model(
        args.frida_gir, args.glib_gir, args.gobject_gir, args.gio_gir, customizations
    )

    artefacts = codegen.generate_all(model)

    with OutputFile(args.output_ts) as f:
        f.write(artefacts["ts"])
    with OutputFile(args.output_dts) as f:
        f.write(artefacts["dts"])
    with OutputFile(args.output_c) as f:
        f.write(artefacts["c"])


class OutputFile:
    def __init__(self, output_path):
        self._output_path = output_path
        self._io = StringIO()

    def __enter__(self):
        return self._io

    def __exit__(self, *exc):
        result = self._io.getvalue()
        if self._output_path.exists():
            existing_contents = self._output_path.read_text(encoding="utf-8")
            if existing_contents == result:
                return False
        self._output_path.write_text(result, encoding="utf-8")
        return False
