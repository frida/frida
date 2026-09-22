#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description="Symbolicate panic backtrace from kernel log")
    parser.add_argument("--log-file", type=argparse.FileType("r"), required=True,
                        help="Path to the kernel log file")
    parser.add_argument("--elf-file", type=str, required=True,
                        help="Path to the ELF file for symbolication")
    parser.add_argument("--base-address", type=lambda x: int(x, 16), required=True,
                        help="Base address where the ELF was loaded (e.g., 0xffffffe8195f0000)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show all backtrace frames, not just symbolicated ones")

    args = parser.parse_args()

    log_content = args.log_file.read()
    args.log_file.close()

    frames = find_last_panic(log_content)
    if not frames:
        print("No panic found in the log file", file=sys.stderr)
        return 1

    print(f"Found last panic with {len(frames)} backtrace frames")
    print(f"ELF base address: 0x{args.base_address:x}")
    print(f"ELF file: {args.elf_file}")
    print()

    symbolicated_count = 0

    for i, (lr_str, fp_str) in enumerate(frames):
        lr_addr = int(lr_str, 16)
        #fp_addr = int(fp_str, 16)

        if lr_addr >= args.base_address:
            function, location = symbolicate_address(args.elf_file, lr_addr, args.base_address)
            offset = lr_addr - args.base_address

            print(f"Frame {i:2d}: lr: {lr_str} (offset: 0x{offset:x}) fp: {fp_str}")

            # Color the function name based on resolution status
            if function == "<unknown>":
                colored_function = Colors.bold_yellow(function)
            elif function == "<error>":
                colored_function = Colors.bold_red(function)
            else:
                colored_function = Colors.bold_green(function)

            print(f"          -> {colored_function}")
            print(f"             {location}")
            print()
            symbolicated_count += 1
        elif args.verbose:
            print(f"Frame {i:2d}: lr: {lr_str} fp: {fp_str} (outside ELF range)")

    if symbolicated_count == 0:
        print("No addresses found within the ELF address range")
        if not args.verbose:
            print("Use --verbose to see all frames")
    else:
        print(f"Symbolicated {symbolicated_count} frame(s)")

    return 0


def find_last_panic(log_content: str):
    panic_pattern = re.compile(
        r"panic\(.*?\n.*?Panicked thread:.*?\n((?:\s*lr:\s*0x[0-9a-fA-F]+\s*fp:\s*0x[0-9a-fA-F]+\n)*)",
        re.DOTALL
    )

    panics = list(panic_pattern.finditer(log_content))

    if not panics:
        return None

    last_panic = panics[-1]
    backtrace_section = last_panic.group(1)

    lr_fp_pattern = re.compile(r"lr:\s*(0x[0-9a-fA-F]+)\s*fp:\s*(0x[0-9a-fA-F]+)")
    frames = lr_fp_pattern.findall(backtrace_section)

    return frames


def symbolicate_address(elf_path: str, address: int, base_address: int):
    try:
        offset = address - base_address

        result = subprocess.run([
            "aarch64-none-elf-addr2line", "-e", elf_path, "-f", "-C", f"0x{offset:x}"
        ], capture_output=True, text=True, check=True)

        lines = result.stdout.strip().split("\n")
        if len(lines) >= 2:
            function = lines[0]
            location = lines[1]

            if function == "??":
                function = "<unknown>"
            if location == "??:0" or location == "??:?":
                location = "<unknown>"

            return function, location
        else:
            return "<unknown>", "<unknown>"

    except (subprocess.CalledProcessError, FileNotFoundError):
        return "<error>", "<error>"


class Colors:
    BOLD = "\033[1m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    RED = "\033[31m"
    RESET = "\033[0m"

    @staticmethod
    def bold_green(text: str):
        return f"{Colors.BOLD}{Colors.GREEN}{text}{Colors.RESET}"

    @staticmethod
    def bold_yellow(text: str):
        return f"{Colors.BOLD}{Colors.YELLOW}{text}{Colors.RESET}"

    @staticmethod
    def bold_red(text: str):
        return f"{Colors.BOLD}{Colors.RED}{text}{Colors.RESET}"


if __name__ == "__main__":
    sys.exit(main())
