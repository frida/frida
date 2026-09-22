import shutil
import subprocess
import sys
from pathlib import Path
from typing import List


def main(argv):
    output_js, priv_dir, npm, *inputs = [Path(d).resolve() for d in argv[1:]]

    try:
        compile_js_agent(output_js, inputs, priv_dir, npm)
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


def compile_js_agent(output_js: Path, inputs: List[Path], priv_dir: Path, npm: Path):
    base_dir = min(inputs, key=lambda p: len(p.parts)).parent
    for f in inputs:
        dest = priv_dir / f.relative_to(base_dir)
        dest.parent.mkdir(exist_ok=True)
        shutil.copy(f, dest)

    run_kwargs = {
        "cwd": priv_dir,
        "stdout": subprocess.PIPE,
        "stderr": subprocess.STDOUT,
        "encoding": "utf-8",
        "check": True,
    }

    try:
        subprocess.run([npm, "install"], **run_kwargs)
        subprocess.run([npm, "run", "build"], **run_kwargs)
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        print(
            "Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")),
            file=sys.stderr,
        )
        sys.exit(1)

    shutil.copy(priv_dir / output_js.name, output_js)


if __name__ == "__main__":
    main(sys.argv)
