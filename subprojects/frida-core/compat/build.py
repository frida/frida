from __future__ import annotations
import argparse
import base64
from collections import OrderedDict
from dataclasses import dataclass, field
import itertools
import locale
import os
from pathlib import Path
import pickle
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import traceback
from typing import Any, Literal, Mapping, Optional, Sequence, Tuple


REPO_ROOT = Path(__file__).resolve().parent.parent
NINJA = os.environ.get("NINJA", "ninja")


Role = Literal["project", "subproject"]


def main(argv):
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers()

    command = subparsers.add_parser("setup", help="setup everything needed to compile")
    command.add_argument("role", help="project vs subproject", choices=["project", "subproject"])
    command.add_argument("builddir", help="build directory", type=Path)
    command.add_argument("top_builddir", help="top build directory", type=Path)
    command.add_argument("frida_version", help="the Frida version")
    command.add_argument("host_os", help="operating system binaries are being built for")
    command.add_argument("host_os_family", help="operating system family, e.g. darwin")
    command.add_argument("host_arch", help="architecture binaries are being built for")
    command.add_argument("host_config", help="configuration binaries are being built for")
    command.add_argument("compat", help="support for targets with a different architecture",
                         type=parse_compat_option_value)
    command.add_argument("assets", help="whether assets are embedded vs installed and loaded at runtime")
    command.add_argument("components", help="which components will be built",
                         type=parse_array_option_value)
    command.add_argument("glib_flavor", help="upstream or frida", choices=["upstream", "frida"])
    command.add_argument("apple_tls", help="whether the parent build is using the Apple TLS backend",
                         choices=["enabled", "disabled"])
    command.add_argument("compilers", help="compiler command arrays", nargs="+")
    command.set_defaults(func=lambda args: setup(args.role,
                                                 args.builddir,
                                                 args.top_builddir,
                                                 args.frida_version,
                                                 args.host_os,
                                                 args.host_os_family,
                                                 args.host_arch,
                                                 args.host_config if args.host_config else None,
                                                 args.compat,
                                                 args.assets,
                                                 args.components,
                                                 args.glib_flavor,
                                                 args.apple_tls,
                                                 parse_compilers(args.compilers)))

    command = subparsers.add_parser("compile", help="compile compatibility assets")
    command.add_argument("privdir", help="directory to store intermediate files", type=Path)
    command.add_argument("state", help="opaque state from the setup step")
    command.set_defaults(func=lambda args: compile(args.privdir, pickle.loads(base64.b64decode(args.state))))

    args = parser.parse_args()
    if "func" in args:
        try:
            args.func(args)
        except subprocess.CalledProcessError as e:
            print(e, file=sys.stderr)
            print("Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")), file=sys.stderr)
            sys.exit(1)
    else:
        parser.print_usage(file=sys.stderr)
        sys.exit(1)


def parse_compat_option_value(v: str) -> set[str]:
    vals = parse_array_option_value(v)

    if len(vals) > 1:
        for choice in {"auto", "disabled"}:
            if choice in vals:
                raise argparse.ArgumentTypeError(f"the compat '{choice}' choice cannot be combined with other choices")

    return vals


def parse_array_option_value(v: str) -> set[str]:
    return {v.strip() for v in v.split(",")}


def parse_compilers(compilers: list[str]) -> Compilers:
    cc = pop_cmd_array_arg(compilers)
    cpp = pop_cmd_array_arg(compilers)
    return Compilers(cc, cpp)


def pop_cmd_array_arg(args: list[str]) -> list[str]:
    result: list[str] = []
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


def setup(role: Role,
          builddir: Path,
          top_builddir: Path,
          frida_version: str,
          host_os: str,
          host_os_family: str,
          host_arch: str,
          host_config: Optional[str],
          compat: set[str],
          assets: str,
          components: set[str],
          glib_flavor: str,
          apple_tls: str,
          compilers: Compilers):
    try:
        outputs: Mapping[str, Sequence[Output]] = OrderedDict()

        outputs[OutputGroup(arch=None)] = [Output("arch_support_bundle", "arch-support.bundle", Path("compat"), "")]

        releng_location = query_releng_location(role)
        ensure_submodules_checked_out(releng_location)
        configure_import_path(releng_location)

        auto_detect = "auto" in compat
        if auto_detect:
            if host_os in {"windows", "macos", "linux", "ios", "tvos", "android"}:
                summary = f"enabled by default for {host_os}-{host_arch}"
                compat = {"native", "emulated"}
            else:
                summary = f"disabled by default for {host_os}-{host_arch}"
                compat = set()
        elif "disabled" in compat:
            summary = "disabled by user"
            compat = set()
        else:
            summary = "enabled by user"
        missing: list[MissingFeature] = []

        if "native" in compat:
            have_toolchain = True
            other_label: Optional[str] = None
            other_triplet: Optional[str] = None
            extra_environ: dict[str, str] = {}

            if host_os == "windows" and host_arch in {"x86_64", "x86"}:
                if host_arch == "x86_64":
                    other_label = "x86"
                    other_kind = "legacy"
                else:
                    other_label = "x86_64"
                    other_kind = "modern"

                if host_config == "mingw":
                    have_toolchain, other_triplet = detect_mingw_toolchain_for(other_label)
                else:
                    have_toolchain = True
                    other_triplet = None

            elif host_os == "linux" and host_arch == "x86_64" and host_config is None:
                other_label = "x86"
                other_triplet = "i686-linux-gnu"
                have_toolchain = shutil.which(other_triplet + "-gcc") is not None
                if not have_toolchain:
                    with (tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", suffix=".c") as probe_c,
                          tempfile.NamedTemporaryFile(delete=False) as probe_executable):
                        try:
                            probe_c.write("int main (void) { return 0; }")
                            probe_c.flush()
                            p = subprocess.run(compilers.cc + ["-m32", probe_c.name, "-o", probe_executable.name],
                                               capture_output=True)
                            if p.returncode == 0:
                                extra_environ["CC"] = shlex.join(compilers.cc + ["-m32"])
                                extra_environ["CXX"] = shlex.join(compilers.cpp + ["-m32"])
                                have_toolchain = True
                        finally:
                            try:
                                os.unlink(probe_executable.name)
                            except:
                                pass

            if not have_toolchain:
                if not auto_detect:
                    raise ToolchainNotFoundError(f"unable to locate toolchain for {other_triplet}")
                missing.append(MissingFeature(other_label, other_triplet))

            if host_os == "windows" and host_arch in {"x86_64", "x86"} and have_toolchain:
                group = OutputGroup(other_label, other_triplet, extra_environ)
                outputs[group] = []
                if "helper" in components:
                    outputs[group] += [
                        Output(identifier=f"helper_{other_kind}",
                               name=HELPER_FILE_WINDOWS.name,
                               file=HELPER_FILE_WINDOWS,
                               target=HELPER_TARGET),
                    ]
                if "agent" in components:
                    outputs[group] += [
                        Output(identifier=f"agent_{other_kind}",
                               name=AGENT_FILE_WINDOWS.name,
                               file=AGENT_FILE_WINDOWS,
                               target=AGENT_TARGET),
                    ]
                if "gadget" in components:
                    outputs[group] += [
                        Output(identifier=f"gadget_{other_kind}",
                               name=GADGET_FILE_WINDOWS.name,
                               file=GADGET_FILE_WINDOWS,
                               target=GADGET_TARGET),
                    ]

            if host_os in {"macos", "ios"} and host_arch in {"arm64e", "arm64"} and host_config != "simulator":
                if host_arch == "arm64e":
                    other_arch = "arm64"
                    kind = "legacy"
                else:
                    other_arch = "arm64e"
                    kind = "modern"
                group = OutputGroup(other_arch)
                outputs[group] = []
                if "helper" in components:
                    outputs[group] += [
                        Output(identifier=f"helper_{kind}",
                               name=f"frida-helper-{other_arch}",
                               file=HELPER_FILE_UNIX,
                               target=HELPER_TARGET),
                    ]
                if "agent" in components:
                    outputs[group] += [
                        Output(identifier=f"agent_{kind}",
                               name=f"frida-agent-{other_arch}.dylib",
                               file=AGENT_FILE_DARWIN,
                               target=AGENT_TARGET),
                    ]
                if "gadget" in components:
                    outputs[group] += [
                        Output(identifier=f"gadget_{kind}",
                               name=f"frida-gadget-{other_arch}.dylib",
                               file=GADGET_FILE_DARWIN,
                               target=GADGET_TARGET),
                    ]
                if "server" in components and assets == "installed":
                    outputs[group] += [
                        Output(identifier=f"server_{kind}",
                               name=f"frida-server-{other_arch}",
                               file=SERVER_FILE_UNIX,
                               target=SERVER_TARGET),
                    ]

            if host_os == "linux" and host_arch == "x86_64" and have_toolchain:
                group = OutputGroup("x86", other_triplet, extra_environ)
                outputs[group] = []
                if "helper" in components:
                    outputs[group] += [
                        Output(identifier="helper_legacy",
                               name=HELPER_FILE_UNIX.name,
                               file=HELPER_FILE_UNIX,
                               target=HELPER_TARGET),
                    ]
                if "agent" in components:
                    outputs[group] += [
                        Output(identifier="agent_legacy",
                               name=AGENT_FILE_ELF.name,
                               file=AGENT_FILE_ELF,
                               target=AGENT_TARGET),
                    ]
                if "gadget" in components:
                    outputs[group] += [
                        Output(identifier="gadget_legacy",
                               name=GADGET_FILE_ELF.name,
                               file=GADGET_FILE_ELF,
                               target=GADGET_TARGET),
                    ]

            if host_os == "android" and host_arch in {"arm64", "x86_64"}:
                group = OutputGroup("arm" if host_arch == "arm64" else "x86")
                outputs[group] = []
                if "helper" in components:
                    outputs[group] += [
                        Output(identifier="helper_legacy",
                               name=HELPER_FILE_UNIX.name,
                               file=HELPER_FILE_UNIX,
                               target=HELPER_TARGET),
                    ]
                if "agent" in components:
                    outputs[group] += [
                        Output(identifier="agent_legacy",
                               name=AGENT_FILE_ELF.name,
                               file=AGENT_FILE_ELF,
                               target=AGENT_TARGET),
                    ]
                if "gadget" in components:
                    outputs[group] += [
                        Output(identifier="gadget_legacy",
                               name=GADGET_FILE_ELF.name,
                               file=GADGET_FILE_ELF,
                               target=GADGET_TARGET),
                    ]

        if "emulated" in compat:
            if host_os == "windows" and host_arch == "arm64":
                for kind, emulated_arch in [("modern", "x86_64"), ("legacy", "x86")]:
                    if host_config == "mingw":
                        found, emulated_triplet = detect_mingw_toolchain_for(emulated_arch)
                        if not found:
                            if not auto_detect:
                                raise ToolchainNotFoundError(f"unable to locate toolchain for {emulated_triplet}")
                            missing.append(MissingFeature(emulated_arch, emulated_triplet))
                            continue
                    else:
                        emulated_triplet = None
                    group = OutputGroup(emulated_arch, emulated_triplet, extra_environ)
                    outputs[group] = []
                    if "helper" in components:
                        outputs[group] += [
                            Output(identifier=f"helper_emulated_{kind}",
                                   name=HELPER_FILE_WINDOWS.name.replace(".exe", f"-{emulated_arch}.exe"),
                                   file=HELPER_FILE_WINDOWS,
                                   target=HELPER_TARGET),
                        ]
                    if "agent" in components:
                        outputs[group] += [
                            Output(identifier=f"agent_emulated_{kind}",
                                   name=AGENT_FILE_WINDOWS.name.replace(".dll", f"-{emulated_arch}.dll"),
                                   file=AGENT_FILE_WINDOWS,
                                   target=AGENT_TARGET),
                        ]

            if host_os == "android" and host_arch in {"x86_64", "x86"} and "agent" in components:
                outputs[OutputGroup("arm")] = [
                    Output(identifier="agent_emulated_legacy",
                           name="frida-agent-arm.so",
                           file=AGENT_FILE_ELF,
                           target=AGENT_TARGET),
                ]
                if host_arch == "x86_64":
                    outputs[OutputGroup("arm64")] = [
                        Output(identifier="agent_emulated_modern",
                               name="frida-agent-arm64.so",
                               file=AGENT_FILE_ELF,
                               target=AGENT_TARGET),
                    ]

        if glib_flavor == "upstream":
            kind = "modern" if arch_is_modern(host_os, host_arch) else "legacy"
            _, agent_file, gadget_file = native_file_paths(host_os)
            group = OutputGroup(host_arch)
            outputs[group] = []
            if "agent" in components:
                outputs[group] += [
                    Output(identifier=f"agent_{kind}",
                           name=arch_suffixed_name(agent_file.name, host_arch),
                           file=agent_file,
                           target=AGENT_TARGET),
                ]
            if "gadget" in components:
                outputs[group] += [
                    Output(identifier=f"gadget_{kind}",
                           name=arch_suffixed_name(gadget_file.name, host_arch),
                           file=gadget_file,
                           target=GADGET_TARGET),
                ]
        elif apple_tls == "enabled" and "agent" in components:
            kind = "modern" if arch_is_modern(host_os, host_arch) else "legacy"
            _, agent_file, _ = native_file_paths(host_os)
            group = OutputGroup(host_arch)
            outputs.setdefault(group, [])
            outputs[group] += [
                Output(identifier=f"agent_{kind}",
                       name=arch_suffixed_name(agent_file.name, host_arch),
                       file=agent_file,
                       target=AGENT_TARGET),
            ]

        outputs = OrderedDict((g, items) for g, items in outputs.items() if items)

        raw_allowed_prebuilds = os.environ.get("FRIDA_ALLOWED_PREBUILDS")
        if raw_allowed_prebuilds is not None:
            allowed_prebuilds = set(filter(None, raw_allowed_prebuilds.split(",")))
        else:
            allowed_prebuilds = None

        state = State(role, builddir, top_builddir, frida_version, host_os, host_os_family, host_config, glib_flavor, apple_tls, allowed_prebuilds, outputs)
        serialized_state = base64.b64encode(pickle.dumps(state)).decode('ascii')

        if missing:
            summary = ", ".join([f"{m.label} disabled due to missing toolchain for {m.triplet}" for m in missing])
        variable_names, output_names = zip(*[(output.identifier, output.name) \
                for output in itertools.chain.from_iterable(outputs.values())])
        print("ok")
        print(summary)
        print(f"{','.join(variable_names)}")
        print(f"{','.join(output_names)}")
        print(DEPFILE_FILENAME)
        print(serialized_state)
    except Exception as e:
        print(f"error {e}")
        print("")
        print(traceback.format_exception(e))


class ToolchainNotFoundError(Exception):
    pass


@dataclass
class Compilers:
    cc: list[str]
    cpp: list[str]


@dataclass
class State:
    role: Role
    builddir: Path
    top_builddir: Path
    frida_version: str
    host_os: str
    host_os_family: str
    host_config: Optional[str]
    glib_flavor: str
    apple_tls: str
    allowed_prebuilds: Optional[set[str]]
    outputs: Mapping[OutputGroup, Sequence[Output]]


@dataclass
class OutputGroup:
    arch: Optional[str]
    triplet: Optional[str] = None
    extra_environ: dict[str, str] = field(default_factory=dict)

    def __eq__(self, other):
        if isinstance(other, OutputGroup):
            return other.arch == self.arch
        return False

    def __hash__(self):
        return hash(self.arch)


@dataclass
class Output:
    identifier: str
    name: str
    file: Path
    target: str


@dataclass
class MissingFeature:
    label: str
    triplet: str


def compile(privdir: Path, state: State):
    releng_location = query_releng_location(state.role)
    subprojects = detect_relevant_subprojects(releng_location)
    if state.role == "subproject":
        grab_subprojects_from_parent(subprojects, releng_location)
    configure_import_path(releng_location)

    from releng.env import call_meson
    from releng.machine_spec import MachineSpec
    from releng.meson_configure import configure
    from releng.meson_make import make

    def call_internal_meson(argv, *args, **kwargs):
        if "stdout" not in kwargs and "stderr" not in kwargs:
            silenced_kwargs = {
                **kwargs,
                "stdout": subprocess.PIPE,
                "stderr": subprocess.STDOUT,
                "encoding": locale.getpreferredencoding(),
            }
        else:
            silenced_kwargs = kwargs
        return call_meson(argv, *args, **silenced_kwargs)

    options: Optional[Sequence[str]] = None
    build_env = scrub_environment(os.environ)
    build_env["FRIDA_RELENG"] = str(releng_location)
    top_builddir = state.top_builddir
    depfile_lines = []
    for group, outputs in state.outputs.items():
        if group.arch is None:
            for o in outputs:
                (state.builddir / o.name).write_bytes(b"")
            continue

        workdir = (privdir / group.arch).resolve()

        if not (workdir / "build.ninja").exists():
            if options is None:
                options = load_meson_options(top_builddir, state.role, set(subprojects.keys()))
                version_opt = next((opt for opt in options if opt.startswith("-Dfrida_version=")), None)
                if version_opt is None:
                    options += [f"-Dfrida_version={state.frida_version}"]

            host_machine = MachineSpec(state.host_os, group.arch, state.host_config, group.triplet)

            if state.glib_flavor == "upstream":
                allowed = None
            else:
                allowed = state.allowed_prebuilds

            if state.glib_flavor == "upstream":
                forwarded_options = [o for o in options if not is_stripped_compat_option(o)]
                forwarded_options += ["-Ddefault_library=static"]
            else:
                forwarded_options = [o for o in options if not is_overridden_compat_option(o)]
            if state.apple_tls == "enabled":
                forwarded_options += ["-Dapple_tls=disabled"]

            configure(sourcedir=REPO_ROOT,
                      builddir=workdir,
                      host_machine=host_machine,
                      environ={**build_env, **group.extra_environ},
                      allowed_prebuilds=allowed,
                      apple_min_os=detect_parent_apple_min_os(top_builddir, state.host_os, state.host_os_family),
                      extra_meson_options=[
                          "-Dcompiler_backend=disabled",
                          "-Dassets=installed",
                          "-Dhelper_modern=",
                          "-Dhelper_legacy=",
                          "-Dagent_modern=",
                          "-Dagent_legacy=",
                          "-Dagent_emulated_modern=",
                          "-Dagent_emulated_legacy=",
                          *forwarded_options,
                      ],
                      call_meson=call_internal_meson,
                      on_progress=lambda progress: None)

        make(sourcedir=REPO_ROOT,
             builddir=workdir,
             targets=[o.target for o in outputs],
             environ=build_env,
             call_meson=call_internal_meson)

        for o in outputs:
            shutil.copy(workdir / o.file, state.builddir / o.name)

            output_relpath = (workdir / o.name).relative_to(top_builddir).as_posix()
            input_paths = shlex.split(subprocess.run([NINJA, "-t", "inputs", o.file],
                                                     cwd=workdir,
                                                     capture_output=True,
                                                     encoding="utf-8",
                                                     check=True).stdout)
            input_entries = []
            for raw_path in input_paths:
                path = Path(raw_path)
                if not path.is_absolute():
                    path = Path(os.path.relpath(workdir / path, top_builddir))
                input_entries.append(escape_depfile_path(path.as_posix()))
            depfile_lines.append(f"{output_relpath}: {' '.join(input_entries)}")

    (state.builddir / DEPFILE_FILENAME).write_text("\n".join(depfile_lines), encoding="utf-8")


def is_stripped_compat_option(o: str) -> bool:
    if is_overridden_compat_option(o):
        return True
    name = o.split("=", 1)[0].removeprefix("-D")
    return name in STRIPPED_COMPAT_OPTIONS or name.endswith(":default_library")


def is_overridden_compat_option(o: str) -> bool:
    name = o.split("=", 1)[0].removeprefix("-D")
    return name in OVERRIDDEN_COMPAT_OPTIONS


def load_meson_options(top_builddir: Path,
                       role: Role,
                       subprojects: set[str]) -> Sequence[str]:
    from mesonbuild import coredata

    return [f"-D{adapt_key(k, role)}={v.value}" for k, v in coredata.load(top_builddir).options.items()
            if option_should_be_forwarded(k, v, role, subprojects)]


def adapt_key(k: "OptionKey", role: Role) -> "OptionKey":
    if role == "subproject" and k.subproject == "frida-core":
        return k.as_root()
    return k


def option_should_be_forwarded(k: "OptionKey",
                               v: "coredata.UserOption[Any]",
                               role: Role,
                               subprojects: set[str]) -> bool:
    from mesonbuild import coredata

    our_project_id = "frida-core" if role == "subproject" else ""
    is_for_us = k.subproject == our_project_id
    is_for_child = k.subproject in subprojects

    if k.is_project():
        if is_for_us:
            if k.name == "compiler_backend":
                return False
            tokens = k.name.split("_")
            if tokens[0] in {"helper", "agent"} and tokens[-1] in {"modern", "legacy"}:
                return False
        if k.subproject and k.machine is not coredata.MachineChoice.HOST:
            return False
        return is_for_us or is_for_child

    if coredata.CoreData.is_per_machine_option(k):
        # The compat build generates its own machine files, so forwarding the
        # parent's build-machine options would inject a foreign toolchain. In a
        # native compat build that conjures a compiler-less build machine and
        # breaks any native: true target.
        return False

    if k.is_builtin():
        if k.name in {"buildtype", "genvslite"}:
            return False
        if not str(v.value):
            return False
        if not is_for_child and role == "subproject" and k.subproject in {"", our_project_id}:
            if not coredata.BUILTIN_OPTIONS[k.as_root()].yielding:
                return k.subproject == our_project_id
            return True

    if k.module == "python":
        if k.name == "install_env" and v.value == "prefix":
            return False
        if not str(v.value):
            return False

    return is_for_us or is_for_child


def scrub_environment(env: Mapping[str, str]) -> Mapping[str, str]:
    from releng.env import TOOLCHAIN_ENVVARS
    clean_env = OrderedDict()
    envvars_to_avoid = {*TOOLCHAIN_ENVVARS, *MSVS_ENVVARS}
    for k, v in env.items():
        if k in envvars_to_avoid:
            continue
        if k.upper() == "PATH" and platform.system() == "Windows":
            v = scrub_windows_devenv_dirs_from_path(v, env)
        clean_env[k] = v
    return clean_env


def scrub_windows_devenv_dirs_from_path(raw_path: str, env: Mapping[str, str]) -> str:
    raw_vcinstalldir = env.get("VCINSTALLDIR")
    if raw_vcinstalldir is None:
        return raw_path
    vcinstalldir = Path(raw_vcinstalldir)
    clean_entries = []
    for raw_entry in raw_path.split(";"):
        entry = Path(raw_entry)
        if entry.is_relative_to(vcinstalldir):
            continue
        if "WINDOWS KITS" in [p.upper() for p in entry.parts]:
            continue
        clean_entries.append(raw_entry)
    return ";".join(clean_entries)


def escape_depfile_path(path: str) -> str:
    return path.replace(" ", "\\ ")


def query_releng_location(role: Role) -> Path:
    if role == "subproject":
        candidate = REPO_ROOT.parent.parent / "releng"
        if candidate.exists():
            return candidate
    return REPO_ROOT / "releng"


def ensure_submodules_checked_out(releng_location: Path):
    if not (releng_location / "meson" / "meson.py").exists():
        subprocess.run(["git", "submodule", "update", "--init", "--recursive", "--depth", "1", "releng"],
                       cwd=releng_location.parent,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT,
                       encoding="utf-8",
                       check=True)


def detect_parent_apple_min_os(top_builddir: Path,
                               host_os: str,
                               host_os_family: str) -> Optional[Mapping[str, str]]:
    if host_os_family != "darwin":
        return None
    pattern = re.compile(r"-target['\"],\s*['\"][^-]+-apple-" + re.escape(host_os) + r"([0-9.]+)")
    for native_file in top_builddir.glob(f"frida-{host_os}-*.txt"):
        match = pattern.search(native_file.read_text(encoding="utf-8"))
        if match is not None:
            return {host_os: match.group(1)}
    return None


def detect_relevant_subprojects(releng_location: Path) -> dict[str, Path]:
    subprojects = detect_relevant_subprojects_in(REPO_ROOT, releng_location)
    gum_location = subprojects.get("frida-gum")
    if gum_location is not None:
        subprojects.update(detect_relevant_subprojects_in(gum_location, releng_location))
    return subprojects


def detect_relevant_subprojects_in(repo_root: Path, releng_location: Path) -> dict[str, Path]:
    result = {}
    for f in (repo_root / "subprojects").glob("*.wrap"):
        name = f.stem
        location = releng_location.parent / "subprojects" / name
        if location.exists():
            result[name] = location
    return result


def grab_subprojects_from_parent(subprojects: dict[str, Path], releng_location: Path):
    for name, location in subprojects.items():
        subp_here = REPO_ROOT / "subprojects" / name
        if subp_here.exists():
            continue

        try:
            subp_here.symlink_to(Path("..") / ".." / name, target_is_directory=True)
            continue
        except OSError as e:
            if not getattr(e, "winerror", None) == 1314:
                raise e

        subprocess.run(["git", "worktree", "add", subp_here],
                       cwd=location,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT,
                       encoding="utf-8",
                       check=True)


def configure_import_path(releng_location: Path):
    sys.path.insert(0, str(releng_location / "meson"))
    sys.path.insert(0, str(releng_location.parent))


def detect_mingw_toolchain_for(arch: str) -> Tuple[bool, Optional[str]]:
    triplet = MINGW_ARCHS.get(arch, arch) + "-w64-mingw32"
    found = shutil.which(triplet + "-gcc") is not None
    return (found, triplet)


def arch_is_modern(host_os: str, host_arch: str) -> bool:
    if host_os in {"macos", "ios", "tvos"} and host_arch == "arm64":
        return host_arch == "arm64e"
    return host_arch in MODERN_ARCHS


def native_file_paths(host_os: str) -> Tuple[Path, Path, Path]:
    if host_os == "windows":
        return (HELPER_FILE_WINDOWS, AGENT_FILE_WINDOWS, GADGET_FILE_WINDOWS)
    elif host_os in {"macos", "ios", "tvos"}:
        return (HELPER_FILE_UNIX, AGENT_FILE_DARWIN, GADGET_FILE_DARWIN)
    else:
        return (HELPER_FILE_UNIX, AGENT_FILE_ELF, GADGET_FILE_ELF)


def arch_suffixed_name(name: str, arch: str) -> str:
    stem, _, ext = name.partition(".")
    if ext:
        return f"{stem}-{arch}.{ext}"
    return f"{stem}-{arch}"


STATE_FILENAME = "state.dat"
DEPFILE_FILENAME = "compat.deps"

STRIPPED_COMPAT_OPTIONS = {
    "default_library",
    "frida-gum:gumjs",
    "frida-gum:inspector",
}

OVERRIDDEN_COMPAT_OPTIONS = {
    "compiler_backend",
    "assets",
    "helper_modern",
    "helper_legacy",
    "agent_modern",
    "agent_legacy",
    "agent_emulated_modern",
    "agent_emulated_legacy",
}

HELPER_TARGET = "frida-helper"
HELPER_FILE_WINDOWS = Path("src") / "frida-helper.exe"
HELPER_FILE_UNIX = Path("src") / "frida-helper"

AGENT_TARGET = "frida-agent"
AGENT_FILE_WINDOWS = Path("lib") / "agent" / "frida-agent.dll"
AGENT_FILE_DARWIN = Path("lib") / "agent" / "frida-agent.dylib"
AGENT_FILE_ELF = Path("lib") / "agent" / "frida-agent.so"

GADGET_TARGET = "frida-gadget"
GADGET_FILE_WINDOWS = Path("lib") / "gadget" / "frida-gadget.dll"
GADGET_FILE_DARWIN = Path("lib") / "gadget" / "frida-gadget.dylib"
GADGET_FILE_ELF = Path("lib") / "gadget" / "frida-gadget.so"

SERVER_TARGET = "frida-server"
SERVER_FILE_UNIX = Path("server") / "frida-server"

MSVS_ENVVARS = {
    "PLATFORM",
    "VCINSTALLDIR",
    "INCLUDE",
    "LIB",
}

MODERN_ARCHS = {"x86_64", "arm64", "arm64e", "mips64", "riscv64"}

MINGW_ARCHS = {
    "x86": "i686",
    "arm64": "aarch64",
}


if __name__ == "__main__":
    main(sys.argv)
