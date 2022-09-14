#!/usr/bin/env python3
import argparse
import base64
from dataclasses import dataclass
from enum import Enum
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Dict, List, Tuple
import urllib.request
if platform.system() == 'Windows':
    import winenv


BUNDLE_URL = "https://build.frida.re/deps/{version}/{filename}"

RELENG_DIR = Path(__file__).parent.resolve()
DEPS_MK_PATH = RELENG_DIR / "deps.mk"
ROOT_DIR = RELENG_DIR.parent
BUILD_DIR = ROOT_DIR / "build"

CONFIG_KEY_VALUE_PATTERN = re.compile(r"^([a-z]\w+) = (.*?)(?<!\\)$", re.MULTILINE | re.DOTALL)
CONFIG_VARIABLE_REF_PATTERN = re.compile(r"\$\((\w+)\)")


class Bundle(Enum):
    TOOLCHAIN = 1,
    SDK = 2,


@dataclass
class PackageSpec:
    name: str
    version: str
    url: str
    recipe: str
    patches: List[str]
    deps: List[str]
    deps_for_build: List[str]
    options: List[str]


@dataclass
class DependencyParameters:
    deps_version: str
    bootstrap_version: str
    packages: Dict[str, PackageSpec]

    def get_package_spec(self, name: str) -> PackageSpec:
        return self.packages[name.replace("-", "_")]


class CommandError(Exception):
    pass


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    bundle_choices = [name.lower() for name in Bundle.__members__]

    command = subparsers.add_parser("sync", help="ensure prebuilt dependencies are up-to-date")
    command.add_argument("bundle", help="bundle to synchronize", choices=bundle_choices)
    command.add_argument("host", help="OS/arch")
    command.add_argument("location", help="filesystem location")
    command.set_defaults(func=lambda args: sync(Bundle[args.bundle.upper()], args.host.lower(), Path(args.location).resolve()))

    command = subparsers.add_parser("roll", help="build and upload prebuilt dependencies if needed")
    command.add_argument("bundle", help="bundle to roll", choices=bundle_choices)
    command.add_argument("host", help="OS/arch")
    command.add_argument("--activate", default=False, action='store_true')
    command.set_defaults(func=lambda args: roll(Bundle[args.bundle.upper()], args.host.lower(), args.activate))

    command = subparsers.add_parser("wait", help="wait for prebuilt dependencies if needed")
    command.add_argument("bundle", help="bundle to wait for", choices=bundle_choices)
    command.add_argument("host", help="OS/arch")
    command.set_defaults(func=lambda args: wait(Bundle[args.bundle.upper()], args.host.lower()))

    command = subparsers.add_parser("bump", help="bump dependency versions")
    command.set_defaults(func=lambda args: bump())

    args = parser.parse_args()
    if 'func' in args:
        try:
            args.func(args)
        except CommandError as e:
            print(e, file=sys.stderr)
            sys.exit(1)
    else:
        parser.print_usage(file=sys.stderr)
        sys.exit(1)


def sync(bundle: Bundle, host: str, location: Path):
    params = read_dependency_parameters()
    version = params.deps_version

    bundle_nick = bundle.name.lower() if bundle != Bundle.SDK else bundle.name

    # XXX: This is Windows-only for now, as we use setup-env.sh to do the heavy lifting on other platforms.
    tokens = host.split("-")
    if len(tokens) == 2:
        tokens += ["release"]
    host_os, host_arch, host_config = tokens
    assert host_os == "windows"

    if bundle == Bundle.SDK:
        msvs_platform = winenv.msvs_platform_from_arch(host_arch)
        subdir_name = f"{msvs_platform}-{host_config.title()}"
        location = location / subdir_name

    if location.exists():
        try:
            cached_version = (location / "VERSION.txt").read_text(encoding='utf-8').strip()
            if cached_version == version:
                return
        except:
            pass
        shutil.rmtree(location)

    (url, filename, suffix) = compute_bundle_parameters(bundle, host, version)

    local_bundle = location.parent / filename
    if local_bundle.exists():
        print("Deploying local {}...".format(bundle_nick), flush=True)
        archive_path = local_bundle
        archive_is_temporary = False
    else:
        if bundle == Bundle.SDK:
            print(f"Downloading SDK {version} for {subdir_name}...", flush=True)
        else:
            print(f"Downloading {bundle_nick} {version}...", flush=True)
        with urllib.request.urlopen(url) as response, tempfile.NamedTemporaryFile(suffix=suffix, delete=False) as archive:
            shutil.copyfileobj(response, archive)
            archive_path = Path(archive.name)
            archive_is_temporary = True
        print(f"Extracting {bundle_nick}...", flush=True)

    try:
        if bundle == Bundle.SDK:
            target_dir = location / "tmp"
        else:
            target_dir = location.parent

        subprocess.run([
            archive_path,
            "-o" + str(target_dir),
            "-y"
        ], capture_output=True, check=True)

        if bundle == Bundle.SDK:
            shutil.move(target_dir / "sdk-windows" / "VERSION.txt", location / "VERSION.txt")
            for file in (target_dir / "sdk-windows" / subdir_name).iterdir():
                shutil.move(file, location)
            shutil.rmtree(target_dir)
    finally:
        if archive_is_temporary:
            archive_path.unlink()


def roll(bundle: Bundle, host: str, activate: bool):
    params = read_dependency_parameters()
    version = params.deps_version

    if activate and bundle == Bundle.SDK:
        configure_bootstrap_version(version)

    (public_url, filename, suffix) = compute_bundle_parameters(bundle, host, version)

    # First do a quick check to avoid hitting S3 in most cases.
    request = urllib.request.Request(public_url)
    request.get_method = lambda: "HEAD"
    try:
        with urllib.request.urlopen(request) as r:
            return
    except urllib.request.HTTPError as e:
        if e.code != 404:
            raise CommandError("network error") from e

    s3_url = "s3://build.frida.re/deps/{version}/{filename}".format(version=version, filename=filename)

    # We will most likely need to build, but let's check S3 to be certain.
    r = subprocess.run(["aws", "s3", "ls", s3_url], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding='utf-8')
    if r.returncode == 0:
        return
    if r.returncode != 1:
        raise CommandError(f"unable to access S3: {r.stdout.strip()}")

    artifact = BUILD_DIR / filename
    if artifact.exists():
        artifact.unlink()

    if host.startswith("windows-"):
        subprocess.run([
                           sys.executable, RELENG_DIR / "build-deps-windows.py",
                           "--bundle=" + bundle.name.lower(),
                           "--host=" + host,
                       ],
                       check=True)
    else:
        if platform.system().endswith("BSD"):
            gnu_make = "gmake"
        else:
            gnu_make = "make"
        subprocess.run([
                           gnu_make,
                           "-C", ROOT_DIR,
                           "-f", "Makefile.{}.mk".format(bundle.name.lower()),
                           "FRIDA_HOST=" + host,
                       ],
                       check=True)

    subprocess.run(["aws", "s3", "cp", artifact, s3_url], check=True)

    # Use the shell for Windows compatibility, where npm generates a .bat script.
    subprocess.run("cfcli purge " + public_url, shell=True, check=True)

    if activate and bundle == Bundle.TOOLCHAIN:
        configure_bootstrap_version(version)


def wait(bundle: Bundle, host: str):
    params = read_dependency_parameters()
    (url, filename, suffix) = compute_bundle_parameters(bundle, host, params.deps_version)

    request = urllib.request.Request(url)
    request.get_method = lambda: "HEAD"
    started_at = time.time()
    while True:
        try:
            with urllib.request.urlopen(request) as r:
                return
        except urllib.request.HTTPError as e:
            if e.code != 404:
                return
        print("Waiting for: {}  Elapsed: {}  Retrying in 5 minutes...".format(url, int(time.time() - started_at)), flush=True)
        time.sleep(5 * 60)


def bump():
    params = read_dependency_parameters()

    auth_blob = base64.b64encode(":".join([
                                              os.environ["GH_USERNAME"],
                                              os.environ["GH_TOKEN"]
                                          ]).encode('utf-8')).decode('utf-8')
    auth_header = "Basic " + auth_blob

    for identifier, pkg in params.packages.items():
        url = pkg.url
        if not url.startswith("https://github.com/frida/"):
            continue

        print(f"*** Checking {pkg.name}")

        repo_name = url.split("/")[-1][:-4]
        branch_name = "next" if repo_name == "capstone" else "main"

        url = f"https://api.github.com/repos/frida/{repo_name}/commits/main"
        request = urllib.request.Request(url)
        request.add_header("Authorization", auth_header)
        with urllib.request.urlopen(request) as r:
            response = json.load(r)

        latest = response['sha']
        if pkg.version == latest:
            print(f"\tup-to-date")
        else:
            print(f"\toutdated")
            print(f"\t\tcurrent: {pkg.version}")
            print(f"\t\t latest: {latest}")

            deps_content = DEPS_MK_PATH.read_text(encoding='utf-8')
            deps_content = re.sub(f"^{identifier}_version = (.+)$", f"{identifier}_version = {latest}",
                                  deps_content, flags=re.MULTILINE)
            DEPS_MK_PATH.write_bytes(deps_content.encode('utf-8'))

            subprocess.run(["git", "add", "releng/deps.mk"], cwd=ROOT_DIR, check=True)
            subprocess.run(["git", "commit", "-m" f"deps: Bump {pkg.name} to {latest[:7]}"], cwd=ROOT_DIR, check=True)

        print("")


def compute_bundle_parameters(bundle: Bundle, host: str, version: str) -> Tuple[str, str, str]:
    suffix = ".exe" if host.startswith("windows-") else ".tar.bz2"
    filename = "{}-{}{}".format(bundle.name.lower(), host, suffix)
    url = BUNDLE_URL.format(version=version, filename=filename)
    return (url, filename, suffix)


def read_dependency_parameters(host_defines: Dict[str, str] = {}) -> DependencyParameters:
    raw_params = host_defines.copy()
    for match in CONFIG_KEY_VALUE_PATTERN.finditer(DEPS_MK_PATH.read_text(encoding='utf-8')):
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
                parse_string_value(raw_params[name + "_name"], raw_params),
                parse_string_value(raw_params[name + "_version"], raw_params),
                parse_string_value(raw_params[name + "_url"], raw_params),
                parse_string_value(raw_params[name + "_recipe"], raw_params),
                parse_array_value(raw_params[name + "_patches"], raw_params),
                parse_array_value(raw_params[name + "_deps"], raw_params),
                parse_array_value(raw_params[name + "_deps_for_build"], raw_params),
                parse_array_value(raw_params[name + "_options"], raw_params))

    return DependencyParameters(
            raw_params["frida_deps_version"],
            raw_params["frida_bootstrap_version"],
            packages)


def configure_bootstrap_version(version):
    deps_content = DEPS_MK_PATH.read_text(encoding='utf-8')
    deps_content = re.sub("^frida_bootstrap_version = (.+)$", "frida_bootstrap_version = {}".format(version),
                          deps_content, flags=re.MULTILINE)
    DEPS_MK_PATH.write_bytes(deps_content.encode('utf-8'))


def parse_string_value(v: str, raw_params: Dict[str, str]) -> str:
    return CONFIG_VARIABLE_REF_PATTERN.sub(lambda match: raw_params.get(match.group(1), ""), v)


def parse_array_value(v: str, raw_params: Dict[str, str]) -> List[str]:
    v = parse_string_value(v, raw_params)
    if v == "":
        return []
    return v.split(" ")


if __name__ == "__main__":
    main()
