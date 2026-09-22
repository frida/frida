#!/usr/bin/env python3

import argparse
from bisect import bisect_left
import csv
from pathlib import Path
import re
import subprocess


RAW_ADDRESS_PATTERN = re.compile(r"\b(0x[0-9a-f]+)\b")


def main():
    parser = argparse.ArgumentParser(description="Symbolicate stack traces.")
    parser.add_argument("--input", dest="input", required=True,
                        help="the DTrace stacks file to symbolicate")
    parser.add_argument("--output", dest="output", required=True,
                        help="where the symbolicated DTrace stacks will be written")
    parser.add_argument("--test-log", dest="test_log", required=True,
                        help="the test log file to use for resolving frida-agent code addresses")
    parser.add_argument("--v8-log", dest="v8_log", required=True,
                        help="the V8 log file to use for resolving code addresses")
    parser.add_argument("--agent", dest="agent", required=True,
                        help="the frida-agent binary")
    args = parser.parse_args()

    csv.field_size_limit(64 * 1024 * 1024)

    agent_start = None
    agent_end = None
    with open(args.test_log, "r", encoding="utf-8") as test_log_file:
        for row in csv.reader(test_log_file):
            event = row[0]
            if event == "agent-range":
                agent_start = int(row[1], 16)
                agent_end = int(row[2], 16)
                break

    agent_addresses = set()
    with open(args.input,  "r", encoding="utf-8") as input_file:
        for line_raw in input_file:
            m = RAW_ADDRESS_PATTERN.search(line_raw)
            if m is not None:
                address = int(m.group(1), 16)
                if address >= agent_start and address < agent_end:
                    agent_addresses.add(address)
    agent_addresses = list(agent_addresses)
    agent_addresses.sort()
    agent_query = subprocess.run([
            "atos",
            "-o", args.agent,
            "-l", hex(agent_start)
        ] + [hex(address) for address in agent_addresses],
        capture_output=True,
        encoding="utf-8",
        check=True)
    agent_symbols = dict(zip(agent_addresses, agent_query.stdout.split("\n")))

    code_ranges = []
    with open(args.v8_log, "r", encoding="utf-8") as v8_log_file:
        for row in csv.reader(v8_log_file):
            event = row[0]
            if event == "code-creation":
                start = int(row[4], 16)
                size = int(row[5])
                end = start + size
                name = row[6]
                code_ranges.append((start, end, name))
    code_ranges.sort(key=lambda r: r[0])

    def symbolicate(m):
        raw_address = m.group(1)
        address = int(raw_address, 16)

        name = agent_symbols.get(address, None)
        if name is not None:
            return name

        index = bisect_left(code_ranges, (address, 0, ""))
        for candidate in code_ranges[index - 1:index + 1]:
            start, end, name = candidate
            if address >= start and address < end:
                return name

        return raw_address

    with open(args.input,  "r", encoding="utf-8") as input_file, \
         open(args.output, "w", encoding="utf-8") as output_file:
        for line_raw in input_file:
            line_symbolicated = RAW_ADDRESS_PATTERN.sub(symbolicate, line_raw)
            output_file.write(line_symbolicated)


if __name__ == "__main__":
    main()
