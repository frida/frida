---
title: Release 0.47
short-description: Release notes for 0.47
...

# New features

## Allow early return from a script

Added the function `subdir_done()`. Its invocation exits the current
script at the point of invocation. All previously invoked build
targets and commands are build/executed. All following ones are
ignored. If the current script was invoked via `subdir()` the parent
script continues normally.

## Concatenate string literals returned from `get_define()`

After obtaining the value of a preprocessor symbol consecutive string
literals are merged into a single string literal. For example a
preprocessor symbol's value `"ab" "cd"` is returned as `"abcd"`.

## ARM compiler(version 6) for C and CPP

Cross-compilation is now supported for ARM targets using ARM compiler
version 6 - ARMCLANG. The required ARMCLANG compiler options for
building a shareable library are not included in the current Meson
implementation for ARMCLANG support, so it cannot build shareable
libraries. This current Meson implementation for ARMCLANG support can
not build assembly files with arm syntax (we need to use armasm
instead of ARMCLANG for the `.s` files with this syntax) and only
supports GNU syntax.

The default extension of the executable output is `.axf`. The
environment path should be set properly for the ARM compiler
executables. The `--target`, `-mcpu` options with the appropriate
values should be mentioned in the cross file as shown in the snippet
below.

```ini
[properties]
c_args      = ['--target=arm-arm-none-eabi', '-mcpu=cortex-m0plus']
cpp_args    = ['--target=arm-arm-none-eabi', '-mcpu=cortex-m0plus']
```

Note:
- The current changes are tested on Windows only.
- PIC support is not enabled by default for ARM,
  if users want to use it, they need to add the required arguments
  explicitly from cross-file(`c_args`/`cpp_args`) or some other way.

## New base build option for LLVM (Apple) bitcode support

When building with clang on macOS, you can now build your static and
shared binaries with embedded bitcode by enabling the `b_bitcode`
[base option](Builtin-options.md#base-options) by passing
`-Db_bitcode=true` to Meson.

This is better than passing the options manually in the environment
since Meson will automatically disable conflicting options such as
`b_asneeded`, and will disable bitcode support on targets that don't
support it such as `shared_module()`.

Since this requires support in the linker, it is currently only
enabled when using Apple ld. In the future it can be extended to clang
on other platforms too.

## New compiler check: `check_header()`

The existing compiler check `has_header()` only checks if the header
exists, either with the `__has_include` C++11 builtin, or by running
the pre-processor.

However, sometimes the header you are looking for is unusable on some
platforms or with some compilers in a way that is only detectable at
compile-time. For such cases, you should use `check_header()` which
will include the header and run a full compile.

Note that `has_header()` is much faster than `check_header()`, so it
should be used whenever possible.

## New action `copy:` for `configure_file()`

In addition to the existing actions `configuration:` and `command:`,
[[configure_file]] now accepts a
keyword argument `copy:` which specifies a new action to copy the file
specified with the `input:` keyword argument to a file in the build
directory with the name specified with the `output:` keyword argument.

These three keyword arguments are, as before, mutually exclusive. You
can only do one action at a time.

## New keyword argument `encoding:` for `configure_file()`

Add a new keyword to
[[configure_file]] that allows
the developer to specify the input and output file encoding. The
default value is the same as before: UTF-8.

In the past, Meson would not handle non-UTF-8/ASCII files correctly,
and in the worst case would try to coerce it to UTF-8 and mangle the
data. UTF-8 is the standard encoding now, but sometimes it is
necessary to process files that use a different encoding.

For additional details see [#3135](https://github.com/mesonbuild/meson/pull/3135).

## New keyword argument `output_format:` for `configure_file()`

When called without an input file, `configure_file` generates a C
header file by default. A keyword argument was added to allow
specifying the output format, for example for use with nasm or yasm:

```meson
conf = configuration_data()
conf.set('FOO', 1)

configure_file('config.asm',
  configuration: conf,
  output_format: 'nasm')
```

## Substitutions in `custom_target(depfile:)`

The `depfile` keyword argument to `custom_target` now accepts the `@BASENAME@`
and `@PLAINNAME@` substitutions.

## Deprecated `build_always:` for custom targets

Setting `build_always` to `true` for a custom target not only marks
the target to be always considered out of date, but also adds it to
the set of default targets. This option is therefore deprecated and
the new option `build_always_stale` is introduced.

`build_always_stale` *only* marks the target to be always considered
out of date, but does not add it to the set of default targets. The
old behaviour can be achieved by combining `build_always_stale` with
`build_by_default`.

The documentation has been updated accordingly.

## New built-in object type: dictionary

Meson dictionaries use a syntax similar to python's dictionaries, but
have a narrower scope: they are immutable, keys can only be string
literals, and initializing a dictionary with duplicate keys causes a
fatal error.

Example usage:

```meson
d = {'foo': 42, 'bar': 'baz'}

foo = d.get('foo')
foobar = d.get('foobar', 'fallback-value')

foreach key, value : d
  Do something with key and value
endforeach
```

## Array options treat `-Dopt=` and `-Dopt=[]` as equivalent

Prior to this change passing -Dopt= to an array opt would be
interpreted as `['']` (an array with an empty string), now `-Dopt=` is
the same as `-Dopt=[]`, an empty list.

## Feature detection based on `meson_version:` in `project()`

Meson will now print a `WARNING:` message during configuration if you
use a function or a keyword argument that was added in a Meson version
that's newer than the version specified inside `project()`. For
example:

```meson
project('featurenew', meson_version: '>=0.43')

cdata = configuration_data()
cdata.set('FOO', 'bar')
message(cdata.get_unquoted('FOO'))
```

This will output:

```
The Meson build system
Version: 0.47.0.dev1
Source dir: C:\path\to\srctree
Build dir: C:\path\to\buildtree
Build type: native build
Project name: featurenew
Project version: undefined
Build machine cpu family: x86_64
Build machine cpu: x86_64
WARNING: Project targeting '>=0.43' but tried to use feature introduced in '0.44.0': configuration_data.get_unquoted()
Message: bar
Build targets in project: 0
WARNING: Project specifies a minimum meson_version '>=0.43' which conflicts with:
 * 0.44.0: {'configuration_data.get_unquoted()'}
```

## New type of build option for features

A new type of [option called `feature`](Build-options.md#features) can
be defined in `meson_options.txt` for the traditional `enabled /
disabled / auto` tristate. The value of this option can be passed to
the `required` keyword argument of functions `dependency()`,
`find_library()`, `find_program()` and `add_languages()`.

A new global option `auto_features` has been added to override the
value of all `auto` features. It is intended to be used by packagers
to have full control on which feature must be enabled or disabled.

## New options to `gnome.gdbus_codegen()`

You can now pass additional arguments to gdbus-codegen using the
`extra_args` keyword. This is the same for the other gnome function
calls.

Meson now automatically adds autocleanup support to the generated
code. This can be modified by setting the autocleanup keyword.

For example:

```meson
sources += gnome.gdbus_codegen('com.mesonbuild.Test',
  'com.mesonbuild.Test.xml',
  autocleanup : 'none',
  extra_args : ['--pragma-once'])
```

## Made 'install' a top level Meson command

You can now run `meson install` in your build directory and it will do
the install. It has several command line options you can toggle the
behaviour that is not in the default `ninja install` invocation. This
is similar to how `meson test` already works.

For example, to install only the files that have changed, you can do:

```console
$ meson install --only-changed
```

## `install_mode:` keyword argument extended to all installable targets

It is now possible to pass an `install_mode` argument to all
installable targets, such as `executable()`, libraries, headers, man
pages and custom/generated targets.

The `install_mode` argument can be used to specify the file mode in
symbolic format and optionally the owner/uid and group/gid for the
installed files.

## New built-in option `install_umask` with a default value 022

This umask is used to define the default permissions of files and
directories created in the install tree. Files will preserve their
executable mode, but the exact permissions will obey the
`install_umask`.

The `install_umask` can be overridden in the Meson command-line:

```console
$ meson --install-umask=027 builddir/
```

A project can also override the default in the `project()` call:

```meson
project('myproject', 'c',
  default_options : ['install_umask=027'])
```

To disable the `install_umask`, set it to `preserve`, in which case
permissions are copied from the files in their origin.

## Octal and binary string literals

Octal and binary integer literals can now be used in build and option files.

```meson
int_493 = 0o755
int_1365 = 0b10101010101
```

## New keyword arguments: 'check' and 'capture' for `run_command()`

If `check:` is `true`, then the configuration will fail if the command
returns a non-zero exit status. The default value is `false` for
compatibility reasons.

`run_command()` used to always capture the output and stored it for
use in build files. However, sometimes the stdout is in a binary
format which is meant to be discarded. For that case, you can now set
the `capture:` keyword argument to `false`.

## Windows resource files dependencies

The `compile_resources()` function of the `windows` module now takes
the `depend_files:` and `depends:` keywords.

When using binutils's `windres`, dependencies on files `#include`'d by
the preprocessor are now automatically tracked.

## Polkit support for privileged installation

When running `install`, if installation fails with a permission error
and `pkexec` is available, Meson will attempt to use it to spawn a
permission dialog for privileged installation and retry the
installation.

If `pkexec` is not available, the old behaviour is retained and you
will need to explicitly run the install step with `sudo`.
