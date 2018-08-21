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
cached_winxpsdk = None
cached_win10sdk = None


def get_msvs_installation_dir():
    global cached_msvs_dir
    if cached_msvs_dir is None:
        if os.path.exists(DEFAULT_TOOLCHAIN_DIR):
            toolchain_dir = DEFAULT_TOOLCHAIN_DIR
        else:
            toolchain_dir = BOOTSTRAP_TOOLCHAIN_DIR
        installations = json.loads(subprocess.check_output([
            os.path.join(toolchain_dir, "bin", "vswhere.exe"),
            "-version", "15.0",
            "-format", "json",
            "-property", "installationPath"
        ]))
        if len(installations) == 0:
            raise MissingDependencyError("Visual Studio 2017 is not installed")
        cached_msvs_dir = installations[0]['installationPath'].rstrip("\\")
    return cached_msvs_dir

def get_msvs_version():
    return "2017"

def get_msvc_tool_dir():
    global cached_msvc_dir
    if cached_msvc_dir is None:
        msvs_dir = get_msvs_installation_dir()
        version = sorted(glob.glob(os.path.join(msvs_dir, "VC", "Tools", "MSVC", "*.*.*")))[-1]
        cached_msvc_dir = os.path.join(msvs_dir, "VC", "Tools", "MSVC", version)
    return cached_msvc_dir

def get_winxp_sdk():
    global cached_winxpsdk
    if cached_winxpsdk is None:
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Microsoft SDKs\Windows\v7.1A")
            try:
                (install_dir, _) = winreg.QueryValueEx(key, "InstallationFolder")
                (version, _) = winreg.QueryValueEx(key, "ProductVersion")
                cached_winxpsdk = (install_dir.rstrip("\\"), version)
            finally:
                winreg.CloseKey(key)
        except Exception as e:
            raise MissingDependencyError("Windows XP SDK is not installed")
    return cached_winxpsdk

def get_win10_sdk():
    global cached_win10sdk
    if cached_win10sdk is None:
        try:
            key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Kits\Installed Roots")
            try:
                (install_dir, _) = winreg.QueryValueEx(key, "KitsRoot10")
                version = os.path.basename(sorted(glob.glob(os.path.join(install_dir, "Include", "*.*.*")))[-1])
                cached_win10sdk = (install_dir.rstrip("\\"), version)
            finally:
                winreg.CloseKey(key)
        except Exception as e:
            raise MissingDependencyError("Windows 10 SDK is not installed")
    return cached_win10sdk
