from __future__ import annotations
from dataclasses import dataclass
import platform
import re
import subprocess
from typing import FrozenSet, List, Optional

if platform.system() == "Windows":
    import ctypes
    from ctypes import wintypes


@dataclass
class MachineSpec:
    os: str
    arch: str
    config: Optional[str] = None
    triplet: Optional[str] = None

    @staticmethod
    def make_from_local_system() -> MachineSpec:
        os = detect_os()
        arch = detect_arch()
        config = None

        if os == "linux":
            try:
                output = subprocess.run(["ldd", "--version"],
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT,
                                        encoding="utf-8").stdout
                if "musl" in output:
                    config = "musl"
            except:
                pass

        return MachineSpec(os, arch, config)

    @staticmethod
    def parse(raw_spec: str) -> MachineSpec:
        os = None
        arch = None
        config = None
        triplet = None

        tokens = raw_spec.split("-")
        if len(tokens) in {3, 4}:
            arch = tokens[0]
            m = TARGET_TRIPLET_ARCH_PATTERN.match(arch)
            if m is not None:
                kernel = tokens[-2]
                system = tokens[-1]

                if kernel == "w64":
                    os = "windows"
                elif kernel == "nto":
                    os = "qnx"
                else:
                    os = kernel

                if arch[0] == "i":
                    arch = "x86"
                elif arch == "arm":
                    if system.endswith("eabihf"):
                        arch = "armhf"
                    elif os == "qnx" and system.endswith("eabi"):
                        arch = "armeabi"
                elif arch == "armeb":
                    arch = "armbe8"
                elif arch == "aarch64":
                    arch = "arm64"
                elif arch == "aarch64_be":
                    arch = "arm64be"
                if system.endswith("_ilp32"):
                    arch += "ilp32"

                if system.startswith("musl"):
                    config = "musl"
                elif kernel == "w64":
                    config = "mingw"

                triplet = raw_spec

        if os is None:
            os, arch, *rest = tokens
            if rest:
                assert len(rest) == 1
                config = rest[0]

        return MachineSpec(os, arch, config, triplet)

    def evolve(self,
               os: Optional[str] = None,
               arch: Optional[str] = None,
               config: Optional[str] = None,
               triplet: Optional[str] = None) -> MachineSpec:
        return MachineSpec(
            os if os is not None else self.os,
            arch if arch is not None else self.arch,
            config if config is not None else self.config,
            triplet if triplet is not None else self.triplet,
        )

    def default_missing(self, recommended_vscrt: Optional[str] = None) -> MachineSpec:
        config = self.config
        if config is None and self.toolchain_is_msvc:
            if recommended_vscrt is not None:
                config = recommended_vscrt
            else:
                config = "mt"
        return self.evolve(config=config)

    def maybe_adapt_to_host(self, host_machine: MachineSpec) -> MachineSpec:
        if self.identifier == host_machine.identifier and host_machine.triplet is not None:
            return host_machine
        if self.os == "windows" and host_machine.os == "windows" and self.can_run(host_machine):
            return host_machine
        return self

    def can_run(self, other: MachineSpec) -> bool:
        if self.arch == other.arch:
            return True
        intel = {"x86", "x86_64"}
        return self.arch in intel and other.arch in intel

    @property
    def identifier(self) -> str:
        parts = [self.os, self.arch]
        if self.config is not None:
            parts += [self.config]
        return "-".join(parts)

    @property
    def os_dash_arch(self) -> str:
        return f"{self.os}-{self.arch}"

    @property
    def os_dash_config(self) -> str:
        parts = [self.os]
        if self.config is not None:
            parts += [self.config]
        return "-".join(parts)

    @property
    def config_tags(self) -> FrozenSet[str]:
        if self.config is None:
            return frozenset()
        return frozenset(self.config.split("_"))

    @property
    def config_is_softfloat(self) -> bool:
        return "softfloat" in self.config_tags

    @property
    def config_is_pic(self) -> bool:
        return "pic" in self.config_tags

    @property
    def config_is_msabi(self) -> bool:
        return "msabi" in self.config_tags

    @property
    def config_is_optimized(self) -> bool:
        if self.toolchain_is_msvc:
            return self.config in {"md", "mt"}
        return True

    @property
    def meson_optimization_options(self) -> List[str]:
        if self.config_is_optimized:
            optimization = "s"
            ndebug = "true"
        else:
            optimization = "0"
            ndebug = "false"
        return [
            f"-Doptimization={optimization}",
            f"-Db_ndebug={ndebug}",
        ]

    @property
    def executable_suffix(self) -> str:
        return ".exe" if self.os == "windows" else ""

    @property
    def msvc_platform(self) -> str:
        return "x64" if self.arch == "x86_64" else self.arch

    @property
    def is_apple(self) -> str:
        return self.os in {"macos", "ios", "watchos", "tvos", "xros"}

    @property
    def is_freestanding(self) -> bool:
        return self.os == "none" or self.config == "kernel"

    @property
    def system(self) -> str:
        if self.config == "kernel":
            return "none"
        return "darwin" if self.is_apple else self.os

    @property
    def subsystem(self) -> str:
        return self.os_dash_config if self.is_apple else self.os

    @property
    def kernel(self) -> str:
        return KERNELS.get(self.os, self.os)

    @property
    def cpu_family(self) -> str:
        arch = self.arch
        return CPU_FAMILIES.get(arch, arch)

    @property
    def cpu(self) -> str:
        arch = self.arch

        mappings_to_search = [
            CPU_TYPES_PER_OS_OVERRIDES.get(self.os, {}),
            CPU_TYPES,
        ]
        for m in mappings_to_search:
            cpu = m.get(arch, None)
            if cpu is not None:
                return cpu

        return arch

    @property
    def endian(self) -> str:
        return "big" if self.arch in BIG_ENDIAN_ARCHS else "little"

    @property
    def pointer_size(self) -> int:
        arch = self.arch
        if arch in {"x86_64", "s390x"}:
            return 8
        if (arch.startswith("arm64") and not arch.endswith("ilp32")) or arch.startswith("mips64"):
            return 8
        return 4

    @property
    def libdatadir(self) -> str:
        return "libdata" if self.os == "freebsd" else "lib"

    @property
    def toolchain_is_msvc(self) -> bool:
        return self.os == "windows" and self.config != "mingw"

    @property
    def toolchain_can_strip(self) -> bool:
        return not self.toolchain_is_msvc

    def __eq__(self, other):
        if isinstance(other, MachineSpec):
            return other.identifier == self.identifier
        return False


def detect_os() -> str:
    os = platform.system().lower()
    if os == "darwin":
        os = "macos"
    return os


def detect_arch() -> str:
    if platform.system() == "Windows":
        return detect_arch_windows()
    arch = platform.machine().lower()
    return ARCHS.get(arch, arch)

def detect_arch_windows():
    try:
        code = detect_arch_windows_modern()
    except AttributeError:
        code = detect_arch_windows_legacy()
    if code == PROCESSOR_ARCHITECTURE_INTEL:
        return "x86"
    elif code in {PROCESSOR_ARCHITECTURE_AMD64, IMAGE_FILE_MACHINE_AMD64}:
        return "x86_64"
    elif code in {PROCESSOR_ARCHITECTURE_ARM64, IMAGE_FILE_MACHINE_ARM64}:
        return "arm64"
    else:
        raise RuntimeError(f"unrecognized native architecture code: {code!r}")

def detect_arch_windows_modern():
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    try:
        is_wow64_process = kernel32.IsWow64Process2
    except AttributeError:
        raise

    is_wow64_process.argtypes = (
        wintypes.HANDLE,
        ctypes.POINTER(wintypes.WORD),
        ctypes.POINTER(wintypes.WORD),
    )
    is_wow64_process.restype = wintypes.BOOL

    process_machine = wintypes.WORD(0)
    native_machine  = wintypes.WORD(0)

    ok = is_wow64_process(
        kernel32.GetCurrentProcess(),
        ctypes.byref(process_machine),
        ctypes.byref(native_machine)
    )
    if not ok:
        raise ctypes.WinError(ctypes.get_last_error())

    return native_machine.value

def detect_arch_windows_legacy():
    class SYSTEM_INFO(ctypes.Structure):
        _fields_ = [
            ("wProcessorArchitecture",      wintypes.WORD),
            ("wReserved",                   wintypes.WORD),
            ("dwPageSize",                  wintypes.DWORD),
            ("lpMinimumApplicationAddress", ctypes.c_void_p),
            ("lpMaximumApplicationAddress", ctypes.c_void_p),
            ("dwActiveProcessorMask",       ctypes.c_void_p),
            ("dwNumberOfProcessors",        wintypes.DWORD),
            ("dwProcessorType",             wintypes.DWORD),
            ("dwAllocationGranularity",     wintypes.DWORD),
            ("wProcessorLevel",             wintypes.WORD),
            ("wProcessorRevision",          wintypes.WORD),
        ]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

    get_native_system_info = kernel32.GetNativeSystemInfo
    get_native_system_info.argtypes = (ctypes.POINTER(SYSTEM_INFO),)
    get_native_system_info.restype = None

    info = SYSTEM_INFO()
    get_native_system_info(ctypes.byref(info))
    return info.wProcessorArchitecture


ARCHS = {
    "amd64": "x86_64",
    "armv7l": "armhf",
    "aarch64": "arm64",
}

KERNELS = {
    "windows": "nt",

    "macos":   "xnu",
    "ios":     "xnu",
    "watchos": "xnu",
    "tvos":    "xnu",

    "qnx":     "nto",
}

CPU_FAMILIES = {
    "armbe8":       "arm",
    "armeabi":      "arm",
    "armhf":        "arm",
    "armv6kz":      "arm",

    "arm64":        "aarch64",
    "arm64be":      "aarch64",
    "arm64beilp32": "aarch64",
    "arm64e":       "aarch64",
    "arm64eoabi":   "aarch64",

    "mipsel":       "mips",
    "mips64el":     "mips64",

    "powerpc":      "ppc"
}

CPU_TYPES = {
    "arm":          "armv7",
    "armbe8":       "armv6",
    "armhf":        "armv7hf",
    "armeabi":      "armv7eabi",
    "armv6kz":      "armv6",

    "arm64":        "aarch64",
    "arm64be":      "aarch64",
    "arm64beilp32": "aarch64",
    "arm64e":       "aarch64",
    "arm64eoabi":   "aarch64",
}

CPU_TYPES_PER_OS_OVERRIDES = {
    "linux": {
        "arm":        "armv5t",
        "armbe8":     "armv6t",
        "armhf":      "armv7a",
        "armv6kz":    "armv6t",

        "mips":       "mips1",
        "mipsel":     "mips1",

        "mips64":     "mips64r2",
        "mips64el":   "mips64r2",
    },
    "android": {
        "x86":        "i686",
    },
    "qnx": {
        "arm":        "armv6",
        "armeabi":    "armv7",
    },
}

BIG_ENDIAN_ARCHS = {
    "arm64be",
    "arm64beilp32",
    "armbe8",
    "mips",
    "mips64",
    "ppc",
    "ppc64",
    "s390x",
}

TARGET_TRIPLET_ARCH_PATTERN = re.compile(r"^(i.86|x86_64|arm\w*|aarch64(_be)?|mips\w*|powerpc|s390x)$")

PROCESSOR_ARCHITECTURE_INTEL = 0
PROCESSOR_ARCHITECTURE_AMD64 = 9
PROCESSOR_ARCHITECTURE_ARM64 = 12

IMAGE_FILE_MACHINE_AMD64 = 0x8664
IMAGE_FILE_MACHINE_ARM64 = 0xAA64

