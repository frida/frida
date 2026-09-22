---
title: Release 0.50.0
short-description: Release notes for 0.50.0
...

# New features

## Added `cmake_module_path` and `cmake_args` to dependency

The CMake dependency backend can now make use of existing
`Find<name>.cmake` files by setting the `CMAKE_MODULE_PATH` with the
new `dependency()` property `cmake_module_path`. The paths given to
`cmake_module_path` should be relative to the project source
directory.

Furthermore the property `cmake_args` was added to give CMake
additional parameters.

## Added PGI compiler support

Nvidia / PGI C, C++ and Fortran
[no-cost](https://www.pgroup.com/products/community.htm) compilers are
now supported. They have been tested on Linux so far.



## Fortran Coarray

Fortran 2008 / 2018 coarray support was added via `dependency('coarray')`

## Libdir defaults to `lib` when cross compiling

Previously `libdir` defaulted to the value of the build machine such
as `lib/x86_64-linux-gnu`, which is almost always incorrect when cross
compiling. It now defaults to plain `lib` when cross compiling. Native
builds remain unchanged and will point to the current system's library
dir.

## Native and Cross File Paths and Directories

A new `[paths]` section has been added to native and cross files. This
can be used to set paths such a prefix and libdir in a persistent way.

## Add warning_level 0 option

Adds support for a warning level 0 which does not enable any static
analysis checks from the compiler

## A builtin target to run clang-format

If you have `clang-format` installed and there is a `.clang-format`
file in the root of your master project, Meson will generate a run
target called `clang-format` so you can reformat all files with one
command:

```meson
ninja clang-format
```


## Added `.path()` method to object returned by `python.find_installation()`

`ExternalProgram` objects as well as the object returned by the
`python3` module provide this method, but the new `python` module did
not.

## Fix ninja console log from generators with multiple output nodes

This resolves [issue
#4760](https://github.com/mesonbuild/meson/issues/4760) where a
generator with multiple output nodes printed an empty string to the
console

## `introspect --buildoptions` can now be used without configured build directory

It is now possible to run `meson introspect --buildoptions /path/to/meson.build`
without a configured build directory.

Running `--buildoptions` without a build directory produces the same
output as running it with a freshly configured build directory.

However, this behavior is not guaranteed if subprojects are
present. Due to internal limitations all subprojects are processed
even if they are never used in a real Meson run.  Because of this
options for the subprojects can differ.

## `include_directories` accepts a string

The `include_directories` keyword argument now accepts plain strings
rather than an include directory object. Meson will transparently
expand it so that a declaration like this:

```meson
executable(..., include_directories: 'foo')
```

Is equivalent to this:

```meson
foo_inc = include_directories('foo')
executable(..., include_directories: foo_inc)
```

## Fortran submodule support

Initial support for Fortran `submodule` was added, where the submodule
is in the same or different file than the parent `module`. The
submodule hierarchy specified in the source Fortran code `submodule`
statements are used by Meson to resolve source file dependencies. For
example:

```fortran
submodule (ancestor:parent) child
```


## Add `subproject_dir` to `--projectinfo` introspection output

This allows applications interfacing with Meson (such as IDEs) to know
about an overridden subproject directory.

## Find library with its headers

The `find_library()` method can now also verify if the library's
headers are found in a single call, using the `has_header()` method
internally.

```meson
# Aborts if the 'z' library is found but not its header file
zlib = find_library('z', has_headers : 'zlib.h')
# Returns not-found if the 'z' library is found but not its header file
zlib = find_library('z', has_headers : 'zlib.h', required : false)
```

Any keyword argument with the `header_` prefix passed to
`find_library()` will be passed to the `has_header()` method with the
prefix removed.

```meson
libfoo = find_library('foo',
  has_headers : ['foo.h', 'bar.h'],
  header_prefix : '#include <baz.h>',
  header_include_directories : include_directories('.'))
```

## NetCDF

NetCDF support for C, C++ and Fortran is added via pkg-config.

## Added the Flang compiler

[Flang](https://github.com/flang-compiler/flang/releases) Fortran
compiler support was added.  As with other Fortran compilers, flang is
specified using `FC=flang meson ..` or similar.

## New `not_found_message` for `dependency()`

You can now specify a `not_found_message` that will be printed if the
specified dependency was not found. The point is to convert constructs
that look like this:

```meson
d = dependency('something', required: false)
if not d.found()
  message('Will not be able to do something.')
endif
```

Into this:

```meson
d = dependency('something',
  required: false,
  not_found_message: 'Will not be able to do something.')
```

Or constructs like this:

```meson
d = dependency('something', required: false)
if not d.found()
  error('Install something by doing XYZ.')
endif
```

into this:

```meson
d = dependency('something',
  not_found_message: 'Install something by doing XYZ.')
```

Which works, because the default value of `required` is `true`.

## Cuda support

Compiling Cuda source code is now supported, though only with the
Ninja backend. This has been tested only on Linux for now.

Because NVidia's Cuda compiler does not produce `.d` dependency files,
dependency tracking does not work.

## `run_command()` accepts `env` kwarg

You can pass [[@env]]
object to [[run_command]], just
like to `test`:

```meson
env = environment()
env.set('FOO', 'bar')
run_command('command', 'arg1', 'arg2', env: env)
```

## `extract_objects:` accepts `File` arguments

The `extract_objects` function now supports File objects to tell it
what to extract. Previously, file paths could only be passed as strings.

## Changed the JSON format of the introspection

All paths used in the Meson introspection JSON format are now
absolute. This affects the `filename` key in the targets introspection
and the output of `--buildsystem-files`.

Furthermore, the `filename` and `install_filename` keys in the targets
introspection are now lists of strings with identical length.

The `--target-files` option is now deprecated, since the same information
can be acquired from the `--targets` introspection API.

## Meson file rewriter

This release adds the functionality to perform some basic modification
on the `meson.build` files from the command line. The currently
supported operations are:

- For build targets:
  - Add/Remove source files
  - Add/Remove targets
  - Modify a select set of kwargs
  - Print some JSON information
- For dependencies:
  - Modify a select set of kwargs
- For the project function:
  - Modify a select set of kwargs
  - Modify the default options list

For more information see the rewriter documentation.

## `introspect --scan-dependencies` can now be used to scan for dependencies used in a project

It is now possible to run `meson introspect --scan-dependencies
/path/to/meson.build` without a configured build directory to scan for
dependencies.

The output format is as follows:

```json
[
  {
    "name": "The name of the dependency",
    "required": true,
    "conditional": false,
    "has_fallback": false
  }
]
```

The `required` keyword specifies whether the dependency is marked as
required in the `meson.build` (all dependencies are required by
default). The `conditional` key indicates whether the `dependency()`
function was called inside a conditional block. In a real Meson run
these dependencies might not be used, thus they _may_ not be required,
even if the `required` key is set. The `has_fallback` key just
indicates whether a fallback was directly set in the `dependency()`
function.

## `introspect --targets` can now be used without configured build directory

It is now possible to run `meson introspect --targets
/path/to/meson.build` without a configured build directory.

The generated output is similar to running the introspection with a
build directory. However, there are some key differences:

- The paths in `filename` now are _relative_ to the future build directory
- The `install_filename` key is completely missing
- There is only one entry in `target_sources`:
  - With the language set to `unknown`
  - Empty lists for `compiler` and `parameters` and `generated_sources`
  - The `sources` list _should_ contain all sources of the target

There is no guarantee that the sources list in `target_sources` is
correct. There might be differences, due to internal limitations. It
is also not guaranteed that all targets will be listed in the output.
It might even be possible that targets are listed, which won't exist
when Meson is run normally. This can happen if a target is defined
inside an if statement. Use this feature with care.

## Added option to introspect multiple parameters at once

Meson introspect can now print the results of multiple introspection
commands in a single call. The results are then printed as a single
JSON object.

The format for a single command was not changed to keep backward
compatibility.

Furthermore the option `-a,--all`, `-i,--indent` and
`-f,--force-object-output` were added to print all introspection
information in one go, format the JSON output (the default is still
compact JSON) and force use the new output format, even if only one
introspection command was given.

A complete introspection dump is also stored in the `meson-info`
directory. This dump will be (re)generated each time meson updates the
configuration of the build directory.

Additionally the format of `meson introspect target` was changed:

  - New: the `sources` key. It stores the source files of a target and their compiler parameters.
  - New: the `defined_in` key. It stores the Meson file where a target is defined
  - New: the `subproject` key. It stores the name of the subproject where a target is defined.
  - Added new target types (`jar`, `shared module`).

## `meson configure` can now print the default options of an unconfigured project

With this release, it is also possible to get a list of all build
options by invoking `meson configure` with the project source
directory or the path to the root `meson.build`. In this case, Meson
will print the default values of all options.

## HDF5

HDF5 support is added via pkg-config.

## Added the `meson-info.json` introspection file

Meson now generates a `meson-info.json` file in the `meson-info`
directory to provide introspection information about the latest Meson
run. This file is updated when the build configuration is changed and
the build files are (re)generated.

## New kwarg `install:` for `configure_file()`

Previously when using `configure_file()`, you could install the
outputted file by setting the `install_dir:` keyword argument. Now,
there is an explicit kwarg `install:` to enable/disable it. Omitting
it will maintain the old behaviour.
