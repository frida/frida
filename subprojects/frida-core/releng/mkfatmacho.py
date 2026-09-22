#!/usr/bin/env python3

import os
import shutil
import struct
import sys


def make_fat_macho(output_path, input_paths):
    """
    Used to create a FAT Mach-O when Apple's lipo tool refuses to do so, such as
    when needing two arm64e slices to support both the new and the old arm64e ABI.
    """
    input_slices = []
    offset = 0x8000
    slice_alignment = 0x4000
    for input_path in input_paths:
        delta = offset % slice_alignment
        if delta != 0:
            offset += slice_alignment - delta

        offset_bits = bin(offset)
        alignment = len(offset_bits[offset_bits.rfind("1") + 1:])

        f = open(input_path, "rb+")

        f.seek(4)
        cpu_type, cpu_subtype = struct.unpack("<II", f.read(8))

        f.seek(0, os.SEEK_END)
        size = f.tell()

        input_slices.append((f, cpu_type, cpu_subtype, offset, size, alignment))

        offset += size

    with open(output_path, "wb") as output_file:
        header = struct.pack(">II", 0xcafebabe, len(input_slices))
        output_file.write(header)

        for (_, cpu_type, cpu_subtype, offset, size, alignment) in input_slices:
            slice_spec = struct.pack(">IIIII", cpu_type, cpu_subtype, offset, size, alignment)
            output_file.write(slice_spec)

        for (input_file, _, _, offset, _, _) in input_slices:
            input_file.seek(0)
            output_file.seek(offset)
            shutil.copyfileobj(input_file, output_file)


if __name__ == '__main__':
    output_path = sys.argv[1]
    input_paths = sys.argv[2:]
    make_fat_macho(output_path, input_paths)
