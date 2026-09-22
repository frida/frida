#!/usr/bin/env python3
from __future__ import annotations
import argparse
import base64
from configparser import ConfigParser
import dataclasses
from dataclasses import dataclass, field
from enum import Enum
import graphlib
import itertools
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
from typing import Callable, Dict, Iterator, List, Optional, Mapping, Sequence, Set, Tuple, Union
import urllib.request

RELENG_DIR = Path(__file__).resolve().parent
ROOT_DIR = RELENG_DIR.parent

if __name__ == "__main__":
    # TODO: Refactor
    sys.path.insert(0, str(ROOT_DIR))
sys.path.insert(0, str(RELENG_DIR / "tomlkit"))

from tomlkit.toml_file import TOMLFile

from releng import env
from releng.progress import Progress, ProgressCallback, print_progress
from releng.machine_spec import MachineSpec


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    default_machine = MachineSpec.make_from_local_system().identifier

    bundle_opt_kwargs = {
        "help": "bundle (default: sdk)",
        "type": parse_bundle_option_value,
    }
    machine_opt_kwargs = {
        "help": f"os/arch (default: {default_machine})",
        "type": MachineSpec.parse,
    }

    command = subparsers.add_parser("sync", help="ensure prebuilt dependencies are up-to-date")
    command.add_argument("bundle", **bundle_opt_kwargs)
    command.add_argument("host", **machine_opt_kwargs)
    command.add_argument("location", help="filesystem location", type=Path)
    command.set_defaults(func=lambda args: sync(args.bundle, args.host, args.location.resolve()))

    command = subparsers.add_parser("roll", help="build and upload prebuilt dependencies if needed")
    command.add_argument("bundle", **bundle_opt_kwargs)
    command.add_argument("host", **machine_opt_kwargs)
    command.add_argument("--build", default=default_machine, **machine_opt_kwargs)
    command.add_argument("--activate", default=False, action='store_true')
    command.add_argument("--post", help="post-processing script")
    command.set_defaults(func=lambda args: roll(args.bundle, args.build, args.host, args.activate,
                                                Path(args.post) if args.post is not None else None))

    command = subparsers.add_parser("build", help="build prebuilt dependencies")
    command.add_argument("--bundle", default=Bundle.SDK, **bundle_opt_kwargs)
    command.add_argument("--build", default=default_machine, **machine_opt_kwargs)
    command.add_argument("--host", default=default_machine, **machine_opt_kwargs)
    command.add_argument("--only", help="only build packages A, B, and C", metavar="A,B,C",
                         type=parse_set_option_value)
    command.add_argument("--exclude", help="exclude packages A, B, and C", metavar="A,B,C",
                         type=parse_set_option_value, default=set())
    command.add_argument("-v", "--verbose", help="be verbose", action="store_true")
    command.set_defaults(func=lambda args: build(args.bundle, args.build, args.host,
                                                 args.only, args.exclude, args.verbose))

    command = subparsers.add_parser("published", help="query whether prebuilt dependencies are already published")
    command.add_argument("bundle", **bundle_opt_kwargs)
    command.add_argument("host", **machine_opt_kwargs)
    command.set_defaults(func=lambda args: query_published(args.bundle, args.host))

    command = subparsers.add_parser("wait", help="wait for prebuilt dependencies if needed")
    command.add_argument("bundle", **bundle_opt_kwargs)
    command.add_argument("host", **machine_opt_kwargs)
    command.set_defaults(func=lambda args: wait(args.bundle, args.host))

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


def parse_bundle_option_value(raw_bundle: str) -> Bundle:
    try:
        return Bundle[raw_bundle.upper()]
    except KeyError:
        choices = "', '".join([e.name.lower() for e in Bundle])
        raise argparse.ArgumentTypeError(f"invalid choice: {raw_bundle} (choose from '{choices}')")


def parse_set_option_value(v: str) -> Set[str]:
    return set([v.strip() for v in v.split(",")])


def query_toolchain_prefix(machine: MachineSpec,
                           cache_dir: Path) -> Path:
    if machine.os == "windows":
        identifier = "windows-x86" if machine.arch in {"x86", "x86_64"} else machine.os_dash_arch
    else:
        identifier = machine.identifier
    return cache_dir / f"toolchain-{identifier}"


def ensure_toolchain(machine: MachineSpec,
                     cache_dir: Path,
                     version: Optional[str] = None,
                     on_progress: ProgressCallback = print_progress) -> Tuple[Path, SourceState]:
    toolchain_prefix = query_toolchain_prefix(machine, cache_dir)
    state = sync(Bundle.TOOLCHAIN, machine, toolchain_prefix, version, on_progress)
    return (toolchain_prefix, state)


def query_sdk_prefix(machine: MachineSpec,
                     cache_dir: Path) -> Path:
    return cache_dir / f"sdk-{machine.identifier}"


def ensure_sdk(machine: MachineSpec,
               cache_dir: Path,
               version: Optional[str] = None,
               on_progress: ProgressCallback = print_progress) -> Tuple[Path, SourceState]:
    sdk_prefix = query_sdk_prefix(machine, cache_dir)
    state = sync(Bundle.SDK, machine, sdk_prefix, version, on_progress)
    return (sdk_prefix, state)


def detect_cache_dir(sourcedir: Path) -> Path:
    raw_location = os.environ.get("FRIDA_DEPS", None)
    if raw_location is not None:
        location = Path(raw_location)
    else:
        location = sourcedir / "deps"
    return location


def sync(bundle: Bundle,
         machine: MachineSpec,
         location: Path,
         version: Optional[str] = None,
         on_progress: ProgressCallback = print_progress) -> SourceState:
    state = SourceState.PRISTINE

    if version is None:
        version = load_dependency_parameters().deps_version

    bundle_nick = bundle.name.lower() if bundle != Bundle.SDK else bundle.name

    if location.exists():
        try:
            cached_version = (location / "VERSION.txt").read_text(encoding="utf-8").strip()
            if cached_version == version:
                return state
        except:
            pass
        shutil.rmtree(location)
        state = SourceState.MODIFIED

    (url, filename) = compute_bundle_parameters(bundle, machine, version)

    local_bundle = location.parent / filename
    if local_bundle.exists():
        on_progress(Progress("Deploying local {}".format(bundle_nick)))
        archive_path = local_bundle
        archive_is_temporary = False
    else:
        if bundle == Bundle.SDK:
            on_progress(Progress(f"Downloading SDK {version} for {machine.identifier}"))
        else:
            on_progress(Progress(f"Downloading {bundle_nick} {version}"))
        try:
            with urllib.request.urlopen(url) as response, \
                    tempfile.NamedTemporaryFile(delete=False) as archive:
                shutil.copyfileobj(response, archive)
                archive_path = Path(archive.name)
                archive_is_temporary = True
            on_progress(Progress(f"Extracting {bundle_nick}"))
        except urllib.error.HTTPError as e:
            if e.code == 404:
                raise BundleNotFoundError(f"missing bundle at {url}") from e
            raise e

    try:
        staging_dir = location.parent / f"_{location.name}"
        if staging_dir.exists():
            shutil.rmtree(staging_dir)
        staging_dir.mkdir(parents=True)

        with tarfile.open(archive_path, "r:xz") as tar:
            tar.extractall(staging_dir)

        suffix_len = len(".frida.in")
        raw_location = location.as_posix()
        for f in staging_dir.rglob("*.frida.in"):
            target = f.parent / f.name[:-suffix_len]
            f.write_text(f.read_text(encoding="utf-8").replace("@FRIDA_TOOLROOT@", raw_location),
                         encoding="utf-8")
            f.rename(target)

        staging_dir.rename(location)
    finally:
        if archive_is_temporary:
            archive_path.unlink()

    return state


def roll(bundle: Bundle,
         build_machine: MachineSpec,
         host_machine: MachineSpec,
         activate: bool,
         post: Optional[Path]):
    params = load_dependency_parameters()
    version = params.deps_version

    if activate and bundle == Bundle.SDK:
        configure_bootstrap_version(version)

    (public_url, filename) = compute_bundle_parameters(bundle, host_machine, version)

    # First do a quick check to avoid hitting S3 in most cases.
    if bundle_is_published(public_url):
        return

    s3_url = "s3://build.frida.re/deps/{version}/{filename}".format(version=version, filename=filename)

    # We will most likely need to build, but let's check S3 to be certain.
    r = subprocess.run(["aws", "s3", "ls", s3_url], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, encoding="utf-8")
    if r.returncode == 0:
        return
    if r.returncode != 1:
        raise CommandError(f"unable to access S3: {r.stdout.strip()}")

    artifact = build(bundle, build_machine, host_machine)

    if post is not None:
        post_script = RELENG_DIR / post
        if not post_script.exists():
            raise CommandError("post-processing script not found")

        subprocess.run([
                           sys.executable, post_script,
                           "--bundle=" + bundle.name.lower(),
                           "--host=" + host_machine.identifier,
                           "--artifact=" + str(artifact),
                           "--version=" + version,
                       ],
                       check=True)

    subprocess.run(["aws", "s3", "cp", artifact, s3_url], check=True)

    # Use the shell for Windows compatibility, where npm generates a .bat script.
    subprocess.run("cfcli purge " + public_url, shell=True, check=True)

    if activate and bundle == Bundle.TOOLCHAIN:
        configure_bootstrap_version(version)


def build(bundle: Bundle,
          build_machine: MachineSpec,
          host_machine: MachineSpec,
          only_packages: Optional[Set[str]] = None,
          excluded_packages: Set[str] = set(),
          verbose: bool = False) -> Path:
    builder = Builder(bundle, build_machine, host_machine, verbose)
    try:
        return builder.build(only_packages, excluded_packages)
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        if e.stdout is not None:
            print("\n=== stdout ===\n" + e.stdout, file=sys.stderr)
        if e.stderr is not None:
            print("\n=== stderr ===\n" + e.stderr, file=sys.stderr)
        sys.exit(1)


def query_published(bundle: Bundle, machine: MachineSpec):
    version = load_dependency_parameters().deps_version
    (public_url, _) = compute_bundle_parameters(bundle, machine, version)
    print(str(bundle_is_published(public_url)).lower())


class Builder:
    def __init__(self,
                 bundle: Bundle,
                 build_machine: MachineSpec,
                 host_machine: MachineSpec,
                 verbose: bool):
        self._bundle = bundle
        self._host_machine = host_machine.default_missing()
        self._build_machine = build_machine.default_missing().maybe_adapt_to_host(self._host_machine)
        self._verbose = verbose
        self._default_library = "static"

        self._params = load_dependency_parameters()
        self._cachedir = detect_cache_dir(ROOT_DIR)
        self._workdir = self._cachedir / "src"

        self._toolchain_prefix: Optional[Path] = None
        self._build_config: Optional[env.MachineConfig] = None
        self._host_config: Optional[env.MachineConfig] = None
        self._build_env: Dict[str, str] = {}
        self._host_env: Dict[str, str] = {}

        self._ansi_supported = os.environ.get("TERM") != "dumb" \
                    and (self._build_machine.os != "windows" or "WT_SESSION" in os.environ)

    def build(self,
              only_packages: Optional[List[str]],
              excluded_packages: Set[str]) -> Path:
        started_at = time.time()
        prepare_ended_at = None
        clone_time_elapsed = None
        build_time_elapsed = None
        build_ended_at = None
        packaging_ended_at = None
        try:
            all_packages = {i: self._resolve_package(p) for i, p in self._params.packages.items() \
                    if self._can_build(p)}
            if only_packages is not None:
                toplevel_packages = [all_packages[identifier] for identifier in only_packages]
                selected_packages = self._resolve_dependencies(toplevel_packages, all_packages)
            elif self._bundle is Bundle.TOOLCHAIN:
                toplevel_packages = [p for p in all_packages.values() if p.scope == "toolchain"]
                selected_packages = self._resolve_dependencies(toplevel_packages, all_packages)
            else:
                selected_packages = {i: p for i, p, in all_packages.items() if p.scope is None}
            selected_packages = {i: p for i, p in selected_packages.items() if i not in excluded_packages}

            packages = [selected_packages[i] for i in iterate_package_ids_in_dependency_order(selected_packages.values())]
            all_deps = itertools.chain.from_iterable([pkg.dependencies for pkg in packages])
            deps_for_build_machine = {dep.identifier for dep in all_deps if dep.for_machine == "build"}

            self._prepare()
            prepare_ended_at = time.time()

            clone_time_elapsed = 0
            build_time_elapsed = 0
            for pkg in packages:
                self._print_package_banner(pkg)

                t1 = time.time()
                self._clone_repo_if_needed(pkg)
                t2 = time.time()
                clone_time_elapsed += t2 - t1

                machines = [self._host_machine]
                if pkg.identifier in deps_for_build_machine:
                    machines += [self._build_machine]
                self._build_package(pkg, machines)
                t3 = time.time()
                build_time_elapsed += t3 - t2
            build_ended_at = time.time()

            artifact_file = self._package()
            packaging_ended_at = time.time()
        finally:
            ended_at = time.time()

            if prepare_ended_at is not None:
                self._print_summary_banner()
                print("      Total: {}".format(format_duration(ended_at - started_at)))

            if prepare_ended_at is not None:
                print("    Prepare: {}".format(format_duration(prepare_ended_at - started_at)))

            if clone_time_elapsed is not None:
                print("      Clone: {}".format(format_duration(clone_time_elapsed)))

            if build_time_elapsed is not None:
                print("      Build: {}".format(format_duration(build_time_elapsed)))

            if packaging_ended_at is not None:
                print("  Packaging: {}".format(format_duration(packaging_ended_at - build_ended_at)))

            print("", flush=True)

        return artifact_file

    def _can_build(self, pkg: PackageSpec) -> bool:
        return self._evaluate_condition(pkg.when)

    def _resolve_package(self, pkg: PackageSpec) -> bool:
        resolved_opts = [opt for opt in pkg.options if self._evaluate_condition(opt.when)]
        resolved_deps = [dep for dep in pkg.dependencies if self._evaluate_condition(dep.when)]
        return dataclasses.replace(pkg,
                                   options=resolved_opts,
                                   dependencies=resolved_deps)

    def _resolve_dependencies(self,
                              packages: Sequence[PackageSpec],
                              all_packages: Mapping[str, PackageSpec]) -> Dict[str, PackageSpec]:
        result = {p.identifier: p for p in packages}
        for p in packages:
            self._resolve_package_dependencies(p, all_packages, result)
        return result

    def _resolve_package_dependencies(self,
                                      package: PackageSpec,
                                      all_packages: Mapping[str, PackageSpec],
                                      resolved_packages: Mapping[str, PackageSpec]):
        for dep in package.dependencies:
            identifier = dep.identifier
            if identifier in resolved_packages:
                continue
            p = all_packages[identifier]
            resolved_packages[identifier] = p
            self._resolve_package_dependencies(p, all_packages, resolved_packages)

    def _evaluate_condition(self, cond: Optional[str]) -> bool:
        if cond is None:
            return True
        global_vars = {
            "Bundle": Bundle,
            "bundle": self._bundle,
            "machine": self._host_machine,
        }
        return eval(cond, global_vars)

    def _prepare(self):
        self._toolchain_prefix, toolchain_state = \
                ensure_toolchain(self._build_machine,
                                 self._cachedir,
                                 version=self._params.bootstrap_version)
        if toolchain_state == SourceState.MODIFIED:
            self._wipe_build_state()

        envdir = self._get_builddir_container()
        envdir.mkdir(parents=True, exist_ok=True)

        menv = {**os.environ}

        if self._bundle is Bundle.TOOLCHAIN:
            extra_ldflags = []
            if self._host_machine.is_apple:
                symfile = envdir / "toolchain-executable.symbols"
                symfile.write_text("# No exported symbols.\n", encoding="utf-8")
                extra_ldflags += [f"-Wl,-exported_symbols_list,{symfile}"]
            elif self._host_machine.os == "freebsd":
                verfile = envdir / "toolchain-executable.version"
                verfile.write_text("\n".join([
                                                 "{",
                                                 "  global:",
                                                 "    # FreeBSD needs these two:",
                                                 "    __progname;",
                                                 "    environ;",
                                                 "",
                                                 "  local:",
                                                 "    *;",
                                                 "};",
                                                 ""
                                             ]),
                                   encoding="utf-8")
                extra_ldflags += [f"-Wl,--version-script,{verfile}"]
            elif self._host_machine.os != "windows":
                verfile = envdir / "toolchain-executable.version"
                verfile.write_text("\n".join([
                                                 "{",
                                                 "  local:",
                                                 "    *;",
                                                 "};",
                                                 ""
                                             ]),
                                   encoding="utf-8")
                extra_ldflags += [f"-Wl,--version-script,{verfile}"]
            if extra_ldflags:
                menv["LDFLAGS"] = shlex.join(extra_ldflags + shlex.split(menv.get("LDFLAGS", "")))

        # The soft-float bare-metal target has no libc beside its compiler: picolibc and
        # the compiler-rt builtins are packages here, so the prefix being filled is also
        # the sysroot everything built after them compiles against.
        if (self._host_machine.os == "none" and self._host_machine.config_is_softfloat) \
                or self._host_machine.config == "kernel":
            menv["FRIDA_HOST_SYSROOT"] = str(self._get_prefix(self._host_machine))

        build_sdk_prefix = None
        host_sdk_prefix = None

        self._build_config, self._host_config = \
                env.generate_machine_configs(self._build_machine,
                                             self._host_machine,
                                             menv,
                                             self._toolchain_prefix,
                                             build_sdk_prefix,
                                             host_sdk_prefix,
                                             self._call_meson,
                                             self._default_library,
                                             envdir)
        self._build_env = self._build_config.make_merged_environment(os.environ)
        self._host_env = self._host_config.make_merged_environment(os.environ)

    def _clone_repo_if_needed(self, pkg: PackageSpec):
        sourcedir = self._get_sourcedir(pkg)

        git = lambda *args, **kwargs: subprocess.run(["git", *args],
                                                     **kwargs,
                                                     capture_output=True,
                                                     encoding="utf-8")

        if sourcedir.exists():
            self._print_status(pkg.name, "Reusing existing checkout")
            current_rev = git("rev-parse", "FETCH_HEAD", cwd=sourcedir, check=True).stdout.strip()
            if current_rev != pkg.version:
                self._print_status(pkg.name, "WARNING: Checkout does not match version in deps.toml")
        else:
            self._print_status(pkg.name, "Cloning")
            clone_shallow(pkg, sourcedir, git)

    def _wipe_build_state(self):
        for path in (self._get_outdir(), self._get_builddir_container()):
            if path.exists():
                self._print_status(path.relative_to(self._workdir).as_posix(), "Wiping")
                shutil.rmtree(path)

    def _build_package(self, pkg: PackageSpec, machines: Sequence[MachineSpec]):
        for machine in machines:
            manifest_path = self._get_manifest_path(pkg, machine)
            action = "skip" if manifest_path.exists() else "build"

            message = "Building" if action == "build" else "Already built"
            message += f" for {machine.identifier}"
            self._print_status(pkg.name, message)

            if action == "build":
                self._build_package_for_machine(pkg, machine)
                assert manifest_path.exists()

    def _build_package_for_machine(self, pkg: PackageSpec, machine: MachineSpec):
        sourcedir = self._get_sourcedir(pkg)
        builddir = self._get_builddir(pkg, machine)

        prefix = self._get_prefix(machine)
        libdir = prefix / "lib"

        strip = "true" if machine.toolchain_can_strip else "false"

        if builddir.exists():
            shutil.rmtree(builddir)

        machine_file_opts = [f"--native-file={self._build_config.machine_file}"]
        pc_opts = [f"-Dpkg_config_path={prefix / machine.libdatadir / 'pkgconfig'}"]
        if self._host_config is not self._build_config and machine is self._host_machine:
            machine_file_opts += [f"--cross-file={self._host_config.machine_file}"]
            pc_path_for_build = self._get_prefix(self._build_machine) / self._build_machine.libdatadir / "pkgconfig"
            pc_opts += [f"-Dbuild.pkg_config_path={pc_path_for_build}"]

        menv = self._host_env if machine is self._host_machine else self._build_env

        meson_kwargs = {
            "env": menv,
            "check": True,
        }
        if not self._verbose:
            meson_kwargs["capture_output"] = True
            meson_kwargs["encoding"] = "utf-8"

        self._call_meson([
                             "setup",
                             builddir,
                             *machine_file_opts,
                             f"-Dprefix={prefix}",
                             f"-Dlibdir={libdir}",
                             *pc_opts,
                             f"-Ddefault_library={self._default_library}",
                             f"-Dbackend=ninja",
                             *machine.meson_optimization_options,
                             f"-Dstrip={strip}",
                             *[opt.value for opt in pkg.options],
                         ],
                         cwd=sourcedir,
                         **meson_kwargs)

        self._call_meson(["install"],
                         cwd=builddir,
                         **meson_kwargs)

        manifest_lines = []
        install_locations = json.loads(self._call_meson(["introspect", "--installed"],
                                                        cwd=builddir,
                                                        capture_output=True,
                                                        encoding="utf-8",
                                                        env=menv).stdout)
        for installed_path in install_locations.values():
            manifest_lines.append(Path(installed_path).relative_to(prefix).as_posix())
        manifest_lines.sort()
        manifest_path = self._get_manifest_path(pkg, machine)
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")

    def _call_meson(self, argv, *args, **kwargs):
        if self._verbose and argv[0] in {"setup", "install"}:
            vanilla_env = os.environ
            meson_env = kwargs["env"]
            changed_env = {k: v for k, v in meson_env.items() if k not in vanilla_env or v != vanilla_env[k]}

            indent = "  "
            env_summary = f" \\\n{indent}".join([f"{k}={shlex.quote(v)}" for k, v in changed_env.items()])
            argv_summary = f" \\\n{3 * indent}".join([str(arg) for arg in argv])

            print(f"> {env_summary} \\\n{indent}meson {argv_summary}", flush=True)

        return env.call_meson(argv, use_submodule=True, *args, **kwargs)

    def _package(self):
        outfile = self._cachedir / f"{self._bundle.name.lower()}-{self._host_machine.identifier}.tar.xz"

        self._print_packaging_banner()
        with tempfile.TemporaryDirectory(prefix="frida-deps") as raw_tempdir:
            tempdir = Path(raw_tempdir)

            self._print_status(outfile.name, "Staging files")
            if self._bundle is Bundle.TOOLCHAIN:
                self._stage_toolchain_files(tempdir)
            else:
                self._stage_sdk_files(tempdir)

            self._adjust_manifests(tempdir)
            self._adjust_files_containing_hardcoded_paths(tempdir)

            (tempdir / "VERSION.txt").write_text(self._params.deps_version + "\n", encoding="utf-8")

            self._print_status(outfile.name, "Assembling")
            with tarfile.open(outfile, "w:xz") as tar:
                tar.add(tempdir, ".")

            self._print_status(outfile.name, "All done")

        return outfile

    def _stage_toolchain_files(self, location: Path) -> List[Path]:
        if self._host_machine.os == "windows":
            toolchain_prefix = self._toolchain_prefix
            mixin_files = [f for f in self._walk_plain_files(toolchain_prefix)
                           if self._file_should_be_mixed_into_toolchain(f)]
            copy_files(toolchain_prefix, mixin_files, location)

        prefix = self._get_prefix(self._host_machine)
        files = [f for f in self._walk_plain_files(prefix)
                 if self._file_is_toolchain_related(f)]
        copy_files(prefix, files, location)

    def _stage_sdk_files(self, location: Path) -> List[Path]:
        prefix = self._get_prefix(self._host_machine)
        files = [f for f in self._walk_plain_files(prefix)
                 if self._file_is_sdk_related(f)]
        copy_files(prefix, files, location)

    def _adjust_files_containing_hardcoded_paths(self, bundledir: Path):
        prefix = self._get_prefix(self._host_machine)

        raw_prefixes = [str(prefix)]
        if self._host_machine.os == "windows":
            raw_prefixes.append(prefix.as_posix())

        for f in self._walk_plain_files(bundledir):
            filepath = bundledir / f
            try:
                text = filepath.read_text(encoding="utf-8")

                new_text = text
                is_pcfile = filepath.suffix == ".pc"
                replacement = "${frida_sdk_prefix}" if is_pcfile else "@FRIDA_TOOLROOT@"
                for p in raw_prefixes:
                    new_text = new_text.replace(p, replacement)

                if new_text != text:
                    filepath.write_text(new_text, encoding="utf-8")
                    if not is_pcfile:
                        filepath.rename(filepath.parent / f"{f.name}.frida.in")
            except UnicodeDecodeError:
                pass

    @staticmethod
    def _walk_plain_files(rootdir: Path) -> Iterator[Path]:
        for dirpath, dirnames, filenames in os.walk(rootdir):
            for filename in filenames:
                f = Path(dirpath) / filename
                if f.is_symlink():
                    continue
                yield f.relative_to(rootdir)

    @staticmethod
    def _adjust_manifests(bundledir: Path):
        for manifest_path in (bundledir / "manifest").glob("*.pkg"):
            lines = []

            prefix = manifest_path.parent.parent
            for entry in manifest_path.read_text(encoding="utf-8").strip().split("\n"):
                if prefix.joinpath(entry).exists():
                    lines.append(entry)

            if lines:
                lines.sort()
                manifest_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            else:
                manifest_path.unlink()

    def _file_should_be_mixed_into_toolchain(self, f: Path) -> bool:
        parts = f.parts
        if parts[0] == "VERSION.txt":
            return False
        if parts[0] == "bin":
            stem = f.stem
            return stem in {"bison", "flex", "m4", "nasm", "vswhere"} or stem.startswith("msys-")
        if parts[0] == "manifest":
            return False

        if self._file_is_vala_toolchain_related(f):
            return False

        return True

    def _file_is_toolchain_related(self, f: Path) -> bool:
        if self._file_is_vala_toolchain_related(f):
            return True

        parts = f.parts
        if parts[0] == "bin":
            if f.suffix == ".pdb":
                return False
            stem = f.stem
            if stem in {"gdbus", "gio", "gobject-query", "gsettings"}:
                return False
            if stem.startswith("gspawn-"):
                return False
            return True
        if parts[0] == "manifest":
            return True

        return False

    def _file_is_vala_toolchain_related(self, f: Path) -> bool:
        if f.suffix in {".vapi", ".deps"}:
            return True

        name = f.name
        if f.suffix == self._host_machine.executable_suffix:
            return name.startswith("vala") or name.startswith("vapi") or name.startswith("gen-introspect")
        if f.parts[0] == "bin" and name.startswith("vala-gen-introspect"):
            return True

        return False

    def _file_is_sdk_related(self, f: Path) -> bool:
        suffix = f.suffix
        if suffix == ".pdb":
            return False
        if suffix in [".vapi", ".deps"]:
            return True

        parts = f.parts
        if parts[0] == "bin":
            return f.name.startswith("v8-mksnapshot-")

        return "share" not in parts

    def _get_outdir(self) -> Path:
        return self._workdir / f"_{self._bundle.name.lower()}.out"

    def _get_sourcedir(self, pkg: PackageSpec) -> Path:
        return self._workdir / pkg.identifier

    def _get_builddir(self, pkg: PackageSpec, machine: MachineSpec) -> Path:
        return self._get_builddir_container() / machine.identifier / pkg.identifier

    def _get_builddir_container(self) -> Path:
        return self._workdir / f"_{self._bundle.name.lower()}.tmp"

    def _get_prefix(self, machine: MachineSpec) -> Path:
        return self._get_outdir() / machine.identifier

    def _get_manifest_path(self, pkg: PackageSpec, machine: MachineSpec) -> Path:
        return self._get_prefix(machine) / "manifest" / f"{pkg.identifier}.pkg"

    def _print_package_banner(self, pkg: PackageSpec):
        if self._ansi_supported:
            print("\n".join([
                "",
                "╭────",
                f"│ 📦 \033[1m{pkg.name}\033[0m",
                "├───────────────────────────────────────────────╮",
                f"│ URL: {pkg.url}",
                f"│ CID: {pkg.version}",
                "├───────────────────────────────────────────────╯",
            ]), flush=True)
        else:
            print("\n".join([
                "",
                f"# {pkg.name}",
                f"- URL: {pkg.url}",
                f"- CID: {pkg.version}",
            ]), flush=True)

    def _print_packaging_banner(self):
        if self._ansi_supported:
            print("\n".join([
                "",
                "╭────",
                f"│ 🏗️  \033[1mPackaging\033[0m",
                "├───────────────────────────────────────────────╮",
            ]), flush=True)
        else:
            print("\n".join([
                "",
                f"# Packaging",
            ]), flush=True)

    def _print_summary_banner(self):
        if self._ansi_supported:
            print("\n".join([
                "",
                "╭────",
                f"│ 🎉 \033[1mDone\033[0m",
                "├───────────────────────────────────────────────╮",
            ]), flush=True)
        else:
            print("\n".join([
                "",
                f"# Done",
            ]), flush=True)

    def _print_status(self, scope: str, *args):
        status = " ".join([str(arg) for arg in args])
        if self._ansi_supported:
            print(f"│ \033[1m{scope}\033[0m :: {status}", flush=True)
        else:
            print(f"# {scope} :: {status}", flush=True)


def wait(bundle: Bundle, machine: MachineSpec):
    params = load_dependency_parameters()
    (url, filename) = compute_bundle_parameters(bundle, machine, params.deps_version)

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
    def run(argv: List[str], **kwargs) -> subprocess.CompletedProcess:
        return subprocess.run(argv,
                              capture_output=True,
                              encoding="utf-8",
                              check=True,
                              **kwargs)

    packages = load_dependency_parameters().packages
    for identifier in iterate_package_ids_in_dependency_order(packages.values()):
        pkg = packages[identifier]
        print(f"# Checking {pkg.name}")
        assert pkg.url.startswith("https://github.com/frida/"), f"{pkg.url}: unhandled URL"

        bump_wraps(identifier, packages, run)

        latest = query_repo_commits(identifier)["sha"]
        if pkg.version == latest:
            print(f"\tdeps.toml is up-to-date")
        else:
            print(f"\tdeps.toml is outdated")
            print(f"\t\tcurrent: {pkg.version}")
            print(f"\t\t latest: {latest}")

            f = TOMLFile(DEPS_TOML_PATH)
            config = f.read()
            config[identifier]["version"] = latest
            f.write(config)

            run(["git", "add", "deps.toml"], cwd=RELENG_DIR)
            run(["git", "commit", "-m" f"deps: Bump {pkg.name} to {latest[:7]}"], cwd=RELENG_DIR)

            packages = load_dependency_parameters().packages

        print("")


def bump_wraps(identifier: str,
               packages: Mapping[str, PackageSpec],
               run: Callable):
    root = query_repo_trees(identifier)
    subp_dir = next((t for t in root["tree"] if t["path"] == "subprojects"), None)
    if subp_dir is None or subp_dir["type"] != "tree":
        print("\tno wraps to bump")
        return

    all_wraps = [(entry, identifier_from_wrap_filename(entry["path"]))
                 for entry in query_github_api(subp_dir["url"])["tree"]
                 if entry["type"] == "blob" and entry["path"].endswith(".wrap")]
    relevant_wraps = [(blob, packages[identifier])
                      for blob, identifier in all_wraps
                      if identifier in packages]
    if not relevant_wraps:
        print(f"\tno relevant wraps, only: {', '.join([blob['path'] for blob, _ in all_wraps])}")
        return

    pending_wraps: List[Tuple[str, str, PackageSpec]] = []
    for blob, spec in relevant_wraps:
        filename = blob["path"]

        response = query_github_api(blob["url"])
        assert response["encoding"] == "base64"
        data = base64.b64decode(response["content"])

        config = ConfigParser()
        config.read_file(data.decode("utf-8").split("\n"))

        if "wrap-git" not in config:
            print(f"\tskipping {filename} as it's not wrap-git")
            continue
        source = config["wrap-git"]

        url = source["url"]
        if not url.startswith("https://github.com/frida/"):
            print(f"\tskipping {filename} as URL is external: {url}")
            continue

        revision = source["revision"]
        if revision == spec.version:
            continue

        pending_wraps.append((filename, revision, spec))
    if not pending_wraps:
        print(f"\tall wraps up-to-date")
        return

    workdir = detect_cache_dir(ROOT_DIR) / "src"
    workdir.mkdir(parents=True, exist_ok=True)

    sourcedir = workdir / identifier
    if sourcedir.exists():
        shutil.rmtree(sourcedir)
    run(["git", "clone", "--depth", "1", f"git@github.com:frida/{identifier}.git"], cwd=workdir)

    subpdir = sourcedir / "subprojects"
    revision_pattern = re.compile(r"^(?P<key_equals>\s*revision\s*=\s*)\S+$", re.MULTILINE)
    for filename, revision, dep in pending_wraps:
        wrapfile = subpdir / filename
        old_config = wrapfile.read_text(encoding="utf-8")
        # Would be simpler to use ConfigParser to write it back out, but we
        # want to preserve the particular style to keep our patches minimal.
        new_config = revision_pattern.sub(fr"\g<key_equals>{dep.version}", old_config)
        wrapfile.write_text(new_config, encoding="utf-8")

        run(["git", "add", filename], cwd=subpdir)

        action = "Pin" if revision == "main" else "Bump"
        run(["git", "commit", "-m" f"subprojects: {action} {dep.name} to {dep.version[:7]}"], cwd=sourcedir)

        print(f"\tdid {action.lower()} {filename} to {dep.version} (from {revision})")

    run(["git", "push"], cwd=sourcedir)


def identifier_from_wrap_filename(filename: str) -> str:
    return filename.split(".", maxsplit=1)[0]


def compute_bundle_parameters(bundle: Bundle,
                              machine: MachineSpec,
                              version: str) -> Tuple[str, str]:
    if bundle == Bundle.TOOLCHAIN and machine.os == "windows":
        os_arch_config = "windows-x86" if machine.arch in {"x86", "x86_64"} else machine.os_dash_arch
    else:
        os_arch_config = machine.identifier
    filename = f"{bundle.name.lower()}-{os_arch_config}.tar.xz"
    url = BUNDLE_URL.format(version=version, filename=filename)
    return (url, filename)


def bundle_is_published(public_url: str) -> bool:
    request = urllib.request.Request(public_url)
    request.get_method = lambda: "HEAD"
    try:
        with urllib.request.urlopen(request) as r:
            return True
    except urllib.request.HTTPError as e:
        if e.code != 404:
            raise CommandError("network error") from e
    return False


def load_dependency_parameters() -> DependencyParameters:
    config = TOMLFile(DEPS_TOML_PATH).read()

    packages = {}
    for identifier, pkg in config.items():
        if identifier == "dependencies":
            continue
        packages[identifier] = PackageSpec(identifier,
                                           pkg["name"],
                                           pkg["version"],
                                           pkg["url"],
                                           list(map(parse_option, pkg.get("options", []))),
                                           list(map(parse_dependency, pkg.get("dependencies", []))),
                                           pkg.get("scope"),
                                           pkg.get("when"))

    p = config["dependencies"]
    return DependencyParameters(p["version"], p["bootstrap_version"], packages)


def iterate_package_ids_in_dependency_order(packages: Sequence[PackageSpec]) -> Iterator[str]:
    ts = graphlib.TopologicalSorter({pkg.identifier: {dep.identifier for dep in pkg.dependencies}
                                     for pkg in packages})
    return ts.static_order()


def configure_bootstrap_version(version: str):
    f = TOMLFile(DEPS_TOML_PATH)
    config = f.read()
    config["dependencies"]["bootstrap_version"] = version
    f.write(config)


def query_repo_commits(repo: str,
                       organization: str = "frida",
                       branch: str = "main") -> dict:
    return query_github_api(make_github_url(f"/repos/{organization}/{repo}/commits/{branch}"))


def query_repo_trees(repo: str,
                     organization: str = "frida",
                     branch: str = "main") -> dict:
    return query_github_api(make_github_url(f"/repos/{organization}/{repo}/git/trees/{branch}"))


def query_github_api(url: str) -> dict:
    request = urllib.request.Request(url)
    request.add_header("Authorization", make_github_auth_header())
    with urllib.request.urlopen(request) as r:
        return json.load(r)


def make_github_url(path: str) -> str:
    return "https://api.github.com" + path


def make_github_auth_header() -> str:
    return "Basic " + base64.b64encode(":".join([
                                           os.environ["GH_USERNAME"],
                                           os.environ["GH_TOKEN"]
                                       ]).encode("utf-8")).decode("utf-8")


def clone_shallow(pkg: PackageSpec, outdir: Path, call_git: Callable):
    outdir.mkdir(parents=True, exist_ok=True)
    git = lambda *args: call_git(*args, cwd=outdir, check=True)
    git("init")
    git("remote", "add", "origin", pkg.url)
    git("fetch", "--depth", "1", "origin", pkg.version)
    git("checkout", "FETCH_HEAD")
    git("submodule", "update", "--init", "--recursive", "--depth", "1")


def parse_option(v: Union[str, dict]) -> OptionSpec:
    if isinstance(v, str):
        return OptionSpec(v)
    return OptionSpec(v["value"], v.get("when"))


def parse_dependency(v: Union[str, dict]) -> OptionSpec:
    if isinstance(v, str):
        return DependencySpec(v)
    return DependencySpec(v["id"], v.get("for_machine"), v.get("when"))


def copy_files(fromdir: Path,
               files: List[Path],
               todir: Path):
    for filename in files:
        src = fromdir / filename
        dst = todir / filename
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(src, dst, follow_symlinks=False)


def format_duration(duration_in_seconds: float) -> str:
    hours, remainder = divmod(duration_in_seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    return "{:02d}:{:02d}:{:02d}".format(int(hours), int(minutes), int(seconds))


class CommandError(Exception):
    pass


DEPS_TOML_PATH = RELENG_DIR / "deps.toml"

BUNDLE_URL = "https://build.frida.re/deps/{version}/{filename}"


class Bundle(Enum):
    TOOLCHAIN = 1,
    SDK = 2,


class BundleNotFoundError(Exception):
    pass


class SourceState(Enum):
    PRISTINE = 1,
    MODIFIED = 2,


@dataclass
class DependencyParameters:
    deps_version: str
    bootstrap_version: str
    packages: Dict[str, PackageSpec]


@dataclass
class PackageSpec:
    identifier: str
    name: str
    version: str
    url: str
    options: List[OptionSpec] = field(default_factory=list)
    dependencies: List[DependencySpec] = field(default_factory=list)
    scope: Optional[str] = None
    when: Optional[str] = None


@dataclass
class OptionSpec:
    value: str
    when: Optional[str] = None


@dataclass
class DependencySpec:
    identifier: str
    for_machine: str = "host"
    when: Optional[str] = None


if __name__ == "__main__":
    main()
