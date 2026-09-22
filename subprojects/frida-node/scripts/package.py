import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path
from typing import Optional


def main(argv: list[str]):
    args = argv[1:]
    strip_command = pop_cmd_array_arg(args)
    strip_enabled = args.pop(0) == "true"
    binding = Path(args.pop(0))
    outfile = Path(args.pop(0))

    intermediate_path = outfile.parent / f"{outfile.name}.tmp"
    shutil.copy(binding, intermediate_path)

    try:
        if strip_enabled and strip_command is not None:
            subprocess.run(
                strip_command + [intermediate_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                encoding="utf-8",
                check=True,
            )

        with tarfile.open(outfile, "w:gz") as tar:
            tar.add(intermediate_path, arcname="build/frida_binding.node")
    except subprocess.CalledProcessError as e:
        print(e, file=sys.stderr)
        print("Output:\n\t| " + "\n\t| ".join(e.output.strip().split("\n")), file=sys.stderr)
        sys.exit(1)
    finally:
        os.unlink(intermediate_path)


def pop_cmd_array_arg(args: list[str]) -> Optional[list[str]]:
    result = []
    first = args.pop(0)
    assert first == ">>>"
    while True:
        cur = args.pop(0)
        if cur == "<<<":
            break
        result.append(cur)
    if len(result) == 1 and not result[0]:
        return None
    return result


if __name__ == "__main__":
    main(sys.argv)
