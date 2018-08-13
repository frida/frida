import argparse
import codecs
import os
import sys


def detect_version(source_dir):
    with codecs.open(os.path.join(source_dir, "include", "v8-version.h"), "r", 'utf-8') as f:
        lines = f.read().split("\n")

    major = extract_version("V8_MAJOR_VERSION", lines)
    minor = extract_version("V8_MINOR_VERSION", lines)
    build = extract_version("V8_BUILD_NUMBER", lines)

    version = "{}.{}.{}".format(major, minor, build)
    api_version = "{}.0".format(major)

    return (version, api_version)

def extract_version(define_name, lines):
    return [int(line.split(" ")[-1]) for line in lines if define_name in line][0]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Detect V8 version.")
    parser.add_argument("source_dir", metavar="path-to-source-dir")
    parser.add_argument("property_name", metavar="property", choices=["version", "api-version"])
    args = parser.parse_args()

    (version, api_version) = detect_version(args.source_dir)

    if args.property_name == "version":
        sys.stdout.write(version)
    else:
        sys.stdout.write(api_version)
