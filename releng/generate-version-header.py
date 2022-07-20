#!/usr/bin/env python3

import os
import subprocess
import sys


def generate_version_header():
    version = "15.2.0.0"
    (major, minor, micro, nano) = version.split(".")

    header = """\
#ifndef __FRIDA_VERSION_H__
#define __FRIDA_VERSION_H__

#define FRIDA_VERSION "{version}"

#define FRIDA_MAJOR_VERSION {major}
#define FRIDA_MINOR_VERSION {minor}
#define FRIDA_MICRO_VERSION {micro}
#define FRIDA_NANO_VERSION {nano}

#endif\n""".format(version=version, major=major, minor=minor, micro=micro, nano=nano)

    if len(sys.argv) == 1:
        sys.stdout.write(header)
        sys.stdout.flush()
    else:
        output_filename = sys.argv[1]
        try:
            with open(output_filename, "rb") as f:
                existing_header = f.read()
                if header == existing_header:
                    return
        except:
            pass
        with open(output_filename, "w") as f:
            f.write(header)


if __name__ == '__main__':
    generate_version_header()
