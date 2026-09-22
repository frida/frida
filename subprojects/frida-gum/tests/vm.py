#!/usr/bin/env python3

import pexpect
import shlex
import sys


def run(arch: str, args: [str]):
    child = pexpect.spawn("arm_now", ["start", arch, "--sync"])

    child.expect("buildroot login: ")
    child.sendline("root")
    child.expect("# ")

    child.sendline(shlex.join(["/root/gum-tests"] + args))
    child.interact()


if __name__ == "__main__":
    arch = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) >= 3 else []
    run(arch, args)
