import glob
import json
import os
import subprocess
import winreg


RELENG_DIR = os.path.abspath(os.path.dirname(__file__))
ROOT_DIR = os.path.dirname(RELENG_DIR)
DEFAULT_TOOLCHAIN_DIR = os.path.join(ROOT_DIR, "build", "toolchain-windows")
BOOTSTRAP_TOOLCHAIN_DIR = os.path.join(ROOT_DIR, "build", "fts-toolchain-windows")

cached_msvs_dir = None
cached_msvc_dir = None
cached_winsdk = None


def get_msvs_installation_dir():
    global cached_msvs_dir
    if cached_msvs_dir is None:
        if os.path.exists(DEFAULT_TOOLCHAIN_DIR):
            toolchain_dir = DEFAULT_TOOLCHAIN_DIR
        else:
            toolchain_dir = BOOTSTRAP_TOOLCHAIN_DIR
        installations = json.loads(subprocess.check_output([
            os.path.join(toolchain_dir, "bin", "vswhere.exe"),
            "-version", "17.0",
            "-format", "json",
            "-property", "installationPath"
        ]))
        if len(installations) == 0:
            raise MissingDependencyError("Visual Studio 2022 is not installed")
        cached_msvs_dir = installations[0]['installationPath'].rstrip("\\")
    return cached_msvs_dir

def get_msvc_tool_dir():
    global cached_msvc_dir
    if cached_msvc_dir is None:
        msvs_dir = get_msvs_installation_dir()
        version = sorted(glob.glob(os.path.join(msvs_dir, "VC", "Tools", "MSVC", "*.*.*")))[-1]
        cached_msvc_dir = os.path.join(msvs_dir, "VC", "Tools", "MSVC", version)
    return cached_msvc_dir

def get_windows_sdk():
    global cached_winsdk
    if cached_winsdk is None:
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Kits\Installed Roots")
            try:
                (install_dir, _) = winreg.QueryValueEx(key, "KitsRoot10")
                version = os.path.basename(sorted(glob.glob(os.path.join(install_dir, "Include", "*.*.*")))[-1])
                cached_winsdk = (install_dir.rstrip("\\"), version)
            finally:
                winreg.CloseKey(key)
        except Exception as e:
            raise MissingDependencyError("Windows 10 SDK is not installed")
    return cached_winsdk

def msvs_platform_from_arch(arch: str) -> str:
    return 'x64' if arch == 'x86_64' else 'Win32'

def msvc_platform_from_arch(arch: str) -> str:
    return 'x64' if arch == 'x86_64' else 'x86'


class MissingDependencyError(Exception):
    pass
