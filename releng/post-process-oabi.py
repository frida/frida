#!/usr/bin/env python3
import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
import tempfile
import urllib.request


ARM64E_URL = "https://build.frida.re/deps/{version}/sdk-ios-arm64e.tar.xz"


class CommandError(Exception):
    pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--bundle", required=True)
    parser.add_argument("--host", required=True)
    parser.add_argument("--artifact", required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    if args.bundle != "sdk":
        raise CommandError("wrong bundle")
    if args.host != "ios-arm64eoabi":
        raise CommandError("wrong host")

    arm64e_sdk_url = ARM64E_URL.format(version=args.version)

    print(f"Downloading {arm64e_sdk_url}")
    with urllib.request.urlopen(arm64e_sdk_url) as response, \
            tempfile.NamedTemporaryFile(suffix=".tar.xz") as archive:
        shutil.copyfileobj(response, archive)
        archive.flush()
        arm64e_artifact_path = Path(archive.name)

        with tempfile.TemporaryDirectory() as patched_artifact_dir:
            patched_artifact_file = Path(patched_artifact_dir) / "patched.tar.xz"

            with tempfile.TemporaryDirectory() as artifact_extracted_dir, \
                    tempfile.TemporaryDirectory() as arm64e_extracted_dir:
                artifact_extracted_path = Path(artifact_extracted_dir)
                arm64e_extracted_path = Path(arm64e_extracted_dir)

                with tarfile.open(arm64e_artifact_path, "r:xz") as arm64e_tar:
                    arm64e_tar.extractall(arm64e_extracted_path)

                    artifact_path = Path(args.artifact)
                    with tarfile.open(artifact_path, "r:xz") as tar:
                        tar.extractall(artifact_extracted_path)

                        print("Patching libffi.a...")
                        steal_object(artifact_extracted_path / "lib" / "libffi.a",
                                     arm64e_extracted_path / "lib" / "libffi.a")
                        with tarfile.open(patched_artifact_file, "w:xz") as patched_tar:
                            patched_tar.add(artifact_extracted_path, arcname="./")

            print(f"Overwriting {artifact_path}")
            shutil.copy(patched_artifact_file, artifact_path)


def steal_object(arm64eoabi_libffi_a_path: Path, arm64e_libffi_a_path: Path):
    """
    Steal just the aarch64_sysv.S.o object file from the arm64e libffi.a in
    order to get the CIE info from the future compiler. Then patch the Mach-O
    header of the stolen object to match the old arm64e ABI. It works because
    the __text section is exactly the same.
    """
    if not arm64eoabi_libffi_a_path.exists():
        raise RuntimeError("input arm64eoabi libffi.a not found")
    if not arm64e_libffi_a_path.exists():
        raise RuntimeError("input arm64e libffi.a not found")

    with tempfile.TemporaryDirectory() as oabi_dir, tempfile.TemporaryDirectory() as nabi_dir:
        perform("ar", "-x", arm64eoabi_libffi_a_path.absolute(), cwd=oabi_dir)
        perform("ar", "-x", arm64e_libffi_a_path.absolute(), cwd=nabi_dir)
        dst = Path(oabi_dir) / "aarch64_sysv.S.o"
        dst.unlink()
        shutil.copy(Path(nabi_dir) / "aarch64_sysv.S.o", dst)
        with dst.open("rb+") as f:
            f.seek(0xb)
            f.write(struct.pack("B", 0))

        perform("ar", "-r", arm64eoabi_libffi_a_path.absolute(), dst.name, cwd=oabi_dir)


def perform(*args, **kwargs):
    print(">", " ".join([str(arg) for arg in args]), flush=True)
    return subprocess.run(args, check=True, **kwargs)


if __name__ == "__main__":
    main()
