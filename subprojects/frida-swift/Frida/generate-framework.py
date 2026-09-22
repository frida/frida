from pathlib import Path
import shutil
import sys


def main(argv: list[str]):
    triplet = argv[1]
    framework_dir, dylib, header, info_plist = [Path(p) for p in argv[2:]]

    if framework_dir.exists():
        shutil.rmtree(framework_dir)
    framework_dir.mkdir()

    shutil.copy(dylib, framework_dir / "Frida")

    hdir = framework_dir / "Headers"
    hdir.mkdir()
    shutil.copy(header, hdir)

    mdir = framework_dir / "Modules"
    mdir.mkdir()
    (mdir / "module.modulemap").write_text("\n".join([
                                               "framework module Frida {",
                                               "  umbrella header \"Frida.h\"",
                                               "  export *",
                                               "",
                                               "  module * { export * }",
                                               "}",
                                               "",
                                           ]),
                                           encoding="utf-8")
    (mdir / "module.private.modulemap").write_text("\n".join([
                                                       "module Frida_Private [extern_c] {",
                                                       "}",
                                                       "",
                                                   ]),
                                                   encoding="utf-8")

    smdir = mdir / "Frida.swiftmodule"
    smdir.mkdir()
    privdir = dylib.parent / f"{dylib.name}.p"
    for asset in {"abi.json",
                  "private.swiftinterface",
                  "swiftdoc",
                  "swiftinterface",
                  "swiftmodule"}:
        shutil.copy(privdir / f"Frida.{asset}",
                    smdir / f"{triplet}.{asset}")
    pdir = smdir / "Project"
    pdir.mkdir()
    shutil.copy(privdir / "Frida.swiftsourceinfo",
                pdir / f"{triplet}.swiftsourceinfo")

    resdir = framework_dir / "Resources"
    resdir.mkdir()
    shutil.copy(info_plist, resdir / "Info.plist")


if __name__ == "__main__":
    main(sys.argv)
