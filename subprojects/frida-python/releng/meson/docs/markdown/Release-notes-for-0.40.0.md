---
title: Release 0.40
short-description: Release notes for 0.40
...

# New features

## Outputs of generators can be used in custom targets in the VS backend

This has been possible with the Ninja backend for a long time but now
the Visual Studio backend works too.

## `compute_int` method in the compiler objects

This method can be used to evaluate the value of an expression. As an
example:

```meson
cc = meson.get_compiler('c')
two = cc.compute_int('1 + 1') # A very slow way of adding two numbers.
```

## Visual Studio 2017 support

There is now a VS2017 backend (`--backend=vs2017`) as well as a
generic VS backend (`--backend=vs`) that autodetects the currently
active VS version.

## Automatic initialization of subprojects that are git submodules

If you have a directory inside your subprojects directory (usually
`subprojects/`) that is a git submodule, Meson will automatically
initialize it if your build files refer to it. This will be done
without needing a wrap file since git contains all the information
needed.

## No download mode for wraps

Added a new option `wrap-mode` that can be toggled to prevent Meson
from downloading dependency projects. Attempting to do so will cause
an error. This is useful for distro packagers and other cases where
you must explicitly enforce that nothing is downloaded from the net
during configuration or build.

## Overriding options per target

Build targets got a new keyword argument `override_options` that can
be used to override system options. As an example if you have a target
that you know can't be built with `-Werror` enabled you can override
the value of the option like this:

```meson
executable('foo', 'foo.c', override_options : ['werror=false'])
```

Note that this does not affect project options, only those options
that come from Meson (language standards, unity builds etc).

## Compiler object get define

Compiler objects got a new method `get_define()` that returns the
given preprocessor symbol as a string.

```meson
cc = meson.get_compiler('c')
one = cc.get_define('__linux__') # returns '1' on Linux hosts
```

## Cygwin support

Meson now works under Cygwin and we have added it to our CI test
matrix.

## Multiple install directories

Custom targets can produce many output files. Previously it was only
possible to install all of them in the same directory, but now you can
install each output in its own directory like this:

```meson
custom_target('two_out',
  output : ['diff.h', 'diff.sh'],
  command : [creator, '@OUTDIR@'],
  install : true,
  install_dir : ['dir1', 'dir2'])
```

For backwards compatibility and for conciseness, if you only specify
one directory all outputs will be installed into it.

The same feature is also available for Vala build targets. For
instance, to install a shared library built by valac, the generated
header, and the generated VAPI (respectively) into the default
locations, you can do:

```meson
shared_library('valalib', 'mylib.vala',
  install : true,
  install_dir : [true, true, true])
```

To install any of the three in a custom directory, just pass it as a
string instead of `true`. To not install it, pass `false`. You can
also pass a single string (as before) and it will cause only the
library to be installed, so this is a backwards-compatible change.

## Can specify method of obtaining dependencies

Some dependencies have many ways of being provided. As an example Qt
can either be detected via `pkg-config` or `qmake`. Until now Meson
has had a heuristic for selecting which method to choose but sometimes
it does the wrong thing. This can now be overridden with the `method`
keyword like this:

```meson
qt5_dep = dependency('qt5', modules : 'core', method : 'qmake')
```

## Link whole contents of static libraries

The default behavior of static libraries is to discard all symbols
that are not directly referenced. This may lead to exported
symbols being lost. Most compilers support "whole archive" linking
that includes all symbols and code of a given static library. This is
exposed with the `link_whole` keyword.

```meson
shared_library('foo', 'foo.c', link_whole : some_static_library)
```

Note that Visual Studio compilers only support this functionality on
VS 2015 and newer.

## Unity builds only for subprojects

Up until now unity builds were either done for every target or none of
them. Now unity builds can be specified to be enabled only for
subprojects, which change seldom, and not for the master project,
which changes a lot. This is enabled by setting the `unity` option to
`subprojects`.

## Running `mesonintrospect` from scripts

Meson now sets the `MESONINTROSPECT` environment variable in addition
to `MESON_SOURCE_ROOT` and other variables when running scripts. It is
guaranteed to point to the correct `mesonintrospect` script, which is
important when running Meson uninstalled from git or when your `PATH`s
may not be set up correctly.

Specifically, the following Meson functions will set it:
`meson.add_install_script()`, `meson.add_postconf_script()`,
`run_command()`, `run_target()`.
