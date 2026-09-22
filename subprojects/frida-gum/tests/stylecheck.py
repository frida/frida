#!/usr/bin/env python3

import json
from pathlib import Path
import re
import sys


MACRO_DEFINE_PATTERN = re.compile(r"^#\s*define\s")
STRING_LITERAL_PATTERN = re.compile(r"\"([^\\\"]|\\.)*\"")


def function_parameters_are_aligned(match):
    lines = match.group(1).rstrip().split("\n")
    if lines[0].endswith(" ="):
        return True

    if len(lines) < 2:
        return False

    if lines[1].endswith(" ="):
        return True

    offset = lines[1].find("(")
    if offset == -1:
        return False

    if offset == len(lines[1]) - 1:
        offset = 3

    expected_num_leading_spaces = offset + 1
    for line in lines[2:]:
        num_leading_spaces = len(line) - len(line.lstrip(" "))
        if num_leading_spaces != expected_num_leading_spaces:
            return False

    return True


COMMON_MISTAKES = [
    (
        "trailing whitespace",
        re.compile(r"([ \t]+)$", re.MULTILINE),
    ),
    (
        "tabs used for indentation",
        re.compile(r"(\t+)"),
    ),
    (
        "line exceeds 80 columns",
        re.compile(r"^.{81}()", re.MULTILINE),
    ),
    (
        "missing space before parentheses",
        re.compile(r"\w()\("),
        ("unless-line-matches", MACRO_DEFINE_PATTERN),
        ("unless-found-inside", STRING_LITERAL_PATTERN),
        ("unless-found-inside", re.compile(r"ElfW\(\w+\)")),
    ),
    (
        "missing space after cast",
        re.compile(r"\([^()]+\)()\w"),
        ("unless-found-inside", STRING_LITERAL_PATTERN),
    ),
    (
        "missing space in pointer declaration",
        re.compile(r"\w+()\* \w+"),
        ("unless-found-inside", STRING_LITERAL_PATTERN),
    ),
    (
        "missing space in pointer declaration",
        re.compile(r"\w+ \*()\w+"),
        ("unless-found-inside", STRING_LITERAL_PATTERN),
        ("unless-line-matches", re.compile(r"\s+return \*")),
    ),
    (
        "blank line after block start",
        re.compile("{\n(\n)"),
    ),
    (
        "blank line before block end",
        re.compile("\n(\n)}"),
    ),
    (
        "two or more consecutive blank lines",
        re.compile("\n(\n{2})"),
    ),
    (
        "opening brace on the same line as the statement opening it",
        re.compile(r"^.+\)[^\n]*({)", re.MULTILINE),
        ("unless-line-matches", MACRO_DEFINE_PATTERN),
        ("unless-found-inside", STRING_LITERAL_PATTERN),
        ("unless-line-matches", re.compile(r".+ = { 0, };$")),
        ("unless-line-matches", re.compile(r".+\) (const|override|const override) { .+; }$")),
        ("unless-line-matches", re.compile(r".+\[=\]\(\) { .+ }")),
        ("unless-line-matches", re.compile(r"^template ")),
    ),
    (
        "incorrectly formatted function definition",
        re.compile(r"^(static [^;{]+){", re.MULTILINE),
        ("unless-true", function_parameters_are_aligned),
    ),
]

COMMENT_PATTERN = re.compile(r"\/\*(.+?)\*\/", re.DOTALL)

INCLUDED_SUBDIRS = [
    "gum",
    "libs",
    Path("bindings") / "gumjs",
    "tests",
]

INCLUDED_EXTENSIONS = {
    ".c",
    ".h",
    ".cpp",
    ".hpp",
}

EXCLUDED_SOURCES = {
    "gum/backend-arm64/asmdefs.h",
    "gum/backend-darwin/substratedclient.c",
    "gum/backend-darwin/substratedclient.h",
    "gum/dlmalloc.c",
    "gum/gummetalhash.c",
    "gum/gummetalhash.h",
    "gum/gumprintf.c",
    "gum/valgrind.h",
}


def main():
    if len(sys.argv) not in {1, 2}:
        print(f"Usage: {sys.argv[0]} [inline-json]", file=sys.stderr)
        sys.exit(1)

    repo_dir = Path(__file__).parent.parent.resolve()

    if len(sys.argv) == 2:
        changed_lines = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
        changed_files = [Path(repo_dir / f) for f in changed_lines.keys()
                         if any(Path(f).is_relative_to(subdir) for subdir in INCLUDED_SUBDIRS)]
        files_to_check = [f for f in changed_files if f.suffix in INCLUDED_EXTENSIONS]
    else:
        changed_lines = None
        files_to_check = []
        for subdir in INCLUDED_SUBDIRS:
            for ext in INCLUDED_EXTENSIONS:
                files_to_check += (repo_dir / subdir).glob(f"**/*{ext}")

    num_mistakes_found = 0
    for path in files_to_check:
        relpath = path.relative_to(repo_dir).as_posix()
        if relpath in EXCLUDED_SOURCES:
            continue

        code = path.read_text(encoding="utf-8")
        lines = code.split("\n")

        comment_lines = set()
        for m in COMMENT_PATTERN.finditer(code):
            start_offset, end_offset = m.span(1)
            start_line = offset_to_line(start_offset, code)
            end_line = offset_to_line(end_offset, code)
            for i in range(start_line, end_line + 1):
                comment_lines.add(i)

        for (description, pattern, *predicates) in COMMON_MISTAKES:
            for match in pattern.finditer(code):
                match_offset = match.start(1)
                line_number = offset_to_line(match_offset, code)

                if line_number in comment_lines:
                    continue

                prev_newline_offset = code.rfind("\n", 0, match_offset)
                if prev_newline_offset == -1:
                    prev_newline_offset = 0
                line_offset = match_offset - prev_newline_offset

                is_actual_mistake = True
                line = lines[line_number - 1]
                for (condition, parameter) in predicates:
                    if condition == "unless-line-matches":
                        if parameter.match(line) is not None:
                            is_actual_mistake = False
                    elif condition == "unless-found-inside":
                        for m in parameter.finditer(line):
                            start, end = m.span()
                            if line_offset >= start and line_offset < end:
                                is_actual_mistake = False
                                break
                    elif condition == "unless-true":
                        if parameter(match):
                            is_actual_mistake = False
                    else:
                        assert False, "unexpected condition"
                    if not is_actual_mistake:
                        break

                if is_actual_mistake \
                        and (changed_lines is None or line_number in changed_lines[relpath]):
                    print(f"{relpath}:{line_number}: {description}")
                    num_mistakes_found += 1

    sys.exit(0 if num_mistakes_found == 0 else 1)


def offset_to_line(i, code):
    return len(code[:i].split("\n"))


if __name__ == "__main__":
    main()
