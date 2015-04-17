#!/usr/bin/env python

import os
import subprocess

build_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
version = subprocess.check_output(["git", "describe", "--tags", "--always", "--long"], cwd=build_dir).strip().replace("-", ".")
(major, minor, micro, nano, commit) = version.split(".")
if nano == "0":
    version = ".".join([major, minor, micro])

print("""\
#ifndef __FRIDA_VERSION_H__
#define __FRIDA_VERSION_H__

#define FRIDA_VERSION "{version}"

#define FRIDA_MAJOR_VERSION {major}
#define FRIDA_MINOR_VERSION {minor}
#define FRIDA_MICRO_VERSION {micro}

#endif""").format(version=version, major=major, minor=minor, micro=micro)
