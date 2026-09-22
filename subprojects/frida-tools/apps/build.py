import os
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List
from zipfile import ZipFile


def main(argv: List[str]):
    npm = argv[1]
    paths = [Path(p).resolve() for p in argv[2:]]
    inputs = paths[:-2]
    output_zip = paths[-2]
    priv_dir = paths[-1]

    try:
        build(npm, inputs, output_zip, priv_dir)
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        print("Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")), file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(e, file=sys.stderr)
        sys.exit(1)


def build(npm: Path, inputs: List[Path], output_zip: Path, priv_dir: Path):
    pkg_file = next((f for f in inputs if f.name == "package.json"))
    pkg_parent = pkg_file.parent

    for srcfile in inputs:
        subpath = Path(os.path.relpath(srcfile, pkg_parent))

        dstfile = priv_dir / subpath
        dstdir = dstfile.parent
        if not dstdir.exists():
            dstdir.mkdir()

        shutil.copy(srcfile, dstfile)

    npm_opts = {
        "cwd": priv_dir,
        "stdout": subprocess.PIPE,
        "stderr": subprocess.STDOUT,
        "encoding": "utf-8",
        "check": True,
    }
    subprocess.run([npm, "install"], **npm_opts)
    subprocess.run([npm, "run", "build"], **npm_opts)

    with ZipFile(output_zip, "w") as outzip:
        dist_dir = priv_dir / "dist"
        for filepath in dist_dir.rglob("*"):
            outzip.write(filepath, filepath.relative_to(dist_dir))


if __name__ == "__main__":
    main(sys.argv)
