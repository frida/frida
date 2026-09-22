from pathlib import Path
import shutil
import subprocess
import sys
import struct


def main(argv):
    args = argv[1:]
    host_os = args.pop(0)
    host_arch = args.pop(0)
    host_toolchain = args.pop(0)
    resource_compiler = args.pop(0)
    lipo = pop_cmd_array_arg(args)
    output_dir = Path(args.pop(0))
    priv_dir = Path(args.pop(0))
    resource_config = args.pop(0)
    helper_modern, helper_legacy, \
            helper_emulated_modern, helper_emulated_legacy \
            = [Path(p) if p else None for p in args[:4]]

    priv_dir.mkdir(exist_ok=True)

    embedded_assets = []
    if host_os == "windows":
        pending_archs = {"arm64", "x86_64", "x86"}
        for helper in {helper_modern, helper_legacy, helper_emulated_modern, helper_emulated_legacy}:
            if helper is None:
                continue
            arch = detect_pefile_arch(helper)
            embedded_helper = priv_dir / f"frida-helper-{arch}.exe"
            shutil.copy(helper, embedded_helper)
            embedded_assets += [embedded_helper]
            pending_archs.remove(arch)
        for missing_arch in pending_archs:
            embedded_helper = priv_dir / f"frida-helper-{missing_arch}.exe"
            embedded_helper.write_bytes(b"")
            embedded_assets += [embedded_helper]
    elif host_os in {"macos", "ios", "watchos", "tvos", "xros"}:
        embedded_helper = priv_dir / "frida-helper"

        if helper_modern is not None and helper_legacy is not None:
            subprocess.run(lipo + [helper_modern, helper_legacy, "-create", "-output", embedded_helper],
                           check=True)
        elif helper_modern is not None:
            shutil.copy(helper_modern, embedded_helper)
        elif helper_legacy is not None:
            shutil.copy(helper_legacy, embedded_helper)
        else:
            embedded_helper.write_bytes(b"")

        embedded_assets += [embedded_helper]
    else:
        embedded_helper_modern = priv_dir / f"frida-helper-64"
        embedded_helper_legacy = priv_dir / f"frida-helper-32"

        if helper_modern is not None:
            shutil.copy(helper_modern, embedded_helper_modern)
        else:
            embedded_helper_modern.write_bytes(b"")

        if helper_legacy is not None:
            shutil.copy(helper_legacy, embedded_helper_legacy)
        else:
            embedded_helper_legacy.write_bytes(b"")

        embedded_assets += [embedded_helper_modern, embedded_helper_legacy]

    subprocess.run([
        resource_compiler,
        f"--toolchain={host_toolchain}",
        f"--machine={host_arch}",
        "--config-filename", resource_config,
        "--output-basename", output_dir / "frida-data-helper-process",
    ] + embedded_assets, check=True)


def pop_cmd_array_arg(args):
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


def detect_pefile_arch(location):
    with location.open(mode="rb") as pe:
        pe.seek(0x3c)
        e_lfanew, = struct.unpack("<I", pe.read(4))
        pe.seek(e_lfanew + 4)
        machine, = struct.unpack("<H", pe.read(2))
    return PE_MACHINES[machine]


PE_MACHINES = {
    0x014c: "x86",
    0x8664: "x86_64",
    0xaa64: "arm64",
}


if __name__ == "__main__":
    main(sys.argv)
