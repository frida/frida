import argparse
from dataclasses import dataclass
from enum import Enum
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from typing import Dict, List
import urllib.request


RELENG_DIR = os.path.abspath(os.path.dirname(__file__))

CONFIG_KEY_VALUE_PATTERN = re.compile(r"^([a-z]\w+) = (.*?)(?<!\\)$", re.MULTILINE | re.DOTALL)
CONFIG_VARIABLE_REF_PATTERN = re.compile(r"\$\((\w+)\)")


class Bundle(Enum):
    TOOLCHAIN = 1,
    SDK = 2,


@dataclass
class PackageSpec:
    version: str
    url: str
    hash: str
    recipe: str
    patches: List[str]
    deps: List[str]
    deps_for_build: List[str]
    options: List[str]


@dataclass
class DependencyParameters:
    toolchain_version: str
    sdk_version: str
    bootstrap_version: str
    packages: Dict[str, PackageSpec]

    def get_package_spec(self, name: str) -> PackageSpec:
        return self.packages[name.replace("-", "_")]


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    command = subparsers.add_parser("sync", help="ensure prebuilt dependencies are up-to-date")
    command.add_argument("bundle", help="bundle to synchronize", choices=[name.lower() for name in Bundle.__members__])
    command.add_argument("os_arch", help="OS/arch")
    command.add_argument("location", help="filesystem location")

    arguments = parser.parse_args()

    params = read_dependency_parameters()

    sync(Bundle[arguments.bundle.upper()], arguments.os_arch, Path(arguments.location), params)


def sync(bundle: Bundle, os_arch: str, location: Path, params: DependencyParameters):
    current_version = params.toolchain_version if bundle == Bundle.TOOLCHAIN else params.sdk_version

    bundle_nick = bundle.name.lower() if bundle != Bundle.SDK else bundle.name

    if location.exists():
        try:
            version = (location / "VERSION.txt").read_text(encoding='utf-8').strip()
            if version == current_version:
                return
        except:
            pass
        shutil.rmtree(location)

    suffix = ".exe" if os_arch.startswith("windows-") else ".tar.bz2"
    filename = "{}-{}{}".format(bundle.name.lower(), os_arch, suffix)

    local_bundle = location.parent / filename
    if local_bundle.exists():
        print("Deploying local {}...".format(bundle_nick), flush=True)
        archive_path = local_bundle
        archive_is_temporary = False
    else:
        print("Downloading {}...".format(bundle_nick), flush=True)
        url = "https://build.frida.re/deps/{version}/{filename}".format(version=current_version, filename=filename)
        with urllib.request.urlopen(url) as response, tempfile.NamedTemporaryFile(suffix=suffix, delete=False) as archive:
            shutil.copyfileobj(response, archive)
            archive_path = Path(archive.name)
            archive_is_temporary = True
        print("Extracting {}...".format(bundle_nick), flush=True)

    try:
        # XXX: This is Windows-only for now, as we use setup-env.sh to do the heavy lifting on other platforms.
        subprocess.run([
            archive_path,
            "-o" + str(location.parent),
            "-y"
        ], check=True)
    finally:
        if archive_is_temporary:
            archive_path.unlink()


def read_dependency_parameters(host_defines: Dict[str, str] = {}) -> DependencyParameters:
    raw_params = host_defines.copy()
    with open(os.path.join(RELENG_DIR, "deps.mk"), encoding='utf-8') as f:
        for match in CONFIG_KEY_VALUE_PATTERN.finditer(f.read()):
            key, value = match.group(1, 2)
            value = value \
                    .replace("\\\n", " ") \
                    .replace("\t", " ") \
                    .replace("$(NULL)", "") \
                    .strip()
            while "  " in value:
                value = value.replace("  ", " ")
            raw_params[key] = value

    packages = {}
    for key in [k for k in raw_params.keys() if k.endswith("_recipe")]:
        name = key[:-7]
        packages[name] = PackageSpec(
                parse_string_value(raw_params[name + "_version"], raw_params),
                parse_string_value(raw_params[name + "_url"], raw_params),
                parse_string_value(raw_params[name + "_hash"], raw_params),
                parse_string_value(raw_params[name + "_recipe"], raw_params),
                parse_array_value(raw_params[name + "_patches"], raw_params),
                parse_array_value(raw_params[name + "_deps"], raw_params),
                parse_array_value(raw_params[name + "_deps_for_build"], raw_params),
                parse_array_value(raw_params[name + "_options"], raw_params))

    return DependencyParameters(
            raw_params["frida_toolchain_version"],
            raw_params["frida_sdk_version"],
            raw_params["frida_bootstrap_version"],
            packages)


def parse_string_value(v: str, raw_params: Dict[str, str]) -> str:
    return CONFIG_VARIABLE_REF_PATTERN.sub(lambda match: raw_params.get(match.group(1), ""), v)


def parse_array_value(v: str, raw_params: Dict[str, str]) -> List[str]:
    v = parse_string_value(v, raw_params)
    if v == "":
        return []
    return v.split(" ")


if __name__ == "__main__":
    main()
