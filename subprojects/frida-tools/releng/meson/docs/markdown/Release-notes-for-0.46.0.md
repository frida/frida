---
title: Release 0.46
short-description: Release notes for 0.46
...

# New features

## Allow early return from a script

Added the function `subdir_done()`. Its invocation exits the current
script at the point of invocation. All previously invoked build
targets and commands are build/executed. All following ones are
ignored. If the current script was invoked via `subdir()` the parent
script continues normally.

## Log output slightly changed

The format of some human-readable diagnostic messages has changed in
minor ways. In case you are parsing these messages, you may need to
adjust your code.

## ARM compiler for C and CPP

Cross-compilation is now supported for ARM targets using ARM compiler
- ARMCC. The current implementation does not support shareable
libraries. The default extension of the output is .axf. The
environment path should be set properly for the ARM compiler
executables. The '--cpu' option with the appropriate target type
should be mentioned in the cross file as shown in the snippet below.

```ini
[properties]
c_args      = ['--cpu=Cortex-M0plus']
cpp_args    = ['--cpu=Cortex-M0plus']

```

## Building both shared and static libraries

A new function `both_libraries()` has been added to build both shared
and static libraries at the same time. Source files will be compiled
only once and object files will be reused to build both shared and
static libraries, unless `b_staticpic` user option or `pic:` keyword
argument are set to false in which case sources will be compiled
twice.

The returned `buildtarget` object always represents the shared library.

## Compiler object can now be passed to run_command()

This can be used to run the current compiler with the specified
arguments to obtain additional information from it. One of the use
cases is to get the location of development files for the GCC plugins:

```meson
cc = meson.get_compiler('c')
result = run_command(cc, '-print-file-name=plugin')
plugin_dev_path = result.stdout().strip()
```

## declare_dependency() now supports `link_whole:`

`declare_dependency()` now supports the `link_whole:` keyword argument which
transparently works for build targets which use that dependency.

## Old command names are now errors

The old executable names `mesonintrospect`, `mesonconf`,
`mesonrewriter` and `mesontest` have been deprecated for a long time.
Starting from this version they no longer do anything but instead
always error out. All functionality is available as subcommands in the
main `meson` binary.

## Meson and meson configure now accept the same arguments

Previously Meson required that builtin arguments (like prefix) be
passed as `--prefix` to `meson` and `-Dprefix` to `meson configure`.
`meson` now accepts -D form like `meson configure` has. `meson
configure` also accepts the `--prefix` form, like `meson` has.

## Recursively extract objects

The `recursive:` keyword argument has been added to
`extract_all_objects()`. When set to `true` it will also return
objects passed to the `objects:` argument of this target. By default
only objects built for this target are returned to maintain backward
compatibility with previous versions. The default will eventually be
changed to `true` in a future version.

```meson
lib1 = static_library('a', 'source.c', objects : 'prebuilt.o')
lib2 = static_library('b', objects : lib1.extract_all_objects(recursive : true))
```

## Can override find_program()

It is now possible to override the result of `find_program` to point
to a custom program you want. The overriding is global and applies to
every subproject from there on. Here is how you would use it.

In master project

```meson
subproject('mydep')
```

In the called subproject:

```meson
prog = find_program('my_custom_script')
meson.override_find_program('mycodegen', prog)
```

In master project (or, in fact, any subproject):

```meson
genprog = find_program('mycodegen')
```

Now `genprog` points to the custom script. If the dependency had come
from the system, then it would point to the system version.

You can also use the return value of `configure_file()` to override
a program in the same way as above:

```meson
prog_script = configure_file(input : 'script.sh.in',
                             output : 'script.sh',
                             configuration : cdata)
meson.override_find_program('mycodegen', prog_script)
```

## New functions: has_link_argument() and friends

A new set of methods has been added to [[@compiler]]
objects to test if the linker
supports given arguments.

- [[compiler.has_link_argument]]
- [[compiler.has_multi_link_arguments]]
- [[compiler.get_supported_link_arguments]]
- [[compiler.first_supported_link_argument]]

## "meson help" now shows command line help

Command line parsing is now less surprising. "meson help" is now
equivalent to "meson --help" and "meson help <subcommand>" is
equivalent to "meson <subcommand> --help", instead of creating a build
directory called "help" in these cases.

## Autogeneration of simple meson.build files

A feature to generate a meson.build file compiling given C/C++ source
files into a single executable has been added to "meson init". By
default, it will take all recognizable source files in the current
directory. You can also specify a list of dependencies with the -d
flag and automatically invoke a build with the -b flag to check if the
code builds with those dependencies.

For example,

```meson
meson init -fbd sdl2,gl
```

will look for C or C++ files in the current directory, generate a
meson.build for them with the dependencies of sdl2 and gl and
immediately try to build it, overwriting any previous meson.build and
build directory.

## install_data() supports `rename:`

The `rename:` keyword argument is used to change names of the installed
files. Here's how you install and rename the following files:

- `file1.txt` into `share/myapp/dir1/data.txt`
- `file2.txt` into `share/myapp/dir2/data.txt`

```meson
install_data(['file1.txt', 'file2.txt'],
             rename : ['dir1/data.txt', 'dir2/data.txt'],
             install_dir : 'share/myapp')
```

## Support for lcc compiler for e2k (Elbrus) architecture

In this version, a support for lcc compiler for Elbrus processors
based on [e2k
microarchitecture](https://en.wikipedia.org/wiki/Elbrus_2000) has been
added.

Examples of such CPUs:
* [Elbrus-8S](https://en.wikipedia.org/wiki/Elbrus-8S);
* Elbrus-4S;
* [Elbrus-2S+](https://en.wikipedia.org/wiki/Elbrus-2S%2B).

Such compiler have a similar behavior as gcc (basic option compatibility),
but, in is not strictly compatible with gcc as of current version.

Major differences as of version 1.21.22:
* it does not support LTO and PCH;
* it suffers from the same dependency file creation error as icc;
* it has minor differences in output, especially version output;
* it differently reacts to lchmod() detection;
* some backend messages are produced in ru_RU.KOI8-R even if LANG=C;
* its preprocessor treats some characters differently.

So every noted difference is properly handled now in Meson.

## String escape character sequence update

Single-quoted strings in Meson have been taught the same set of escape
sequences as in Python. It is therefore now possible to use arbitrary
bytes in strings, like for example `NUL` (`\0`) and other ASCII
control characters. See the chapter about [*Strings* in
*Syntax*](Syntax.md#strings) for more details.

Potential backwards compatibility issue: Any valid escape sequence
according to the new rules will be interpreted as an escape sequence
instead of the literal characters. Previously only the following
escape sequences were supported in single-quote strings: `\'`, `\\`
and `\n`.

Note that the behaviour of triple-quoted (multiline) strings has not
changed. They behave like raw strings and do not support any escape
sequences.

## New `forcefallback` wrap mode

A new wrap mode was added, `--wrap-mode=forcefallback`. When this is
set, dependencies for which a fallback was provided will always use
it, even if an external dependency exists and satisfies the version
requirements.

## Relaxing of target name requirements

In earlier versions of Meson you could only have one target of a given
name for each type. For example you could not have two executables
named `foo`. This requirement is now relaxed so that you can have
multiple targets with the same name, as long as they are in different
subdirectories.

Note that projects that have multiple targets with the same name can
not be built with the `flat` layout or any backend that writes outputs
in the same directory.

## Addition of OpenMP dependency

An OpenMP dependency (`openmp`) has been added that encapsulates the
various flags used by compilers to enable OpenMP and checks for the
existence of the `omp.h` header. The `language` keyword may be passed
to force the use of a specific compiler for the checks.

## Added new partial_dependency method to dependencies and libraries

It is now possible to use only part of a dependency in a target. This
allows, for example, to only use headers with convenience libraries to
avoid linking to the same library multiple times.

```meson
dep = dependency('xcb')

helper = static_library(
  'helper',
  ['helper1.c', 'helper2.c'],
  dependencies : dep.partial_dependency(includes : true),
]

final = shared_library(
  'final',
  ['final.c'],
  dependencies : dep,
)
```

A partial dependency will have the same name version as the full
dependency it is derived from, as well as any values requested.

## Improved generation of pkg-config files for static only libraries.

Previously pkg-config files generated by the pkgconfig modules for
static libraries with dependencies could only be used in a
dependencies with `static: true`.

Now the generated file contains the needed dependencies libraries
directly within `Requires` and `Libs` for build static libraries
passed via the `libraries` keyword argument.

Projects that install both a static and a shared version of a library
should use the result of
[[both_libraries]] to the
pkg-config file generator or use
[[configure_file]] for more
complicated setups.

## Improvements to pkgconfig module

A `StaticLibrary` or `SharedLibrary` object can optionally be passed
as first positional argument of the `generate()` method. If one is provided a
default value will be provided for all required fields of the pc file:
- `install_dir` is set to `pkgconfig` folder in the same location than the provided library.
- `description` is set to the project's name followed by the library's name.
- `name` is set to the library's name.

Generating a .pc file is now as simple as:

```meson
pkgconfig.generate(mylib)
```

## pkgconfig.generate() requires parameters non-string arguments

`pkgconfig.generate()` `requires:` and `requires_private:` keyword
arguments now accept pkgconfig-dependencies and libraries that
pkgconfig-files were generated for.

## Generic python module

Meson now has is a revamped and generic (python 2 and 3) version of
the python3 module. With [this new interface](Python-module.md),
projects can now fully specify the version of python they want to
build against / install sources to, and can do so against multiple
major or minor versions in parallel.

## test() now supports the `depends:` keyword argument

Build targets and custom targets can be listed in the `depends:`
keyword argument of test function. These targets will be built before
test is run even if they have `build_by_default : false`.
