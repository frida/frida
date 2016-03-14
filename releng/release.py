#!/usr/bin/env python

if __name__ == '__main__':
    import glob
    import os
    import platform
    import subprocess

    system = platform.system()

    build_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    if system == 'Darwin':
        build_os = 'mac-x86_64'
    else:
        build_os = system.lower()
    toolchain_dir = os.path.join(build_dir, "build", "toolchain-" + build_os)
    frida_core_dir = os.path.join(build_dir, "frida-core")
    frida_python_dir = os.path.join(build_dir, "frida-python")
    frida_node_dir = os.path.join(build_dir, "frida-node")

    if system == 'Windows':
        ssh = r"C:\Program Files (x86)\PuTTY\plink.exe"
        scp = r"C:\Program Files (x86)\PuTTY\pscp.exe"
    else:
        ssh = "ssh"
        scp = "scp"

    raw_version = subprocess.check_output(["git", "describe", "--tags", "--always", "--long"], cwd=build_dir).strip().replace("-", ".")
    (major, minor, micro, nano, commit) = raw_version.split(".")
    version = "%d.%d.%d" % (int(major), int(minor), int(micro))

    def upload_to_pypi(interpreter, extension, extra_env = {}, sdist = False):
        env = {}
        env.update(os.environ)
        env.update({
            'FRIDA_VERSION': version,
            'FRIDA_EXTENSION': extension
        })
        env.update(extra_env)

        targets = []
        if sdist:
            targets.append("sdist")
        targets.extend(["bdist_egg", "upload"])

        subprocess.call([interpreter, "setup.py"] + targets, cwd=os.path.join(frida_python_dir, "src"), env=env)

    def upload_to_npm(node, publish):
        node_bin_dir = os.path.dirname(node)
        npm = os.path.join(node_bin_dir, "npm")
        if system == 'Windows':
            npm += '.cmd'
        env = dict(os.environ)
        env.update({
            'PATH': node_bin_dir + os.pathsep + os.getenv('PATH'),
            'FRIDA': build_dir
        })
        def do(args):
            exit_code = subprocess.call(args, cwd=frida_node_dir, env=env)
            if exit_code != 0:
                raise RuntimeError("Failed to run: " + " ".join(args))
        def reset():
            tags = [tag.strip() for tag in subprocess.check_output(["git", "tag", "-l"], cwd=frida_node_dir, env=env).split("\n") if len(tag.strip()) > 0]
            for tag in tags:
                do(["git", "tag", "-d", tag])
            do(["git", "reset", "--hard", "origin/master"])
            do(["git", "clean", "-xffd"])
        reset()
        do([npm, "version", version])
        if publish:
            do([npm, "publish"])
        do([npm, "install"])
        do([npm, "run", "prebuild", "--", "-t", "4.0.0", "-t", "5.0.0"])
        packages = glob.glob(os.path.join(frida_node_dir, "prebuilds", "*.tar.gz"))
        for package in packages:
            remote_path = "node/v" + version
            do([ssh, "buildmaster@build.frida.re", "mkdir -p /home/buildmaster/public_html/" + remote_path])
            do([scp, package, "buildmaster@build.frida.re:/home/buildmaster/public_html/" + remote_path])
        reset()

    def upload_ios_deb(server):
        env = {
            'FRIDA_VERSION': version,
            'FRIDA_TOOLCHAIN': toolchain_dir
        }
        env.update(os.environ)
        deb = os.path.join(build_dir, "frida_%s_iphoneos-arm.deb" % version)
        subprocess.call([os.path.join(frida_core_dir, "tools", "package-server.sh"), server, deb], env=env)
        subprocess.call([scp, deb, "buildmaster@build.frida.re:/home/buildmaster/public_html/debs/"])
        subprocess.call([ssh, "buildmaster@build.frida.re", "/home/buildmaster/cydia/sync-repo"])
        subprocess.call([scp, "buildmaster@build.frida.re:/home/buildmaster/public_html/Release", "Release"])
        subprocess.call(["gpg", "-abs", "-o", "Release.gpg", "Release"])
        subprocess.call([scp, "Release.gpg", "buildmaster@build.frida.re:/home/buildmaster/public_html/Release.gpg"])
        os.unlink("Release")
        os.unlink("Release.gpg")
        os.unlink(deb)

    if int(nano) == 0:
        if system == 'Windows':
            upload_to_pypi(r"C:\Program Files (x86)\Python27\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
            upload_to_pypi(r"C:\Program Files\Python27\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "x64-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
            upload_to_pypi(r"C:\Program Files (x86)\Python 3.5\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "lib", "python3.5", "site-packages", "_frida.pyd"))
            upload_to_pypi(r"C:\Program Files\Python 3.5\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "x64-Release", "lib", "python3.5", "site-packages", "_frida.pyd"), sdist=True)
            upload_to_npm(r"C:\Program Files (x86)\nodejs\node.exe", publish=False)
            upload_to_npm(r"C:\Program Files\nodejs\node.exe", publish=True)
        elif system == 'Darwin':
            upload_to_pypi("/usr/bin/python2.6",
                os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python2.6", "site-packages", "_frida.so"))
            for osx_minor in xrange(7, 12):
                upload_to_pypi("/usr/bin/python2.7",
                    os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python2.7", "site-packages", "_frida.so"),
                    { '_PYTHON_HOST_PLATFORM': "macosx-10.%d-intel" % osx_minor })
            upload_to_pypi("/usr/local/bin/python3.5",
                os.path.join(build_dir, "build", "frida-mac-universal", "lib", "python3.5", "site-packages", "_frida.so"))
            upload_to_npm("/opt/node-64/bin/node", publish=False)
            upload_ios_deb(os.path.join(build_dir, "build", "frida-ios-universal", "bin", "frida-server"))
        elif system == 'Linux':
            upload_to_pypi("/opt/python27-32/bin/python2.7",
                os.path.join(build_dir, "build", "frida_stripped-linux-i386", "lib", "python2.7", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python27-32/lib", '_PYTHON_HOST_PLATFORM': "linux-i686" })
            upload_to_pypi("/opt/python27-64/bin/python2.7",
                os.path.join(build_dir, "build", "frida_stripped-linux-x86_64", "lib", "python2.7", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python27-64/lib", '_PYTHON_HOST_PLATFORM': "linux-x86_64" })
            upload_to_pypi("/opt/python35-32/bin/python3.5",
                os.path.join(build_dir, "build", "frida_stripped-linux-i386", "lib", "python3.5", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python35-32/lib", '_PYTHON_HOST_PLATFORM': "linux-i686" })
            upload_to_pypi("/opt/python35-64/bin/python3.5",
                os.path.join(build_dir, "build", "frida_stripped-linux-x86_64", "lib", "python3.5", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python35-64/lib", '_PYTHON_HOST_PLATFORM': "linux-x86_64" })
            upload_to_npm("/opt/node-32/bin/node", publish=False)
            upload_to_npm("/opt/node-64/bin/node", publish=False)
