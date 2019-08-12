#!/usr/bin/env python3

from __future__ import print_function
import codecs
from collections import OrderedDict
from glob import glob
import os
import pipes
import platform
import re
import shutil
import subprocess
import sys
import tempfile

if platform.system() == 'Windows':
    import winenv


INCLUDE_PATTERN = re.compile("#include\s+[<\"](.*?)[>\"]")

DEVKITS = {
    "frida-gum": ("frida-gum-1.0", ("frida-1.0", "gum", "gum.h")),
    "frida-gumjs": ("frida-gumjs-1.0", ("frida-1.0", "gumjs", "gumscriptbackend.h")),
    "frida-core": ("frida-core-1.0", ("frida-1.0", "frida-core.h")),
}


def generate_devkit(kit, host, flavor, output_dir):
    package, umbrella_header = DEVKITS[kit]

    frida_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

    library_filename = compute_library_filename(kit)
    (extra_ldflags, thirdparty_symbol_mappings) = generate_library(package, frida_root, host, flavor, output_dir, library_filename)

    umbrella_header_path = compute_umbrella_header_path(frida_root, host, flavor, package, umbrella_header)

    header_filename = kit + ".h"
    if not os.path.exists(umbrella_header_path):
        raise Exception("Header not found: {}".format(umbrella_header_path))
    header = generate_header(package, frida_root, host, kit, flavor, umbrella_header_path, thirdparty_symbol_mappings)
    with codecs.open(os.path.join(output_dir, header_filename), "w", 'utf-8') as f:
        f.write(header)

    example_filename = kit + "-example.c"
    example = generate_example(example_filename, package, frida_root, host, kit, flavor, extra_ldflags)
    with codecs.open(os.path.join(output_dir, example_filename), "w", 'utf-8') as f:
        f.write(example)

    if platform.system() == 'Windows':
        for msvs_asset in glob(asset_path("{}-*.sln".format(kit))) + glob(asset_path("{}-*.vcxproj*".format(kit))):
            shutil.copy(msvs_asset, output_dir)

    return [header_filename, library_filename, example_filename]

def generate_header(package, frida_root, host, kit, flavor, umbrella_header_path, thirdparty_symbol_mappings):
    if platform.system() == 'Windows':
        (win10_sdk_dir, win10_sdk_version) = winenv.get_win10_sdk()

        include_dirs = [
            os.path.join(winenv.get_msvc_tool_dir(), "include"),
            os.path.join(win10_sdk_dir, "Include", win10_sdk_version, "ucrt"),
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "lib", "glib-2.0", "include"),
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "include", "glib-2.0"),
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "include", "glib-2.0"),
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "include", "json-glib-1.0"),
            os.path.join(frida_root, "capstone", "include", "capstone"),
            os.path.join(frida_root, "frida-gum"),
            os.path.join(frida_root, "frida-gum", "bindings")
        ]
        includes = ["/I" + include_dir for include_dir in include_dirs]

        preprocessor = subprocess.Popen(
            [msvs_cl_exe(host), "/nologo", "/E", umbrella_header_path] + includes,
            cwd=msvs_runtime_path(host),
            shell=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE)
        stdout, stderr = preprocessor.communicate()
        if preprocessor.returncode != 0:
            raise Exception("Failed to spawn preprocessor: " + stderr.decode('utf-8'))
        lines = stdout.decode('utf-8').split('\n')

        mapping_prefix = "#line "
        header_refs = [line[line.index("\"") + 1:line.rindex("\"")].replace("\\\\", "/") for line in lines if line.startswith(mapping_prefix)]

        header_files = deduplicate(header_refs)
        frida_root_slashed = frida_root.replace("\\", "/")
        header_files = [header_file for header_file in header_files if bool(re.match('^' + frida_root_slashed, header_file, re.I))]
    else:
        rc = env_rc(frida_root, host, flavor)
        header_dependencies = subprocess.check_output(
            ["(. \"{rc}\" && $CPP $CFLAGS -M $($PKG_CONFIG --cflags {package}) \"{header}\")".format(rc=rc, package=package, header=umbrella_header_path)],
            shell=True).decode('utf-8')
        header_lines = header_dependencies.strip().split("\n")[1:]
        header_files = [line.rstrip("\\").strip() for line in header_lines]
        header_files = [header_file for header_file in header_files
            if header_file.startswith(frida_root) and "/ndk-" not in header_file[len(frida_root):]
        ]

    devkit_header_lines = []
    umbrella_header = header_files[0]
    processed_header_files = set([umbrella_header])
    ingest_header(umbrella_header, header_files, processed_header_files, devkit_header_lines)
    if kit == "frida-gumjs":
        inspector_server_header = os.path.join(os.path.dirname(umbrella_header_path), "guminspectorserver.h")
        ingest_header(inspector_server_header, header_files, processed_header_files, devkit_header_lines)
    if kit == "frida-core" and host.startswith("android-"):
        selinux_header = os.path.join(os.path.dirname(umbrella_header_path), "frida-selinux.h")
        ingest_header(selinux_header, header_files, processed_header_files, devkit_header_lines)
    devkit_header = u"".join(devkit_header_lines)

    if package.startswith("frida-gum"):
        config = """#ifndef GUM_STATIC
# define GUM_STATIC
#endif

"""
    else:
        config = ""

    if platform.system() == 'Windows':
        deps = ["dnsapi", "iphlpapi", "psapi", "winmm", "ws2_32"]
        if package == "frida-core-1.0":
            deps.extend(["advapi32", "gdi32", "kernel32", "ole32", "shell32", "shlwapi", "user32"])
        deps.sort()

        frida_pragmas = "#pragma comment(lib, \"{}\")".format(compute_library_filename(kit))
        dep_pragmas = "\n".join(["#pragma comment(lib, \"{}.lib\")".format(dep) for dep in deps])

        config += frida_pragmas + "\n\n" + dep_pragmas + "\n\n"

    if len(thirdparty_symbol_mappings) > 0:
        public_mappings = []
        for original, renamed in extract_public_thirdparty_symbol_mappings(thirdparty_symbol_mappings):
            public_mappings.append((original, renamed))
            if not "define {0}".format(original) in devkit_header:
                continue
            def fixup_macro(match):
                prefix = match.group(1)
                suffix = re.sub(r"\b{0}\b".format(original), renamed, match.group(2))
                return "#undef {0}\n".format(original) + prefix + original + suffix
            devkit_header = re.sub(r"^([ \t]*#[ \t]*define[ \t]*){0}\b((.*\\\n)*.*)$".format(original), fixup_macro, devkit_header, flags=re.MULTILINE)

        config += "#ifndef __FRIDA_SYMBOL_MAPPINGS__\n"
        config += "#define __FRIDA_SYMBOL_MAPPINGS__\n\n"
        config += "\n".join(["#define {0} {1}".format(original, renamed) for original, renamed in public_mappings]) + "\n\n"
        config += "#endif\n\n"

    return (config + devkit_header).replace("\r\n", "\n")

def ingest_header(header, all_header_files, processed_header_files, result):
    with codecs.open(header, "r", 'utf-8') as f:
        for line in f:
            match = INCLUDE_PATTERN.match(line.strip())
            if match is not None:
                name = match.group(1)
                inline = False
                for other_header in all_header_files:
                    if other_header.endswith("/" + name):
                        inline = True
                        if not other_header in processed_header_files:
                            processed_header_files.add(other_header)
                            ingest_header(other_header, all_header_files, processed_header_files, result)
                        break
                if not inline:
                    result.append(line)
            else:
                result.append(line)

def generate_library(package, frida_root, host, flavor, output_dir, library_filename):
    if platform.system() == 'Windows':
        return generate_library_windows(package, frida_root, host, flavor, output_dir, library_filename)
    else:
        return generate_library_unix(package, frida_root, host, flavor, output_dir, library_filename)

def generate_library_windows(package, frida_root, host, flavor, output_dir, library_filename):
    glib = [
        sdk_lib_path("libglib-2.0.a", frida_root, host),
    ]
    gobject = glib + [
        sdk_lib_path("libgobject-2.0.a", frida_root, host),
        sdk_lib_path("libffi.a", frida_root, host),
    ]
    gmodule = glib + [
        sdk_lib_path("libgmodule-2.0.a", frida_root, host),
    ]
    gio = glib + gobject + gmodule + [
        sdk_lib_path("libgio-2.0.a", frida_root, host),
        sdk_lib_path("libz.a", frida_root, host),
    ]

    tls_provider = [
        sdk_lib_path(os.path.join("gio", "modules", "libgioschannel-static.a"), frida_root, host),
    ]

    json_glib = glib + gobject + [
        sdk_lib_path("libjson-glib-1.0.a", frida_root, host),
    ]

    gee = glib + gobject + [
        sdk_lib_path("libgee-0.8.a", frida_root, host),
    ]

    sqlite = [
        sdk_lib_path("libsqlite3.a", frida_root, host),
    ]

    libsoup = [
        sdk_lib_path("libsoup-2.4.a", frida_root, host),
        sdk_lib_path("libpsl.a", frida_root, host),
        sdk_lib_path("libxml2.a", frida_root, host),
    ]

    v8 = [
        sdk_lib_path("libv8-7.0.a", frida_root, host),
    ]

    capstone = [
        internal_arch_lib_path("capstone", frida_root, host)
    ]

    gum_lib = internal_arch_lib_path("gum", frida_root, host)
    gum_deps = deduplicate(glib + gobject + gio + capstone)
    gumjs_deps = deduplicate([gum_lib] + gum_deps + tls_provider + json_glib + sqlite + libsoup + v8)
    frida_core_deps = deduplicate(glib + gobject + gio + tls_provider + json_glib + gmodule + gee + libsoup)

    if package == "frida-gum-1.0":
        package_lib_path = gum_lib
        package_lib_deps = gum_deps
    elif package == "frida-gumjs-1.0":
        package_lib_path = internal_arch_lib_path("gumjs", frida_root, host)
        package_lib_deps = gumjs_deps
    elif package == "frida-core-1.0":
        package_lib_path = internal_noarch_lib_path("frida-core", frida_root, host)
        package_lib_deps = frida_core_deps
    else:
        raise Exception("Unhandled package")

    input_libs = [package_lib_path] + package_lib_deps

    subprocess.check_output(
        [msvs_lib_exe(host), "/nologo", "/out:" + os.path.join(output_dir, library_filename)] + input_libs,
        cwd=msvs_runtime_path(host),
        shell=False)

    extra_flags = [os.path.basename(lib_path) for lib_path in input_libs]
    thirdparty_symbol_mappings = []

    return (extra_flags, thirdparty_symbol_mappings)

def generate_library_unix(package, frida_root, host, flavor, output_dir, library_filename):
    output_path = os.path.join(output_dir, library_filename)

    try:
        os.unlink(output_path)
    except:
        pass

    rc = env_rc(frida_root, host, flavor)
    ar = probe_env(rc, "echo $AR")

    library_flags = subprocess.check_output(
        ["(. \"{rc}\" && $PKG_CONFIG --static --libs {package})".format(rc=rc, package=package)],
        shell=True).decode('utf-8').strip().split(" ")
    library_dirs = infer_library_dirs(library_flags)
    library_names = infer_library_names(library_flags)
    library_paths, extra_flags = resolve_library_paths(library_names, library_dirs)
    extra_flags += infer_linker_flags(library_flags)

    ar_version = subprocess.Popen([ar, "--version"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT).communicate()[0].decode('utf-8')
    mri_supported = ar_version.startswith("GNU ar ")

    if mri_supported:
        mri = ["create " + output_path]
        mri += ["addlib " + path for path in library_paths]
        mri += ["save", "end"]
        raw_mri = "\n".join(mri)
        ar = subprocess.Popen([ar, "-M"], stdin=subprocess.PIPE)
        ar.communicate(input=raw_mri.encode('utf-8'))
        if ar.returncode != 0:
            raise Exception("ar failed")
    else:
        combined_dir = tempfile.mkdtemp(prefix="devkit")
        object_names = set()

        for library_path in library_paths:
            scratch_dir = tempfile.mkdtemp(prefix="devkit")

            subprocess.check_output([ar, "x", library_path], cwd=scratch_dir)
            for object_path in glob(os.path.join(scratch_dir, "*.o")):
                object_name = os.path.basename(object_path)
                while object_name in object_names:
                    object_name = "_" + object_name
                object_names.add(object_name)
                shutil.move(object_path, os.path.join(combined_dir, object_name))

            shutil.rmtree(scratch_dir)

        subprocess.check_output([ar, "rcs", output_path] + list(object_names), cwd=combined_dir)

        shutil.rmtree(combined_dir)

    objcopy = probe_env(rc, "echo $OBJCOPY")
    if len(objcopy) > 0:
        thirdparty_symbol_mappings = get_thirdparty_symbol_mappings(output_path, rc)

        renames = "\n".join(["{0} {1}".format(original, renamed) for original, renamed in thirdparty_symbol_mappings]) + "\n"
        with tempfile.NamedTemporaryFile() as renames_file:
            renames_file.write(renames.encode('utf-8'))
            renames_file.flush()
            subprocess.check_call([objcopy, "--redefine-syms=" + renames_file.name, output_path])
    else:
        thirdparty_symbol_mappings = []

    return (extra_flags, thirdparty_symbol_mappings)

def extract_public_thirdparty_symbol_mappings(mappings):
    public_prefixes = ["g_", "glib_", "gobject_", "gio_", "gee_", "json_"]
    return [(original, renamed) for original, renamed in mappings if any([original.startswith(prefix) for prefix in public_prefixes])]

def get_thirdparty_symbol_mappings(library, rc):
    return [(name, "_frida_" + name) for name in get_thirdparty_symbol_names(library, rc)]

def get_thirdparty_symbol_names(library, rc):
    visible_names = list(set([name for kind, name in get_symbols(library, rc) if kind in ('T', 'D', 'B', 'R', 'C')]))
    visible_names.sort()

    frida_prefixes = ["frida", "_frida", "gum", "_gum"]
    thirdparty_names = [name for name in visible_names if not any([name.startswith(prefix) for prefix in frida_prefixes])]

    return thirdparty_names

def get_symbols(library, rc):
    result = []

    nm = probe_env(rc, "echo $NM")

    for line in subprocess.check_output([nm, library]).decode('utf-8').split("\n"):
        tokens = line.split(" ")
        if len(tokens) < 3:
            continue
        (kind, name) = tokens[-2:]
        result.append((kind, name))

    return result

def infer_library_dirs(flags):
    return [flag[2:] for flag in flags if flag.startswith("-L")]

def infer_library_names(flags):
    return [flag[2:] for flag in flags if flag.startswith("-l")]

def infer_linker_flags(flags):
    return [flag for flag in flags if flag.startswith("-Wl")]

def resolve_library_paths(names, dirs):
    paths = []
    flags = []
    for name in names:
        library_path = None
        for d in dirs:
            candidate = os.path.join(d, "lib{}.a".format(name))
            if os.path.exists(candidate):
                library_path = candidate
                break
        if library_path is not None:
            paths.append(library_path)
        else:
            flags.append("-l{}".format(name))
    return (deduplicate(paths), flags)

def generate_example(filename, package, frida_root, host, kit, flavor, extra_ldflags):
    if platform.system() == 'Windows':
        os_flavor = "windows"
    else:
        os_flavor = "unix"

    example_filename = "{}-example-{}.c".format(kit, os_flavor)
    with codecs.open(asset_path(example_filename), "rb", 'utf-8') as f:
        example_code = f.read()

    if platform.system() == 'Windows':
        return example_code
    else:
        rc = env_rc(frida_root, host, flavor)

        cc = probe_env(rc, "echo $CC")
        cflags = probe_env(rc, "echo $CFLAGS")
        ldflags = probe_env(rc, "echo $LDFLAGS")

        (cflags, ldflags) = trim_flags(cflags, " ".join([" ".join(extra_ldflags), ldflags]))

        params = {
            "cc": cc,
            "cflags": cflags,
            "ldflags": ldflags,
            "source_filename": filename,
            "program_filename": os.path.splitext(filename)[0],
            "library_name": kit
        }

        preamble = """\
/*
 * Compile with:
 *
 * %(cc)s %(cflags)s %(source_filename)s -o %(program_filename)s -L. -l%(library_name)s %(ldflags)s
 *
 * Visit www.frida.re to learn more about Frida.
 */""" % params

        return preamble + "\n\n" + example_code

def asset_path(name):
    return os.path.join(os.path.dirname(__file__), "devkit-assets", name)

def env_rc(frida_root, host, flavor):
    return os.path.join(frida_root, "build", "frida{}-env-{}.rc".format(flavor, host))

def msvs_cl_exe(host):
    return msvs_tool_path(host, "cl.exe")

def msvs_lib_exe(host):
    return msvs_tool_path(host, "lib.exe")

def msvs_tool_path(host, tool):
    if host == "windows-x86_64":
        return os.path.join(winenv.get_msvc_tool_dir(), "bin", "HostX86", "x64", tool)
    else:
        return os.path.join(winenv.get_msvc_tool_dir(), "bin", "HostX86", "x86", tool)

def msvs_runtime_path(host):
    return os.path.join(winenv.get_msvc_tool_dir(), "bin", "HostX86", "x86")

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
    if platform.system() == 'Windows':
        return "{}.lib".format(kit)
    else:
        return "lib{}.a".format(kit)

def compute_umbrella_header_path(frida_root, host, flavor, package, umbrella_header):
    if platform.system() == 'Windows':
        if package == "frida-gum-1.0":
            return os.path.join(frida_root, "frida-gum", "gum", "gum.h")
        elif package == "frida-gumjs-1.0":
            return os.path.join(frida_root, "frida-gum", "bindings", "gumjs", umbrella_header[-1])
        elif package == "frida-core-1.0":
            return os.path.join(frida_root, "build", "tmp-windows", msvs_arch_config(host), "frida-core", "api", "frida-core.h")
        else:
            raise Exception("Unhandled package")
    else:
        return os.path.join(frida_root, "build", "frida" + flavor + "-" + host, "include", *umbrella_header)

def sdk_lib_path(name, frida_root, host):
    return os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "lib", name)

def internal_noarch_lib_path(name, frida_root, host):
    return os.path.join(frida_root, "build", "tmp-windows", msvs_arch_config(host), name, name + ".lib")

def internal_arch_lib_path(name, frida_root, host):
    lib_name = name + msvs_arch_suffix(host)
    return os.path.join(frida_root, "build", "tmp-windows", msvs_arch_config(host), lib_name, lib_name + ".lib")

def probe_env(rc, command):
    return subprocess.check_output([
        "(. \"{rc}\" && PACKAGE_TARNAME=frida-devkit . $CONFIG_SITE && {command})".format(rc=rc, command=command)
    ], shell=True).decode('utf-8').strip()

def trim_flags(cflags, ldflags):
    trimmed_cflags = []
    trimmed_ldflags = []

    pending_cflags = cflags.split(" ")
    while len(pending_cflags) > 0:
        flag = pending_cflags.pop(0)
        if flag == "-include":
            pending_cflags.pop(0)
        else:
            trimmed_cflags.append(flag)

    trimmed_cflags = deduplicate(trimmed_cflags)
    existing_cflags = set(trimmed_cflags)

    pending_ldflags = ldflags.split(" ")
    while len(pending_ldflags) > 0:
        flag = pending_ldflags.pop(0)
        if flag in ("-arch", "-isysroot") and flag in existing_cflags:
            pending_ldflags.pop(0)
        else:
            trimmed_ldflags.append(flag)

    pending_ldflags = trimmed_ldflags
    trimmed_ldflags = []
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
            trimmed_ldflags.append("-Wl," + ",".join(raw_flags))

        if flag is not None and flag not in existing_cflags:
            trimmed_ldflags.append(flag)

    return (" ".join(trimmed_cflags), " ".join(trimmed_ldflags))

def deduplicate(items):
    return list(OrderedDict.fromkeys(items))


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("kit")
    parser.add_argument("host")
    parser.add_argument("outdir")
    parser.add_argument("-t", "--thin", help="build without cross-arch support", action='store_true')

    arguments = parser.parse_args()

    kit = arguments.kit
    host = arguments.host
    outdir = os.path.abspath(arguments.outdir)
    if arguments.thin:
        flavor = "_thin"
    else:
        flavor = ""

    try:
        os.makedirs(outdir)
    except:
        pass

    generate_devkit(kit, host, flavor, outdir)
