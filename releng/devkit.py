#!/usr/bin/env python3

import argparse
from collections import OrderedDict
from glob import glob
import machine_file
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import tempfile

if platform.system() == "Windows":
    import winenv
    from xml.etree import ElementTree
    from xml.etree.ElementTree import QName


FRIDA_ROOT = Path(__file__).resolve().parent.parent

INCLUDE_PATTERN = re.compile("#include\s+[<\"](.*?)[>\"]")

DEVKITS = {
    "frida-gum": ("frida-gum-1.0", Path("frida-1.0") / "gum" / "gum.h"),
    "frida-gumjs": ("frida-gumjs-1.0", Path("frida-1.0") / "gumjs" / "gumscriptbackend.h"),
    "frida-core": ("frida-core-1.0", Path("frida-1.0") / "frida-core.h"),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("kit")
    parser.add_argument("host")
    parser.add_argument("outdir")
    parser.add_argument("-t", "--thin", help="build without cross-arch support", action="store_true")

    arguments = parser.parse_args()

    kit = arguments.kit
    host = arguments.host
    outdir = Path(arguments.outdir).resolve()
    if arguments.thin:
        flavor = "_thin"
    else:
        flavor = ""
        fat_machine_file = query_machine_file_path(host, flavor)
        if fat_machine_file is not None \
                and not fat_machine_file.exists() \
                and query_machine_file_path(host, "_thin").exists():
            flavor = "_thin"

    outdir.mkdir(parents=True, exist_ok=True)

    generate_devkit(kit, host, flavor, outdir)


def generate_devkit(kit, host, flavor, output_dir):
    package, umbrella_header = DEVKITS[kit]

    meson_config = load_meson_config(host, flavor)

    library_filename = compute_library_filename(kit)
    (extra_ldflags, thirdparty_symbol_mappings) = generate_library(package,
                                                                   host,
                                                                   flavor,
                                                                   meson_config,
                                                                   output_dir,
                                                                   library_filename)

    umbrella_header_path = compute_umbrella_header_path(host, flavor, package, umbrella_header)

    header_file = output_dir / f"{kit}.h"
    if not umbrella_header_path.exists():
        raise Exception(f"Header not found: {umbrella_header_path}")
    header_source = generate_header(package,
                                    host,
                                    kit,
                                    flavor,
                                    meson_config,
                                    umbrella_header_path,
                                    thirdparty_symbol_mappings)
    header_file.write_text(header_source, encoding="utf-8")

    example_file = output_dir / f"{kit}-example.c"
    example_source = generate_example(example_file,
                                      package,
                                      host,
                                      kit,
                                      flavor,
                                      meson_config,
                                      extra_ldflags)
    example_file.write_text(example_source, encoding="utf-8")

    extra_files = []

    if platform.system() == "Windows":
        for msvs_asset in glob(str(asset_path(f"{kit}-*.sln"))) + glob(str(asset_path(f"{kit}-*.vcxproj*"))):
            shutil.copy(msvs_asset, output_dir)
            extra_files.append(Path(msvs_asset).name)

    return [header_file.name, library_filename, example_file.name] + extra_files


def generate_header(package, host, kit, flavor, meson_config, umbrella_header_path, thirdparty_symbol_mappings):
    if platform.system() == "Windows":
        (win_sdk_dir, win_sdk_version) = winenv.get_windows_sdk()

        include_dirs = [
            winenv.get_msvc_tool_dir() / "include",
            win_sdk_dir / "Include" / win_sdk_version / "ucrt",
            FRIDA_ROOT / "build" / "sdk-windows" / msvs_arch_config(host) / "lib" / "glib-2.0" / "include",
            FRIDA_ROOT / "build" / "sdk-windows" / msvs_arch_config(host) / "include" / "glib-2.0",
            FRIDA_ROOT / "build" / "sdk-windows" / msvs_arch_config(host) / "include" / "json-glib-1.0",
            FRIDA_ROOT / "build" / "sdk-windows" / msvs_arch_config(host) / "include" / "capstone",
            internal_include_path("gum", host),
            FRIDA_ROOT / "frida-gum",
            FRIDA_ROOT / "frida-gum" / "bindings",
        ]
        includes = ["/I" + str(include_dir) for include_dir in include_dirs]

        preprocessor = subprocess.run([msvs_cl_exe(host), "/nologo", "/E", umbrella_header_path] + includes,
                                      cwd=msvs_runtime_path(host),
                                      stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE,
                                      encoding="utf-8")
        if preprocessor.returncode != 0:
            raise Exception("Failed to spawn preprocessor: {preprocessor.stderr}")
        lines = preprocessor.stdout.split("\n")

        mapping_prefix = "#line "
        header_refs = [line[line.index("\"") + 1:line.rindex("\"")].replace("\\\\", "/") for line in lines if line.startswith(mapping_prefix)]

        header_files = deduplicate(header_refs)
        frida_root_slashed = FRIDA_ROOT.as_posix()
        header_files = [Path(h) for h in header_files if bool(re.match("^" + frida_root_slashed, h, re.I))]
    else:
        pkg_cflags = query_pkgconfig_cflags(package, meson_config)
        header_dependencies = subprocess.run(
            meson_config["c"] + meson_config["c_args"] + pkg_cflags + ["-E", "-M", umbrella_header_path],
            capture_output=True,
            encoding="utf-8",
            check=True).stdout
        header_lines = header_dependencies.strip().split("\n")[1:]
        header_files = [Path(line.rstrip("\\").strip()) for line in header_lines]
        header_files = [h for h in header_files if h.is_relative_to(FRIDA_ROOT)]

    devkit_header_lines = []
    umbrella_header = header_files[0]
    processed_header_files = {umbrella_header}
    ingest_header(umbrella_header, header_files, processed_header_files, devkit_header_lines)
    if kit == "frida-gumjs":
        inspector_server_header = umbrella_header_path.parent / "guminspectorserver.h"
        ingest_header(inspector_server_header, header_files, processed_header_files, devkit_header_lines)
    if kit == "frida-core" and host.startswith("android-"):
        selinux_header = umbrella_header_path.parent / "frida-selinux.h"
        ingest_header(selinux_header, header_files, processed_header_files, devkit_header_lines)
    devkit_header = u"".join(devkit_header_lines)

    if package.startswith("frida-gum"):
        config = """#ifndef GUM_STATIC
# define GUM_STATIC
#endif

"""
    else:
        config = ""

    if platform.system() == "Windows":
        deps = ["dnsapi", "iphlpapi", "psapi", "shlwapi", "winmm", "ws2_32"]
        if package == "frida-core-1.0":
            deps.extend(["advapi32", "crypt32", "gdi32", "kernel32", "ole32", "secur32", "shell32", "user32"])
        deps.sort()

        frida_pragmas = f"#pragma comment(lib, \"{compute_library_filename(kit)}\")"
        dep_pragmas = "\n".join([f"#pragma comment(lib, \"{dep}.lib\")" for dep in deps])

        config += frida_pragmas + "\n\n" + dep_pragmas + "\n\n"

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


def generate_library(package, host, flavor, meson_config, output_dir, library_filename):
    if platform.system() == "Windows":
        return generate_library_windows(package, host, flavor, output_dir, library_filename)
    else:
        return generate_library_unix(package, host, flavor, meson_config, output_dir, library_filename)


def generate_library_windows(package, host, flavor, output_dir, library_filename):
    pcre2 = [
        sdk_lib_path("libpcre2-8.a", host),
    ]
    libffi = [
        sdk_lib_path("libffi.a", host),
    ]
    zlib = [
        sdk_lib_path("libz.a", host),
    ]
    libbrotlidec = [
        sdk_lib_path("libbrotlicommon.a", host),
        sdk_lib_path("libbrotlidec.a", host),
    ]

    glib = pcre2 + [
        sdk_lib_path("libglib-2.0.a", host),
    ]
    gobject = glib + libffi + [
        sdk_lib_path("libgobject-2.0.a", host),
    ]
    gmodule = glib + [
        sdk_lib_path("libgmodule-2.0.a", host),
    ]
    gio = glib + gobject + gmodule + zlib + [
        sdk_lib_path("libgio-2.0.a", host),
    ]

    openssl = [
        sdk_lib_path("libssl.a", host),
        sdk_lib_path("libcrypto.a", host),
    ]

    tls_provider = openssl + [
        sdk_lib_path(Path("gio") / "modules" / "libgioopenssl.a", host),
    ]

    nice = [
        sdk_lib_path("libnice.a", host),
    ]

    usrsctp = [
        sdk_lib_path("libusrsctp.a", host),
    ]

    json_glib = glib + gobject + [
        sdk_lib_path("libjson-glib-1.0.a", host),
    ]

    gee = glib + gobject + [
        sdk_lib_path("libgee-0.8.a", host),
    ]

    nghttp2 = [
        sdk_lib_path("libnghttp2.a", host),
    ]

    sqlite = [
        sdk_lib_path("libsqlite3.a", host),
    ]

    libpsl = [
        sdk_lib_path("libpsl.a", host),
    ]

    libsoup = nghttp2 + sqlite + libbrotlidec + libpsl + [
        sdk_lib_path("libsoup-3.0.a", host),
    ]

    capstone = [
        sdk_lib_path("libcapstone.a", host)
    ]

    quickjs = [
        sdk_lib_path("libquickjs.a", host)
    ]

    tinycc = [
        sdk_lib_path("libtcc.a", host)
    ]

    v8 = []

    build_props = ElementTree.parse(FRIDA_ROOT / "releng" / "frida.props")
    frida_v8_tag = str(QName("http://schemas.microsoft.com/developer/msbuild/2003", "FridaV8"))

    for elem in build_props.iter():
        if elem.tag == frida_v8_tag:
            if elem.text == "Enabled":
                v8 += [
                    sdk_lib_path("libv8-10.0.a", host),
                ]
            break

    gum_lib = internal_arch_lib_path("gum", host)
    gum_deps = deduplicate(glib + gobject + capstone)
    gum = [gum_lib] + gum_deps

    gumjs_lib = internal_arch_lib_path("gumjs", host)
    gumjs_deps = deduplicate(gum + quickjs + v8 + gio + tls_provider + json_glib + tinycc + sqlite)
    gumjs = [gumjs_lib] + gumjs_deps

    gumjs_inspector_lib = internal_noarch_lib_path("gumjs-inspector", host)
    gumjs_inspector_deps = deduplicate(gum + json_glib + libsoup)
    gumjs_inspector = [gumjs_inspector_lib] + gumjs_inspector_deps

    frida_core_lib = internal_noarch_lib_path("frida-core", host)
    frida_core_deps = deduplicate(glib
                                  + gobject
                                  + gio
                                  + tls_provider
                                  + nice
                                  + openssl
                                  + usrsctp
                                  + json_glib
                                  + gmodule
                                  + gee
                                  + libsoup
                                  + gum
                                  + gumjs_inspector
                                  + libbrotlidec
                                  + capstone)
    frida_core = [frida_core_lib] + frida_core_deps

    if package == "frida-gum-1.0":
        input_libs = gum
    elif package == "frida-gumjs-1.0":
        input_libs = gumjs
    elif package == "frida-core-1.0":
        input_libs = frida_core
    else:
        raise Exception("Unhandled package")

    subprocess.run([msvs_lib_exe(host), "/nologo", "/out:" + str(output_dir / library_filename)] + input_libs,
                   cwd=msvs_runtime_path(host),
                   capture_output=True,
                   check=True)

    extra_flags = [lib_path.name for lib_path in input_libs]
    thirdparty_symbol_mappings = []

    return (extra_flags, thirdparty_symbol_mappings)


def generate_library_unix(package, host, flavor, meson_config, output_dir, library_filename):
    output_path = output_dir / library_filename
    output_path.unlink(missing_ok=True)

    library_flags = subprocess.run(meson_config["pkgconfig"] + ["--static", "--libs", package],
                                   capture_output=True,
                                   encoding="utf-8",
                                   check=True).stdout.strip().split(" ")
    library_dirs = infer_library_dirs(library_flags)
    library_names = infer_library_names(library_flags)
    library_paths, extra_flags = resolve_library_paths(library_names, library_dirs)
    extra_flags += infer_linker_flags(library_flags)

    v8_libs = [path for path in library_paths if path.name.startswith("libv8")]
    if len(v8_libs) > 0:
        v8_libdir = v8_libs[0].parent
        libcxx_libs = [Path(p) for p in glob(str(v8_libdir / "c++" / "*.a"))]
        library_paths.extend(libcxx_libs)

    ar = meson_config["ar"]
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
    elif host.startswith("macos-") or host.startswith("ios-"):
        subprocess.run(["xcrun", "libtool", "-static", "-o", output_path] + library_paths,
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
    if objcopy is not None:
        thirdparty_symbol_mappings = get_thirdparty_symbol_mappings(output_path, meson_config)

        renames = "\n".join([f"{original} {renamed}" for original, renamed in thirdparty_symbol_mappings]) + "\n"
        with tempfile.NamedTemporaryFile() as renames_file:
            renames_file.write(renames.encode("utf-8"))
            renames_file.flush()
            subprocess.run(objcopy + ["--redefine-syms=" + renames_file.name, output_path],
                           check=True)
    else:
        thirdparty_symbol_mappings = []

    return (extra_flags, thirdparty_symbol_mappings)


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

    for line in subprocess.run(meson_config["nm"] + [library],
                               capture_output=True,
                               encoding="utf-8",
                               check=True).stdout.split("\n"):
        tokens = line.split(" ")
        if len(tokens) < 3:
            continue
        (kind, name) = tokens[-2:]
        result.append((kind, name))

    return result


def infer_library_dirs(flags):
    return [Path(flag[2:]) for flag in flags if flag.startswith("-L")]


def infer_library_names(flags):
    return [flag[2:] for flag in flags if flag.startswith("-l")]


def infer_linker_flags(flags):
    return [flag for flag in flags if flag.startswith("-Wl") or flag == "-pthread"]


def resolve_library_paths(names, dirs):
    paths = []
    flags = []
    for name in names:
        library_path = None
        for d in dirs:
            candidate = d / f"lib{name}.a"
            if candidate.exists():
                library_path = candidate
                break
        if library_path is not None:
            paths.append(library_path)
        else:
            flags.append(f"-l{name}")
    return (deduplicate(paths), flags)


def generate_example(source_file, package, host, kit, flavor, meson_config, extra_ldflags):
    os_flavor = "windows" if platform.system() == "Windows" else "unix"

    example_code = asset_path(f"{kit}-example-{os_flavor}.c").read_text(encoding="utf-8")

    if platform.system() == "Windows":
        return example_code
    else:
        if host.split("-")[0] in ["macos", "ios", "android"]:
            cc = "clang++" if kit == "frida-gumjs" else "clang"
        else:
            cc = "g++" if kit == "frida-gumjs" else "gcc"
        cflags = meson_config["common_flags"] + meson_config["c_args"]
        ldflags = meson_config["c_link_args"]

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


def asset_path(name):
    return Path(__file__).parent / "devkit-assets" / name


def load_meson_config(host, flavor):
    path = query_machine_file_path(host, flavor)
    if path is None:
        return None

    return machine_file.load(path)


def query_machine_file_path(host, flavor):
    if platform.system() == "Windows":
        return None

    return FRIDA_ROOT / "build" / f"frida{flavor}-{host}.txt"


def query_pkgconfig_cflags(package, meson_config):
    raw_flags = subprocess.run(meson_config["pkgconfig"] + ["--cflags", package],
                               capture_output=True,
                               encoding="utf-8",
                               check=True).stdout
    return shlex.split(raw_flags)


def msvs_cl_exe(host):
    return msvs_tool_path(host, "cl.exe")


def msvs_lib_exe(host):
    return msvs_tool_path(host, "lib.exe")


def msvs_tool_path(host, tool):
    if host == "windows-x86_64":
        return Path(winenv.get_msvc_tool_dir()) / "bin" / "HostX86" / "x64" / tool
    else:
        return Path(winenv.get_msvc_tool_dir()) / "bin" / "HostX86" / "x86" / tool


def msvs_runtime_path(host):
    return Path(winenv.get_msvc_tool_dir()) / "bin" / "HostX86" / "x86"


def msvs_arch_config(host):
    if host == "windows-x86_64":
        return "x64-Release"
    else:
        return "Win32-Release"


def msvs_arch_suffix(host):
    if host == "windows-x86_64":
        return "-64"
    else:
        return "-32"


def compute_library_filename(kit):
    if platform.system() == "Windows":
        return f"{kit}.lib"
    else:
        return f"lib{kit}.a"


def compute_umbrella_header_path(host, flavor, package, umbrella_header):
    if platform.system() == "Windows":
        if package == "frida-gum-1.0":
            return FRIDA_ROOT / "frida-gum" / "gum" / "gum.h"
        elif package == "frida-gumjs-1.0":
            return FRIDA_ROOT / "frida-gum" / "bindings" / "gumjs" / umbrella_header.name
        elif package == "frida-core-1.0":
            return FRIDA_ROOT / "build" / "tmp-windows" / msvs_arch_config(host) / "frida-core" / "api" / "frida-core.h"
        else:
            raise Exception("Unhandled package")
    else:
        p = FRIDA_ROOT / "build" / ("frida" + flavor + "-" + host)
        if host.startswith("ios-"):
            p = p / "usr"
        return p / "include" / umbrella_header


def sdk_lib_path(name, host):
    return FRIDA_ROOT / "build" / "sdk-windows" / msvs_arch_config(host) / "lib" / name


def internal_include_path(name, host):
    return FRIDA_ROOT / "build" / "tmp-windows" / msvs_arch_config(host) / (name + msvs_arch_suffix(host))


def internal_noarch_lib_path(name, host):
    return FRIDA_ROOT / "build" / "tmp-windows" / msvs_arch_config(host) / name / f"{name}.lib"


def internal_arch_lib_path(name, host):
    lib_name = name + msvs_arch_suffix(host)
    return FRIDA_ROOT / "build" / "tmp-windows" / msvs_arch_config(host) / lib_name / f"{lib_name}.lib"


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


if __name__ == "__main__":
    main()
