#!/usr/bin/env python3

import codecs
import glob
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import v8
import winenv


PLATFORMS = ['x86_64', 'x86']
CONFIGURATIONS = ['Debug', 'Release']
TOOL_TARGET_RUNTIMES = ['static']
LIBRARY_TARGET_RUNTIMES = ['static', 'dynamic']
COMPRESSION_LEVEL = 9

FRIDA_BASE_URL = "https://github.com/frida"
BOOTSTRAP_TOOLCHAIN_URL = "https://build.frida.re/toolchain-20190404-windows-x86.exe"
VALA_VERSION = "0.46"
VALA_TARGET_GLIB = "2.62"


RELENG_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.dirname(RELENG_DIR)
BOOTSTRAP_TOOLCHAIN_DIR = os.path.join(ROOT_DIR, "build", "fts-toolchain-windows")
BOOTSTRAP_VALAC = "valac-0.46.exe"

MESON = os.path.join(RELENG_DIR, "meson", "meson.py")
NINJA = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "ninja.exe")
VALAC_FILENAME = "valac-{}.exe".format(VALA_VERSION)
VALAC_PATTERN = re.compile(r"valac-\d+\.\d+.exe$")
VALA_TOOLCHAIN_VAPI_SUBPATH_PATTERN = re.compile(r"share\\vala-\d+\.\d+\\vapi$")

cached_meson_params = {}

build_platform = 'x86_64' if platform.machine().endswith("64") else 'x86'


def main():
    check_environment()

    started_at = time.time()
    build_ended_at = None
    packaging_ended_at = None
    try:
        for platform in PLATFORMS:
            for configuration in CONFIGURATIONS:
                build_meson_modules(platform, configuration)

        for platform in PLATFORMS:
            for configuration in CONFIGURATIONS:
                for runtime in LIBRARY_TARGET_RUNTIMES:
                    build_v8(platform, configuration, runtime)

        build_ended_at = time.time()

        package()

        packaging_ended_at = time.time()
    finally:
        ended_at = time.time()

        if build_ended_at is not None:
            print("")
            print("*** TIME SPENT")
            print("")
            print("      Total: {}".format(format_duration(ended_at - started_at)))

        if build_ended_at is not None:
            print("      Build: {}".format(format_duration(build_ended_at - started_at)))

        if packaging_ended_at is not None:
            print("  Packaging: {}".format(format_duration(packaging_ended_at - build_ended_at)))

    end = time.time()


def check_environment():
    ensure_bootstrap_toolchain()

    try:
        winenv.get_msvs_installation_dir()
        winenv.get_winxp_sdk()
        winenv.get_win10_sdk()
    except MissingDependencyError as e:
        print("ERROR: {}".format(e), file=sys.stderr)
        sys.exit(1)

    for tool in ["git", "py"]:
        if shutil.which(tool) is None:
            print("ERROR: {} not found".format(tool), file=sys.stderr)
            sys.exit(1)


def build_meson_modules(platform, configuration):
    modules = [
        ("zlib", "zlib.pc", []),
        ("libffi", "libffi.pc", []),
        ("sqlite", "sqlite3.pc", []),
        ("glib", "glib-2.0.pc", ["internal_pcre=true", "tests=false"]),
        ("glib-schannel", "glib-schannel-static.pc", []),
        ("libgee", "gee-0.8.pc", []),
        ("json-glib", "json-glib-1.0.pc", ["introspection=false", "tests=false"]),
        ("libpsl", "libpsl.pc", []),
        ("libxml2", "libxml-2.0.pc", []),
        ("libsoup", "libsoup-2.4.pc", ["gssapi=false", "tls_check=false", "gnome=false", "introspection=false", "vapi=false", "tests=false"]),
        ("vala", VALAC_FILENAME, []),
        ("pkg-config", "pkg-config.exe", []),
    ]
    for (name, artifact_name, options) in modules:
        if artifact_name.endswith(".pc"):
            artifact_subpath = os.path.join("lib", "pkgconfig", artifact_name)
            runtime_flavors = LIBRARY_TARGET_RUNTIMES
        elif artifact_name.endswith(".exe"):
            artifact_subpath = os.path.join("bin", artifact_name)
            runtime_flavors = TOOL_TARGET_RUNTIMES
        else:
            raise NotImplementedError("Unsupported artifact type")
        for runtime in runtime_flavors:
            artifact_path = os.path.join(get_prefix_path(platform, configuration, runtime), artifact_subpath)
            if not os.path.exists(artifact_path):
                build_meson_module(name, platform, configuration, runtime, options)

def build_meson_module(name, platform, configuration, runtime, options):
    print("*** Building name={} platform={} runtime={} configuration={}".format(name, platform, configuration, runtime))
    env_dir, shell_env = get_meson_params(platform, configuration, runtime)

    source_dir = os.path.join(ROOT_DIR, name)
    build_dir = os.path.join(env_dir, name)
    build_type = 'minsize' if configuration == 'Release' else 'debug'
    prefix = get_prefix_path(platform, configuration, runtime)
    option_flags = ["-D" + option for option in options]

    if not os.path.exists(source_dir):
        perform("git", "clone", "--recurse-submodules", make_frida_repo_url(name), cwd=ROOT_DIR)

    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

    perform(
        "py", "-3", MESON,
        build_dir,
        "--buildtype", build_type,
        "--prefix", prefix,
        "--default-library", "static",
        "--backend", "ninja",
        "-Db_vscrt=" + vscrt_from_configuration_and_runtime(configuration, runtime),
        *option_flags,
        cwd=source_dir,
        env=shell_env
    )

    perform(NINJA, "install", cwd=build_dir, env=shell_env)

def get_meson_params(platform, configuration, runtime):
    global cached_meson_params

    identifier = ":".join([platform, configuration, runtime])

    params = cached_meson_params.get(identifier, None)
    if params is None:
        params = generate_meson_params(platform, configuration, runtime)
        cached_meson_params[identifier] = params

    return params

def generate_meson_params(platform, configuration, runtime):
    env = generate_meson_env(platform, configuration, runtime)
    return (env.path, env.shell_env)

def generate_meson_env(platform, configuration, runtime):
    prefix = get_prefix_path(platform, configuration, runtime)
    env_dir = get_tmp_path(platform, configuration, runtime)
    if not os.path.exists(env_dir):
        os.makedirs(env_dir)

    vc_dir = os.path.join(winenv.get_msvs_installation_dir(), "VC")
    vc_install_dir = vc_dir + "\\"

    msvc_platform = platform_to_msvc(platform)
    msvc_dir = winenv.get_msvc_tool_dir()
    msvc_bin_dir = os.path.join(msvc_dir, "bin", "Host" + platform_to_msvc(build_platform), msvc_platform)

    msvc_dll_dirs = []
    if platform != build_platform:
        build_msvc_platform = platform_to_msvc(build_platform)
        msvc_dll_dirs.append(os.path.join(msvc_dir, "bin", "Host" + build_msvc_platform, build_msvc_platform))

    (winxp_sdk_dir, winxp_sdk_version) = winenv.get_winxp_sdk()
    if platform == 'x86':
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
    if platform == 'x86':
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
    with codecs.open(env_path, "w", 'utf-8') as f:
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
    with codecs.open(rc_wrapper_path, "w", 'utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
"{rc_path}" {flags} %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(rc_path=rc_path, flags=clflags))

    with codecs.open(os.path.join(env_dir, "meson.bat"), "w", 'utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
py -3 "{meson_path}" %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(meson_path=MESON))

    pkgconfig_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "pkg-config.exe")
    pkgconfig_lib_dir = os.path.join(prefix, "lib", "pkgconfig")
    pkgconfig_wrapper_path = os.path.join(env_dir, "pkg-config.bat")
    with codecs.open(pkgconfig_wrapper_path, "w", 'utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
SET PKG_CONFIG_PATH={pkgconfig_lib_dir}
"{pkgconfig_path}" --static %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(pkgconfig_path=pkgconfig_path, pkgconfig_lib_dir=pkgconfig_lib_dir))

    flex_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "flex.exe")
    flex_wrapper_path = os.path.join(env_dir, "flex.py")
    with codecs.open(os.path.join(env_dir, "flex.bat"), "w", 'utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
py -3 "{wrapper_path}" %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(wrapper_path=flex_wrapper_path))
    with codecs.open(flex_wrapper_path, "w", 'utf-8') as f:
        f.write("""import subprocess
import sys

args = [arg.replace("/", "\\\\") for arg in sys.argv[1:]]
sys.exit(subprocess.call([r"{flex_path}"] + args))
""".format(flex_path=flex_path))

    bison_path = os.path.join(BOOTSTRAP_TOOLCHAIN_DIR, "bin", "bison.exe")
    bison_wrapper_path = os.path.join(env_dir, "bison.py")
    with codecs.open(os.path.join(env_dir, "bison.bat"), "w", 'utf-8') as f:
        f.write("""@ECHO OFF
SETLOCAL EnableExtensions
SET _res=0
py -3 "{wrapper_path}" %* || SET _res=1
ENDLOCAL & SET _res=%_res%
EXIT /B %_res%""".format(wrapper_path=bison_wrapper_path))
    with codecs.open(bison_wrapper_path, "w", 'utf-8') as f:
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


class MesonEnv(object):
    def __init__(self, path, shell_env):
        self.path = path
        self.shell_env = shell_env


def build_v8(platform, configuration, runtime):
    prefix = get_prefix_path(platform, configuration, runtime)

    lib_dir = os.path.join(prefix, "lib")
    pkgconfig_dir = os.path.join(lib_dir, "pkgconfig")
    if len(glob.glob(os.path.join(pkgconfig_dir, "v8-*.pc"))) > 0:
        return

    checkout_dir = os.path.join(ROOT_DIR, "v8-checkout")

    depot_dir = os.path.join(checkout_dir, "depot_tools")
    if not os.path.exists(depot_dir):
        perform("git", "clone", "--recurse-submodules", "https://chromium.googlesource.com/chromium/tools/depot_tools.git",
            r"v8-checkout\depot_tools", cwd=ROOT_DIR)

    gclient = os.path.join(depot_dir, "gclient.bat")
    gn = os.path.join(depot_dir, "gn.bat")

    env = {}
    env.update(os.environ)
    env["PATH"] = depot_dir + ";" + env["PATH"]
    env["DEPOT_TOOLS_WIN_TOOLCHAIN"] = "0"

    config_path = os.path.join(checkout_dir, ".gclient")
    if not os.path.exists(config_path):
        spec = """solutions = [ {{ "url": "{url}", "managed": False, "name": "v8", "deps_file": "DEPS", "custom_deps": {{}}, }}, ]""" \
            .format(url=make_frida_repo_url("v8"))
        perform(gclient, "config", "--spec", spec, cwd=checkout_dir, env=env)

    source_dir = os.path.join(checkout_dir, "v8")
    if not os.path.exists(source_dir):
        perform(gclient, "sync", cwd=checkout_dir, env=env)

    build_dir = os.path.join(get_tmp_path(platform, configuration, runtime), "v8")
    if not os.path.exists(os.path.join(build_dir, "build.ninja")):
        if os.path.exists(build_dir):
            shutil.rmtree(build_dir)

        if configuration == 'Release':
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
            "target_cpu=\"{}\"".format(platform_to_msvc(platform)),
        ] + configuration_args + [
            "use_crt=\"{}\"".format(runtime),
            "is_clang=false",
            "visual_studio_path=\"{}\"".format(winenv.get_msvs_installation_dir()),
            "visual_studio_version=\"{}\"".format(winenv.get_msvs_version()),
            "wdk_path=\"{}\"".format(win10_sdk_dir),
            "windows_sdk_path=\"{}\"".format(win10_sdk_dir),
            "symbol_level=0",
            "use_thin_lto=false",
            "v8_monolithic=true",
            "v8_use_external_startup_data=false",
            "is_component_build=false",
            "v8_enable_debugging_features=false",
            "v8_enable_disassembler=false",
            "v8_enable_gdbjit=false",
            "v8_enable_i18n_support=false",
            "v8_untrusted_code_mitigations=false",
            "treat_warnings_as_errors=false",
            "strip_absolute_paths_from_debug_symbols=true",
            "use_goma=false",
            "v8_embedder_string=\"-frida\"",
        ])

        perform(gn, "gen", os.path.relpath(build_dir, start=source_dir), "--args=" + args, cwd=source_dir, env=env)

    monolith_path = os.path.join(build_dir, "obj", "v8_monolith.lib")
    perform(NINJA, "v8_monolith", cwd=build_dir, env=env)

    version, api_version = v8.detect_version(source_dir)

    include_dir = os.path.join(prefix, "include", "v8-" + api_version, "v8")
    for header_dir in [os.path.join(source_dir, "include"), os.path.join(build_dir, "gen", "include")]:
        header_files = [os.path.relpath(path, header_dir) for path in glob.glob(os.path.join(header_dir, "**", "*.h"), recursive=True)]
        copy_files(header_dir, header_files, include_dir)

    if not os.path.exists(pkgconfig_dir):
        os.makedirs(pkgconfig_dir)

    libv8_path = os.path.join(lib_dir, "libv8-{}.a".format(api_version))
    shutil.copyfile(monolith_path, libv8_path)

    with codecs.open(os.path.join(pkgconfig_dir, "v8-{}.pc".format(api_version)), "w", 'utf-8') as f:
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

    prefixes_dir = os.path.join(ROOT_DIR, "build", "fts-windows")
    prefixes_skip_len = len(prefixes_dir) + 1

    sdk_built_files = []
    for prefix in glob.glob(os.path.join(prefixes_dir, "*-static")):
        for root, dirs, files in os.walk(prefix):
            relpath = root[prefixes_skip_len:]
            included_files = map(lambda name: os.path.join(relpath, name),
                filter(lambda filename: file_is_sdk_related(relpath, filename), files))
            sdk_built_files.extend(included_files)
        dynamic_libs = glob.glob(os.path.join(prefix[:-7] + "-dynamic", "lib", "**", "*.a"), recursive=True)
        dynamic_libs = [path[prefixes_skip_len:] for path in dynamic_libs]
        sdk_built_files.extend(dynamic_libs)

    toolchain_files = []
    for root, dirs, files in os.walk(get_prefix_path('x86', 'Release', 'static')):
        relpath = root[prefixes_skip_len:]
        included_files = map(lambda name: os.path.join(relpath, name),
            filter(lambda filename: file_is_vala_toolchain_related(relpath, filename) or filename == "pkg-config.exe", files))
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

def file_is_sdk_related(directory, filename):
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

def file_is_vala_toolchain_related(directory, filename):
    base, ext = os.path.splitext(filename)
    ext = ext[1:]
    if ext in ('vapi', 'deps'):
        return is_vala_toolchain_vapi_directory(directory)
    return VALAC_PATTERN.match(filename) is not None

def is_vala_toolchain_vapi_directory(directory):
    return VALA_TOOLCHAIN_VAPI_SUBPATH_PATTERN.search(directory) is not None

def transform_identity(srcfile):
    return srcfile

def transform_sdk_dest(srcfile):
    parts = os.path.dirname(srcfile).split("\\")
    rootdir = parts[0]
    subpath = "\\".join(parts[1:])

    filename = os.path.basename(srcfile)

    platform, configuration, runtime = rootdir.split("-")
    rootdir = "-".join([
        platform_to_msvs(platform),
        configuration.title()
    ])

    if runtime == 'dynamic' and subpath.split("\\")[0] == "lib":
        subpath = "lib-dynamic" + subpath[3:]

    return os.path.join(rootdir, subpath, filename)

def transform_toolchain_dest(srcfile):
    return srcfile[srcfile.index("\\") + 1:]


def ensure_bootstrap_toolchain():
    if os.path.exists(BOOTSTRAP_TOOLCHAIN_DIR):
        return

    print("Downloading bootstrap toolchain...")
    with urllib.request.urlopen(BOOTSTRAP_TOOLCHAIN_URL) as response, \
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
        finally:
            shutil.rmtree(tempdir)
    finally:
        os.unlink(toolchain_archive_path)

def get_prefix_path(platform, configuration, runtime):
    return os.path.join(ROOT_DIR, "build", "fts-windows", "{}-{}-{}".format(platform, configuration.lower(), runtime))

def get_tmp_path(platform, configuration, runtime):
    return os.path.join(ROOT_DIR, "build", "fts-tmp-windows", "{}-{}-{}".format(platform, configuration.lower(), runtime))

def make_frida_repo_url(name):
    return "{}/{}.git".format(FRIDA_BASE_URL, name)

def platform_to_msvs(platform):
    return 'x64' if platform == 'x86_64' else 'Win32'

def platform_to_msvc(platform):
    return 'x64' if platform == 'x86_64' else 'x86'

def vscrt_from_configuration_and_runtime(configuration, runtime):
    result = "md" if runtime == 'dynamic' else "mt"
    if configuration == 'Debug':
        result += "d"
    return result



def perform(*args, **kwargs):
    print(" ".join(args))
    subprocess.run(args, check=True, **kwargs)

def copy_files(fromdir, files, todir, transformdest=transform_identity):
    for file in files:
        src = os.path.join(fromdir, file)
        dst = os.path.join(todir, transformdest(file))
        dstdir = os.path.dirname(dst)
        if not os.path.isdir(dstdir):
            os.makedirs(dstdir)
        shutil.copyfile(src, dst)

def format_duration(duration_in_seconds):
    hours, remainder = divmod(duration_in_seconds, 3600)
    minutes, seconds = divmod(remainder, 60)
    return "{:02d}:{:02d}:{:02d}".format(int(hours), int(minutes), int(seconds))


class MissingDependencyError(Exception):
    pass


if __name__ == '__main__':
    main()
