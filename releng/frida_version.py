#!/usr/bin/env python3

from dataclasses import dataclass
import os
from pathlib import Path
import subprocess


RELENG_DIR = Path(__file__).parent.resolve()
ROOT_DIR = RELENG_DIR.parent


@dataclass
class FridaVersion:
    name: str
    major: int
    minor: int
    micro: int
    nano: int
    commit: str


def detect() -> FridaVersion:
    description = subprocess.run(["git", "describe", "--tags", "--always", "--long"],
                                 cwd=ROOT_DIR,
                                 capture_output=True,
                                 encoding="utf-8").stdout

    tokens = description.strip().replace("-", ".").split(".")
    if len(tokens) > 1:
        (major, minor, micro, nano, commit) = tokens
        major = int(major)
        minor = int(minor)
        micro = int(micro)
        nano = int(nano)
        if nano > 0:
            micro += 1
    else:
        major = 0
        minor = 0
        micro = 0
        nano = 0
        commit = tokens[0]

    if nano == 0 and len(tokens) != 1:
        version_name = f"{major}.{minor}.{micro}"
    else:
        version_name = f"{major}.{minor}.{micro}-dev.{nano - 1}"

    return FridaVersion(version_name, major, minor, micro, nano, commit)


if __name__ == "__main__":
    print(detect().name)
