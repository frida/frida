#!/bin/sh
# Quick feedback loop: regenerate bindings via run_bindgen.sh, then compile the
# generated extension.c with the exact flags ninja would use, without linking.
set -e

repo=$(cd "$(dirname "$0")/.." && pwd)

"$repo/tests/run_bindgen.sh"

python3 - "$repo" "$@" <<'EOF'
import json
import shlex
import subprocess
import sys

repo = sys.argv[1]
extra = sys.argv[2:]

cdb = json.load(open(f"{repo}/build/compile_commands.json"))
entry = next(e for e in cdb if e["file"].endswith("extension.c"))
args = shlex.split(entry["command"])

cmd = []
i = 0
while i < len(args):
    a = args[i]
    if a in ("-o", "-MF", "-MQ", "-c"):
        i += 2
        continue
    if a == "-MD":
        i += 1
        continue
    cmd.append(a)
    i += 1
cmd += ["-fsyntax-only", f"{repo}/build/bindgen-out/extension.c"] + extra

sys.exit(subprocess.run(cmd, cwd=entry["directory"]).returncode)
EOF
