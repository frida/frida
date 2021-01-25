#!/usr/bin/env python3

import argparse
import json
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

def query_defines(source_dir, build_dir, gn=None, env=None):
    result = query_gn("//:v8_header_features", "defines", source_dir, build_dir, gn, env)
    return result["//:v8_header_features"]["defines"]

def query_libs(source_dir, build_dir, gn=None, env=None):
    result = query_gn("//:v8_monolith", "libs", source_dir, build_dir, gn, env)
    return result["//:v8_monolith"]["libs"]

def patch_config_header(header_path, source_dir, build_dir, gn=None, env=None):
    defines = query_defines(source_dir, build_dir, gn, env)

    with open(header_path, "rb") as f:
        code = f.read().decode('utf-8')

    newline = "\r\n" if "\r" in code else "\n"
    section_delimiter = 3 * newline
    section_heading = "// Build configuration."

    top, bottom = code.split(section_delimiter, maxsplit=1)

    if bottom.startswith(section_heading):
        bottom = bottom.split(section_delimiter, maxsplit=1)[1]

    lines = [section_heading]
    lines += ["#define {} 1".format(d) for d in defines]
    middle = newline.join(lines)

    code = section_delimiter.join([top, middle, bottom])

    with open(header_path, "wb") as f:
        f.write(code.encode('utf-8'))

def query_gn(label_or_pattern, what, source_dir, build_dir, gn, env):
    if gn is None:
        gn = shutil.which("gn")
        if gn is None:
            raise ValueError("unable to find “gn”; is it on your PATH?")

    args = [
        os.path.abspath(gn),
        "desc",
        os.path.relpath(build_dir, start=source_dir),
        label_or_pattern,
        "--format=json",
    ]
    if what is not None:
        args.append(what)

    raw_result = subprocess.check_output(args, cwd=source_dir, env=env).decode('utf-8')

    return json.loads(raw_result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Introspect and manipulate V8 build artifacts.")

    def on_get(args):
        prop = args.property_name
        if prop in ("version", "api-version"):
            try:
                (version, api_version) = detect_version(args.source_dir)
            except Exception as e:
                parser.exit(1, str(e) + "\n")

            if prop == "version":
                result = version
            else:
                result = api_version
        elif prop == "libs":
            libs = query_libs(args.source_dir, get_build_dir(args), args.gn)
            result = " ".join(["-l" + lib for lib in libs])
        else:
            raise ValueError("unexpected property")

        sys.stdout.write(result)

    def on_patch(args):
        try:
            patch_config_header(args.header_path, args.source_dir, get_build_dir(args), args.gn)
        except Exception as e:
            parser.exit(1, str(e) + "\n")

    def get_build_dir(args):
        result = args.build_dir
        if result is None:
            parser.exit(1, "the following arguments are required: -b/--build-dir\n")
        return result

    def on_missing_command(args):
        parser.print_help(file=sys.stderr)
        parser.exit(2)

    parser.add_argument("-s", "--source-dir", metavar="path-to-source-dir", required=True)
    parser.add_argument("-b", "--build-dir", metavar="path-to-build-dir", default=None)
    parser.add_argument("-g", "--gn", metavar="path-to-gn", default=None)
    parser.set_defaults(func=on_missing_command)

    subparsers = parser.add_subparsers(help="sub-command help")

    get_command = subparsers.add_parser("get", help="get a property")
    get_command.add_argument("property_name", metavar="property", choices=["version", "api-version", "libs"])
    get_command.set_defaults(func=on_get)

    patch_command = subparsers.add_parser("patch", help="patch v8config.h to add build configuration defines")
    patch_command.add_argument("header_path", metavar="path-to-v8config-header")
    patch_command.set_defaults(func=on_patch)

    args = parser.parse_args()
    args.func(args)
