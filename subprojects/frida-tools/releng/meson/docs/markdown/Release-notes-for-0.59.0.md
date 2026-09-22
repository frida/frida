---
title: Release 0.59.0
short-description: Release notes for 0.59.0
...

# New features

## Unescaped variables in pkgconfig files

Spaces in variable values are escaped with `\`, this is required in the case the
value is a path that and is used in `cflags` or `libs` arguments. This was an
undocumented behaviour that caused issues in the case the variable is a space
separated list of items.

For backward compatibility reasons this behaviour could not be changed, new
keyword arguments have thus been added: `unescaped_variables` and
`unescaped_uninstalled_variables`.

```meson
pkg = import('pkgconfig')
...
pkg.generate(lib,
  variables: {
    'mypath': '/path/with spaces/are/escaped',
  },
  unescaped_variables: {
    'mylist': 'Hello World Is Not Escaped',
  },
)
```

## The custom_target() function now accepts a feed argument

It is now possible to provide a `feed: true` argument to `custom_target()` to
pipe the target's input file to the program's standard input.

## Separate functions for qt preprocess

`qt.preprocess` is a large, complicated function that does a lot of things,
a new set of `compile_*` functions have been provided as well. These are
conceptually simpler, as they do a single thing.

## Cython as as first class language

Meson now supports Cython as a first class language. This means you can write:

```meson
project('my project', 'cython')

py = import('python').find_installation()
dep_py = py.dependency()

py.extension_module(
    'foo',
    'foo.pyx',
    dependencies : dep_py,
)
```

And avoid the step through a generator that was previously required.

## Support for the Wine Resource Compiler

Users can now choose `wrc` as the `windres` binary in their cross files and
`windows.compile_resources` will handle it correctly. Together with `winegcc`
patches in Wine 6.12 this enables basic support for compiling projects as a
winelib by specifying `winegcc`/`wineg++` as the compiler and `wrc` as the
resource compiler in a cross file.

## New `vs2012` and `vs2013` backend options

Adds the ability to generate Visual Studio 2012 and 2013 projects.  This is an
extension to the existing Visual Studio 2010 projects so that it is no longer
required to manually upgrade the generated Visual Studio 2010 projects.

Generating Visual Studio 2010 projects has also been fixed since its developer
command prompt does not provide a `%VisualStudioVersion%` envvar.

## Developer environment

Expand the support for the `link_whole:` project option for pre-Visual Studio 2015
Update 2, where previously Visual Studio 2015 Update 2 or later was required for
this, for the Ninja backend as well as the vs2010 (as well as the newly-added
vs2012 and vs2013 backends).

## Fs Module now accepts files objects

It is now possible to define a `files()` object and run most Fs module
functions on the file, rather than passing a string and hoping it is in the
same directory.


## Compiler argument checking for `get_supported_arguments`

The compiler method `get_supported_arguments` now supports
a new keyword argument named `checked` that can be set to
one of `warn`, `require` or `off` (defaults to `off`) to
enforce argument checks.

## New custom dependency for libintl

Meson can now find the library needed for translating messages via gettext.
This works both on systems where libc provides gettext, such as GNU or musl,
and on systems where the gettext project's standalone intl support library is
required, such as macOS.

Rather than doing something such as:

```
intl_dep = dependency('', required: false)

if cc.has_function('ngettext')
  intl_found = true
else
  intl_dep = cc.find_library('intl', required: false)
  intl_found = intl_dep.found()
endif

if intl_found
  # build options that need gettext
  conf.set('ENABLE_NLS', 1)
endif
```

one may simply use:

```
intl_dep = dependency('intl', required: false)

if intl_dep.found()
  # build options that need gettext
  conf.set('ENABLE_NLS', 1)
endif
```

## Parallelized `meson subprojects` commands

All `meson subprojects` commands are now run on each subproject in parallel by
default. The number of processes can be controlled with `--num-processes`
argument.

This speeds up considerably IO-bound operations such as downloads and git fetch.

## Using Vala no longer requires C in the project languages

Meson will now add C automatically. Since the use of C is an implementation
detail of Vala, Meson shouldn't require users to add it.

## The `import()` function gains `required` and `disabler` arguments

In addition, modules now have a `found()` method, like programs and
dependencies. This allows them to be conditionally required, and used in most
places that an object with a `found()` method can be.

## Objective C/C++ standard versions

Objective C and C++ compilations will from now on use the language
versions set in `c_std` and `cpp_std`, respectively. It is not
possible to set the language version separately for Objective C and
plain C.

## Qt.preprocess source arguments deprecated

The `qt.preprocess` method currently has this signature:
`qt.preprocess(name: str | None, *srcs: str)`, this is not a nice signature
because it's confusing, and there's a `sources` keyword argument as well.
Both of these pass sources through unmodified, this is a bit of a historical
accident, and not the way that any other module works. These have been
deprecated, so instead of:
```meson
sources = qt.preprocess(
    name,
    list, of, sources,
    sources : [more, sources],
    ... # things to process,
)

executable(
    'foo',
    sources,
)
```
use
```meson
processed = qt.preprocess(
    name,
    ... # thins to process
)

executable(
    'foo',
    'list', 'of', 'sources', 'more', 'sources', processed,
)
```

## New `build target` methods

The [[@build_tgt]] object now supports
the following two functions, to ensure feature compatibility with
[[@external_program]] objects:

- `found()`: Always returns `true`. This function is meant
  to make executables objects feature compatible with
  `external program` objects. This simplifies
  use-cases where an executable is used instead of an external program.

- `path()`: **(deprecated)** does the exact same as `full_path()`.
  **NOTE:** This function is solely kept for compatibility
  with `external program` objects. It will be
  removed once the, also deprecated, corresponding `path()` function in the
  `external program` object is removed.

## Automatically set up Visual Studio environment

When Meson is run on Windows it will automatically set up the
environment to use Visual Studio if no other compiler toolchain
can be detected. This means that you can run Meson commands from
any command prompt or directly from any IDE. This sets up the
64 bit native environment. If you need any other, then you
need to set it up manually as before.

## `gnome.compile_schemas()` sets `GSETTINGS_SCHEMA_DIR` into devenv

When using `gnome.compile_schemas()` the location of the compiled schema is
added to `GSETTINGS_SCHEMA_DIR` environment variable when using
[`meson devenv`](Commands.md#devenv) command.

## `update_desktop_database` added to `gnome.post_install()`

Applications that install a `.desktop` file containing a `MimeType` need to update
the cache upon installation. Most applications do that using a custom script,
but it can now be done by Meson directly.

See [`gnome.post_install()`](Gnome-module.md#gnomepost_install).
