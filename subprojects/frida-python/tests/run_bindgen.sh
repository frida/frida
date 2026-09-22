#!/bin/sh
# Quick feedback loop: run the bindgen against the .gir files from the build
# tree, without going through ninja. Outputs land in tests/bindgen-out/.
set -e

repo=$(dirname "$0")/..
girdir=$repo/build/subprojects/frida-core/src/api
outdir=$repo/build/bindgen-out
mkdir -p "$outdir"

PYTHONPATH=$repo/frida:$repo/frida-bindgen exec python3 -m frida_bindgen \
	--frida-gir="$girdir/Frida-1.0.gir" \
	--glib-gir="$girdir/GLib-2.0.gir" \
	--gobject-gir="$girdir/GObject-2.0.gir" \
	--gio-gir="$girdir/Gio-2.0.gir" \
	--output-py="$outdir/__init__.py" \
	--output-aio="$outdir/aio.py" \
	--output-pyi="$outdir/_frida.pyi" \
	--output-c="$outdir/extension.c" \
	"$@"
