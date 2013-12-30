#!/usr/bin/env python

if __name__ == '__main__':
    import os
    import platform
    import subprocess

    build_dir = os.path.dirname(os.path.realpath(__file__))
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

    if int(nano) == 0:
        system = platform.system()
        if system == 'Windows':
            upload_to_pypi(r"C:\Program Files (x86)\Python27\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
            upload_to_pypi(r"C:\Program Files\Python27\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "x64-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
        elif system == 'Darwin':
            upload_to_pypi("python2.6",
                os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python2.6", "site-packages", "_frida.so"))
            upload_to_pypi("python2.7",
                os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python2.7", "site-packages", "_frida.so"))
        elif system == 'Linux':
            upload_to_pypi("python2.7",
                os.path.join(build_dir, "build", "frida-linux-x86_64-stripped", "lib", "python2.7", "site-packages", "_frida.so"))
