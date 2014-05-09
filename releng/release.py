#!/usr/bin/env python

if __name__ == '__main__':
    import os
    import platform
    import subprocess

    system = platform.system()

    build_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    if system == 'Darwin':
        build_os = 'mac'
    else:
        build_os = system.lower()
    toolchain_dir = os.path.join(build_dir, "build", "toolchain-" + build_os)
    frida_core_dir = os.path.join(build_dir, "frida-core")
    frida_python_dir = os.path.join(build_dir, "frida-python")

    raw_version = subprocess.check_output(["git", "describe", "--tags", "--always", "--long"], cwd=build_dir).strip().replace("-", ".")
    (major, minor, micro, nano, commit) = raw_version.split(".")
    version = "%d.%d.%d" % (int(major), int(minor), int(micro))

    def upload_to_pypi(interpreter, extension):
        env = {
            'FRIDA_VERSION': version,
            'FRIDA_EXTENSION': extension
        }
        env.update(os.environ)
        subprocess.call([interpreter, "setup.py", "bdist_egg", "upload"], cwd=os.path.join(frida_python_dir, "src"), env=env)

    def upload_ios_deb(server):
        env = {
            'FRIDA_VERSION': version,
            'FRIDA_TOOLCHAIN': toolchain_dir
        }
        env.update(os.environ)
        deb = os.path.join(build_dir, "frida_%s_iphoneos-arm.deb" % version)
        subprocess.call([os.path.join(frida_core_dir, "tools", "package-server.sh"), server, deb], env=env)
        subprocess.call(["scp", deb, "buildmaster@build.frida.re:/home/buildmaster/public_html/debs/"])
        subprocess.call(["ssh", "buildmaster@build.frida.re", "/home/buildmaster/cydia/sync-repo"])
        os.unlink(deb)

    if int(nano) == 0:
        if system == 'Windows':
            upload_to_pypi(r"C:\Program Files (x86)\Python27\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
            upload_to_pypi(r"C:\Program Files\Python27\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "x64-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
            upload_to_pypi(r"C:\Program Files (x86)\Python33\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "lib", "python3.3", "site-packages", "_frida.pyd"))
            upload_to_pypi(r"C:\Program Files\Python33\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "x64-Release", "lib", "python3.3", "site-packages", "_frida.pyd"))
        elif system == 'Darwin':
            upload_to_pypi("python2.6",
                os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python2.6", "site-packages", "_frida.so"))
            upload_to_pypi("python2.7",
                os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python2.7", "site-packages", "_frida.so"))
            upload_to_pypi("python3.3",
                os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python3.3", "site-packages", "_frida.so"))
            upload_ios_deb(os.path.join(build_dir, "build", "frida-ios-arm", "bin", "frida-server"))
        elif system == 'Linux':
            upload_to_pypi("python2.7",
                os.path.join(build_dir, "build", "frida-linux-x86_64-stripped", "lib", "python2.7", "site-packages", "_frida.so"))
            upload_to_pypi("python3.3",
                os.path.join(build_dir, "build", "frida-linux-x86_64-stripped", "lib", "python3.3", "site-packages", "_frida.so"))
