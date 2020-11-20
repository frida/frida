#!/usr/bin/env python3

from dataclasses import dataclass
from enum import Enum
from glob import glob
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Callable, Dict, List, Tuple
import urllib.request

from deps import read_dependency_parameters, DependencyParameters, PackageSpec
import v8
import winenv


class PackageType(Enum):
    TOOL = 1,
    LIBRARY = 2,


class SourceState(Enum):
    PRISTINE = 1,
    MODIFIED = 2,


EnvDir = str
ShellEnv = Dict[str, str]


@dataclass
class MesonEnv:
    path: str
    shell_env: ShellEnv



class MissingDependencyError(Exception):
    pass


ARCHITECTURES = {
    PackageType.TOOL: ['x86'],
    PackageType.LIBRARY: ['x86_64', 'x86'],
}
CONFIGURATIONS = {
    PackageType.TOOL: ['Release'],
    PackageType.LIBRARY: ['Debug', 'Release'],
}
RUNTIMES = {
    PackageType.TOOL: ['static'],
    PackageType.LIBRARY: ['static', 'dynamic'],
}
COMPRESSION_LEVEL = 9

BOOTSTRAP_TOOLCHAIN_URL = "https://build.frida.re/toolchain-{version}-windows-x86.exe"
VALA_TARGET_GLIB = "2.66"


RELENG_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.dirname(RELENG_DIR)
DEPS_DIR = os.path.join(ROOT_DIR, "deps")
BOOTSTRAP_TOOLCHAIN_DIR = os.path.join(ROOT_DIR, "build", "fts-toolchain-windows")
BOOTSTRAP_VALAC = "valac-0.50.exe"

MESON = os.path.join(RELENG_DIR, "meson", "meson.py")
NINJA = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "ninja.exe")
VALAC_PATTERN = re.compile(r"valac-\d+\.\d+.exe$")
VALA_TOOLCHAIN_VAPI_SUBPATH_PATTERN = re.compile(r"share\\vala-\d+\.\d+\\vapi$")


PACKAGES = [
    ("zlib", "zlib.pc"),
    ("sqlite", "sqlite3.pc"),
    ("libffi", "libffi.pc"),
    ("glib", "glib-2.0.pc"),
    ("glib-schannel", "gioschannel.pc"),
    ("libgee", "gee-0.8.pc"),
    ("json-glib", "json-glib-1.0.pc"),
    ("libpsl", "libpsl.pc"),
    ("libxml2", "libxml-2.0.pc"),
    ("libsoup", "libsoup-2.4.pc"),
    ("capstone", "capstone.pc"),
    ("quickjs", "quickjs.pc"),
    ("tinycc", "libtcc.pc"),
    ("vala", "valac*.exe"),
    ("pkg-config", "pkg-config.exe"),
    ("v8", "v8*.pc"),
]

HOST_DEFINES = {
    "capstone_archs": "x86",
}


cached_meson_params = {}

build_arch = 'x86_64' if platform.machine().endswith("64") else 'x86'


def main():
    check_environment()

    params = read_dependency_parameters(HOST_DEFINES)

    started_at = time.time()
    sync_ended_at = None
    build_ended_at = None
    packaging_ended_at = None
    try:
        synchronize(params)
        sync_ended_at = time.time()

        for name, artifact_name in PACKAGES:
            build(name, artifact_name, params.get_package_spec(name))
        build_ended_at = time.time()

        package()
        packaging_ended_at = time.time()
    finally:
        ended_at = time.time()

        if sync_ended_at is not None:
            print("")
            print("*** TIME SPENT")
            print("")
            print("      Total: {}".format(format_duration(ended_at - started_at)))

        if sync_ended_at is not None:
            print("       Sync: {}".format(format_duration(sync_ended_at - started_at)))

        if build_ended_at is not None:
            print("      Build: {}".format(format_duration(build_ended_at - sync_ended_at)))

        if packaging_ended_at is not None:
            print("  Packaging: {}".format(format_duration(packaging_ended_at - build_ended_at)))


def check_environment():
    try:
        winenv.get_msvs_installation_dir()
        winenv.get_winxp_sdk()
        winenv.get_win10_sdk()
    except MissingDependencyError as e:
        print("ERROR: {}".format(e), file=sys.stderr)
        sys.exit(1)

    for tool in ["7z", "git", "py"]:
        if shutil.which(tool) is None:
            print("ERROR: {} not found".format(tool), file=sys.stderr)
            sys.exit(1)


def synchronize(params: DependencyParameters):
    toolchain_state = ensure_bootstrap_toolchain(params.bootstrap_version)
    if toolchain_state == SourceState.MODIFIED:
        wipe_build_state()
    for name, _ in PACKAGES:
        pkg_state = grab_and_prepare(name, params.get_package_spec(name), params)
        if pkg_state == SourceState.MODIFIED:
            wipe_build_state()

def grab_and_prepare(name: str, spec: PackageSpec, params: DependencyParameters) -> SourceState:
    if spec.recipe != 'custom':
        return grab_and_prepare_regular_package(name, spec)

    assert name == 'v8'
    return grab_and_prepare_v8_package(spec, params.get_package_spec("depot_tools"))

def grab_and_prepare_regular_package(name: str, spec: PackageSpec) -> SourceState:
    assert spec.hash == ""
    assert spec.patches == []

    source_dir = os.path.join(DEPS_DIR, name)
    if os.path.exists(source_dir):
        if query_git_head(source_dir) == spec.version:
            source_state = SourceState.PRISTINE
        else:
            print("{name}: synchronizing".format(name=name))
            perform("git", "fetch", "-q", cwd=source_dir)
            perform("git", "checkout", "-q", spec.version, cwd=source_dir)
            source_state = SourceState.MODIFIED
    else:
        print("{name}: cloning into deps\\{name}".format(name=name))
        if not os.path.exists(DEPS_DIR):
            os.makedirs(DEPS_DIR)
        perform("git", "clone", "-q", "--recurse-submodules", spec.url, name, cwd=DEPS_DIR)
        perform("git", "checkout", "-q", spec.version, cwd=source_dir)
        source_state = SourceState.PRISTINE

    return source_state

def grab_and_prepare_v8_package(v8_spec: PackageSpec, depot_spec: PackageSpec) -> SourceState:
    assert v8_spec.hash == ""
    assert v8_spec.patches == []
    assert v8_spec.deps == []
    assert v8_spec.deps_for_build == []

    assert depot_spec.deps == []
    assert depot_spec.deps_for_build == []
    grab_and_prepare_regular_package("depot_tools", depot_spec)
    depot_dir = os.path.join(DEPS_DIR, "depot_tools")
    gclient = os.path.join(depot_dir, "gclient.bat")
    env = make_v8_env(depot_dir)

    checkout_dir = os.path.join(DEPS_DIR, "v8-checkout")

    source_dir = os.path.join(checkout_dir, "v8")
    source_exists = os.path.exists(source_dir)
    if source_exists and query_git_head(source_dir) == v8_spec.version:
        return SourceState.PRISTINE

    if source_exists:
        print("v8: synchronizing")
        source_state = SourceState.MODIFIED
    else:
        print("v8: cloning into deps\\v8-checkout")
        source_state = SourceState.PRISTINE

    spec = """solutions = [ {{ "url": "{url}@{version}", "managed": False, "name": "v8", "deps_file": "DEPS", "custom_deps": {{}}, }}, ]""" \
        .format(url=v8_spec.url, version=v8_spec.version)
    perform(gclient, "config", "--spec", spec, cwd=checkout_dir, env=env)

    perform(gclient, "sync", cwd=checkout_dir, env=env)

    return source_state


def wipe_build_state():
    print("*** Wiping build state")
    locations = [
        ("existing packages", get_prefix_root()),
        ("build directories", get_tmp_root()),
    ]
    for description, path in locations:
        if os.path.exists(path):
            print("Wiping", description)
            shutil.rmtree(path)


def build(name: str, artifact_name: str, spec: PackageSpec):
    if artifact_name.endswith(".exe"):
        artifact_subpath = os.path.join("bin", artifact_name)
        pkg_type = PackageType.TOOL
    elif artifact_name.endswith(".pc"):
        artifact_subpath = os.path.join("lib", "pkgconfig", artifact_name)
        pkg_type = PackageType.LIBRARY
    else:
        raise NotImplementedError("unsupported artifact type")

    archs = ARCHITECTURES[pkg_type]
    configs = CONFIGURATIONS[pkg_type]
    runtimes = RUNTIMES[pkg_type]

    for arch in archs:
        for config in configs:
            for runtime in runtimes:
                existing_artifacts = glob(os.path.join(get_prefix_path(arch, config, runtime), artifact_subpath))
                if len(existing_artifacts) == 0:
                    if spec.recipe == 'meson':
                        build_using_meson(name, arch, config, runtime, spec)
                    else:
                        assert name == 'v8'
                        assert spec.recipe == 'custom'
                        build_v8(arch, config, runtime, spec)

def build_using_meson(name: str, arch: str, config: str, runtime: str, spec: PackageSpec):
    print("*** Building name={} arch={} runtime={} config={} spec={}".format(name, arch, config, runtime, spec))
    env_dir, shell_env = get_meson_params(arch, config, runtime)

    source_dir = os.path.join(DEPS_DIR, name)
    build_dir = os.path.join(env_dir, name)
    prefix = get_prefix_path(arch, config, runtime)
    optimization = 's' if config == 'Release' else '0'
    ndebug = 'true' if config == 'Release' else 'false'

    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

    perform(
        "py", "-3", MESON,
        build_dir,
        "--prefix", prefix,
        "--default-library", "static",
        "--backend", "ninja",
        "-Doptimization=" + optimization,
        "-Db_ndebug=" + ndebug,
        "-Db_vscrt=" + vscrt_from_configuration_and_runtime(config, runtime),
        *spec.options,
        cwd=source_dir,
        env=shell_env
    )

    perform(NINJA, "install", cwd=build_dir, env=shell_env)

def get_meson_params(arch: str, config: str, runtime: str) -> Tuple[EnvDir, ShellEnv]:
    global cached_meson_params

    identifier = ":".join([arch, config, runtime])

    params = cached_meson_params.get(identifier, None)
    if params is None:
        params = generate_meson_params(arch, config, runtime)
        cached_meson_params[identifier] = params

    return params

def generate_meson_params(arch: str, config: str, runtime: str) -> Tuple[EnvDir, ShellEnv]:
    env = generate_meson_env(arch, config, runtime)
    return (env.path, env.shell_env)

def generate_meson_env(arch: str, config: str, runtime: str) -> MesonEnv:
    prefix = get_prefix_path(arch, config, runtime)
    env_dir = get_tmp_path(arch, config, runtime)
    if not os.path.exists(env_dir):
        os.makedirs(env_dir)

    vc_dir = os.path.join(winenv.get_msvs_installation_dir(), "VC")
    vc_install_dir = vc_dir + "\\"

    msvc_platform = msvc_platform_from_arch(arch)
    msvc_dir = winenv.get_msvc_tool_dir()
    msvc_bin_dir = os.path.join(msvc_dir, "bin", "Host" + msvc_platform_from_arch(build_arch), msvc_platform)

    msvc_dll_dirs = []
    if arch != build_arch:
        build_msvc_platform = msvc_platform_from_arch(build_arch)
        msvc_dll_dirs.append(os.path.join(msvc_dir, "bin", "Host" + build_msvc_platform, build_msvc_platform))

    (winxp_sdk_dir, winxp_sdk_version) = winenv.get_winxp_sdk()
    if arch == 'x86':
        winxp_bin_dir = os.path.join(winxp_sdk_dir, "Bin")
        winxp_lib_dir = os.path.join(winxp_sdk_dir, "Lib")
    else:
        winxp_bin_dir = os.path.join(winxp_sdk_dir, "Bin", msvc_platform)
        winxp_lib_dir = os.path.join(winxp_sdk_dir, "Lib", msvc_platform)

    clflags = "/D" + " /D".join([
      "_USING_V110_SDK71_",
      "_UNICODE",
      "UNICODE",
    ])

    platform_cflags = []
    if arch == 'x86':
        platform_cflags += ["/arch:SSE2"]

    cflags = " ".join(platform_cflags)

    cxxflags = " ".join(platform_cflags + [
        # Relax C++11 compliance for XP compatibility.
        "/Zc:threadSafeInit-",
    ])

    (win10_sdk_dir, win10_sdk_version) = winenv.get_win10_sdk()

    m4_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "m4.exe")
    bison_pkgdatadir = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "share", "bison")

    vala_flags = "--target-glib=" + VALA_TARGET_GLIB

    exe_path = ";".join([
        os.path.join(prefix, "bin"),
        env_dir,
        os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin"),
        winxp_bin_dir,
        msvc_bin_dir,
    ] + msvc_dll_dirs)

    include_path = ";".join([
        os.path.join(msvc_dir, "include"),
        os.path.join(msvc_dir, "atlmfc", "include"),
        os.path.join(vc_dir, "Auxiliary", "VS", "include"),
        os.path.join(win10_sdk_dir, "Include", win10_sdk_version, "ucrt"),
        os.path.join(winxp_sdk_dir, "Include"),
    ])

    library_path = ";".join([
        os.path.join(msvc_dir, "lib", msvc_platform),
        os.path.join(msvc_dir, "atlmfc", "lib", msvc_platform),
        os.path.join(vc_dir, "Auxiliary", "VS", "lib", msvc_platform),
        os.path.join(win10_sdk_dir, "Lib", win10_sdk_version, "ucrt", msvc_platform),
        winxp_lib_dir,
    ])

    env_path = os.path.join(env_dir, "env.bat")
    with open(env_path, "w", encoding='utf-8') as f:
        f.write("""@ECHO OFF
set PATH={exe_path};%PATH%
set INCLUDE={include_path}
set LIB={library_path}
set CL={clflags}
set CFLAGS={cflags}
set CXXFLAGS={cxxflags}
set VCINSTALLDIR={vc_install_dir}
set Platform={platform}
set VALA={valac}
set VALAFLAGS={vala_flags}
set DEPOT_TOOLS_WIN_TOOLCHAIN=0
""".format(
            exe_path=exe_path,
            include_path=include_path,
            library_path=library_path,
            clflags=clflags,
            cflags=cflags,
            cxxflags=cxxflags,
            vc_install_dir=vc_install_dir,
            platform=msvc_platform,
            valac=BOOTSTRAP_VALAC,
            vala_flags=vala_flags
        ))

    rc_path = os.path.join(winxp_bin_dir, "rc.exe")
    rc_wrapper_path = os.path.join(env_dir, "rc.bat")
    with open(rc_wrapper_path, "w", encoding='utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
"{rc_path}" {flags} %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(rc_path=rc_path, flags=clflags))

    with open(os.path.join(env_dir, "meson.bat"), "w", encoding='utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
py -3 "{meson_path}" %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(meson_path=MESON))

    pkgconfig_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "pkg-config.exe")
    pkgconfig_lib_dir = os.path.join(prefix, "lib", "pkgconfig")
    pkgconfig_wrapper_path = os.path.join(env_dir, "pkg-config.bat")
    with open(pkgconfig_wrapper_path, "w", encoding='utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
SET PKG_CONFIG_PATH={pkgconfig_lib_dir}
"{pkgconfig_path}" --static %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(pkgconfig_path=pkgconfig_path, pkgconfig_lib_dir=pkgconfig_lib_dir))

    flex_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "flex.exe")
    flex_wrapper_path = os.path.join(env_dir, "flex.py")
    with open(os.path.join(env_dir, "flex.bat"), "w", encoding='utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
py -3 "{wrapper_path}" %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(wrapper_path=flex_wrapper_path))
    with open(flex_wrapper_path, "w", encoding='utf-8') as f:
        f.write("""import subprocess
import sys

args = [arg.replace("/", "\\\\") for arg in sys.argv[1:]]
sys.exit(subprocess.call([r"{flex_path}"] + args))
""".format(flex_path=flex_path))

    bison_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "bison.exe")
    bison_wrapper_path = os.path.join(env_dir, "bison.py")
    with open(os.path.join(env_dir, "bison.bat"), "w", encoding='utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
py -3 "{wrapper_path}" %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(wrapper_path=bison_wrapper_path))
    with open(bison_wrapper_path, "w", encoding='utf-8') as f:
        f.write("""\
import os
import subprocess
import sys

os.environ["BISON_PKGDATADIR"] = r"{bison_pkgdatadir}"
os.environ["M4"] = r"{m4_path}"

args = [arg.replace("/", "\\\\") for arg in sys.argv[1:]]
sys.exit(subprocess.call([r"{bison_path}"] + args))
""".format(
        bison_path=bison_path,
        bison_pkgdatadir=bison_pkgdatadir,
        m4_path=m4_path
    ))

    shell_env = {}
    shell_env.update(os.environ)
    shell_env["PATH"] = exe_path + ";" + shell_env["PATH"]
    shell_env["INCLUDE"] = include_path
    shell_env["LIB"] = library_path
    shell_env["CL"] = clflags
    shell_env["CFLAGS"] = cflags
    shell_env["CXXFLAGS"] = cxxflags
    shell_env["VCINSTALLDIR"] = vc_install_dir
    shell_env["Platform"] = msvc_platform
    shell_env["VALAC"] = BOOTSTRAP_VALAC
    shell_env["VALAFLAGS"] = vala_flags

    return MesonEnv(env_dir, shell_env)


def build_v8(arch: str, config: str, runtime: str, spec: PackageSpec):
    depot_dir = os.path.join(DEPS_DIR, "depot_tools")
    gn = os.path.join(depot_dir, "gn.bat")
    env = make_v8_env(depot_dir)

    source_dir = os.path.join(DEPS_DIR, "v8-checkout", "v8")

    build_dir = os.path.join(get_tmp_path(arch, config, runtime), "v8")
    if not os.path.exists(os.path.join(build_dir, "build.ninja")):
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)

        if config == 'Release':
            configuration_args = [
                "is_official_build=true",
                "is_debug=false",
                "v8_enable_v8_checks=false",
            ]
        else:
            configuration_args = [
                "is_debug=true",
                "v8_enable_v8_checks=true",
            ]

        (win10_sdk_dir, win10_sdk_version) = winenv.get_win10_sdk()

        args = " ".join([
            "target_cpu=\"{}\"".format(msvc_platform_from_arch(arch)),
        ] + configuration_args + [
            "use_crt=\"{}\"".format(runtime),
            "is_clang=false",
            "visual_studio_path=\"{}\"".format(winenv.get_msvs_installation_dir()),
            "visual_studio_version=\"{}\"".format(winenv.get_msvs_version()),
            "wdk_path=\"{}\"".format(win10_sdk_dir),
            "windows_sdk_path=\"{}\"".format(win10_sdk_dir),
            "symbol_level=0",
            "strip_absolute_paths_from_debug_symbols=true",
        ] + spec.options)

        perform(gn, "gen", os.path.relpath(build_dir, start=source_dir), "--args=" + args, cwd=source_dir, env=env)

    monolith_path = os.path.join(build_dir, "obj", "v8_monolith.lib")
    perform(NINJA, "v8_monolith", cwd=build_dir, env=env)

    version, api_version = v8.detect_version(source_dir)

    prefix = get_prefix_path(arch, config, runtime)

    include_dir = os.path.join(prefix, "include", "v8-" + api_version, "v8")
    for header_dir in [os.path.join(source_dir, "include"), os.path.join(build_dir, "gen", "include")]:
        header_files = [os.path.relpath(path, header_dir) for path in glob(os.path.join(header_dir, "**", "*.h"), recursive=True)]
        copy_files(header_dir, header_files, include_dir)

    v8.patch_config_header(os.path.join(include_dir, "v8config.h"), source_dir, build_dir, gn, env)

    lib_dir = os.path.join(prefix, "lib")

    pkgconfig_dir = os.path.join(lib_dir, "pkgconfig")
    if not os.path.exists(pkgconfig_dir):
        os.makedirs(pkgconfig_dir)

    libv8_path = os.path.join(lib_dir, "libv8-{}.a".format(api_version))
    shutil.copyfile(monolith_path, libv8_path)

    with open(os.path.join(pkgconfig_dir, "v8-{}.pc".format(api_version)), "w", encoding='utf-8') as f:
        f.write("""\
prefix={prefix}
libdir=${{prefix}}/lib
includedir=${{prefix}}/include/v8-{api_version}

Name: V8
Description: V8 JavaScript Engine
Version: {version}
Libs: -L${{libdir}} -lv8-{api_version}
Libs.private: {libs_private}
Cflags: -I${{includedir}} -I${{includedir}}/v8""".format(
            prefix=prefix.replace("\\", "/"),
            version=version,
            api_version=api_version,
            libs_private="-lshlwapi -lwinmm"
        ))

def make_v8_env(depot_dir: str) -> ShellEnv:
    env = {}
    env.update(os.environ)
    env["PATH"] = depot_dir + ";" + env["PATH"]
    env["DEPOT_TOOLS_WIN_TOOLCHAIN"] = "0"
    return env


def package():
    toolchain_filename = "toolchain-windows-x86.exe"
    toolchain_path = os.path.join(ROOT_DIR, "build", toolchain_filename)

    sdk_filename = "sdk-windows-any.exe"
    sdk_path = os.path.join(ROOT_DIR, "build", sdk_filename)

    print("About to assemble:")
    print("\t* " + toolchain_filename)
    print("\t* " + sdk_filename)
    print()
    print("Determining what to include...")

    prefixes_dir = get_prefix_root()
    prefixes_skip_len = len(prefixes_dir) + 1

    sdk_built_files = []
    for prefix in glob(os.path.join(prefixes_dir, "*-static")):
        for root, dirs, files in os.walk(prefix):
            relpath = root[prefixes_skip_len:]
            included_files = map(lambda name: os.path.join(relpath, name),
                filter(lambda filename: file_is_sdk_related(relpath, filename), files))
            sdk_built_files.extend(included_files)
        dynamic_libs = glob(os.path.join(prefix[:-7] + "-dynamic", "lib", "**", "*.a"), recursive=True)
        dynamic_libs = [path[prefixes_skip_len:] for path in dynamic_libs]
        sdk_built_files.extend(dynamic_libs)

    toolchain_files = []
    for root, dirs, files in os.walk(get_prefix_path('x86', 'Release', 'static')):
        relpath = root[prefixes_skip_len:]
        included_files = map(lambda name: os.path.join(relpath, name),
            filter(lambda filename: file_is_vala_toolchain_related(relpath, filename) or filename in ("pkg-config.exe", "glib-genmarshal", "glib-mkenums"), files))
        toolchain_files.extend(included_files)

    toolchain_mixin_files = []
    for root, dirs, files in os.walk(BOOTSTRAP_TOOLCHAIN_DIR):
        relpath = root[len(BOOTSTRAP_TOOLCHAIN_DIR) + 1:]
        included_files = map(lambda name: os.path.join(relpath, name),
            filter(lambda filename: not file_is_vala_toolchain_related(relpath, filename), files))
        toolchain_mixin_files.extend(included_files)

    sdk_built_files.sort()
    toolchain_files.sort()

    print("Copying files...")
    tempdir = tempfile.mkdtemp(prefix="frida-package")

    copy_files(prefixes_dir, sdk_built_files, os.path.join(tempdir, "sdk-windows"), transform_sdk_dest)

    toolchain_tempdir = os.path.join(tempdir, "toolchain-windows")
    copy_files(BOOTSTRAP_TOOLCHAIN_DIR, toolchain_mixin_files, toolchain_tempdir)
    copy_files(prefixes_dir, toolchain_files, toolchain_tempdir, transform_toolchain_dest)

    print("Compressing...")
    prevdir = os.getcwd()
    os.chdir(tempdir)

    compression_switch = "-mx{}".format(COMPRESSION_LEVEL)

    perform("7z", "a", compression_switch, "-sfx7zCon.sfx", "-r", toolchain_path, "toolchain-windows")

    perform("7z", "a", compression_switch, "-sfx7zCon.sfx", "-r", sdk_path, "sdk-windows")

    os.chdir(prevdir)
    shutil.rmtree(tempdir)

    print("All done.")

def file_is_sdk_related(directory: str, filename: str):
    parts = directory.split("\\")
    rootdir = parts[0]
    subdir = parts[1]
    subpath = "\\".join(parts[1:])

    if subdir == "bin":
        return False

    if subdir == "lib" and ("vala" in subpath or "vala" in filename or "vapigen" in filename):
        return False

    base, ext = os.path.splitext(filename)
    ext = ext[1:]

    if ext == "h" and base.startswith("vala"):
        return False

    if ext in ("vapi", "deps"):
        return not is_vala_toolchain_vapi_directory(directory)

    return "\\share\\" not in directory

def file_is_vala_toolchain_related(directory: str, filename: str) -> bool:
    base, ext = os.path.splitext(filename)
    ext = ext[1:]
    if ext in ('vapi', 'deps'):
        return is_vala_toolchain_vapi_directory(directory)
    return VALAC_PATTERN.match(filename) is not None

def is_vala_toolchain_vapi_directory(directory: str) -> bool:
    return VALA_TOOLCHAIN_VAPI_SUBPATH_PATTERN.search(directory) is not None

def transform_identity(srcfile: str) -> str:
    return srcfile

def transform_sdk_dest(srcfile: str) -> str:
    parts = os.path.dirname(srcfile).split("\\")
    rootdir = parts[0]
    subpath = "\\".join(parts[1:])

    filename = os.path.basename(srcfile)

    arch, config, runtime = rootdir.split("-")
    rootdir = "-".join([
        msvs_platform_from_arch(arch),
        config.title()
    ])

    if runtime == 'dynamic' and subpath.split("\\")[0] == "lib":
        subpath = "lib-dynamic" + subpath[3:]

    return os.path.join(rootdir, subpath, filename)

def transform_toolchain_dest(srcfile: str) -> str:
    return srcfile[srcfile.index("\\") + 1:]


def ensure_bootstrap_toolchain(bootstrap_version: str) -> SourceState:
    version_stamp_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "VERSION.txt")
    if os.path.exists(BOOTSTRAP_TOOLCHAIN_DIR):
        try:
            with open(version_stamp_path, "r", encoding='utf-8') as f:
                version = f.read().strip()
            if version == bootstrap_version:
                return SourceState.PRISTINE
        except:
            pass
        shutil.rmtree(BOOTSTRAP_TOOLCHAIN_DIR)

        source_state = SourceState.MODIFIED
    else:
        source_state = SourceState.PRISTINE

    print("Downloading bootstrap toolchain...")
    with urllib.request.urlopen(BOOTSTRAP_TOOLCHAIN_URL.format(version=bootstrap_version)) as response, \
            tempfile.NamedTemporaryFile(suffix=".exe", delete=False) as archive:
        shutil.copyfileobj(response, archive)
        toolchain_archive_path = archive.name

    print("Extracting bootstrap toolchain...")
    try:
        tempdir = tempfile.mkdtemp(prefix="frida-bootstrap-toolchain")
        try:
            try:
                subprocess.check_output([
                    toolchain_archive_path,
                    "-o" + tempdir,
                    "-y"
                ])
            except subprocess.CalledProcessError as e:
                print("Oops:", e.output.decode('utf-8'))
                raise e
            shutil.move(os.path.join(tempdir, "toolchain-windows"), BOOTSTRAP_TOOLCHAIN_DIR)
            with open(version_stamp_path, "w", encoding='utf-8') as f:
                f.write(bootstrap_version)
        finally:
            shutil.rmtree(tempdir)
    finally:
        os.unlink(toolchain_archive_path)

    return source_state

def get_prefix_root() -> str:
    return os.path.join(ROOT_DIR, "build", "fts-windows")

def get_prefix_path(arch: str, config: str, runtime: str) -> str:
    return os.path.join(get_prefix_root(), "{}-{}-{}".format(arch, config.lower(), runtime))

def get_tmp_root() -> str:
    return os.path.join(ROOT_DIR, "build", "fts-tmp-windows")

def get_tmp_path(arch: str, config: str, runtime: str) -> str:
    return os.path.join(get_tmp_root(), "{}-{}-{}".format(arch, config.lower(), runtime))

def msvs_platform_from_arch(arch: str) -> str:
    return 'x64' if arch == 'x86_64' else 'Win32'

def msvc_platform_from_arch(arch: str) -> str:
    return 'x64' if arch == 'x86_64' else 'x86'

def vscrt_from_configuration_and_runtime(config: str, runtime: str) -> str:
    result = "md" if runtime == 'dynamic' else "mt"
    if config == 'Debug':
        result += "d"
    return result


def perform(*args, **kwargs):
    print(" ".join(args))
    subprocess.run(args, check=True, **kwargs)

def query_git_head(repo_path: str) -> str:
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo_path, encoding='utf-8').strip()

def copy_files(fromdir: str, files: List[str], todir: str, transformdest: Callable[[str], str] = transform_identity):
    for file in files:
        src = os.path.join(fromdir, file)
        dst = os.path.join(todir, transformdest(file))
        dstdir = os.path.dirname(dst)
        if not os.path.isdir(dstdir):
            os.makedirs(dstdir)
        shutil.copyfile(src, dst)

def format_duration(duration_in_seconds: float) -> str:
    hours, remainder = divmod(duration_in_seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    return "{:02d}:{:02d}:{:02d}".format(int(hours), int(minutes), int(seconds))


if __name__ == '__main__':
    main()
