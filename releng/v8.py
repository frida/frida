#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys


def detect_version(source_dir):
    with open(os.path.join(source_dir, "include", "v8-version.h"), "r", encoding='utf-8') as f:
        lines = f.read().split("\n")

    major = extract_version("V8_MAJOR_VERSION", lines)
    minor = extract_version("V8_MINOR_VERSION", lines)
    build = extract_version("V8_BUILD_NUMBER", lines)

    version = "{}.{}.{}".format(major, minor, build)
    api_version = "{}.0".format(major)

    return (version, api_version)

def extract_version(define_name, lines):
    return [int(line.split(" ")[-1]) for line in lines if define_name in line][0]

def patch_config_header(header_path, source_dir, build_dir, gn=None, env=None):
    if gn is None:
        gn = shutil.which("gn")
        if gn is None:
            raise ValueError("unable to find “gn”; is it on your PATH?")

    gn = os.path.abspath(gn)

    config_defines = subprocess.check_output([gn, "desc", os.path.relpath(build_dir, start=source_dir), ":v8_header_features", "defines"],
            cwd=source_dir, env=env).decode('utf-8').rstrip().split("\n")

    with open(header_path, "rb") as f:
        code = f.read().decode('utf-8')

    newline = "\r\n" if "\r" in code else "\n"
    section_delimiter = 3 * newline
    section_heading = "// Build configuration."

    top, bottom = code.split(section_delimiter, maxsplit=1)

    if bottom.startswith(section_heading):
        bottom = bottom.split(section_delimiter, maxsplit=1)[1]

    lines = [section_heading]
    lines += ["#define {} 1".format(d) for d in config_defines]
    middle = newline.join(lines)

    code = section_delimiter.join([top, middle, bottom])

    with open(header_path, "wb") as f:
        f.write(code.encode('utf-8'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Introspect and manipulate V8 build artifacts.")

    def on_get(args):
        try:
            (version, api_version) = detect_version(args.source_dir)
        except Exception as e:
            parser.exit(1, str(e) + "\n")

        if args.property_name == "version":
            sys.stdout.write(version)
        else:
            sys.stdout.write(api_version)

    def on_patch(args):
        try:
            patch_config_header(args.header_path, args.source_dir, args.build_dir, args.gn)
        except Exception as e:
            parser.exit(1, str(e) + "\n")

    def on_missing_command(args):
        parser.print_help(file=sys.stderr)
        parser.exit(2)

    parser.set_defaults(func=on_missing_command)

    subparsers = parser.add_subparsers(help="sub-command help")

    get_command = subparsers.add_parser("get", help="get a property")
    get_command.add_argument("property_name", metavar="property", choices=["version", "api-version"])
    get_command.add_argument("-s", "--source-dir", type=str, metavar="path-to-source-dir", required=True)
    get_command.set_defaults(func=on_get)

    patch_command = subparsers.add_parser("patch", help="patch v8config.h to add build configuration defines")
    patch_command.add_argument("header_path", metavar="path-to-v8config-header")
    patch_command.add_argument("-s", "--source-dir", metavar="path-to-source-dir", required=True)
    patch_command.add_argument("-b", "--build-dir", metavar="path-to-build-dir", required=True)
    patch_command.add_argument("-G", "--gn", metavar="path-to-gn", default=None)
    patch_command.set_defaults(func=on_patch)

    args = parser.parse_args()
    args.func(args)
