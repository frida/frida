from configparser import ConfigParser
from pathlib import Path
import shlex
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
    ndk_found = False
    try:
        ndk_root = Path(environ["ANDROID_NDK_ROOT"])
        if ndk_root.is_absolute():
            ndk_props_file = ndk_root / "source.properties"
            ndk_found = ndk_props_file.exists()
    except:
        pass
    if not ndk_found:
        raise NdkNotFoundError(f"ANDROID_NDK_ROOT must be set to the location of your r{NDK_REQUIRED} NDK")

    if sdk_prefix is not None:
        props = ConfigParser()
        raw_props = ndk_props_file.read_text(encoding="utf-8")
        props.read_string("[source]\n" + raw_props)
        rev = props["source"]["Pkg.Revision"]
        tokens = rev.split(".")
        major_version = int(tokens[0])
        if major_version != NDK_REQUIRED:
            raise NdkVersionError(f"NDK r{NDK_REQUIRED} is required (found r{major_version}, which is unsupported)")

    android_build_os = "darwin" if build_machine.os == "macos" else build_machine.os
    android_build_arch = "x86_64" if build_machine.os in {"macos", "linux"} else build_machine.arch
    android_api = 21

    llvm_bindir = ndk_root / "toolchains" / "llvm" / "prebuilt" / f"{android_build_os}-{android_build_arch}" / "bin"
    libcxx_include_dir = llvm_bindir.parent / "sysroot" / "usr" / "include" / "c++" / "v1"

    binaries = config["binaries"]
    for (identifier, tool_name, *rest) in NDK_BINARIES:
        path = llvm_bindir / f"{tool_name}{build_machine.executable_suffix}"

        argv = [str(path)]
        if len(rest) != 0:
            argv += rest[0]

        raw_val = strv_to_meson(argv)
        if identifier in {"c", "cpp"}:
            raw_val += " + common_flags"

        binaries[identifier] = raw_val

    common_flags = [
        "-target", f"{machine.cpu}-none-linux-android{android_api}",
    ]
    c_like_flags = [
        "-DANDROID",
        "-ffunction-sections",
        "-fdata-sections",
    ]
    cxx_like_flags = []
    cxx_link_flags = [
        "-static-libstdc++",
    ]
    linker_flags = [
        "-Wl,-z,relro",
        "-Wl,-z,noexecstack",
        "-Wl,--gc-sections",
    ]

    read_envflags = lambda name: shlex.split(environ.get(name, ""))

    common_flags += ARCH_COMMON_FLAGS.get(machine.arch, [])

    c_like_flags += ARCH_C_LIKE_FLAGS.get(machine.arch, [])
    c_like_flags += read_envflags("CPPFLAGS")

    if _needs_patched_fstream(machine, android_api):
        overlay_dir = _generate_libcpp_overlay(libcxx_include_dir, outdir)
        cxx_like_flags += ["-isystem", str(overlay_dir)]

    linker_flags += ARCH_LINKER_FLAGS.get(machine.arch, [])
    linker_flags += read_envflags("LDFLAGS")

    constants = config["constants"]
    constants["common_flags"] = strv_to_meson(common_flags)
    constants["c_like_flags"] = strv_to_meson(c_like_flags)
    constants["linker_flags"] = strv_to_meson(linker_flags)
    constants["cxx_like_flags"] = strv_to_meson(cxx_like_flags)
    constants["cxx_link_flags"] = strv_to_meson(cxx_link_flags)

    options = config["built-in options"]
    options["c_args"] = "c_like_flags + " + strv_to_meson(read_envflags("CFLAGS"))
    options["cpp_args"] = "c_like_flags + cxx_like_flags + " + strv_to_meson(read_envflags("CXXFLAGS"))
    options["c_link_args"] = "linker_flags"
    options["cpp_link_args"] = "linker_flags + cxx_link_flags"
    options["b_lundef"] = "true"


def _needs_patched_fstream(machine: MachineSpec, android_api: int) -> bool:
    return machine.pointer_size == 4 and android_api < 24


def _generate_libcpp_overlay(libcxx_include_dir: Path, outdir: Path) -> Path:
    src = libcxx_include_dir / "fstream"
    if not src.exists():
        raise FileNotFoundError(f"Missing libc++ header: {src}")

    overlay_dir = outdir / "libcpp-overlay"
    overlay_dir.mkdir(parents=True, exist_ok=True)

    patched = src.read_text(encoding="utf-8")
    patched = _patch_fstream_header(patched)

    dst = overlay_dir / "fstream"
    old_contents = dst.read_text(encoding="utf-8") if dst.exists() else None
    if old_contents != patched:
        dst.write_text(patched, encoding="utf-8")

    return overlay_dir


def _patch_fstream_header(header: str) -> str:
    old_fseek = """#    elif defined(_NEWLIB_VERSION)
  return fseek(__file, __offset, __whence);
#    else
  return ::fseeko(__file, __offset, __whence);
#    endif"""

    new_fseek = """#    elif defined(_NEWLIB_VERSION) || (defined(__ANDROID__) && __SIZEOF_POINTER__ == 4 && __ANDROID_API__ < 24)
  return fseek(__file, __offset, __whence);
#    else
  return ::fseeko(__file, __offset, __whence);
#    endif"""

    old_ftell = """#    elif defined(_NEWLIB_VERSION)
  return ftell(__file);
#    else
  return ftello(__file);
#    endif"""

    new_ftell = """#    elif defined(_NEWLIB_VERSION) || (defined(__ANDROID__) && __SIZEOF_POINTER__ == 4 && __ANDROID_API__ < 24)
  return ftell(__file);
#    else
  return ftello(__file);
#    endif"""

    result = header.replace(old_fseek, new_fseek)
    result = result.replace(old_ftell, new_ftell)

    if result == header:
        raise ValueError("Failed to patch libc++ fstream header; expected patterns not found")

    return result


class NdkNotFoundError(Exception):
    pass


class NdkVersionError(Exception):
    pass


NDK_REQUIRED = 29

NDK_BINARIES = [
    ("c",       "clang"),
    ("cpp",     "clang++"),
    ("ar",      "llvm-ar"),
    ("nm",      "llvm-nm"),
    ("ranlib",  "llvm-ranlib"),
    ("strip",   "llvm-strip", ["--strip-all"]),
    ("readelf", "llvm-readelf"),
    ("objcopy", "llvm-objcopy"),
    ("objdump", "llvm-objdump"),
]

ARCH_COMMON_FLAGS = {
    "x86": [
        "-march=pentium4",
    ],
    "arm": [
        "-march=armv7-a",
        "-mfloat-abi=softfp",
        "-mfpu=vfpv3-d16",
        "-mthumb",
    ]
}

ARCH_C_LIKE_FLAGS = {
    "x86": [
        "-mfpmath=sse",
        "-mstackrealign",
    ]
}

ARCH_LINKER_FLAGS = {
    "arm": [
        "-Wl,--fix-cortex-a8",
    ]
}
