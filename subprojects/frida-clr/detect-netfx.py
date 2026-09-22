import os
from pathlib import Path
import sys
import winreg
from typing import Optional


NETFX_VERSION = "4.8.1"


def main():
    runtime_location = find_runtime()
    if runtime_location is None:
        print(f".NET Framework {NETFX_VERSION} is not installed", file=sys.stderr)
        sys.exit(1)

    sdk_location = find_sdk()
    if sdk_location is None:
        print(f".NET Framework {NETFX_VERSION} Developer Pack is not installed", file=sys.stderr)
        sys.exit(1)

    print(str(runtime_location))
    print(str(sdk_location))


def find_runtime() -> Optional[Path]:
    program_files = Path(os.environ.get("ProgramFiles(x86)", os.environ["ProgramFiles"]))
    runtime_root = program_files / "Reference Assemblies" / "Microsoft" / "Framework" / ".NETFramework" / f"v{NETFX_VERSION}"
    if not runtime_root.is_dir():
        return None
    return runtime_root


def find_sdk() -> Optional[Path]:
    roots = ["WOW6432Node\\", ""]
    for root in roots:
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, f"SOFTWARE\\{root}Microsoft\\Microsoft SDKs\\NETFXSDK\\{NETFX_VERSION}")
        except OSError as e:
            continue
        install_dir, _ = winreg.QueryValueEx(key, "KitsInstallationFolder")
        return Path(install_dir)
    return None


if __name__ == "__main__":
    main()
