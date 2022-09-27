#!/usr/bin/env python3

import frida_version
from pathlib import Path
import sys


def generate_version_header():
    v = frida_version.detect()

    header = """\
#ifndef __FRIDA_VERSION_H__
#define __FRIDA_VERSION_H__

#define FRIDA_VERSION "{version}"

#define FRIDA_MAJOR_VERSION {major}
#define FRIDA_MINOR_VERSION {minor}
#define FRIDA_MICRO_VERSION {micro}
#define FRIDA_NANO_VERSION {nano}

#endif\n""".format(version=v.name, major=v.major, minor=v.minor, micro=v.micro, nano=v.nano)

    if len(sys.argv) == 1:
        sys.stdout.write(header)
        sys.stdout.flush()
    else:
        output_filename = Path(sys.argv[1])

        if output_filename.exists():
            try:
                existing_header = output_filename.read_text(encoding="utf-8")
                if header == existing_header:
                    return
            except:
                pass

        output_filename.write_text(header, encoding="utf-8")


if __name__ == '__main__':
    generate_version_header()
