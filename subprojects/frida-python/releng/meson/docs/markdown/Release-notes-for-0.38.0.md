---
title: Release 0.38
short-description: Release notes for 0.38
...

## Uninstall target

Meson allows you to uninstall an install step by invoking the
uninstall target. This will remove all files installed as part of
install. Note that this does not restore the original files. This also
does not undo changes done by custom install scripts (because they can
do arbitrary install operations).

## Support for arbitrary test setups

Sometimes you need to run unit tests with special settings. For
example under Valgrind. Usually this requires extra command line
options for the tool. This is supported with the new *test setup*
feature. For example to set up a test run with Valgrind, you'd write
this in a `meson.build` file:

```meson
add_test_setup('valgrind',
  exe_wrapper : [vg, '--error-exitcode=1', '--leak-check=full'],
  timeout_multiplier : 100)
```

This tells Meson to run tests with Valgrind using the given options
and multiplying the test timeout values by 100. To run this test setup
simply issue the following command:

```console
$ mesontest --setup=valgrind
```

## Intel C/C++ compiler support

As usual, just set `CC=icc CXX=icpc` and Meson will use it as the
C/C++ compiler. Currently only Linux is supported.

## Get values from configuration data objects

Now it is possible to query values stored in configuration data
objects.

```meson
cdata.set('key', 'value')
cdata.get('key') # returns 'value'
cdata.get('nokey', 'default') # returns 'default'
cdata.get('nokey') # halts with an error
```

## Python 3 module support

Building Python 3 extension modules has always been possible, but it
is now even easier:

```meson
py3_mod = import('python3')
pylib = py3_mod.extension_module('modname',
  'modsource.c',
  dependencies : py3_dep)
```

## Default options to subprojects

Projects can specify overriding values for subprojects'
`default_options` when invoking a subproject:

```meson
subproject('foo', default_options : ['optname=overridevalue'])
dependency('some-dep', fallback : ['some_subproject', 'some_dep'], default_options : ['optname=overridevalue'])
```

The effect is the same as if the default options were written in the
subproject's `project` call.

## Set targets to be built (or not) by default

Build targets got a new keyword `build_by_default` which tells whether
the target should be built by default when running e.g. `ninja`.
Custom targets are not built by default but other targets are. Any
target that is tagged as installed or to be built always is also built
by default, regardless of the value of this keyword.

## Add option to mesonconf to wipe cached data.

Meson caches the results of dependency lookups. Sometimes these may
get out of sync with the system state. Mesonconf now has a
`--clearcache` option to clear these values so they will be
re-searched from the system upon next compile.

## Can specify file permissions and owner when installing data

The new `install_mode` keyword argument can be used to specify file
permissions and uid/gid of files when doing the install. This allows
you to, for example, install suid root scripts.

## `has_header()` checks are now faster

When using compilers that implement the [`__has_include()`
preprocessor
macro](https://clang.llvm.org/docs/LanguageExtensions.html#include-file-checking-macros),
the check is now ~40% faster.

## Array indexing now supports fallback values

The second argument to the array
[[list.get]] function is now returned
if the specified index could not be found

```meson
array = [10, 11, 12, 13]
array.get(0) # this will return `10`
array.get(4) # this will give an error about invalid index
array.get(4, 0) # this will return `0`
```

## Silent mode for Mesontest

The Meson test executor got a new argument `-q` (and `--quiet`) that
suppresses all output of successful tests. This makes interactive
usage nicer because only errors are printed.
