---
title: Release 0.45
short-description: Release notes for 0.45
...

# New features

## Python minimum version is now 3.5

Meson will from this version on require Python version 3.5 or newer.

## Config-Tool based dependencies can be specified in a cross file

Tools like LLVM and pcap use a config tool for dependencies, this is a
script or binary that is run to get configuration information (cflags,
ldflags, etc) from.

These binaries may now be specified in the `binaries` section of a
cross file.

```ini
[binaries]
cc = ...
llvm-config = '/usr/bin/llvm-config32'
```

## Visual Studio C# compiler support

In addition to the Mono C# compiler we also support Visual Studio's C#
compiler. Currently this is only supported on the Ninja backend.

## Removed two deprecated features

The standalone `find_library` function has been a no-op for a long
time. Starting with this version it becomes a hard error.

There used to be a keywordless version of `run_target` which looked
like this:

```meson
run_target('targetname', 'command', 'arg1', 'arg2')
```

This is now an error. The correct format for this is now:

```meson
run_target('targetname',
  command : ['command', 'arg1', 'arg2'])
```

## Experimental FPGA support

This version adds support for generating, analysing and uploading FPGA
programs using the [IceStorm
toolchain](http://www.clifford.at/icestorm/). This support is
experimental and is currently limited to the `iCE 40` series of FPGA
chips.

FPGA generation integrates with other parts of Meson seamlessly. As an
example, [here](https://github.com/jpakkane/lm32) is an example
project that compiles a simple firmware into Verilog and combines that
with an lm32 softcore processor.

## Generator outputs can preserve directory structure

Normally when generating files with a generator, Meson flattens the
input files so they all go in the same directory. Some code
generators, such as Protocol Buffers, require that the generated files
have the same directory layout as the input files used to generate
them. This can now be achieved like this:

```meson
g = generator(...) # Compiles protobuf sources
generated = gen.process('com/mesonbuild/one.proto',
  'com/mesonbuild/two.proto',
  preserve_path_from : meson.current_source_dir())
```

This would cause the following files to be generated inside the target
private directory:

    com/mesonbuild/one.pb.h
    com/mesonbuild/one.pb.cc
    com/mesonbuild/two.pb.h
    com/mesonbuild/two.pb.cc

## Hexadecimal string literals

Hexadecimal integer literals can now be used in build and option files.

```meson
int_255 = 0xFF
```

## b_ndebug : if-release

The value `if-release` can be given for the `b_ndebug` project option.

This will make the `NDEBUG` pre-compiler macro to be defined for
release type builds as if the `b_ndebug` project option had the
value `true` defined for it.

## `install_data()` defaults to `{datadir}/{projectname}`

If `install_data()` is not given an `install_dir` keyword argument, the
target directory defaults to `{datadir}/{projectname}` (e.g.
`/usr/share/myproj`).

## install_subdir() supports strip_directory

If strip_directory=true install_subdir() installs directory contents
instead of directory itself, stripping basename of the source directory.

## Integer options

There is a new integer option type with optional minimum and maximum
values. It can be specified like this in the `meson_options.txt` file:

```meson
option('integer_option', type : 'integer', min : 0, max : 5, value : 3)
```

## New method meson.project_license()

The `meson` builtin object now has a `project_license()` method that
returns a list of all licenses for the project.

## Rust cross-compilation

Cross-compilation is now supported for Rust targets. Like other
cross-compilers, the Rust binary must be specified in your cross file.
It should specify a `--target` (as installed by `rustup target`) and a
custom linker pointing to your C cross-compiler. For example:

```ini
[binaries]
c = '/usr/bin/arm-linux-gnueabihf-gcc-7'
rust = [
    'rustc',
    '--target', 'arm-unknown-linux-gnueabihf',
    '-C', 'linker=/usr/bin/arm-linux-gnueabihf-gcc-7',
]
```

## Rust compiler-private library disambiguation

When building a Rust target with Rust library dependencies, an
`--extern` argument is now specified to avoid ambiguity between the
dependency library, and any crates of the same name in `rustc`'s
private sysroot.

## Project templates

Meson ships with predefined project templates. To start a new project
from scratch, simply go to an empty directory and type:

    meson init --name=myproject --type=executable --language=c

## Improve test setup selection

Test setups are now identified (also) by the project they belong to
and it is possible to select the used test setup from a specific
project. E.g.  to use a test setup `some_setup` from project
`some_project` for all executed tests one can use

    meson test --setup some_project:some_setup

Should one rather want test setups to be used from the same project as
where the current test itself has been defined, one can use just

    meson test --setup some_setup

In the latter case every (sub)project must have a test setup `some_setup`
defined in it.

## Can use custom targets as Windows resource files

The `compile_resources()` function of the `windows` module can now be
used on custom targets as well as regular files.

## Can promote dependencies with wrap command

The `promote` command makes it easy to copy nested dependencies to the
top level.

    meson wrap promote scommon

This will search the project tree for a subproject called `scommon`
and copy it to the top level.

If there are many embedded subprojects with the same name, you have to
specify which one to promote manually like this:

    meson wrap promote subprojects/s1/subprojects/scommon

## Yielding subproject option to superproject

Normally project options are specific to the current project. However
sometimes you want to have an option whose value is the same over all
projects. This can be achieved with the new `yield` keyword for
options. When set to `true`, getting the value of this option in
`meson.build` files gets the value from the option with the same name
in the master project (if such an option exists).
