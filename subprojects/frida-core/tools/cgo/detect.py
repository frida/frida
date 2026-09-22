from __future__ import annotations

import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

if platform.system() == "Windows":
    import winreg


FRIDA_OS_TO_GO_OS = {
    "macos": "darwin",
}

FRIDA_ABI_TO_GO_ARCH = {
    "armbe8": "armbe",
    "armhf": "arm",
    "mips64el": "mips64le",
    "mipsel": "mipsle",
    "x86": "386",
    "x86_64": "amd64",
}

FRIDA_ABI_TO_MINGW_FLAVOR = {
    "arm64": "clangarm64",
    "x86": "mingw32",
    "x86_64": "mingw64",
}

FRIDA_ABI_TO_MINGW_ARCH = {
    "arm64": "aarch64",
    "x86": "i686",
    "x86_64": "x86_64",
}

MACHINE_FLAG_PATTERN = re.compile(
    r"^-m(?:arch|float-abi|fpu|thumb|fpmath|stackrealign|cpu|tune|fp32|abi)(?:=.*)?$"
)


def main(argv: List[str]):
    args = argv[1:]
    go = Path(args.pop(0))
    host_os = args.pop(0)
    host_os_family = args.pop(0)
    host_abi = args.pop(0)
    libc = args.pop(0)
    exe_suffix = args.pop(0)
    shlib_suffix = args.pop(0)
    cc_id = args.pop(0)
    assets_mode = args.pop(0)
    cc_cmd_array = pop_cmd_array_arg(args)
    ar_cmd_array = pop_cmd_array_arg(args)
    nm_cmd_array = pop_cmd_array_arg(args)
    ranlib_cmd_array = pop_cmd_array_arg(args)

    try:
        mode, config_path = detect_config(
            go,
            host_os,
            host_os_family,
            host_abi,
            libc,
            exe_suffix,
            shlib_suffix,
            cc_id,
            assets_mode,
            cc_cmd_array,
            ar_cmd_array,
            nm_cmd_array,
            ranlib_cmd_array,
        )
        print("ok")
        print(mode)
        print(config_path)
    except Exception as e:
        print("error")
        print(str(e).replace("\n", "\\n"))


def pop_cmd_array_arg(args: List[str]) -> List[str]:
    result = []
    first = args.pop(0)
    assert first == ">>>"
    while True:
        cur = args.pop(0)
        if cur == "<<<":
            break
        result.append(cur)
    if len(result) == 1 and not result[0]:
        return None
    return result


def detect_config(
    go: Path,
    host_os: str,
    host_os_family: str,
    host_abi: str,
    libc: str,
    exe_suffix: str,
    shlib_suffix: str,
    cc_id: str,
    assets_mode: str,
    cc_cmd_array: List[str],
    ar_cmd_array: Optional[List[str]],
    nm_cmd_array: Optional[List[str]],
    ranlib_cmd_array: Optional[List[str]],
) -> Tuple[str, Path]:
    source_root = Path(os.environ["MESON_SOURCE_ROOT"])
    build_root = Path(os.environ["MESON_BUILD_ROOT"])
    subdir = Path(os.environ["MESON_SUBDIR"])

    work_dir = build_root / subdir / "tools" / "cgo"
    work_dir.mkdir(parents=True, exist_ok=True)
    for f in ("go.mod", "main.go"):
        shutil.copyfile(source_root / subdir / "tools" / "cgo" / f, work_dir / f)

    extra_go_args = []
    mingw = None
    env = {
        **os.environ,
        "CGO_ENABLED": "1",
    }

    if cc_id == "msvc":
        toolchain = MinGWToolchain.detect(host_abi)
        mingw = {
            "triplet": toolchain.triplet,
            "prefix": str(toolchain.prefix),
            "bindir": str(toolchain.bindir),
            "cc": str(toolchain.cc),
            "libcc": str(toolchain.libcc),
        }
        env["CC"] = str(toolchain.cc)
        env["PATH"] = str(toolchain.cc.parent) + ";" + env["PATH"]
        ar_cmd_array = [str(toolchain.ar)]
        nm_cmd_array = [str(toolchain.nm)]
        ranlib_cmd_array = [str(toolchain.ranlib)]
    else:
        env["CC"] = shlex.join(
            [arg for arg in cc_cmd_array if MACHINE_FLAG_PATTERN.fullmatch(arg) is None]
        )
    extra_go_args.append(f"-ldflags=-extar={ar_cmd_array[0]}")

    if host_os == "macos":
        extra_go_args.append("-ldflags=-extldflags=-mmacosx-version-min=11.0")
        env["MACOSX_DEPLOYMENT_TARGET"] = "11.0"

    def run(*args):
        return subprocess.run(
            args,
            cwd=work_dir,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            encoding="utf-8",
            check=True,
        )

    goos = FRIDA_OS_TO_GO_OS.get(host_os, host_os)
    goarch = FRIDA_ABI_TO_GO_ARCH.get(host_abi, host_abi)
    needed = f"{goos}/{goarch}"
    supported = run("go", "tool", "dist", "list").stdout.splitlines()
    if needed not in supported:
        raise NotSupportedError(f"Go does not support {needed}")
    env["GOOS"] = goos
    env["GOARCH"] = goarch

    apple_non_macos = host_os_family == "darwin" and host_os != "macos"
    if apple_non_macos:
        mode = "c-archive"
    elif libc == "musl":
        mode = "pie"
    elif host_os == "android" or assets_mode == "installed":
        mode = "c-shared"
    else:
        mode = "c-archive"

    if mode == "pie":
        extra_go_args += ["-tags=frida_compiler_backend_executable"]

    try:
        run(go, "build", f"-buildmode={mode}", "-o", "cgotest.a", "-buildvcs=false")
    except subprocess.CalledProcessError as e:
        raise BuildError(e.output.strip())

    config = {
        "mode": mode,
        "os": host_os,
        "os_family": goos,
        "abi": host_abi,
        "exe_suffix": exe_suffix,
        "shlib_suffix": shlib_suffix,
        "extra_go_args": extra_go_args,
        "env": env,
        "ar": ar_cmd_array,
        "nm": nm_cmd_array,
        "ranlib": ranlib_cmd_array,
    }
    if mingw is not None:
        config["mingw"] = mingw

    config_path = work_dir / "go-config.json"
    config_path.write_text(json.dumps(config), encoding="utf-8")

    return config["mode"], config_path


class BuildError(Exception):
    pass


class NotSupportedError(Exception):
    pass


@dataclass
class MinGWToolchain:
    triplet: str
    prefix: Path
    bindir: Path
    cc: Path
    ar: Path
    nm: Path
    ranlib: Path
    libcc: Path

    @staticmethod
    def detect(host_abi: str) -> MinGWToolchain:
        msys_prefix = find_msys2_prefix_from_environment()

        if msys_prefix is None:
            msys_prefix = find_msys2_prefix_from_registry()

        if msys_prefix is None:
            msys_prefix = Path(os.environ["SystemDrive"] + r"\msys64")
            if not msys_prefix.exists():
                msys_prefix = None

        if msys_prefix is not None:
            flavor = host_abi_to_mingw_flavor(host_abi)
            mingw_prefix = msys_prefix / flavor
            if not mingw_prefix.exists():
                mingw_prefix = None
        else:
            mingw_prefix = None

        mingw_cc = host_abi_to_mingw_cc(host_abi)

        if mingw_prefix is not None:
            mingw_bindir = mingw_prefix / "bin"
            cc_path = mingw_bindir / f"{mingw_cc}.exe"
            if not cc_path.exists():
                cc_path = None
        else:
            raw_cc_path = shutil.which(mingw_cc)
            cc_path = Path(raw_cc_path) if raw_cc_path is not None else None
            if cc_path is not None:
                mingw_bindir = cc_path.parent
                mingw_prefix = mingw_bindir.parent

        if cc_path is None:
            raise MinGWNotFoundError(
                "could not find an MSYS2/MinGW toolchain under msys64, "
                "nor a MinGW-based GCC/Clang on PATH matching the requested host_abi"
            )

        def query_cc(*args):
            return subprocess.run(
                [cc_path, *args],
                cwd=cc_path.parent,
                capture_output=True,
                encoding="utf-8",
                check=True,
            ).stdout.rstrip()

        try:
            triplet = query_cc("-dumpmachine")
        except subprocess.CalledProcessError:
            raise MinGWNotFoundError(
                f"unable to detect the machine triplet of {cc_path}"
            )

        actual_mingw_arch = triplet.split("-", 1)[0]
        expected_mingw_arch = FRIDA_ABI_TO_MINGW_ARCH[host_abi]
        if actual_mingw_arch != expected_mingw_arch:
            raise MinGWNotFoundError(
                f"compiler at {cc_path} is for {actual_mingw_arch}, expected {expected_mingw_arch}"
            )

        if host_abi != "arm64" and "mingw" not in triplet:
            raise MinGWNotFoundError(
                f"compiler at {cc_path} does not target MinGW, its triplet is {triplet}"
            )

        ar_path = cc_path.parent / "ar.exe"
        nm_path = cc_path.parent / "nm.exe"
        ranlib_path = cc_path.parent / "ranlib.exe"

        try:
            libcc_name = query_cc("-print-libgcc-file-name")
        except subprocess.CalledProcessError:
            raise MinGWNotFoundError(
                f"unable to detect the libgcc file name of {cc_path}"
            )
        libcc_path = Path(libcc_name).resolve()

        return MinGWToolchain(
            triplet,
            mingw_prefix,
            mingw_bindir,
            cc_path,
            ar_path,
            nm_path,
            ranlib_path,
            libcc_path,
        )


class MinGWNotFoundError(Exception):
    pass


def host_abi_to_mingw_flavor(abi: str) -> str:
    try:
        return FRIDA_ABI_TO_MINGW_FLAVOR[abi]
    except KeyError:
        raise MinGWNotFoundError(f"unsupported host_abi: {abi}")


def host_abi_to_mingw_cc(abi: str) -> str:
    return "clang" if abi == "arm64" else "gcc"


def find_msys2_prefix_from_environment() -> Optional[Path]:
    location = os.environ.get("MSYS2_LOCATION")
    if location is None:
        return None
    return Path(location)


def find_msys2_prefix_from_registry() -> Optional[Path]:
    UNINSTALL_PATHS = [
        (
            winreg.HKEY_CURRENT_USER,
            r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall",
        ),
        (
            winreg.HKEY_CURRENT_USER,
            r"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall",
        ),
        (
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall",
        ),
        (
            winreg.HKEY_LOCAL_MACHINE,
            r"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall",
        ),
    ]

    for hive, subkey_path in UNINSTALL_PATHS:
        try:
            with winreg.OpenKey(hive, subkey_path) as uninstall_root:
                num_subkeys, _, _ = winreg.QueryInfoKey(uninstall_root)
                for i in range(num_subkeys):
                    try:
                        subkey_name = winreg.EnumKey(uninstall_root, i)
                        with winreg.OpenKey(uninstall_root, subkey_name) as app_key:
                            disp_name, _ = winreg.QueryValueEx(app_key, "DisplayName")
                            if disp_name.startswith("MSYS2"):
                                install_loc, _ = winreg.QueryValueEx(
                                    app_key, "InstallLocation"
                                )
                                return Path(install_loc)
                    except FileNotFoundError:
                        continue
                    except OSError:
                        continue
        except FileNotFoundError:
            continue
        except OSError:
            continue

    return None


if __name__ == "__main__":
    main(sys.argv)
