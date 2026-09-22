from pathlib import Path
import subprocess
import sys


def main(argv: list[str]):
    qmltyperegistrations, qmltypes, privdir, qt_prefix, qt_libdir, qt_libexecdir, \
            *moc_sources = [Path(p) for p in argv[1:]]

    metadir = qt_libdir / "metatypes"
    if not metadir.exists():
        metadir = qt_prefix / "metatypes"
        assert metadir.exists()
    foreign_types = next(metadir.glob("qt6qml_*metatypes.json"))

    privdir.mkdir(exist_ok=True)

    try:
        metatypes = privdir / "metatypes.json"

        run_kwargs = {
            "stdout": subprocess.PIPE,
            "stderr": subprocess.STDOUT,
            "encoding": "utf-8",
            "check": True,
        }
        subprocess.run([
                           qt_libexecdir / "moc",
                           "--collect-json",
                           "-o", metatypes,
                           *[f.parent / f"{f.name}.json" for f in moc_sources],
                       ],
                       **run_kwargs)
        subprocess.run([
                           qt_libexecdir / "qmltyperegistrar",
                           f"--generate-qmltypes={qmltypes}",
                           "--import-name=Frida",
                           "--major-version=1",
                           "--minor-version=0",
                           f"--foreign-types={foreign_types}",
                           "-o", qmltyperegistrations,
                           metatypes,
                       ],
                       **run_kwargs)
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        print("Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main(sys.argv)
