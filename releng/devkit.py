#!/usr/bin/env python

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

INCLUDE_PATTERN = re.compile("#include\s+[<\"](.*?)[>\"]")

DEVKITS = {
    "frida-gum": ("frida-gum-1.0", ("frida-1.0", "gum", "gum.h")),
    "frida-gumjs": ("frida-gumjs-1.0", ("frida-1.0", "gumjs", "gumscriptbackend.h")),
    "frida-core": ("frida-core-1.0", ("frida-1.0", "frida-core.h")),
}

# TODO: auto-detect these:
MSVS_DIR = r"C:\Program Files (x86)\Microsoft Visual Studio\2017\Enterprise"
WINDOWS_SDK_DIR = r"C:\Program Files (x86)\Windows Kits\10"

def generate_devkit(kit, host, output_dir):
    package, umbrella_header = DEVKITS[kit]

    frida_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

    umbrella_header_path = compute_umbrella_header_path(frida_root, host, package, umbrella_header)

    header_filename = kit + ".h"
    if not os.path.exists(umbrella_header_path):
        raise Exception("Header not found: {}".format(umbrella_header_path))
    header = generate_header(package, frida_root, host, kit, umbrella_header_path)
    with open(os.path.join(output_dir, header_filename), "w") as f:
        f.write(header)

    library_filename = compute_library_filename(kit)
    extra_ldflags = generate_library(package, frida_root, host, output_dir, library_filename)

    example_filename = kit + "-example.c"
    example = generate_example(example_filename, package, frida_root, host, kit, extra_ldflags)
    with open(os.path.join(output_dir, example_filename), "w") as f:
        f.write(example)

    if platform.system() == 'Windows':
        for msvs_asset in glob(asset_path("{}-*.sln".format(kit))) + glob(asset_path("{}-*.vcxproj*".format(kit))):
            shutil.copy(msvs_asset, output_dir)

    return [header_filename, library_filename, example_filename]

def generate_header(package, frida_root, host, kit, umbrella_header_path):
    if platform.system() == 'Windows':
        include_dirs = [
            MSVS_DIR + r"\VC\Tools\MSVC\14.10.25017\include",
            WINDOWS_SDK_DIR + r"\Include\10.0.14393.0\ucrt",
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "lib", "glib-2.0", "include"),
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "include", "glib-2.0"),
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "include", "glib-2.0"),
            os.path.join(frida_root, "build", "sdk-windows", msvs_arch_config(host), "include", "json-glib-1.0"),
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
        rc = env_rc(frida_root, host)
        header_dependencies = subprocess.check_output(
            ["(. \"{rc}\" && $CPP $CFLAGS -M $($PKG_CONFIG --cflags {package}) \"{header}\")".format(rc=rc, package=package, header=umbrella_header_path)],
            shell=True).decode('utf-8')
        header_lines = header_dependencies.strip().split("\n")[1:]
        header_files = [line.rstrip("\\").strip() for line in header_lines]
        header_files = [header_file for header_file in header_files if header_file.startswith(frida_root)]

    devkit_header_lines = []
    umbrella_header = header_files[0]
    processed_header_files = set([umbrella_header])
    ingest_header(umbrella_header, header_files, processed_header_files, devkit_header_lines)
    devkit_header = "".join(devkit_header_lines)

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
            deps.append("shlwapi")
        deps.sort()

        frida_pragmas = "#pragma comment(lib, \"{}\")".format(compute_library_filename(kit))
        dep_pragmas = "\n".join(["#pragma comment(lib, \"{}.lib\")".format(dep) for dep in deps])

        config += frida_pragmas + "\n\n" + dep_pragmas + "\n\n"

    return config + devkit_header

def ingest_header(header, all_header_files, processed_header_files, result):
    with open(header, "r") as f:
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

def generate_library(package, frida_root, host, output_dir, library_filename):
    if platform.system() == 'Windows':
        return generate_library_windows(package, frida_root, host, output_dir, library_filename)
    else:
        return generate_library_unix(package, frida_root, host, output_dir, library_filename)

def generate_library_windows(package, frida_root, host, output_dir, library_filename):
    glib = [
        sdk_lib_path("glib-2.0.lib", frida_root, host),
        sdk_lib_path("intl.lib", frida_root, host),
    ]
    gobject = glib + [
        sdk_lib_path("gobject-2.0.lib", frida_root, host),
        sdk_lib_path("ffi.lib", frida_root, host),
    ]
    gmodule = glib + [
        sdk_lib_path("gmodule-2.0.lib", frida_root, host),
    ]
    gio = glib + gobject + gmodule + [
        sdk_lib_path("gio-2.0.lib", frida_root, host),
        sdk_lib_path("z.lib", frida_root, host),
    ]

    json_glib = glib + gobject + [
        sdk_lib_path("json-glib-1.0.lib", frida_root, host),
    ]

    gee = glib + gobject + [
        sdk_lib_path("gee-0.8.lib", frida_root, host),
    ]

    v8 = [
        sdk_lib_path("v8_base_0.lib", frida_root, host),
        sdk_lib_path("v8_base_1.lib", frida_root, host),
        sdk_lib_path("v8_base_2.lib", frida_root, host),
        sdk_lib_path("v8_base_3.lib", frida_root, host),
        sdk_lib_path("v8_libbase.lib", frida_root, host),
        sdk_lib_path("v8_libplatform.lib", frida_root, host),
        sdk_lib_path("v8_libsampler.lib", frida_root, host),
        sdk_lib_path("v8_snapshot.lib", frida_root, host),
    ]

    gum_lib = internal_arch_lib_path("gum", frida_root, host)
    gum_deps = deduplicate(glib + gobject + gio)
    gumjs_deps = deduplicate([gum_lib] + gum_deps + json_glib + v8)
    frida_core_deps = deduplicate(glib + gobject + gio + json_glib + gmodule + gee)

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
    input_pdbs = [os.path.splitext(input_lib)[0] + ".pdb" for input_lib in input_libs]
    input_pdbs = [input_pdb for input_pdb in input_pdbs if os.path.exists(input_pdb)]

    subprocess.check_output(
        [msvs_lib_exe(host), "/nologo", "/out:" + os.path.join(output_dir, library_filename)] + input_libs,
        cwd=msvs_runtime_path(host),
        shell=False)

    for pdb in input_pdbs:
        shutil.copy(pdb, output_dir)

    extra_flags = [os.path.basename(lib_path) for lib_path in input_libs]

    return extra_flags

def generate_library_unix(package, frida_root, host, output_dir, library_filename):
    output_path = os.path.join(output_dir, library_filename)

    try:
        os.unlink(output_path)
    except:
        pass

    rc = env_rc(frida_root, host)
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

    return extra_flags

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

def generate_example(filename, package, frida_root, host, kit, extra_ldflags):
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
        rc = env_rc(frida_root, host)

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

def env_rc(frida_root, host):
    return os.path.join(frida_root, "build", "frida-env-{}.rc".format(host))

def msvs_cl_exe(host):
    return msvs_tool_path(host, "cl.exe")

def msvs_lib_exe(host):
    return msvs_tool_path(host, "lib.exe")

def msvs_tool_path(host, tool):
    if host == "windows-x86_64":
        return MSVS_DIR + r"\VC\Tools\MSVC\14.10.25017\bin\HostX86\x64\{0}".format(tool)
    else:
        return MSVS_DIR + r"\VC\Tools\MSVC\14.10.25017\bin\HostX86\x86\{0}".format(tool)

def msvs_runtime_path(host):
    return MSVS_DIR + r"\VC\Tools\MSVC\14.10.25017\bin\HostX86\x86"

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

def compute_umbrella_header_path(frida_root, host, package, umbrella_header):
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
        return os.path.join(frida_root, "build", "frida-" + host, "include", *umbrella_header)

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
    if len(sys.argv) != 4:
        print("Usage: {0} kit host outdir".format(sys.argv[0]), file=sys.stderr)
        sys.exit(1)

    kit = sys.argv[1]
    host = sys.argv[2]
    outdir = os.path.abspath(sys.argv[3])

    try:
        os.makedirs(outdir)
    except:
        pass

    generate_devkit(kit, host, outdir)
