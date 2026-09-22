import shutil
import subprocess
import sys
from pathlib import Path
from typing import List


def main(argv):
    output_dir, priv_dir, npm, *inputs = [Path(d).resolve() for d in argv[1:]]

    try:
        compile_bridges(inputs, output_dir, priv_dir, npm)
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        print("Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")), file=sys.stderr)
        sys.exit(2)
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


def compile_bridges(inputs: List[Path], output_dir: Path, priv_dir: Path, npm: Path):
    pkg_file = next((f for f in inputs if f.name == "package.json"))
    pkg_parent = pkg_file.parent

    for srcfile in inputs:
        subpath = srcfile.relative_to(pkg_parent)

        dstfile = priv_dir / subpath
        dstdir = dstfile.parent
        if not dstdir.exists():
            dstdir.mkdir()

        shutil.copy(srcfile, dstfile)

    run_kwargs = {
        "cwd": priv_dir,
        "stdout": subprocess.PIPE,
        "stderr": subprocess.STDOUT,
        "encoding": "utf-8",
        "check": True,
    }

    subprocess.run([npm, "install"], **run_kwargs)
    subprocess.run([npm, "run", "build"], **run_kwargs)

    for outname in [p.stem + ".js" for p in inputs if p.stem != "rollup.config" and p.suffix == ".ts"]:
        shutil.copy(priv_dir / outname, output_dir / outname)
    (output_dir / "bridges.bundle").write_bytes(b"")


if __name__ == "__main__":
    main(sys.argv)
