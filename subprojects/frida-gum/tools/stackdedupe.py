#!/usr/bin/env python3

import argparse
from collections.abc import Iterable
from pathlib import Path
import re
from typing import T


STACK_PATTERN = re.compile(r"^(.+) (\d+)$")


def main():
    parser = argparse.ArgumentParser(description="Deduplicate subsequent identical stack frames.")
    parser.add_argument("--input", dest="input", required=True,
                        help="the file to symbolicate")
    parser.add_argument("--output", dest="output", required=True,
                        help="where the symbolicated file will be written")
    args = parser.parse_args()

    with Path(args.input).open(encoding="utf-8") as input_file, \
            Path(args.output).open("w", encoding="utf-8") as output_file:
        stacks = {}
        for line_raw in input_file:
            m = STACK_PATTERN.match(line_raw)
            assert m is not None

            frames = m.group(1).split(";")
            count = int(m.group(2))

            compressed_frames = deduplicate_subsequent(frames)

            raw_frames = ";".join(compressed_frames)
            existing_count = stacks.get(raw_frames, 0)
            stacks[raw_frames] = existing_count + count

        for raw_frames, count in stacks.items():
            output_file.write(f"{raw_frames} {count}\n")


def deduplicate_subsequent(l: Iterable[T]) -> list[T]:
    if len(l) == 0:
        return []
    result = [l[0]]
    for i in range(1, len(l)):
        if l[i] != l[i - 1]:
            result.append(l[i])
    return result


if __name__ == "__main__":
    main()
