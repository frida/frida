from pathlib import Path
import sys


def main(argv: list[str]):
    modulemap = Path(argv[1])
    core_header = argv[2]

    modulemap.write_text("\n".join([
                             "module FridaCore [extern_c] {",
                             f'  header "{core_header}"',
                             "  export *",
                             "}",
                             "",
                             "",
                         ]),
                         encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv)
