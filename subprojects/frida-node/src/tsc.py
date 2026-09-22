from pathlib import Path
import shutil
import subprocess
import sys


def main(argv: list[str]):
    outdir, privdir, npm, package_json, package_lock_json, tsconfig, *sources = [Path(s) for s in argv[1:]]

    try:
        privdir.mkdir(exist_ok=True)
        for asset in [package_json, package_lock_json, tsconfig]:
            shutil.copy(asset, privdir)

        srcdir = privdir / "src"
        if srcdir.exists():
            shutil.rmtree(srcdir)
        srcdir.mkdir()
        for asset in sources:
            shutil.copy(asset, srcdir)

        srcoutdir = privdir / "build" / "src"
        if srcoutdir.exists():
            shutil.rmtree(srcoutdir)

        run_kwargs = {
            "stdout": subprocess.PIPE,
            "stderr": subprocess.STDOUT,
            "encoding": "utf-8",
            "check": True,
        }
        subprocess.run([npm, "ci", "--ignore-scripts"],
                       cwd=privdir,
                       **run_kwargs)
        subprocess.run([npm, "exec", "tsc"],
                       cwd=privdir,
                       **run_kwargs)

        for asset in (s for s in sources if not s.name.endswith(".d.ts")):
            shutil.copy(srcoutdir / (asset.stem + ".js"), outdir)
            shutil.copy(srcoutdir / (asset.stem + ".d.ts"), outdir)
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        print("Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv)
