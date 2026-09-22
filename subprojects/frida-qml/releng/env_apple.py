from configparser import ConfigParser
from pathlib import Path
import shlex
import subprocess
from typing import Callable, Dict, List, Optional

from .machine_file import strv_to_meson
from .machine_spec import MachineSpec


def init_machine_config(machine: MachineSpec,
                        build_machine: MachineSpec,
                        is_cross_build: bool,
                        environ: Dict[str, str],
                        toolchain_prefix: Optional[Path],
                        sdk_prefix: Optional[Path],
                        call_selected_meson: Callable,
                        config: ConfigParser,
                        outpath: List[str],
                        outenv: Dict[str, str],
                        outdir: Path,
                        apple_min_os: Optional[Dict[str, str]] = None):
    xcenv = {**environ}
    if machine.arch == "arm64eoabi":
        try:
            xcenv["DEVELOPER_DIR"] = (Path(xcenv["XCODE11"]) / "Contents" / "Developer").as_posix()
        except KeyError:
            raise Xcode11NotFoundError("for arm64eoabi support, XCODE11 must be set to the location of your Xcode 11 app bundle")

    def xcrun(*args):
        try:
            return subprocess.run(["xcrun"] + list(args),
                                  env=xcenv,
                                  capture_output=True,
                                  encoding="utf-8",
                                  check=True).stdout.strip()
        except subprocess.CalledProcessError as e:
            raise XCRunError("\n\t| ".join(e.stderr.strip().split("\n")))

    clang_arch = APPLE_CLANG_ARCHS.get(machine.arch, machine.arch)

    overrides = apple_min_os or {}
    os_minver = (overrides.get(machine.os_dash_arch)
                 or overrides.get(machine.os)
                 or APPLE_MINIMUM_OS_VERSIONS.get(machine.os_dash_arch,
                                                  APPLE_MINIMUM_OS_VERSIONS[machine.os]))

    building_for_the_kernel = machine.config == "kernel"

    target = f"{clang_arch}-apple-{machine.os}{os_minver}"
    if machine.config is not None and not building_for_the_kernel:
        target += "-" + machine.config

    sdk_name = APPLE_SDKS[machine.os if building_for_the_kernel else machine.os_dash_config]
    sdk_path = xcrun("--sdk", sdk_name, "--show-sdk-path")

    use_static_libcxx = sdk_prefix is not None \
            and (sdk_prefix / "lib" / "c++" / "libc++.a").exists() \
            and machine.os != "watchos"

    binaries = config["binaries"]
    clang_path = None
    for (identifier, tool_name, *rest) in APPLE_BINARIES:
        if tool_name.startswith("#"):
            binaries[identifier] = binaries[tool_name[1:]]
            continue

        path = xcrun("--sdk", sdk_name, "-f", tool_name)
        if tool_name == "clang":
            clang_path = Path(path)

        argv = [path]
        if len(rest) != 0:
            argv += rest[0]
        if identifier == "cpp" and not use_static_libcxx:
            argv += ["-stdlib=libc++"]
        if identifier == "swift":
            argv += ["-target", target, "-sdk", sdk_path]

        raw_val = str(argv)
        if identifier in {"c", "cpp"}:
            raw_val += " + common_flags"

        binaries[identifier] = raw_val

    read_envflags = lambda name: shlex.split(environ.get(name, ""))

    c_like_flags = read_envflags("CPPFLAGS")

    linker_flags = ["-Wl,-dead_strip"]
    linker_flags += read_envflags("LDFLAGS")

    common_flags = [
        "-target", target,
        "-isysroot", sdk_path,
    ]
    if building_for_the_kernel:
        common_flags += [
            "-mkernel",
            "-nostdlibinc",
            "-fno-builtin",
            "-fno-common",
            "-fno-ptrauth-function-pointer-type-discrimination",
            "-fno-stack-check",
        ]
        sysroot = sdk_prefix if sdk_prefix is not None else environ.get("FRIDA_HOST_SYSROOT")
        if sysroot is not None:
            common_flags += ["-isystem", f"{sysroot}/include"]
            linker_flags += [f"-L{sysroot}/lib"]
        linker_flags += [
            "-nostdlib",
            "-static",
            "-Wl,-e,_main",
            "-lc",
            "-lm",
        ]

    constants = config["constants"]
    constants["common_flags"] = strv_to_meson(common_flags)
    constants["c_like_flags"] = strv_to_meson(c_like_flags)
    constants["linker_flags"] = strv_to_meson(linker_flags)

    if use_static_libcxx:
        constants["cxx_like_flags"] = strv_to_meson([
            "-nostdinc++",
            "-isystem" + str(sdk_prefix / "include" / "c++"),
        ])
        constants["cxx_link_flags"] = strv_to_meson([
            "-nostdlib++",
            "-L" + str(sdk_prefix / "lib" / "c++"),
            "-lc++",
            "-lc++abi",
        ])
    else:
        constants["cxx_like_flags"] = strv_to_meson([])
        constants["cxx_link_flags"] = strv_to_meson([])

    options = config["built-in options"]
    options["c_args"] = "c_like_flags + " + strv_to_meson(read_envflags("CFLAGS"))
    options["cpp_args"] = "c_like_flags + cxx_like_flags + " + strv_to_meson(read_envflags("CXXFLAGS"))
    options["objc_args"] = "c_like_flags + " + strv_to_meson(read_envflags("OBJCFLAGS"))
    options["objcpp_args"] = "c_like_flags + cxx_like_flags + " + strv_to_meson(read_envflags("OBJCXXFLAGS"))
    options["c_link_args"] = "linker_flags"
    options["cpp_link_args"] = "linker_flags + cxx_link_flags"
    options["objc_link_args"] = "linker_flags"
    options["objcpp_link_args"] = "linker_flags + cxx_link_flags"
    options["b_lundef"] = "true"


class XCRunError(Exception):
    pass


class Xcode11NotFoundError(Exception):
    pass


APPLE_SDKS = {
    "macos":             "macosx",
    "ios":               "iphoneos",
    "ios-simulator":     "iphonesimulator",
    "watchos":           "watchos",
    "watchos-simulator": "watchsimulator",
    "tvos":              "appletvos",
    "tvos-simulator":    "appletvsimulator",
    "xros":              "xros",
}

APPLE_CLANG_ARCHS = {
    "x86":        "i386",
    "arm":        "armv7",
    "arm64eoabi": "arm64e",
}

APPLE_MINIMUM_OS_VERSIONS = {
    "macos":        "10.13",
    "macos-arm64":  "11.0",
    "macos-arm64e": "11.0",
    "ios":          "8.0",
    "watchos":      "9.0",
    "tvos":         "13.0",
    "xros":         "26.0",
}

APPLE_BINARIES = [
    ("c",                 "clang"),
    ("cpp",               "clang++"),
    ("objc",              "#c"),
    ("objcpp",            "#cpp"),
    ("swift",             "swiftc"),

    ("ar",                "ar"),
    ("nm",                "llvm-nm"),
    ("ranlib",            "ranlib"),
    ("strip",             "strip", ["-Sx"]),
    ("libtool",           "libtool"),

    ("install_name_tool", "install_name_tool"),
    ("otool",             "otool"),
    ("codesign",          "codesign"),
    ("lipo",              "lipo"),
]
