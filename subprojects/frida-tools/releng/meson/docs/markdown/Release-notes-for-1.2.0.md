---
title: Release 1.2.0
short-description: Release notes for 1.2.0
...

# New features

Meson 1.2.0 was released on 17 July 2023
## Added Metrowerks C/C++ toolchains

Added support for the Metrowerks Embedded ARM and Metrowerks Embedded PowerPC toolchains (https://www.nxp.com/docs/en/reference-manual/CWMCUKINCMPREF.pdf).

The implementation is somewhat experimental. It has been tested on a few projects and works fairly well, but may have issues.

## Added str.splitlines method

[[str.splitlines]] can now be used to split a string into an array of lines.

## `generator.process(generator.process(...))`

Added support for code like this:
```meson
gen1 = generator(...)
gen2 = generator(...)
gen2.process(gen1.process('input.txt'))
```

## Extra files keyword in `declare_dependency`

`declare_dependency` have a new `extra_files` keyword,
to add extra files to a target. It is used mostly for IDE integration.

## Added a new '--genvslite' option for use with 'meson setup ...'

To facilitate a more usual visual studio work-flow of supporting and switching between
multiple build configurations (buildtypes) within the same solution, among other
[reasons](https://github.com/mesonbuild/meson/pull/11049), use of this new option
has the effect of setting up multiple ninja back-end-configured build directories,
named with their respective buildtype suffix.  E.g. 'somebuilddir_debug',
'somebuilddir_release', etc. as well as a '_vs'-suffixed directory that contains the
generated multi-buildtype solution.  Building/cleaning/rebuilding in the solution
now launches the meson build (compile) of the corresponding buildtype-suffixed build
directory, instead of using Visual Studio's native engine.

## `gnome.generate_gir()` now supports `env` kwarg

`gnome.generate_gir()` now accepts the `env` kwarg which lets you set environment variables.

## More data in introspection files

- Used compilers are listed in `intro-compilers.json`
- Informations about `host`, `build` and `target` machines 
  are lister in `intro-machines.json`
- `intro-dependencies.json` now includes internal dependencies,
  and relations between dependencies.
- `intro-targets.json` now includes dependencies, `vs_module_defs`,
  `win_subsystem`, and linker parameters.

## Machine objects get `kernel` and `subsystem` properties

Meson has traditionally provided a `system` property to detect the
system being run on. However this is not enough to reliably
differentiate between e.g. an iOS platform from a watchOS one. Two new
properties, namely `kernel` and `subsystem` have been added so these
setups can be reliably detected.

These new properties are not necessary in cross files for now, but if
they are not defined and a build file tries to access them, Meson will
exit with a hard error. It is expected that at some point in the
future defining the new properties will become mandatory.

## default_options and override_options may now be dictionaries

Instead of passing them as `default_options : ['key=value']`, they can now be
passed as `default_options : {'key': 'value'}`, and the same for
`override_options`.

## New override of `find_program('meson')`

In some cases, it has been useful for build scripts to access the Meson command
used to invoke the build script. This has led to various ad-hoc solutions that
can be very brittle and project-specific.

```meson
meson_prog = find_program('meson')
```

This call will supply the build script with an external program pointing at the
invoked Meson.

Because Meson also uses `find_program` for program lookups internally, this
override will also be handled in cases similar to the following:

```meson
custom_target(
  # ...
  command: [
    'meson',
  ],
  # ...
)

run_command(
  'meson',
  # ...
)

run_target(
  'tgt',
  command: [
    'meson',
    # ...
  ]
)
```

## Find more specific python version on Windows

You can now use `python3.x`, where `x` is the minor version,
to find a more specific version of python on Windows, when
using the python module. On other platforms, it was already
working as `python3.x` is the executable name.

## Python module can now compile bytecode

A new builtin option is available: `-Dpython.bytecompile=2`. It can be used to
compile bytecode for all pure python files installed via the python module.

## rust.bindgen allows passing extra arguments to rustc

This may be necessary to pass extra `cfg`s or to change warning levels.

## Support for defining crate names of Rust dependencies in Rust targets

Rust supports defining a different crate name for a dependency than what the
actual crate name during compilation of that dependency was.

This allows using multiple versions of the same crate at once, or simply using
a shorter name of the crate for convenience.

```meson
a_dep = dependency('some-very-long-name')

my_executable = executable('my-executable', 'src/main.rs',
  rust_dependency_map : {
    'some_very_long_name' : 'a',
  },
  dependencies : [a_dep],
)
```

## A machine file may be used to pass extra arguments to clang in a bindgen call

Because of the way that bindgen proxies arguments to clang the only choice to
add extra arguments currently is to wrap bindgen in a script, since the
arguments must come after a `--`. This is inelegant, and not very portable. Now
a `bindgen_clang_arguments` field may be placed in the machine file for the host
machine, and these arguments will be added to every bindgen call for clang. This
is intended to be useful for things like injecting `--target` arguments.

## Add a `link_with` keyword to `rust.test()`

This can already be be worked around by creating `declare_dependency()` objects
to pass to the `dependencies` keyword, but this cuts out the middle man.

## Rust now supports the b_ndebug option

Which controls the `debug_assertions` cfg, which in turn controls
`debug_assert!()` macro. This macro is roughly equivalent to C's `assert()`, as
it can be toggled with command line options, unlike Rust's `assert!()`, which
cannot be turned off, and is not designed to be.

## Wildcards in list of tests to run

The `meson test` command now accepts wildcards in the list of test names.
For example `meson test basic*` will run all tests whose name begins
with "basic".

meson will report an error if the given test name does not match any
existing test. meson will log a warning if two redundant test names
are given (for example if you give both "proj:basic" and "proj:").

## New for the generation of Visual Studio vcxproj projects

When vcxproj is generated, another file vcxproj.filters is generated in parallel.
It enables to set a hierarchy of the files inside the solution following their place on filesystem.

