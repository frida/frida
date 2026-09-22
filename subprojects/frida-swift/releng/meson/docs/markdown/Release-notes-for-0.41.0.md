---
title: Release 0.41
short-description: Release notes for 0.41
...

# New features

## Dependency Handler for LLVM

Native support for linking against LLVM using the `dependency` function.

## vcs_tag keyword fallback is now optional

The `fallback` keyword in `vcs_tag()` is now optional. If not given,
its value defaults to the return value of `meson.project_version()`.

## Better quoting of special characters in ninja command invocations

The ninja backend now quotes special characters that may be
interpreted by ninja itself, providing better interoperability with
custom commands. This support may not be perfect; please report any
issues found with special characters to the issue tracker.

## Pkgconfig support for custom variables

The Pkgconfig module object can add arbitrary variables to the generated .pc
file with the new `variables` keyword:
```meson
pkg.generate(libraries : libs,
             subdirs : h,
             version : '1.0',
             name : 'libsimple',
             filebase : 'simple',
             description : 'A simple demo library.',
             variables : ['datadir=${prefix}/data'])
```

## A target for creating tarballs

Creating distribution tarballs is simple:

    ninja dist

This will create a `.tar.xz` archive of the source code including
submodules without any revision control information. This command also
verifies that the resulting archive can be built, tested and
installed. This is roughly equivalent to the `distcheck` target in
other build systems. Currently this only works for projects using Git
and only with the Ninja backend.

## Support for passing arguments to Rust compiler

Targets for building rust now take a `rust_args` keyword.

## Code coverage export for tests

Code coverage can be generated for tests by passing the `--cov` argument to
the `run_tests.py` test runner. Note, since multiple processes are used,
coverage must be combined before producing a report (`coverage3 combine`).

## Reproducible builds

All known issues have been fixed and Meson can now build reproducible Debian
packages out of the box.

## Extended template substitution in configure_file

The output argument of `configure_file()` is parsed for @BASENAME@ and
@PLAINNAME@ substitutions.

## Cross-config property for overriding whether an exe wrapper is needed

The new `needs_exe_wrapper` property allows overriding auto-detection for
cases where `build_machine` appears to be compatible with `host_machine`,
but actually isn't. For example when:
- `build_machine` is macOS and `host_machine` is the iOS Simulator
- the `build_machine`'s libc is glibc but the `host_machine` libc is uClibc
- code relies on kernel features not available on the `build_machine`

## Support for capturing stdout of a command in configure_file

`configure_file()` now supports a new keyword - `capture`. When this argument
is set to true, Meson captures `stdout` of the `command` and writes it to
the target file specified as `output`.
