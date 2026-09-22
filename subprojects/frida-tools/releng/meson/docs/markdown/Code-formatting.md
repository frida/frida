---
short-description: Code formatting
...

# clang-format

*Since 0.50.0*

When `clang-format` is installed and a `.clang-format` file is found at the main
project's root source directory, Meson automatically adds a `clang-format` target
that reformat all C and C++ files (currently only with Ninja backend).

```sh
ninja -C builddir clang-format
```

*Since 0.58.0*

It is possible to restrict files to be reformatted with optional
`.clang-format-include` and `.clang-format-ignore` files.

The file `.clang-format-include` contains a list of patterns matching the files
that will be reformatted. The `**` pattern matches this directory and all
subdirectories recursively. Empty lines and lines starting with `#` are ignored.
If `.clang-format-include` is not found, the pattern defaults to `**/*` which
means all files recursively in the source directory but has the disadvantage to
walk the whole source tree which could be slow in the case it contains lots of
files.

Example of `.clang-format-include` file:
```
# All files in src/ and its subdirectories
src/**/*

# All files in include/ but not its subdirectories
include/*
```

The file `.clang-format-ignore` contains a list of patterns matching the files
that will be excluded. Files matching the include list (see above) that match
one of the ignore pattern will not be reformatted. Unlike include patterns, ignore
patterns does not support `**` and a single `*` match any characters including
path separators. Empty lines and lines starting with `#` are ignored.

The build directory and file without a well known C or C++ suffix are always
ignored.

Example of `.clang-format-ignore` file:
```
# Skip C++ files in src/ directory
src/*.cpp
```

Note that `.clang-format-ignore` has the same format as used by
[`run-clang-format.py`](https://github.com/Sarcasm/run-clang-format).

A new target `clang-format-check` has been added. It returns an error code if
any file needs to be reformatted. This is intended to be used by CI.

*Since 0.60.0*

If `.clang-format-include` file is missing and source files are in a git
repository, only files tracked by git will be included.
