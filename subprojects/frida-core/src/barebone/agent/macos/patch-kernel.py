#!/usr/bin/env python3
"""Make a kernel the agent can write to and run code in."""
import struct
import sys

NOP = 0xD503201F
FORBIDS_BELOW = 0x3800
ALLOWS_BELOW = 0x2000


def main(source, destination):
    image = bytearray(open(source, "rb").read())
    secs = sections(image)

    print("pages the pmap has locked down:")
    locked = stop_refusing(image, secs, strings_saying(bytes(image), secs, b"locked down"),
                           "may be mapped")
    print("what a table forbids underneath it:")
    below = stop_forbidding_execution_below(image, secs)
    print("executable mappings in the kernel's own pmap:")
    executable = stop_refusing(image, secs,
                               strings_saying(bytes(image), secs, b"executable mapping"),
                               "may be made")

    if not (locked and below and executable):
        sys.exit("kernel does not look like one this tool knows")

    open(destination, "wb").write(bytes(image))
    print(f"\n{locked + below + executable} changes -> {destination}")


def stop_refusing(image, secs, saying, what):
    undone = 0
    for segment, name, addr, size, foff in secs:
        if name != "__text":
            continue
        code = bytes(image[foff:foff + size])
        blocks = {addr + b
                  for b in (start_of_block(code, s) for s in complaints_at(code, addr, saying))
                  if b is not None}
        if not blocks:
            continue
        for where, at, word in branches_into(code, addr, blocks):
            struct.pack_into("<I", image, foff + at, NOP)
            print(f"  {segment} {where:#x}  {word:#010x} -> nop   ({what})")
            undone += 1
    return undone


def stop_forbidding_execution_below(image, secs):
    undone = 0
    for segment, name, addr, size, foff in secs:
        if name != "__text":
            continue
        code = bytes(image[foff:foff + size])
        for i in range(0, len(code) - 4, 4):
            word = struct.unpack_from("<I", code, i)[0]
            builds_high = (word & 0xFF800000) == 0xF2800000 and ((word >> 21) & 3) == 3
            if not builds_high or ((word >> 5) & 0xFFFF) != FORBIDS_BELOW:
                continue
            into = word & 0x1F
            struct.pack_into("<I", image, foff + i, 0xF2E00000 | (ALLOWS_BELOW << 5) | into)
            print(f"  {segment} {addr + i:#x}  {word:#010x} -> "
                  f"movk x{into}, #{ALLOWS_BELOW:#06x}, lsl 48"
                  f"   (a table no longer forbids what is under it)")
            undone += 1
    return undone


def strings_saying(image, secs, wanted):
    where = set()
    at = image.find(wanted)
    while at != -1:
        start = image.rfind(b"\0", 0, at) + 1
        for _, name, addr, size, foff in secs:
            if name == "__cstring" and foff <= start < foff + size:
                where.add(addr + (start - foff))
        at = image.find(wanted, at + 1)
    return where


def complaints_at(code, base, about):
    said = []
    for i in range(0, len(code) - 4, 4):
        word = struct.unpack_from("<I", code, i)[0]
        if (word & 0x9F000000) != 0x90000000:
            continue
        into = word & 0x1F
        page = (((word >> 5) & 0x7FFFF) << 2) | ((word >> 29) & 3)
        if page & (1 << 20):
            page -= 1 << 21
        page = ((base + i) & ~0xFFF) + (page << 12)
        for j in range(i + 4, min(i + 48, len(code) - 4), 4):
            after = struct.unpack_from("<I", code, j)[0]
            if (after & 0xFF800000) == 0x91000000 and ((after >> 5) & 0x1F) == into:
                if page + ((after >> 10) & 0xFFF) in about:
                    said.append(i)
                break
    return said


def start_of_block(code, at):
    i = at
    while i >= 4:
        word = struct.unpack_from("<I", code, i - 4)[0]
        if (word & 0xFC000000) in (0x94000000, 0x14000000):
            return i
        if (word & 0xFFFFFC1F) == 0xD65F0000 or word == 0xD65F0FFF:
            return i
        i -= 4
    return None


def branches_into(code, base, blocks):
    found = []
    for i in range(0, len(code) - 4, 4):
        word = struct.unpack_from("<I", code, i)[0]
        if (word & 0xFF000010) == 0x54000000 or (word & 0x7E000000) == 0x34000000:
            far, sign = (word >> 5) & 0x7FFFF, 1 << 18
        elif (word & 0x7E000000) == 0x36000000:
            far, sign = (word >> 5) & 0x3FFF, 1 << 13
        else:
            continue
        if far & sign:
            far -= sign << 1
        if base + i + far * 4 in blocks:
            found.append((base + i, i, word))
    return found


def sections(image):
    count = struct.unpack_from("<I", image, 16)[0]
    at, found = 32, []
    for _ in range(count):
        kind, size = struct.unpack_from("<II", image, at)
        if kind == 0x19:
            how_many = struct.unpack_from("<I", image, at + 64)[0]
            so = at + 72
            for _ in range(how_many):
                found.append((
                    image[so + 16:so + 32].rstrip(b"\0").decode(),
                    image[so:so + 16].rstrip(b"\0").decode(),
                    *struct.unpack_from("<QQ", image, so + 32),
                    struct.unpack_from("<I", image, so + 48)[0],
                ))
                so += 80
        at += size
    return found


if len(sys.argv) != 3:
    sys.exit(f"usage: {sys.argv[0]} <kernel> <patched-kernel>")

main(sys.argv[1], sys.argv[2])
