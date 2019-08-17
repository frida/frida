#!/usr/bin/env python3
from __future__ import print_function

if __name__ == '__main__':
    from contextlib import contextmanager
    from devkit import generate_devkit
    from distutils.spawn import find_executable
    import codecs
    import glob
    import os
    import platform
    import re
    import shutil
    import subprocess
    import sys
    import tempfile

    system = platform.system()
    worker = sys.argv[1]

    build_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    if system == 'Darwin':
        build_os = 'macos-x86_64'
    else:
        build_os = system.lower()
    toolchain_dir = os.path.join(build_dir, "build", "toolchain-" + build_os)
    frida_core_dir = os.path.join(build_dir, "frida-core")
    frida_python_dir = os.path.join(build_dir, "frida-python")
    frida_node_dir = os.path.join(build_dir, "frida-node")
    frida_tools_dir = os.path.join(build_dir, "frida-tools")

    if system == 'Windows':
        szip = r"C:\Program Files\7-Zip\7z.exe"
        ssh = r"C:\Program Files (x86)\PuTTY\plink.exe"
        scp = r"C:\Program Files (x86)\PuTTY\pscp.exe"
    else:
        szip = "7z"
        ssh = "ssh"
        scp = "scp"

    raw_version = subprocess.check_output(["git", "describe", "--tags", "--always", "--long"], cwd=build_dir).decode('utf-8').strip().replace("-", ".")
    (major, minor, micro, nano, commit) = raw_version.split(".")
    version = "%d.%d.%d" % (int(major), int(minor), int(micro))
    tag_name = str(version)

    def upload_python_bindings_to_pypi(interpreter, extension, extra_env = {}, sdist = False):
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

        subprocess.call([interpreter, "setup.py"] + targets, cwd=frida_python_dir, env=env)

    def upload_python_debs(distro_name, package_name_prefix, interpreter, extension, upload):
        env = {}
        env.update(os.environ)
        env.update({
            'FRIDA_VERSION': version,
            'FRIDA_EXTENSION': extension
        })

        for module_dir in [frida_python_dir, frida_tools_dir]:
            subprocess.check_call([
                "fpm",
                "--iteration=1." + distro_name,
                "--maintainer=Ole André Vadla Ravnås <oleavr@frida.re>",
                "--vendor=Frida",
                "--category=Libraries",
                "--python-bin=" + interpreter,
                "--python-package-name-prefix=" + package_name_prefix,
                "--python-install-bin=/usr/bin",
                "--python-install-lib=/usr/lib/{}/dist-packages".format(os.path.basename(interpreter)),
                "-s", "python",
                "-t", "deb",
                "setup.py"
            ], cwd=module_dir, env=env)

            packages = glob.glob(os.path.join(module_dir, "*.deb"))
            try:
                for package in packages:
                    with open(package, "rb") as f:
                        upload(os.path.basename(package), "application/x-deb", f)
            finally:
                for package in packages:
                    os.unlink(package)

    def upload_python_rpms(distro_name, package_name_prefix, interpreter, extension, upload):
        env = {}
        env.update(os.environ)
        env.update({
            'FRIDA_VERSION': version,
            'FRIDA_EXTENSION': extension
        })

        subprocess.check_call([
            "fpm",
            "--name={}-prompt-toolkit".format(package_name_prefix),
            "--version=1.0.15",
            "--iteration=1." + distro_name,
            "--maintainer=Ole André Vadla Ravnås <oleavr@frida.re>",
            "--vendor=Frida",
            "--python-bin=" + interpreter,
            "--python-package-name-prefix=" + package_name_prefix,
            "-s", "python",
            "-t", "rpm",
            "prompt-toolkit"
        ], cwd=frida_python_dir)

        for module_dir in [frida_python_dir, frida_tools_dir]:
            subprocess.check_call([
                "fpm",
                "--iteration=1." + distro_name,
                "--maintainer=Ole André Vadla Ravnås <oleavr@frida.re>",
                "--vendor=Frida",
                "--python-bin=" + interpreter,
                "--python-package-name-prefix=" + package_name_prefix,
                "-s", "python",
                "-t", "rpm",
                "setup.py"
            ], cwd=module_dir, env=env)

            packages = glob.glob(os.path.join(module_dir, "*.rpm"))
            try:
                for package in packages:
                    with open(package, "rb") as f:
                        upload(os.path.basename(package), "application/x-rpm", f)
            finally:
                for package in packages:
                    os.unlink(package)

    def upload_node_bindings_to_npm(node, upload_to_github, publish, python2_interpreter=None, extra_build_args=[], extra_build_env=None):
        node_bin_dir = os.path.dirname(node)
        npm = os.path.join(node_bin_dir, "npm")
        if system == 'Windows':
            npm += '.cmd'

        env = dict(os.environ)
        env.update({
            'PATH': node_bin_dir + os.pathsep + os.getenv('PATH'),
            'FRIDA': build_dir
        })
        if python2_interpreter is not None:
            env['PYTHON'] = python2_interpreter

        def do(args, **kwargs):
            quoted_args = []
            for arg in args:
                if " " in arg:
                    # Assumes none of our arguments contain quotes
                    quoted_args.append('"{}"'.format(arg))
                else:
                    quoted_args.append(arg)
            command = " ".join(quoted_args)
            exit_code = subprocess.call(command, cwd=frida_node_dir, env=env, shell=True, **kwargs)
            if exit_code != 0:
                raise RuntimeError("Failed to run: " + command)
        def do_build_command(args):
            env_args = [". " + extra_build_env, "&&"] if extra_build_env is not None else []
            do(env_args + args + extra_build_args)
        def reset():
            do(["git", "clean", "-xffd"])
        reset()
        with package_version_temporarily_set_to(version, frida_node_dir):
            do_build_command([npm, "install"])
            if publish:
                do([npm, "publish"])
            do_build_command([npm, "run", "prebuild", "--", "-t", "8.0.0", "-t", "10.0.0", "-t", "12.0.0"])
            do_build_command([npm, "run", "prebuild", "--", "-t", "5.0.0", "-r", "electron"])
            packages = glob.glob(os.path.join(frida_node_dir, "prebuilds", "*.tar.gz"))
            for package in packages:
                with open(package, 'rb') as package_file:
                    upload_to_github(os.path.basename(package), "application/gzip", package_file.read())
        reset()

    def upload_meta_modules_to_npm(node):
        for module in ["frida-gadget-ios"]:
            upload_meta_module_to_npm(node, module)

    def upload_meta_module_to_npm(node, module_name):
        module_dir = os.path.join(build_dir, "releng", "modules", module_name)
        with package_version_temporarily_set_to(version, module_dir):
            subprocess.check_call(["npm", "publish"], cwd=module_dir)

    @contextmanager
    def package_version_temporarily_set_to(version, module_dir):
        package_json_path = os.path.join(module_dir, "package.json")

        with codecs.open(package_json_path, "rb", 'utf-8') as f:
            package_json_original = f.read()

        package_json_versioned = re.sub(r'"version": "(.+)",', r'"version": "{}",'.format(version), package_json_original)
        with codecs.open(package_json_path, "wb", 'utf-8') as f:
            f.write(package_json_versioned)

        try:
            yield
        finally:
            with codecs.open(package_json_path, "wb", 'utf-8') as f:
                f.write(package_json_original)

    def upload_ios_deb(name, server):
        env = {
            'FRIDA_VERSION': version,
            'FRIDA_TOOLCHAIN': toolchain_dir
        }
        env.update(os.environ)
        deb = os.path.join(build_dir, "{}_{}_iphoneos-arm.deb".format(name, version))
        subprocess.call([os.path.join(frida_core_dir, "tools", "package-server.sh"), server, deb], env=env)
        subprocess.call([scp, deb, "frida@build.frida.re:/home/frida/public_html/debs/"])
        subprocess.call([ssh, "frida@build.frida.re", "cd /home/frida/public_html" +
            " && reprepro -Vb . --confdir /home/frida/.reprepo --ignore=forbiddenchar includedeb stable debs/" + os.path.basename(deb) +
            " && cp dists/stable/main/binary-iphoneos-arm/Packages.gz ."])
        os.unlink(deb)

    def upload_ios_debug_symbols():
        unstripped_ios_binaries = [
            "build/tmp-ios-arm64/frida-core/server/frida-server",
            "build/tmp-ios-arm64/frida-core/lib/agent/frida-agent.dylib",
        ]

        output_dir = tempfile.mkdtemp(prefix="frida-symbols")
        try:
            for binary_path in unstripped_ios_binaries:
                dwarf_name = os.path.basename(binary_path) + ".dwarf"
                dwarf_path = os.path.join(output_dir, dwarf_name)

                subprocess.check_call(["dsymutil", "-f", "--minimize", "-o", dwarf_path, binary_path], cwd=build_dir)

                load_commands = subprocess.check_output(["otool", "-l", dwarf_path]).decode('utf-8')
                uuid = [line.split(" ")[-1] for line in load_commands.split("\n") if "uuid " in line][0]

                remote_dwarf_path = "/home/frida/public_html/symbols/ios/" + uuid + ".dwarf"
                subprocess.check_call([scp, dwarf_path, "frida@build.frida.re:" + remote_dwarf_path])
        finally:
            shutil.rmtree(output_dir)

    def get_github_uploader():
        from agithub.GitHub import GitHub
        import requests

        with open(os.path.expanduser("~/.frida-release-github-token"), "r") as f:
            token = f.read().strip()

        g = GitHub(token=token)
        def repo():
            return g.repos.frida.frida

        status, data = repo().releases.tags[tag_name].get()
        if status != 200:
            if status == 404:
                status, data = repo().releases.post(body={
                    'tag_name': tag_name,
                    'name': "Frida {}".format(version),
                    'body': "See http://www.frida.re/news/ for details.",
                })
            else:
                raise RuntimeError("Unexpected error trying to get current release; status={} data={}".format(status, data))

        upload_url = data['upload_url']
        upload_url = upload_url[:upload_url.index("{")]

        def upload(name, mimetype, data):
            try:
                r = requests.post(
                    url=upload_url,
                    params={
                        "name": name,
                    },
                    headers={
                        "Authorization": "Token {}".format(token),
                        "Content-Type": mimetype,
                    },
                    data=data)
                r.raise_for_status()
                print("Uploaded", name)
            except Exception as e:
                print("Skipping {}: {}".format(name, e))

        return upload

    def upload_file(name_template, path, upload, compression='xz'):
        if compression == 'xz':
            if system == 'Windows':
                asset_filename = (name_template + ".xz").format(version=version)
                data = subprocess.check_output([szip, "a", "-txz", "-so", asset_filename, path])
            else:
                asset_filename = (name_template + ".xz").format(version=version)
                data = subprocess.check_output(["xz", "-z", "-c", "-T", "0", path])
            upload(asset_filename, "application/x-xz", data)
        else:
            assert compression == 'gz'
            assert system != 'Windows'
            asset_filename = (name_template + ".gz").format(version=version)
            data = subprocess.check_output(["gzip", "-c", path])
            upload(asset_filename, "application/gzip", data)

    def upload_directory(name_template, path, upload):
        tarball_filename = (name_template + ".tar").format(version=version)
        asset_filename = tarball_filename + ".xz"

        output_dir = tempfile.mkdtemp(prefix="frida-release")
        try:
            dist_dir = os.path.join(output_dir, "dist")
            shutil.copytree(path, dist_dir)
            subprocess.check_call(["tar", "cf", "../" + tarball_filename, "."], cwd=dist_dir)
            subprocess.check_call(["xz", "-T", "0", tarball_filename], cwd=output_dir)
            with open(os.path.join(output_dir, asset_filename), 'rb') as f:
                tarball = f.read()
        finally:
            shutil.rmtree(output_dir)

        upload(asset_filename, "application/x-xz", tarball)

    def upload_devkits(host, upload, flavor=""):
        kits = [
            "frida-gum",
            "frida-gumjs",
            "frida-core",
        ]

        for kit in kits:
            if host.startswith("windows-"):
                asset_filename = "{}-devkit-{}-{}.exe".format(kit, version, host)
                asset_mimetype = "application/octet-stream"
            else:
                tarball_filename = "{}-devkit-{}-{}.tar".format(kit, version, host)
                asset_filename = tarball_filename + ".xz"
                asset_mimetype = "application/x-xz"

            output_dir = tempfile.mkdtemp(prefix="frida-release")
            try:
                try:
                    filenames = generate_devkit(kit, host, flavor, output_dir)
                except Exception as e:
                    print("Skipping {}: {}".format(asset_filename, e))
                    continue
                if host.startswith("windows-"):
                    subprocess.check_call([szip, "a", "-sfx7zCon.sfx", "-r", asset_filename, "."], cwd=output_dir)
                else:
                    subprocess.check_call(["tar", "cf", tarball_filename] + filenames, cwd=output_dir)
                    subprocess.check_call(["xz", "-T", "0", tarball_filename], cwd=output_dir)
                with open(os.path.join(output_dir, asset_filename), 'rb') as f:
                    asset_data = f.read()
            finally:
                shutil.rmtree(output_dir)

            upload(asset_filename, asset_mimetype, asset_data)

    if int(nano) == 0:
        if worker == 'windows':
            upload = get_github_uploader()

            upload_devkits("windows-x86", upload)
            upload_devkits("windows-x86_64", upload)

            upload_file("frida-server-{version}-windows-x86.exe", os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "bin", "frida-server.exe"), upload)
            upload_file("frida-server-{version}-windows-x86_64.exe", os.path.join(build_dir, "build", "frida-windows", "x64-Release", "bin", "frida-server.exe"), upload)

            upload_file("frida-gadget-{version}-windows-x86.dll", os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "bin", "frida-gadget.dll"), upload)
            upload_file("frida-gadget-{version}-windows-x86_64.dll", os.path.join(build_dir, "build", "frida-windows", "x64-Release", "bin", "frida-gadget.dll"), upload)

            upload_python_bindings_to_pypi(r"C:\Program Files (x86)\Python 2.7\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
            upload_python_bindings_to_pypi(r"C:\Program Files\Python 2.7\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "x64-Release", "lib", "python2.7", "site-packages", "_frida.pyd"))
            upload_python_bindings_to_pypi(r"C:\Program Files (x86)\Python 3.7\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "Win32-Release", "lib", "python3.7", "site-packages", "_frida.pyd"))
            upload_python_bindings_to_pypi(r"C:\Program Files\Python 3.7\python.exe",
                os.path.join(build_dir, "build", "frida-windows", "x64-Release", "lib", "python3.7", "site-packages", "_frida.pyd"), sdist=True)

            python2_interpreter=r"C:\Program Files\Python 2.7\python.exe"
            upload_node_bindings_to_npm(r"C:\Program Files (x86)\nodejs\node.exe", upload, publish=False, python2_interpreter=python2_interpreter)
            upload_node_bindings_to_npm(r"C:\Program Files\nodejs\node.exe", upload, publish=False, python2_interpreter=python2_interpreter)
        elif worker == 'mac':
            upload = get_github_uploader()

            upload_devkits("macos-x86", upload)
            upload_devkits("macos-x86_64", upload)
            upload_devkits("ios-x86", upload)
            upload_devkits("ios-x86_64", upload)
            upload_devkits("ios-arm", upload)
            upload_devkits("ios-arm64", upload)

            upload_file("frida-server-{version}-macos-x86_64", os.path.join(build_dir, "build", "frida-macos-x86_64", "bin", "frida-server"), upload)
            upload_file("frida-server-{version}-ios-arm", os.path.join(build_dir, "build", "frida-ios-arm", "bin", "frida-server"), upload)
            upload_file("frida-server-{version}-ios-arm64", os.path.join(build_dir, "build", "frida-ios-arm64", "bin", "frida-server"), upload)

            upload_file("frida-gadget-{version}-macos-universal.dylib", os.path.join(build_dir, "build", "frida-macos-universal", "lib", "frida-gadget.dylib"), upload)
            upload_file("frida-gadget-{version}-ios-universal.dylib", os.path.join(build_dir, "build", "frida-ios-universal", "lib", "frida-gadget.dylib"), upload)
            upload_file("frida-gadget-{version}-ios-universal.dylib", os.path.join(build_dir, "build", "frida-ios-universal", "lib", "frida-gadget.dylib"), upload, compression='gz')

            upload_directory("frida-swift-{version}-macos-x86_64", os.path.join(build_dir, "frida-swift", "build", "Release"), upload)

            upload_directory("frida-qml-{version}-macos-x86_64", os.path.join(build_dir, "build", "frida-macos-x86_64", "lib", "qt5", "qml"), upload)

            upload_python_bindings_to_pypi("/usr/bin/python2.7",
                os.path.join(build_dir, "build", "frida-macos-universal", "lib", "python2.7", "site-packages", "_frida.so"),
                { '_PYTHON_HOST_PLATFORM': "macosx-10.9-intel" })
            upload_python_bindings_to_pypi("/usr/local/bin/python3.6",
                os.path.join(build_dir, "build", "frida-macos-universal", "lib", "python3.6", "site-packages", "_frida.so"))

            upload_node_bindings_to_npm("/opt/node-64/bin/node", upload, publish=True)
            upload_meta_modules_to_npm("/opt/node-64/bin/node")

            upload_ios_deb("frida", os.path.join(build_dir, "build", "frida-ios-arm64", "bin", "frida-server"))
            upload_ios_deb("frida32", os.path.join(build_dir, "build", "frida-ios-arm", "bin", "frida-server"))

            upload_ios_debug_symbols()
        elif worker == 'linux-x86':
            upload = get_github_uploader()

            upload_devkits("linux-x86", upload)
            upload_devkits("linux-x86_64", upload)

            upload_file("frida-server-{version}-linux-x86", os.path.join(build_dir, "build", "frida-linux-x86", "bin", "frida-server"), upload)
            upload_file("frida-server-{version}-linux-x86_64", os.path.join(build_dir, "build", "frida-linux-x86_64", "bin", "frida-server"), upload)

            upload_file("frida-gadget-{version}-linux-x86.so", os.path.join(build_dir, "build", "frida-linux-x86", "lib", "frida-gadget.so"), upload)
            upload_file("frida-gadget-{version}-linux-x86_64.so", os.path.join(build_dir, "build", "frida-linux-x86_64", "lib", "frida-gadget.so"), upload)

            upload_python_bindings_to_pypi("/opt/python27-32/bin/python2.7",
                os.path.join(build_dir, "build", "frida-linux-x86", "lib", "python2.7", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python27-32/lib", '_PYTHON_HOST_PLATFORM': "linux-i686" })
            upload_python_bindings_to_pypi("/opt/python27-64/bin/python2.7",
                os.path.join(build_dir, "build", "frida-linux-x86_64", "lib", "python2.7", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python27-64/lib", '_PYTHON_HOST_PLATFORM': "linux-x86_64" })
            upload_python_bindings_to_pypi("/opt/python36-32/bin/python3.6",
                os.path.join(build_dir, "build", "frida-linux-x86", "lib", "python3.6", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python36-32/lib", '_PYTHON_HOST_PLATFORM': "linux-i686" })
            upload_python_bindings_to_pypi("/opt/python36-64/bin/python3.6",
                os.path.join(build_dir, "build", "frida-linux-x86_64", "lib", "python3.6", "site-packages", "_frida.so"),
                { 'LD_LIBRARY_PATH': "/opt/python36-64/lib", '_PYTHON_HOST_PLATFORM': "linux-x86_64" })

            upload_node_bindings_to_npm("/opt/node-32/bin/node", upload, publish=False)
            upload_node_bindings_to_npm("/opt/node-64/bin/node", upload, publish=False)
        elif worker == 'android':
            upload = get_github_uploader()

            upload_devkits("android-x86", upload)
            upload_devkits("android-x86_64", upload)
            upload_devkits("android-arm", upload)
            upload_devkits("android-arm64", upload)

            upload_file("frida-server-{version}-android-x86", os.path.join(build_dir, "build", "frida-android-x86", "bin", "frida-server"), upload)
            upload_file("frida-server-{version}-android-x86_64", os.path.join(build_dir, "build", "frida-android-x86_64", "bin", "frida-server"), upload)
            upload_file("frida-server-{version}-android-arm", os.path.join(build_dir, "build", "frida-android-arm", "bin", "frida-server"), upload)
            upload_file("frida-server-{version}-android-arm64", os.path.join(build_dir, "build", "frida-android-arm64", "bin", "frida-server"), upload)

            upload_file("frida-inject-{version}-android-x86", os.path.join(build_dir, "build", "frida-android-x86", "bin", "frida-inject"), upload)
            upload_file("frida-inject-{version}-android-x86_64", os.path.join(build_dir, "build", "frida-android-x86_64", "bin", "frida-inject"), upload)
            upload_file("frida-inject-{version}-android-arm", os.path.join(build_dir, "build", "frida-android-arm", "bin", "frida-inject"), upload)
            upload_file("frida-inject-{version}-android-arm64", os.path.join(build_dir, "build", "frida-android-arm64", "bin", "frida-inject"), upload)

            upload_file("frida-gadget-{version}-android-x86.so", os.path.join(build_dir, "build", "frida-android-x86", "lib", "frida-gadget.so"), upload)
            upload_file("frida-gadget-{version}-android-x86_64.so", os.path.join(build_dir, "build", "frida-android-x86_64", "lib", "frida-gadget.so"), upload)
            upload_file("frida-gadget-{version}-android-arm.so", os.path.join(build_dir, "build", "frida-android-arm", "lib", "frida-gadget.so"), upload)
            upload_file("frida-gadget-{version}-android-arm64.so", os.path.join(build_dir, "build", "frida-android-arm64", "lib", "frida-gadget.so"), upload)
        elif worker == 'ubuntu_16_04-x86_64':
            upload = get_github_uploader()

            upload_python_debs("ubuntu-xenial", "python", "/usr/bin/python2.7",
                os.path.join(build_dir, "build", "frida-linux-x86_64", "lib", "python2.7", "site-packages", "_frida.so"),
                upload)
            upload_python_debs("ubuntu-xenial", "python3", "/usr/bin/python3.5",
                os.path.join(build_dir, "build", "frida-linux-x86_64", "lib", "python3.5", "site-packages", "_frida.so"),
                upload)
        elif worker == 'ubuntu_16_04-arm64':
            upload = get_github_uploader()

            upload_devkits("linux-arm64", upload)

            upload_file("frida-server-{version}-linux-arm64", os.path.join(build_dir, "build", "frida_thin-linux-arm64", "bin", "frida-server"), upload)

            upload_file("frida-gadget-{version}-linux-arm64.so", os.path.join(build_dir, "build", "frida_thin-linux-arm64", "lib", "frida-gadget.so"), upload)

            upload_python_bindings_to_pypi("/usr/bin/python2.7",
                os.path.join(build_dir, "build", "frida_thin-linux-arm64", "lib", "python2.7", "site-packages", "_frida.so"))
            upload_python_bindings_to_pypi("/usr/bin/python3.5",
                os.path.join(build_dir, "build", "frida_thin-linux-arm64", "lib", "python3.5", "site-packages", "_frida.so"))

            upload_node_bindings_to_npm("/usr/bin/node", upload, publish=False)
        elif worker == 'ubuntu_18_04-x86_64':
            upload = get_github_uploader()

            upload_python_debs("ubuntu-bionic", "python", "/usr/bin/python2.7",
                os.path.join(build_dir, "build", "frida_thin-linux-x86_64", "lib", "python2.7", "site-packages", "_frida.so"),
                upload)
            upload_python_debs("ubuntu-bionic", "python3", "/usr/bin/python3.6",
                os.path.join(build_dir, "build", "frida_thin-linux-x86_64", "lib", "python3.6", "site-packages", "_frida.so"),
                upload)
        elif worker == 'fedora_28-x86_64':
            upload = get_github_uploader()

            upload_python_rpms("fc28", "python2", "/usr/bin/python2.7",
                os.path.join(build_dir, "build", "frida-linux-x86_64", "lib", "python2.7", "site-packages", "_frida.so"),
                upload)
            upload_python_rpms("fc28", "python3", "/usr/bin/python3.6",
                os.path.join(build_dir, "build", "frida-linux-x86_64", "lib", "python3.6", "site-packages", "_frida.so"),
                upload)
