---
title: Release 0.58.0
short-description: Release notes for 0.58.0
...

# New features

## New `meson.global_build_root()` and `meson.global_source_root()` methods

Returns the root source and build directory of the main project.

Those are direct replacement for `meson.build_root()` and `meson.source_root()`
that have been deprecated since 0.56.0. In some rare occasions they could not be
replaced by `meson.project_source_root()` or `meson.current_source_dir()`, in
which case the new methods can now be used instead. Old methods are still
deprecated because their names are not explicit enough and created many issues
when a project is being used as a subproject.

## Developer environment

New method `meson.add_devenv()` adds an [`environment()`](#environment) object
to the list of environments that will be applied when using `meson devenv`
command line. This is useful for developers who wish to use the project without
installing it, it is often needed to set for example the path to plugins
directory, etc. Alternatively, a list or dictionary can be passed as first
argument.

``` meson
devenv = environment()
devenv.set('PLUGINS_PATH', meson.current_build_dir())
...
meson.add_devenv(devenv)
```

New command line has been added: `meson devenv -C builddir [<command>]`.
It runs a command, or open interactive shell if no command is provided, with
environment setup to run project from the build directory, without installation.

These variables are set in environment in addition to those set using `meson.add_devenv()`:
- `MESON_DEVENV` is defined to `'1'`.
- `MESON_PROJECT_NAME` is defined to the main project's name.
- `PKG_CONFIG_PATH` includes the directory where Meson generates `-uninstalled.pc`
  files.
- `PATH` includes every directory where there is an executable that would be
  installed into `bindir`. On windows it also includes every directory where there
  is a DLL needed to run those executables.
- `LD_LIBRARY_PATH` includes every directory where there is a shared library that
  would be installed into `libdir`. This allows to run system application using
  custom build of some libraries. For example running system GEdit when building
  GTK from git. On OSX the environment variable is `DYLD_LIBRARY_PATH` and
  `PATH` on Windows.
- `GI_TYPELIB_PATH` includes every directory where a GObject Introspection
  typelib is built. This is automatically set when using `gnome.generate_gir()`.

## `-pipe` no longer used by default

Meson used to add the `-pipe` command line argument to all compilers
that supported it, but no longer does. If you need this, then you can
add it manually. However note that you should not do this unless you
have actually measured that it provides performance improvements. In
our tests we could not find a case where adding `-pipe` made
compilation faster and using `-pipe` [can cause sporadic build
failures in certain
cases](https://github.com/mesonbuild/meson/issues/8508).

## `meson.add_dist_script()` allowed in subprojects

`meson.add_dist_script()` can now be invoked from a subproject, it was a hard
error in earlier versions. Subproject dist scripts will only be executed
when running `meson dist --include-subprojects`. `MESON_PROJECT_SOURCE_ROOT`,
`MESON_PROJECT_BUILD_ROOT` and `MESON_PROJECT_DIST_ROOT` environment variables
are set when dist scripts are run. They are identical to `MESON_SOURCE_ROOT`,
`MESON_BUILD_ROOT` and `MESON_DIST_ROOT` for main project scripts, but for
subproject scripts they have the path to the root of the subproject appended,
usually `subprojects/<subproject-name>`.

Note that existing dist scripts likely need to be modified to use those new
environment variables instead of `MESON_DIST_ROOT` to work properly when used
from a subproject.

## Do not add custom target dir to header path if `implicit_include_directories` is `false`

If you do the following:

```meson
# in some subdirectory
gen_h = custom_target(...)
# in some other directory
executable('foo', 'foo.c', gen_h)
```

then the output directory of the custom target is automatically added
to the header search path. This is convenient, but sometimes it can
lead to problems. Starting with this version, the directory will no
longer be put in the search path if the target has
`implicit_include_directories: false`. In these cases you need to set
up the path manually with `include_directories`.

## Multiple append() and prepend() in `environment()` object

`append()` and `prepend()` methods can now be called multiple times
on the same `varname`. Earlier Meson versions would warn and only the last
operation was taking effect.

```meson
env = environment()

# MY_PATH will be '0:1:2:3'
env.set('MY_PATH', '1')
env.append('MY_PATH', '2')
env.append('MY_PATH', '3')
env.prepend('MY_PATH', '0')
```


## `dep.get_variable(varname)`

`dep.get_variable()` now has `varname` as first positional argument.
It is used as default value for `cmake`, `pkgconfig`, `configtool` and `internal`
keyword arguments. It is useful in the common case where `pkgconfig` and `internal`
use the same variable name, in which case it's easier to write `dep.get_variable('foo')`
instead of `dep.get_variable(pkgconfig: 'foo', internal: 'foo')`.


## clang-format include and ignore lists

When clang-format is installed and a `.clang-format` file is found at the main
project's root source directory, Meson automatically adds a `clang-format` target
that reformat all C and C++ files.

It is now possible to restrict files to be reformatted with optional
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

A new target `clang-format-check` has been added. It returns an error code if
any file needs to be reformatted. This is intended to be used by CI.

## Introducing format strings to the Meson language

In addition to the conventional `'A string @0@ to be formatted @1@'.format(n, m)`
method of formatting strings in the Meson language, there's now the additional
`f'A string @n@ to be formatted @m@'` notation that provides a non-positional
and clearer alternative. Meson's format strings are currently restricted to
identity-expressions, meaning `f'format @'m' + 'e'@'` will not parse.

## Skip subprojects installation

It is now possible to skip installation of some or all subprojects. This is
useful when subprojects are internal dependencies static linked into the main
project.

By default all subprojects are still installed.
- `meson install -C builddir --skip-subprojects` installs only the main project.
- `meson install -C builddir --skip-subprojects foo,bar` installs the main project
  and all subprojects except for subprojects `foo` and `bar` if they are used.

## String `.replace()`

String objects now have a method called replace for replacing all instances of a
substring in a string with another.

```meson
s = 'aaabbb'
s = s.replace('aaa', 'bbb')
# 's' is now 'bbbbbb'
```

## `meson.get_cross_property()` has been deprecated

It's a pure subset of `meson.get_external_property`, and works strangely in
host == build configurations, since it would be more accurately described as
`get_host_property`.

## New `range()` function

``` meson
    rangeobject range(stop)
    rangeobject range(start, stop[, step])
```

Return an opaque object that can be only be used in `foreach` statements.
- `start` must be integer greater or equal to 0. Defaults to 0.
- `stop` must be integer greater or equal to `start`.
- `step` must be integer greater or equal to 1. Defaults to 1.

It cause the `foreach` loop to be called with the value from `start` included
to `stop` excluded with an increment of `step` after each loop.

```meson
# Loop 15 times with i from 0 to 14 included.
foreach i : range(15)
   ...
endforeach
```

The range object can also be assigned to a variable and indexed.
```meson
r = range(5, 10, 2)
assert(r[2] == 9)
```


## Xcode improvements

The Xcode backend has been much improved and should now we usable
enough for day to day development.

## Use fallback from wrap file when force fallback

Optional dependency like below will now fallback to the subproject
defined in the wrap file in the case `wrap_mode` is set to `forcefallback`
or `force_fallback_for` contains the subproject.

```meson
# required is false because we could fallback to cc.find_library(), but in the
# forcefallback case this now configure the subproject.
dep = dependency('foo-1.0', required: false)
if not dep.found()
  dep = cc.find_library('foo', has_headers: 'foo.h')
endif
```

```ini
[wrap-file]
...
[provide]
dependency_names = foo-1.0
```

## `error()` with multiple arguments

Just like `warning()` and `message()`, `error()` can now take more than one
argument that will be separated by space.

## Specify man page locale during installation

Locale directories can now be passed to `install_man`:

```meson
# instead of
# install_data('foo.fr.1', install_dir: join_paths(get_option('mandir'), 'fr', 'man1'), rename: 'foo.1')`
install_man('foo.fr.1', locale: 'fr')
```

## Passing `custom_target()` output to `pkg.generate()`

It is now allowed to pass libraries generated by a `custom_target()` to
pkg-config file generator. The output filename must have a known library extension
such as `.a`, `.so`, etc.

## JNI System Dependency

When building projects such as those interacting with the JNI, you need access
to a few header files located in a Java installation. This system dependency
will add the correct include paths to your target. It assumes that either
`JAVA_HOME` will be set to a valid Java installation, or the default `javac` on
your system is a located in the `bin` directory of a Java installation. Note:
symlinks are resolved.

```meson
jni_dep = dependency('jni', version : '>=1.8')
```

Currently this system dependency only works on `linux`, `win32`, and `darwin`.
This can easily be extended given the correct information about your compiler
and platform in an issue.

## `meson subprojects update --reset` now re-extract tarballs

When using `--reset` option, the source tree of `[wrap-file]` subprojects is now
deleted and re-extracted from cached tarballs, or re-downloaded. This is because
Meson has no way to know if the source tree or the wrap file has been modified,
and `--reset` should guarantee that latest code is being used on next reconfigure.

Use `--reset` with caution if you do local changes on non-git subprojects.

## Allow using generator with CustomTarget or Index of CustomTarget.

Calling `generator.process()` with either a CustomTarget or Index of CustomTarget
as files is now permitted.

## Qt Dependency uses a Factory

This separates the Pkg-config and QMake based discovery methods into two
distinct classes in the backend. This allows using
`dependency.get_variable()` and `dependency.get_pkg_config_variable()`, as
well as being a cleaner implementation.

## Purge subprojects folder

It is now possible to purge a subprojects folder of artifacts created
from wrap-based subprojects including anything in `packagecache`. This is useful
when you want to return to a completely clean source tree or busting caches with
stale patch directories or caches. By default the command will only print out
what it is removing. You need to pass `--confirm` to the command for actual
artifacts to be purged.

By default all wrap-based subprojects will be purged.

- `meson subprojects purge` prints non-cache wrap artifacts which will be
purged.
- `meson subprojects purge --confirm` purges non-cache wrap artifacts.
- `meson subprojects purge --confirm --include-cache` also removes the cache
artifacts.
- `meson subprojects purge --confirm subproj1 subproj2` removes non-cache wrap
artifacts associated with the listed subprojects.

## Check if native or cross-file properties exist

It is now possible to check whether a native property or a cross-file property
exists with `meson.has_external_property('foo')`. This is useful if the
property in question is a boolean and one wants to distinguish between
"set" and "not provided" which can't be done the usual way by passing a
fallback parameter to `meson.get_external_property()` in this particular case.

## `summary()` accepts features

Build feature options can be passed to `summary()` as the value to be printed.

## Address sanitizer support for Visual Studio

The `b_sanitize` option for enabling Address sanitizer now works with
the Visual Studio compilers. This requires [a sufficiently new version
of Visual
Studio](https://devblogs.microsoft.com/cppblog/address-sanitizer-for-msvc-now-generally-available/).
