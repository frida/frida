#!/usr/bin/env python3

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import re
import subprocess
import sys
from typing import List


RELENG_DIR = Path(__file__).resolve().parent
ROOT_DIR = RELENG_DIR.parent


@dataclass
class FridaVersion:
    name: str
    major: int
    minor: int
    micro: int
    nano: int
    commit: str


def main(argv: List[str]):
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", nargs="?", type=Path, default=ROOT_DIR)
    args = parser.parse_args()

    version = detect(args.repo)
    print(version.name)


def detect(repo: Path) -> FridaVersion:
    version_name = "0.0.0"
    major = 0
    minor = 0
    micro = 0
    nano = 0
    commit = ""

    if not (repo / ".git").exists():
        return FridaVersion(version_name, major, minor, micro, nano, commit)

    proc = subprocess.run(
        ["git", "describe", "--tags", "--always", "--long"],
        cwd=repo,
        capture_output=True,
        encoding="utf-8",
        check=False,
    )
    description = proc.stdout.strip()

    if "-" not in description:
        commit = description
        return FridaVersion(version_name, major, minor, micro, nano, commit)

    parts = description.rsplit("-", 2)
    if len(parts) != 3:
        raise VersionParseError(f"Unexpected format from git describe: {description!r}")

    tag_part, distance_str, commit_part = parts
    commit = commit_part.lstrip("g")

    try:
        distance = int(distance_str)
    except ValueError as exc:
        raise VersionParseError(f"Invalid distance in {description!r}") from exc

    nano = distance

    m = re.match(r"^(\d+)\.(\d+)\.(\d+)(?:-(.+))?$", tag_part)
    if m is None:
        raise VersionParseError(
            f"Tag does not match expected semver pattern: {tag_part!r}"
        )

    major = int(m.group(1))
    minor = int(m.group(2))
    micro = int(m.group(3))
    suffix = m.group(4)

    if suffix is None:
        if distance == 0:
            version_name = f"{major}.{minor}.{micro}"
        else:
            micro += 1
            version_name = f"{major}.{minor}.{micro}-dev.{distance - 1}"
    else:
        base = f"{major}.{minor}.{micro}-{suffix}"
        if distance == 0:
            version_name = base
        else:
            version_name = f"{base}-dev.{distance - 1}"

    return FridaVersion(version_name, major, minor, micro, nano, commit)


class VersionParseError(Exception):
    pass


if __name__ == "__main__":
    main(sys.argv)
