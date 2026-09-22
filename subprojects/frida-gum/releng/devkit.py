from collections import OrderedDict
from enum import Enum
import itertools
import locale
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
from typing import Mapping, Sequence, Union

from . import env
from .machine_spec import MachineSpec


REPO_ROOT = Path(__file__).resolve().parent.parent

DEVKITS = {
    "frida-gum": ("frida-gum-1.0", Path("gum") / "gum.h"),
    "frida-gumjs": ("frida-gumjs-1.0", Path("gumjs") / "gumscriptbackend.h"),
    "frida-core": ("frida-core-1.0", Path("frida-core.h")),
}

ASSETS_PATH = Path(__file__).parent / "devkit-assets"

INCLUDE_PATTERN = re.compile(r"#include\s+[<\"](.*?)[>\"]")


class DepSymbolScope(str, Enum):
    PREFIXED = "prefixed"
    ORIGINAL = "original"


class CompilerApplication:
    def __init__(self,
                 kit: str,
                 machine: MachineSpec,
                 meson_config: Mapping[str, Union[str, Sequence[str]]],
                 output_dir: Path,
                 dep_symbol_scope: DepSymbolScope = DepSymbolScope.PREFIXED):
        self.kit = kit
        package, umbrella_header = DEVKITS[kit]
        self.package = package
        self.umbrella_header = umbrella_header

        self.machine = machine
        self.meson_config = meson_config
        self.compiler_argument_syntax = None
        self.output_dir = output_dir
        self.dep_symbol_scope = dep_symbol_scope
        self.library_filename = None

    def run(self):
        output_dir = self.output_dir
        kit = self.kit

        self.compiler_argument_syntax = detect_compiler_argument_syntax(self.meson_config)
        self.library_filename = compute_library_filename(self.kit, self.compiler_argument_syntax)

        output_dir.mkdir(parents=True, exist_ok=True)

        (extra_ldflags, thirdparty_symbol_mappings) = self._generate_library()

        umbrella_header_path = compute_umbrella_header_path(self.machine,
                                                            self.package,
                                                            self.umbrella_header,
                                                            self.meson_config)

        header_file = output_dir / f"{kit}.h"
        if not umbrella_header_path.exists():
            raise Exception(f"Header not found: {umbrella_header_path}")
        header_source = self._generate_header(umbrella_header_path, thirdparty_symbol_mappings)
        header_file.write_text(header_source, encoding="utf-8")

        example_file = output_dir / f"{kit}-example.c"
        example_source = self._generate_example(example_file, extra_ldflags)
        example_file.write_text(example_source, encoding="utf-8")

        extra_files = []

        extra_files += self._generate_gir()

        if self.compiler_argument_syntax == "msvc":
            for msvs_asset in itertools.chain(ASSETS_PATH.glob(f"{kit}-*.sln"), ASSETS_PATH.glob(f"{kit}-*.vcxproj*")):
                shutil.copy(msvs_asset, output_dir)
                extra_files.append(msvs_asset.name)

        return [header_file.name, self.library_filename, example_file.name] + extra_files

    def _generate_gir(self):
        if self.kit != "frida-core":
            return []

        gir_path = Path(query_pkgconfig_variable("frida_girdir", self.package, self.meson_config)) / "Frida-1.0.gir"
        gir_name = "frida-core.gir"

        shutil.copy(gir_path, self.output_dir / gir_name)

        return [gir_name]

    def _generate_header(self, umbrella_header_path, thirdparty_symbol_mappings):
        kit = self.kit
        package = self.package
        machine = self.machine
        meson_config = self.meson_config

        c_args = meson_config.get("c_args", [])

        include_cflags = query_pkgconfig_cflags(package, meson_config)

        if self.compiler_argument_syntax == "msvc":
            preprocessor = subprocess.run(meson_config["c"] + c_args + ["/nologo", "/E", umbrella_header_path] + include_cflags,
                                          stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE,
                                          encoding="utf-8")
            if preprocessor.returncode != 0:
                raise Exception(f"Failed to spawn preprocessor: {preprocessor.stderr}")
            lines = preprocessor.stdout.split("\n")

            mapping_prefix = "#line "
            header_refs = [line[line.index("\"") + 1:line.rindex("\"")].replace("\\\\", "/") for line in lines if line.startswith(mapping_prefix)]

            header_files = deduplicate(header_refs)
            frida_root_slashed = REPO_ROOT.as_posix()
            header_files = [Path(h) for h in header_files if bool(re.match("^" + frida_root_slashed, h, re.I))]
        else:
            header_dependencies = subprocess.run(
                meson_config["c"] + c_args + include_cflags + ["-E", "-M", umbrella_header_path],
                capture_output=True,
                encoding="utf-8",
                check=True).stdout
            _, raw_header_files = header_dependencies.split(": ", maxsplit=1)
            header_files = [Path(item) for item in shlex.split(raw_header_files) if item != "\n"]
            header_files = [h for h in header_files if h.is_relative_to(REPO_ROOT)]

        header_files = exclude_libc_headers(header_files, meson_config, include_cflags)

        devkit_header_lines = []
        umbrella_header = header_files[0]
        processed_header_files = {umbrella_header}
        ingest_header(umbrella_header, header_files, processed_header_files, devkit_header_lines)
        if kit in {"frida-gum", "frida-gumjs"} and machine.is_freestanding:
            gum_dir = umbrella_header_path.parent
            if kit == "frida-gumjs":
                gum_dir = gum_dir.parent.parent / "gum"
            barebone_header = gum_dir / "backend-barebone" / "include" / "gum" / "gumbarebone.h"
            ingest_header(barebone_header, header_files, processed_header_files, devkit_header_lines)
        if kit == "frida-gumjs" and not machine.is_freestanding:
            inspector_server_header = umbrella_header_path.parent / "guminspectorserver.h"
            ingest_header(inspector_server_header, header_files, processed_header_files, devkit_header_lines)
        if kit == "frida-core" and machine.os == "android":
            selinux_header = umbrella_header_path.parent / "frida-selinux.h"
            ingest_header(selinux_header, header_files, processed_header_files, devkit_header_lines)
        if kit == "frida-core" and machine.os not in {"windows", "none"}:
            for header in query_gio_unix_headers(meson_config):
                ingest_header(header, header_files, processed_header_files, devkit_header_lines)
        devkit_header = u"".join(devkit_header_lines)

        if package.startswith("frida-gum"):
            config = """#ifndef GUM_STATIC
# define GUM_STATIC
#endif

"""
        else:
            config = ""

        if machine.os == "windows":
            deps = ["dnsapi", "iphlpapi", "psapi", "shlwapi", "winmm", "ws2_32"]
            if package == "frida-core-1.0":
                deps.extend(["advapi32", "crypt32", "gdi32", "kernel32", "ole32", "secur32", "shell32", "user32"])
            deps.sort()

            frida_pragmas = f"#pragma comment(lib, \"{compute_library_filename(kit, self.compiler_argument_syntax)}\")"
            dep_pragmas = "\n".join([f"#pragma comment(lib, \"{dep}.lib\")" for dep in deps])

            config += f"#ifdef _MSC_VER\n\n{frida_pragmas}\n\n{dep_pragmas}\n\n#endif\n\n"

        if len(thirdparty_symbol_mappings) > 0:
            public_mappings = []
            for original, renamed in extract_public_thirdparty_symbol_mappings(thirdparty_symbol_mappings):
                public_mappings.append((original, renamed))
                if f"define {original}" not in devkit_header and f"define  {original}" not in devkit_header:
                    continue
                def fixup_macro(match):
                    prefix = match.group(1)
                    suffix = re.sub(f"\\b{original}\\b", renamed, match.group(2))
                    return f"#undef {original}\n{prefix}{original}{suffix}"
                devkit_header = re.sub(r"^([ \t]*#[ \t]*define[ \t]*){0}\b((.*\\\n)*.*)$".format(original), fixup_macro, devkit_header, flags=re.MULTILINE)

            config += "#ifndef __FRIDA_SYMBOL_MAPPINGS__\n"
            config += "#define __FRIDA_SYMBOL_MAPPINGS__\n\n"
            config += "\n".join([f"#define {original} {renamed}" for original, renamed in public_mappings]) + "\n\n"
            config += "#endif\n\n"

        return (config + devkit_header).replace("\r\n", "\n")

    def _generate_library(self):
        library_flags = call_pkgconfig(["--static", "--libs", self.package], self.meson_config).split(" ")

        library_dirs = infer_library_dirs(library_flags)
        library_names = infer_library_names(library_flags)
        library_paths, extra_flags = resolve_library_paths(library_names, library_dirs, self.machine,
                                                           self._build_root())
        extra_flags += infer_linker_flags(library_flags)

        if self.compiler_argument_syntax == "msvc":
            thirdparty_symbol_mappings = self._do_generate_library_msvc(library_paths)
        else:
            thirdparty_symbol_mappings = self._do_generate_library_unix(library_paths)

        return (extra_flags, thirdparty_symbol_mappings)

    def _build_root(self):
        for candidate in [self.output_dir, *self.output_dir.parents]:
            if is_meson_build_root(candidate):
                return candidate
        return None

    def _do_generate_library_msvc(self, library_paths):
        subprocess.run(self.meson_config["lib"] + ["/nologo", "/out:" + str(self.output_dir / self.library_filename)] + library_paths,
                       capture_output=True,
                       encoding="utf-8",
                       check=True)

        thirdparty_symbol_mappings = []

        return thirdparty_symbol_mappings

    def _do_generate_library_unix(self, library_paths):
        output_path = self.output_dir / self.library_filename
        output_path.unlink(missing_ok=True)

        v8_libs = [path for path in library_paths if path.name.startswith("libv8")]
        if len(v8_libs) > 0:
            v8_libdir = v8_libs[0].parent
            libcxx_libs = list((v8_libdir / "c++").glob("*.a"))
            library_paths.extend(libcxx_libs)

        meson_config = self.meson_config

        ar = meson_config.get("ar", ["ar"])
        ar_help = subprocess.run(ar + ["--help"],
                                 stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT,
                                 encoding="utf-8").stdout
        mri_supported = "-M [<mri-script]" in ar_help

        if mri_supported:
            mri = ["create " + str(output_path)]
            mri += [f"addlib {path}" for path in library_paths]
            mri += ["save", "end"]
            subprocess.run(ar + ["-M"],
                           input="\n".join(mri),
                           encoding="utf-8",
                           check=True)
        elif self.machine.is_apple:
            subprocess.run(meson_config.get("libtool", ["xcrun", "libtool"]) +
                                ["-static", "-o", output_path] + library_paths,
                           capture_output=True,
                           check=True)
        else:
            combined_dir = Path(tempfile.mkdtemp(prefix="devkit"))
            object_names = set()

            for library_path in library_paths:
                scratch_dir = Path(tempfile.mkdtemp(prefix="devkit"))

                subprocess.run(ar + ["x", library_path],
                               cwd=scratch_dir,
                               capture_output=True,
                               check=True)
                for object_name in [entry.name for entry in scratch_dir.iterdir() if entry.name.endswith(".o")]:
                    object_path = scratch_dir / object_name
                    while object_name in object_names:
                        object_name = "_" + object_name
                    object_names.add(object_name)
                    shutil.move(object_path, combined_dir / object_name)

                shutil.rmtree(scratch_dir)

            subprocess.run(ar + ["rcs", output_path] + list(object_names),
                           cwd=combined_dir,
                           capture_output=True,
                           check=True)

            shutil.rmtree(combined_dir)

        objcopy = meson_config.get("objcopy", None)
        if self.dep_symbol_scope is DepSymbolScope.PREFIXED and objcopy is not None:
            thirdparty_symbol_mappings = get_thirdparty_symbol_mappings(output_path, meson_config)
            renames = "\n".join([f"{original} {renamed}" for original, renamed in thirdparty_symbol_mappings]) + "\n"
            with tempfile.NamedTemporaryFile() as renames_file:
                renames_file.write(renames.encode("utf-8"))
                renames_file.flush()
                subprocess.run(objcopy + ["--redefine-syms=" + renames_file.name, output_path],
                               check=True)
        else:
            thirdparty_symbol_mappings = []

        return thirdparty_symbol_mappings

    def _generate_example(self, source_file, extra_ldflags):
        kit = self.kit
        machine = self.machine

        os_flavor = "windows" if machine.os == "windows" else "unix"

        example_code = (ASSETS_PATH / f"{kit}-example-{os_flavor}.c").read_text(encoding="utf-8")

        if machine.os == "windows":
            return example_code
        else:
            if machine.is_apple or machine.os == "android":
                cc = "clang++" if kit == "frida-gumjs" else "clang"
            else:
                cc = "g++" if kit == "frida-gumjs" else "gcc"
            meson_config = self.meson_config
            cflags = meson_config.get("common_flags", []) + meson_config.get("c_args", [])
            ldflags = meson_config.get("c_link_args", [])

            (cflags, ldflags) = tweak_flags(cflags, extra_ldflags + ldflags)

            if cc == "g++":
                ldflags.append("-static-libstdc++")

            params = {
                "cc": cc,
                "cflags": shlex.join(cflags),
                "ldflags": shlex.join(ldflags),
                "source_filename": source_file.name,
                "program_filename": source_file.stem,
                "library_name": kit
            }

            preamble = """\
/*
 * Compile with:
 *
 * %(cc)s %(cflags)s %(source_filename)s -o %(program_filename)s -L. -l%(library_name)s %(ldflags)s
 *
 * Visit https://frida.re to learn more about Frida.
 */""" % params

            return preamble + "\n\n" + example_code


def ingest_header(header, all_header_files, processed_header_files, result):
    with header.open(encoding="utf-8") as f:
        for line in f:
            match = INCLUDE_PATTERN.match(line.strip())
            if match is not None:
                name_parts = tuple(match.group(1).split("/"))
                num_parts = len(name_parts)
                inline = False
                for other_header in all_header_files:
                    if other_header.parts[-num_parts:] == name_parts:
                        inline = True
                        if other_header not in processed_header_files:
                            processed_header_files.add(other_header)
                            ingest_header(other_header, all_header_files, processed_header_files, result)
                        break
                if not inline:
                    result.append(line)
            else:
                result.append(line)


def extract_public_thirdparty_symbol_mappings(mappings):
    public_prefixes = ["g_", "glib_", "gobject_", "gio_", "gee_", "json_", "cs_"]
    return [(original, renamed) for original, renamed in mappings if any([original.startswith(prefix) for prefix in public_prefixes])]


def get_thirdparty_symbol_mappings(library, meson_config):
    return [(name, "_frida_" + name) for name in get_thirdparty_symbol_names(library, meson_config)]


def get_thirdparty_symbol_names(library, meson_config):
    visible_names = list(set([name for kind, name in get_symbols(library, meson_config) if kind in ("T", "D", "B", "R", "C")]))
    visible_names.sort()

    frida_prefixes = ["frida", "_frida", "gum", "_gum"]
    thirdparty_names = [name for name in visible_names if not any([name.startswith(prefix) for prefix in frida_prefixes])]

    return thirdparty_names


def get_symbols(library, meson_config):
    result = []

    for line in subprocess.run(meson_config.get("nm", "nm") + [library],
                               capture_output=True,
                               encoding="utf-8",
                               check=True).stdout.split("\n"):
        tokens = line.split(" ")
        if len(tokens) < 3:
            continue
        (kind, name) = tokens[-2:]
        result.append((kind, name))

    return result


def exclude_libc_headers(headers, meson_config, include_cflags):
    sysroot = next((Path(flag[10:]) for flag in meson_config["c"] if flag.startswith("--sysroot=")), None)
    if sysroot is None:
        return headers

    # Bare-metal targets get their libc from the SDK, which puts it inside the very
    # tree everything else is inlined from. It is still the consumer's to provide,
    # so keep to what came in through an -I of its own.
    package_dirs = [d for d in infer_include_dirs(include_cflags)
                    if d.is_relative_to(sysroot) and d != sysroot / "include"]
    return [h for h in headers
            if not h.is_relative_to(sysroot) or any([h.is_relative_to(d) for d in package_dirs])]


def infer_include_dirs(flags):
    return [Path(flag[2:]) for flag in flags if flag.startswith("-I")]


def infer_library_dirs(flags):
    return [Path(flag[2:]) for flag in flags if flag.startswith("-L")]


def infer_library_names(flags):
    return [flag[2:] for flag in flags if flag.startswith("-l")]


def infer_linker_flags(flags):
    return [flag for flag in flags if flag.startswith("-Wl") or flag == "-pthread"]


def resolve_library_paths(names, dirs, machine, build_root=None):
    if build_root is not None:
        dirs = wrap_overrides_first(dirs, build_root)

    paths = []
    flags = []
    for name in names:
        library_path = None
        for d in dirs:
            candidate = d / f"lib{name}.a"
            if candidate.exists():
                library_path = candidate
                break
        if library_path is not None and not is_os_library(library_path, machine):
            paths.append(library_path)
        else:
            flags.append(f"-l{name}")
    return (deduplicate(paths), flags)


def wrap_overrides_first(dirs, build_root):
    prefix = build_root.resolve()
    inside_build_tree = lambda d: d.resolve() == prefix or prefix in d.resolve().parents
    return [d for d in dirs if inside_build_tree(d)] + [d for d in dirs if not inside_build_tree(d)]


def is_os_library(path, machine):
    if machine.os == "linux":
        return path.name in {"libdl.a", "libm.a", "libpthread.a"}
    return False


def is_meson_build_root(path):
    return (path / "meson-info").is_dir()


def query_gio_unix_headers(meson_config):
    include_dirs = [Path(flag[2:]) for flag in query_pkgconfig_cflags("gio-unix-2.0", meson_config)
                    if flag.startswith("-I")]
    candidates = [d / "gio" / "gunixfdmessage.h" for d in include_dirs]
    return [c for c in candidates if c.exists()]


def query_pkgconfig_cflags(package, meson_config):
    raw_flags = call_pkgconfig(["--cflags", package], meson_config)
    return shlex.split(raw_flags)


def query_pkgconfig_variable(name, package, meson_config):
    return call_pkgconfig([f"--variable={name}", package], meson_config)


def call_pkgconfig(argv, meson_config):
    pc_env = {
        **os.environ,
        "PKG_CONFIG_PATH": os.pathsep.join(meson_config.get("pkg_config_path", [])),
    }
    return subprocess.run(meson_config.get("pkg-config", ["pkg-config"]) + argv,
                          capture_output=True,
                          encoding="utf-8",
                          check=True,
                          env=pc_env).stdout.strip()


def detect_compiler_argument_syntax(meson_config):
    if "Microsoft " in subprocess.run(meson_config["c"],
                      capture_output=True,
                      encoding=locale.getpreferredencoding()).stderr:
        return "msvc"

    return "unix"


def compute_library_filename(kit, compiler_argument_syntax):
    if compiler_argument_syntax == "msvc":
        return f"{kit}.lib"
    else:
        return f"lib{kit}.a"


def compute_umbrella_header_path(machine, package, umbrella_header, meson_config):
    for incdir in infer_include_dirs(query_pkgconfig_cflags(package, meson_config)):
        candidate = (incdir / umbrella_header)
        if candidate.exists():
            return candidate
    raise Exception(f"Unable to resolve umbrella header path for {umbrella_header}")


def tweak_flags(cflags, ldflags):
    tweaked_cflags = []
    tweaked_ldflags = []

    pending_cflags = cflags[:]
    while len(pending_cflags) > 0:
        flag = pending_cflags.pop(0)
        if flag == "-include":
            pending_cflags.pop(0)
        else:
            tweaked_cflags.append(flag)

    tweaked_cflags = deduplicate(tweaked_cflags)
    existing_cflags = set(tweaked_cflags)

    pending_ldflags = ldflags[:]
    seen_libs = set()
    seen_flags = set()
    while len(pending_ldflags) > 0:
        flag = pending_ldflags.pop(0)
        if flag in ("-arch", "-isysroot") and flag in existing_cflags:
            pending_ldflags.pop(0)
        else:
            if flag == "-isysroot":
                sysroot = pending_ldflags.pop(0)
                if "MacOSX" in sysroot:
                    tweaked_ldflags.append("-isysroot \"$(xcrun --sdk macosx --show-sdk-path)\"")
                elif "iPhoneOS" in sysroot:
                    tweaked_ldflags.append("-isysroot \"$(xcrun --sdk iphoneos --show-sdk-path)\"")
                continue
            elif flag == "-L":
                pending_ldflags.pop(0)
                continue
            elif flag.startswith("-L"):
                continue
            elif flag.startswith("-l"):
                if flag in seen_libs:
                    continue
                seen_libs.add(flag)
            elif flag == "-pthread":
                if flag in seen_flags:
                    continue
                seen_flags.add(flag)
            tweaked_ldflags.append(flag)

    pending_ldflags = tweaked_ldflags
    tweaked_ldflags = []
    while len(pending_ldflags) > 0:
        flag = pending_ldflags.pop(0)

        raw_flags = []
        while flag.startswith("-Wl,"):
            raw_flags.append(flag[4:])
            if len(pending_ldflags) > 0:
                flag = pending_ldflags.pop(0)
            else:
                flag = None
                break
        if len(raw_flags) > 0:
            merged_flags = "-Wl," + ",".join(raw_flags)
            if "--icf=" in merged_flags:
                tweaked_ldflags.append("-fuse-ld=gold")
            tweaked_ldflags.append(merged_flags)

        if flag is not None and flag not in existing_cflags:
            tweaked_ldflags.append(flag)

    return (tweaked_cflags, tweaked_ldflags)


def deduplicate(items):
    return list(OrderedDict.fromkeys(items))
