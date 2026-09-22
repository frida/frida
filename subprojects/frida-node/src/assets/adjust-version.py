from pathlib import Path
import re
import sys


def main(argv: list[str]):
    version = argv[1]
    inpkg = Path(argv[2])
    outpkg = Path(argv[3])

    vanilla_pkg = inpkg.read_text(encoding="utf-8")
    adjusted_pkg = re.sub(r'(?P<prefix>"version": ")[^"]+(?P<suffix>")',
                          f"\\g<prefix>{version}\\g<suffix>",
                          vanilla_pkg)
    outpkg.write_text(adjusted_pkg, encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv)
