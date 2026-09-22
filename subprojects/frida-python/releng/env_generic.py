from collections import OrderedDict
from configparser import ConfigParser
import locale
from pathlib import Path
import shutil
import subprocess
import tempfile
from typing import Callable, Dict, List, Optional, Mapping, Sequence, Tuple

from . import winenv
from .machine_file import strv_to_meson
from .machine_spec import MachineSpec


def target_abi_of(machine: MachineSpec) -> Optional[str]:
    if machine.config_is_msabi:
        return "microsoft"
    return None


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
    allow_undefined_symbols = machine.os == "freebsd"

    options = config["built-in options"]
    options["c_args"] = "c_like_flags"
    options["cpp_args"] = "c_like_flags + cxx_like_flags"
    options["c_link_args"] = "linker_flags"
    options["cpp_link_args"] = "linker_flags + cxx_link_flags"
    options["b_lundef"] = str(not allow_undefined_symbols).lower()
    softfloat = machine.config_is_softfloat
    if softfloat:
        options["b_staticpic"] = str(machine.config_is_pic).lower()

    binaries = config["binaries"]
    cc = None
    common_flags = []
    c_like_flags = []
    linker_flags = []
    cxx_like_flags = []
    cxx_link_flags = []

    triplet = machine.triplet
    if triplet is not None:
        try:
            cc, gcc_binaries = resolve_gcc_binaries(toolprefix=triplet + "-")
            binaries.update(gcc_binaries)
        except CompilerNotFoundError:
            pass

    diagnostics = None
    if cc is None:
        with tempfile.TemporaryDirectory() as raw_prober_dir:
            prober_dir = Path(raw_prober_dir)
            machine_file = prober_dir / "machine.txt"

            argv = [
                "env2mfile",
                "-o", machine_file,
                "--native" if machine == build_machine else "--cross",
            ]

            if machine != build_machine:
                argv += [
                    "--system", machine.system,
                    "--subsystem", machine.subsystem,
                    "--kernel", machine.kernel,
                    "--cpu-family", machine.cpu_family,
                    "--cpu", machine.cpu,
                    "--endian", machine.endian,
                ]

            process = call_selected_meson(argv,
                                          cwd=raw_prober_dir,
                                          env=environ,
                                          stdout=subprocess.PIPE,
                                          stderr=subprocess.STDOUT,
                                          encoding=locale.getpreferredencoding())
            if process.returncode == 0:
                mcfg = ConfigParser()
                mcfg.read(machine_file)

                for section in mcfg.sections():
                    copy = config[section] if section in config else OrderedDict()
                    for key, val in mcfg.items(section):
                        if section == "binaries":
                            argv = eval(val.replace("\\", "\\\\"))
                            if not Path(argv[0]).is_absolute():
                                path = shutil.which(argv[0])
                                if path is None:
                                    raise BinaryNotFoundError(f"unable to locate {argv[0]}")
                                argv[0] = path
                            val = strv_to_meson(argv)
                            if key in {"c", "cpp"}:
                                val += " + common_flags"
                        if key in copy and section == "built-in options" and key.endswith("_args"):
                            val = val + " + " + copy[key]
                        copy[key] = val
                    config[section] = copy

                raw_cc = binaries.get("c", None)
                if raw_cc is not None:
                    cc = eval(raw_cc.replace("\\", "\\\\"), None, {"common_flags": []})
            else:
                diagnostics = process.stdout

    linker_flavor = None

    if cc is not None \
            and machine.os == "windows" \
            and machine.toolchain_is_msvc:
        try:
            linker_flavor = detect_linker_flavor(cc)
        except LinkerDetectionError:
            pass
        detected_wrong_toolchain = linker_flavor != "msvc"
        if detected_wrong_toolchain:
            cc = None
            linker_flavor = None

    if cc is None:
        if machine.os == "windows":
            detect_tool_path = lambda name: winenv.detect_msvs_tool_path(machine, build_machine, name, toolchain_prefix)

            cc = [str(detect_tool_path("cl.exe"))]
            lib = [str(detect_tool_path("lib.exe"))]
            link = [str(detect_tool_path("link.exe"))]
            assembler_name = MSVC_ASSEMBLER_NAMES[machine.arch]
            assembler_tool = [str(detect_tool_path(assembler_name + ".exe"))]

            raw_cc = strv_to_meson(cc) + " + common_flags"
            binaries["c"] = raw_cc
            binaries["cpp"] = raw_cc
            binaries["lib"] = strv_to_meson(lib) + " + common_flags"
            binaries["link"] = strv_to_meson(link) + " + common_flags"
            binaries[assembler_name] = strv_to_meson(assembler_tool) + " + common_flags"

            runtime_dirs = winenv.detect_msvs_runtime_path(machine, build_machine, toolchain_prefix)
            outpath.extend(runtime_dirs)

            vs_dir = winenv.detect_msvs_installation_dir(toolchain_prefix)
            outenv["VSINSTALLDIR"] = str(vs_dir) + "\\"
            outenv["VCINSTALLDIR"] = str(vs_dir / "VC") + "\\"
            outenv["Platform"] = machine.msvc_platform
            include_paths = winenv.detect_msvs_include_path(toolchain_prefix)
            library_paths = winenv.detect_msvs_library_path(machine, toolchain_prefix)
            # The flags scope the paths per-machine, which cross builds need as
            # the build and host toolchains differ. The environment additionally
            # reaches the build-machine compiler in a native build, whose native
            # targets don't pick up the host machine's compiler flags.
            for path in include_paths:
                c_like_flags += [f"/I{path}"]
            for path in library_paths:
                linker_flags += [f"/LIBPATH:{path}"]
            outenv["INCLUDE"] = ";".join([str(path) for path in include_paths])
            outenv["LIB"] = ";".join([str(path) for path in library_paths])
        elif machine != build_machine \
                and "CC" not in environ \
                and "CFLAGS" not in environ \
                and machine.os == build_machine.os \
                and machine.os == "linux" \
                and machine.pointer_size == 4 \
                and build_machine.pointer_size == 8:
            try:
                cc, gcc_binaries = resolve_gcc_binaries()
                binaries.update(gcc_binaries)
                common_flags += ["-m32"]
            except CompilerNotFoundError:
                pass

    if cc is None:
        suffix = ":\n" + diagnostics if diagnostics is not None else ""
        if machine.os == "none" and diagnostics is None:
            if triplet is not None:
                suffix = f"\n\nLooked for {triplet}-gcc and the rest of that toolchain on PATH."
            else:
                suffix = "".join([
                    "\n\nNothing names one for a bare-metal target. Either say which to use:",
                    "\n\n    CC=clang CXX=clang++ AR=llvm-ar RANLIB=llvm-ranlib NM=llvm-nm \\",
                    "\n        STRIP=llvm-strip OBJCOPY=llvm-objcopy READELF=llvm-readelf \\",
                    "\n        ./releng/deps.py build --bundle=sdk --host=" + machine.identifier,
                    "\n\nor pass --host=<triplet> with that toolchain on PATH, which is looked",
                    "\nup as <triplet>-gcc, <triplet>-nm, and so on.",
                ])
        raise CompilerNotFoundError("no C compiler found" + suffix)

    if "cpp" not in binaries:
        raise CompilerNotFoundError("no C++ compiler found")

    if linker_flavor is None:
        linker_flavor = detect_linker_flavor(cc)

    strip_binary = binaries.get("strip", None)
    if strip_binary is not None:
        strip_arg = "-Sx" if linker_flavor == "apple" else "--strip-all"
        binaries["strip"] = strip_binary[:-1] + f", '{strip_arg}']"

    if linker_flavor == "msvc":
        for gnu_tool in ["ar", "as", "ld", "nm", "objcopy", "objdump",
                         "ranlib", "readelf", "size", "strip", "windres"]:
            binaries.pop(gnu_tool, None)

        c_like_flags += [
            "/GS-",
            "/Gy",
            "/Zc:inline",
            "/fp:fast",
        ]
        if machine.arch == "x86":
            c_like_flags += ["/arch:SSE2"]

        # Relax C++11 compliance for XP compatibility.
        cxx_like_flags += ["/Zc:threadSafeInit-"]
    else:
        if machine.os == "qnx":
            common_flags += ARCH_COMMON_FLAGS_QNX.get(machine.arch, [])
        else:
            common_flags += ARCH_COMMON_FLAGS_UNIX.get(machine.arch, [])
        if not softfloat:
            c_like_flags += ARCH_C_LIKE_FLAGS_UNIX.get(machine.arch, [])
        bare = machine.os == "none"
        if softfloat:
            common_flags += ARCH_SOFTFLOAT_FLAGS_UNIX.get(machine.arch, [])
            common_flags += ["-fPIC"] if machine.config_is_pic else ["-fno-pic"]
            # A Linux module is mapped into the top 2GB and is built absolute, the kernel
            # code model saying so. A position-independent image goes where the host puts
            # it, thus it keeps the default model.
            if machine.arch == "x86_64" and not machine.config_is_pic:
                common_flags += ["-mcmodel=kernel"]

            if bare:
                target_arch = BARE_TARGET_ARCHS.get(machine.arch, machine.cpu_family)
                target_env = BARE_TARGET_ENVIRONMENTS.get(machine.arch, "elf")
                common_flags += [f"--target={target_arch}-none-{target_env}"]
                # picolibc is a package like any other here, so the libc lives in the
                # SDK rather than beside the compiler. While rolling there is no SDK
                # yet, and the prefix being filled is what to compile against. Not
                # -resource-dir: that is where the compiler keeps its own float.h, and
                # aiming it at either of those loses it.
                sysroot = sdk_prefix if sdk_prefix is not None else environ.get("FRIDA_HOST_SYSROOT")
                if sysroot is not None:
                    # Only clang's bare-metal driver, which it selects for arm64 but
                    # not for the x86 targets, searches the sysroot on its own.
                    # Spelling both paths out covers the targets where it does not.
                    common_flags += [
                        f"--sysroot={sysroot}",
                        "-isystem", f"{sysroot}/include",
                    ]
                    linker_flags += [f"-L{sysroot}/lib"]

        c_like_flags += [
            "-ffunction-sections",
            "-fdata-sections",
        ]

        if bare:
            # The host's GNU ld cannot be told to target this. Name the runtime
            # ourselves too: clang would look for its own copy beside the compiler,
            # and the one that matches this ABI is the one in the sysroot.
            linker_flags += [
                "-fuse-ld=lld",
                "-Wl,-no-pie",
                "-nostdlib",
                "-lc",
                "-lclang_rt.builtins",
                # Nothing here ends up in an executable, but configure checks do, and
                # the C library leaves its console and heap bounds to whatever
                # environment it lands in. Lend them picolibc's own stub host and an
                # empty heap, so those links fail only over the symbol being checked.
                "-ldummyhost",
                "-Wl,--defsym=__heap_start=0",
                "-Wl,--defsym=__heap_end=0",
            ]
        elif linker_flavor.startswith("gnu-"):
            linker_flags += ["-static-libgcc"]
            if machine.os != "windows":
                linker_flags += ["-Wl,-z,noexecstack"]
            if machine.os not in {"windows", "none"}:
                linker_flags += ["-Wl,-z,relro"]
            cxx_link_flags += ["-static-libstdc++"]

        if linker_flavor == "apple" and not bare:
            linker_flags += ["-Wl,-dead_strip"]
        elif not bare:
            # Would leave a link with no entry point holding on to nothing at all,
            # and every configure check that links would then trivially pass.
            linker_flags += ["-Wl,--gc-sections"]
        if linker_flavor == "gnu-gold":
            linker_flags += ["-Wl,--icf=all"]

    constants = config["constants"]
    constants["common_flags"] = strv_to_meson(common_flags)
    constants["c_like_flags"] = strv_to_meson(c_like_flags)
    constants["linker_flags"] = strv_to_meson(linker_flags)
    constants["cxx_like_flags"] = strv_to_meson(cxx_like_flags)
    constants["cxx_link_flags"] = strv_to_meson(cxx_link_flags)


def resolve_gcc_binaries(toolprefix: str = "") -> Tuple[List[str], Dict[str, str]]:
    cc = None
    binaries = OrderedDict()

    for identifier in GCC_TOOL_IDS:
        name = GCC_TOOL_NAMES.get(identifier, identifier)
        full_name = toolprefix + name

        val = shutil.which(full_name)
        if val is None:
            raise CompilerNotFoundError(f"missing {full_name}")

        # QNX SDP 6.5 gcc-* tools are broken, erroring out with:
        # > sorry - this program has been built without plugin support
        # We detect this and use the tool without the gcc-* prefix.
        if name.startswith("gcc-"):
            p = subprocess.run([val, "--version"], capture_output=True)
            if p.returncode != 0:
                full_name = toolprefix + name[4:]
                val = shutil.which(full_name)
                if val is None:
                    raise CompilerNotFoundError(f"missing {full_name}")

        if identifier == "c":
            cc = [val]

        extra = " + common_flags" if identifier in {"c", "cpp"} else ""

        binaries[identifier] = strv_to_meson([val]) + extra

    return (cc, binaries)


def detect_linker_flavor(cc: List[str]) -> str:
    linker_version = subprocess.run(cc + ["-Wl,--version"],
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT,
                                    encoding=locale.getpreferredencoding()).stdout
    if "Microsoft " in linker_version:
        return "msvc"
    if "GNU ld " in linker_version:
        return "gnu-ld"
    if "GNU gold " in linker_version:
        return "gnu-gold"
    if linker_version.startswith("LLD ") or "compatible with GNU linkers" in linker_version:
        return "lld"
    if linker_version.startswith("ld: "):
        return "apple"

    excerpt = linker_version.split("\n")[0].rstrip()
    raise LinkerDetectionError(f"unknown linker: '{excerpt}'")


class CompilerNotFoundError(Exception):
    pass


class BinaryNotFoundError(Exception):
    pass


class LinkerDetectionError(Exception):
    pass


ARCH_COMMON_FLAGS_UNIX = {
    "x86": [
        "-march=pentium4",
    ],
    "arm": [
        "-march=armv5t",
        "-mthumb",
    ],
    "armbe8": [
        "-mcpu=cortex-a72",
        "-mthumb",
    ],
    "armhf": [
        "-march=armv7-a",
        "-mtune=cortex-a7",
        "-mfpu=neon-vfpv4",
        "-mthumb",
    ],
    "armv6kz": [
        "-march=armv6kz",
        "-mcpu=arm1176jzf-s",
    ],
    "arm64": [
        "-march=armv8-a",
    ],
    "mips": [
        "-march=mips1",
        "-mfp32",
    ],
    "mipsel": [
        "-march=mips1",
        "-mfp32",
    ],
    "mips64": [
        "-march=mips64r2",
        "-mabi=64",
    ],
    "mips64el": [
        "-march=mips64r2",
        "-mabi=64",
    ],
    "s390x": [
        "-march=z10",
        "-m64",
    ],
}

ARCH_COMMON_FLAGS_QNX = {
    "x86": [
        "-march=i686",
    ],
    "arm": [
        "-march=armv6",
        "-mno-unaligned-access",
    ],
    "armeabi": [
        "-march=armv7-a",
        "-mno-unaligned-access",
    ],
}

X86_SOFTFLOAT_FLAGS_UNIX = [
    "-Xclang", "-target-feature", "-Xclang", "+soft-float",
    "-mno-mmx",
    "-mno-sse",
    # long double is x87's 80-bit format here, which soft-float has no
    # lowering for at all: LLVM crashes selecting it rather than calling out
    # to a builtin. Nothing in this stack wants more than a double.
    "-mlong-double-64",
]

# AAPCS64 mandates hardware FP, so only clang can pass doubles in general-purpose
# registers. x18 travels with it because the first host needing this is the Linux
# arm64 kernel, which reserves it.
#
# On x86_64 the same reasoning lands on LLVM's +soft-float, which is the feature
# Rust's own x86_64-unknown-none turns on — the two halves have to agree on where a
# double travels. -mno-sse alone does not: it leaves the ABI returning in xmm0 and
# the compiler then refuses the function outright. There is no driver flag for the
# feature, hence -Xclang. The rest is what any x86_64 kernel module is built with:
# no MMX/SSE, no red zone below the stack pointer that an interrupt would clobber,
# and endbr64 on every address-taken function since CONFIG_X86_KERNEL_IBT faults an
# indirect call that lands on anything else.
ARCH_SOFTFLOAT_FLAGS_UNIX = {
    "x86": X86_SOFTFLOAT_FLAGS_UNIX,
    "x86_64": X86_SOFTFLOAT_FLAGS_UNIX + [
        "-mno-red-zone",
        "-fcf-protection=branch",
        # A jump table is reached by an indirect jump, which the compiler marks
        # notrack and the kernel does not permit, having left NOTRACK_EN clear.
        # Its own C is compiled this way for the same reason.
        "-fno-jump-tables",
    ],
    "arm64": [
        "-mabi=aapcs-soft",
        "-mgeneral-regs-only",
        "-ffixed-x18",
    ],
    "arm": [
        "-march=armv7-a",
        "-mcpu=cortex-a7",
        "-mthumb",
        "-mfloat-abi=soft",
        "-mfpu=none",
    ],
}

BARE_TARGET_ARCHS = {
    "x86": "i686",
    "arm": "armv7a",
}

BARE_TARGET_ENVIRONMENTS = {
    "arm": "eabi",
}

ARCH_C_LIKE_FLAGS_UNIX = {
    "x86": [
        "-mfpmath=sse",
        "-mstackrealign",
    ],
}

GCC_TOOL_IDS = [
    "c",
    "cpp",
    "ar",
    "nm",
    "ranlib",
    "strip",
    "readelf",
    "objcopy",
    "objdump",
]

GCC_TOOL_NAMES = {
    "c": "gcc",
    "cpp": "g++",
    "ar": "gcc-ar",
    "nm": "gcc-nm",
    "ranlib": "gcc-ranlib",
}

MSVC_ASSEMBLER_NAMES = {
    "x86": "ml",
    "x86_64": "ml64",
    "arm64": "armasm64",
}
