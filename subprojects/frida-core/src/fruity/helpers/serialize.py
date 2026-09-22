#!/usr/bin/env python3

from pathlib import Path
import r2pipe
import re
import sys


CODE_PATTERN = re.compile(r"private const uint8\[\] (\w+_CODE) = {[^}]+?};")


def main(input_dylib, output_vala):
    r2 = r2pipe.open(str(input_dylib))
    sections = r2.cmdj("iSj")
    code_sections = [s for s in sections if not s["name"].endswith(".__TEXT.__unwind_info")]
    last_section = code_sections[-1]
    last_end = last_section["vaddr"] + last_section["vsize"]
    base_address = code_sections[0]["vaddr"]
    total_size = last_end - base_address

    r2.cmd(f"s {hex(base_address)}; b {hex(total_size)}")
    code = r2.cmdj("pcj")

    identifier = input_dylib.stem.upper().replace("-", "_") + "_CODE"

    def replace_code(match):
        current_identifier = match.group(1)
        if current_identifier != identifier:
            return match.group(0)

        lines = [f"private const uint8[] {identifier} = {{"]
        indent = "\t\t\t"
        current_line = indent
        offset = 0
        for byte in code:
            if offset > 0:
                if len(current_line) >= 110:
                    lines += [current_line + ","]
                    current_line = indent
                else:
                    current_line += ", "
            current_line += f"0x{byte:02x}"
            offset += 1
        lines += [current_line]
        lines += ["\t\t};"]

        return "\n".join(lines)

    current_code = output_vala.read_text(encoding="utf-8")
    updated_code = CODE_PATTERN.sub(replace_code, current_code)
    output_vala.write_text(updated_code, encoding="utf-8")


if __name__ == "__main__":
    input_dylib = Path(sys.argv[1])
    output_vala = Path(sys.argv[2])
    main(input_dylib, output_vala)
