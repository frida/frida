---
title: Release 0.56.0
short-description: Release notes for 0.56.0
...

# New features

## Python 3.5 support will be dropped in the next release

The final [Python 3.5 release was 3.5.10 in
September](https://www.python.org/dev/peps/pep-0478/#id4). This
release series is now End-of-Life (EOL). The only LTS distribution
that still only ships Python 3.5 is Ubuntu 16.04, which will be [EOL
in April 2021](https://ubuntu.com/about/release-cycle).

Python 3.6 has numerous features that we find useful such as improved
support for the `typing` module, f-string support, and better
integration with the `pathlib` module.

As a result, we will begin requiring Python 3.6 or newer in Meson
0.57, which is the next release. Starting with Meson 0.56, we now
print a `NOTICE:` when a `meson` command is run on Python 3.5 to
inform users about this. This notice has also been backported into the
0.55.2 stable release.

## `meson test` can now filter tests by subproject

You could always specify a list of tests to run by passing the names
as arguments to `meson test`. If there were multiple tests with that
name (in the same project or different subprojects), all of them would
be run. Now you can:

1. Run all tests with the specified name from a specific subproject: `meson test subprojname:testname`
1. Run all tests defined in a specific subproject: `meson test subprojectname:`

As before, these can all be specified multiple times and mixed:

```sh
# Run:
# * All tests called 'name1' or 'name2' and
# * All tests called 'name3' in subproject 'bar' and
# * All tests in subproject 'foo'
$ meson test name1 name2 bar:name3 foo:
```

## Native (build machine) compilers not always required by `project()`

When cross-compiling, native (build machine) compilers for the
languages specified in `project()` are not required, if no targets use
them.

## New `extra_files` key in target introspection

The target introspection (`meson introspect --targets`,
`intro-targets.json`) now has the new `extra_files` key which lists
all files specified via the `extra_files` kwarg of a build target (see
`executable()`, etc.)


## Preliminary AIX support

AIX is now supported when compiling with gcc. A number of features are
not supported yet. For example, only gcc is supported (not xlC).
Archives with both 32-bit and 64-bit dynamic libraries are not
generated automatically. The rpath includes both the build and install
rpath, no attempt is made to change the rpath at install time. Most
advanced features (eg. link\_whole) are not supported yet.

## Wraps from subprojects are automatically promoted

It is not required to promote wrap files for subprojects into the main
project any more. When configuring a subproject, Meson will look for
any wrap file or directory in the subproject's `subprojects/`
directory and add them into the global list of available subprojects,
to be used by any future `subproject()` call or `dependency()`
fallback. If a subproject with the same name already exists, the new
wrap file or directory is ignored. That means that the main project
can always override any subproject's wrap files by providing their
own, it also means the ordering in which subprojects are configured
matters, if 2 subprojects provide foo.wrap only the one from the first
subproject to be configured will be used.

This new behavior can be disabled by passing `--wrap-mode=nopromote`.

## `meson.build_root()` and `meson.source_root()` are deprecated

Those function are common source of issue when used in a subproject
because they point to the parent project root which is rarely what is
expected and is a violation of subproject isolation.

`meson.current_source_dir()` and `meson.current_build_dir()` should be
used instead and have been available in all Meson versions. New
functions `meson.project_source_root()` and
`meson.project_build_root()` have been added in Meson 0.56.0 to get
the root of the current (sub)project.

## `dep.as_link_whole()`

Dependencies created with `declare_dependency()` now has new method
`as_link_whole()`. It returns a copy of the dependency object with all
link_with arguments changed to link_whole. This is useful for example
for fallback dependency from a subproject built with
`default_library=static`.

```meson
somelib = static_library('somelib', ...)
dep = declare_dependency(..., link_with: somelib)
library('someotherlib', ..., dependencies: dep.as_link_whole())
```

## Add support for all Windows subsystem types

It is now possible to build things like Windows kernel drivers with
the new `win_subsystem` keyword argument. This replaces the old
`gui_app` keyword argument, which is now deprecated. You should update
your project to use the new style like this:

```meson
# Old way
executable(..., gui_app: 'true')
# New way
executable(..., win_subsystem: 'windows')
```

The argument supports versioning [as described on MSDN
documentation](https://docs.microsoft.com/en-us/cpp/build/reference/subsystem-specify-subsystem).
Thus to build a Windows kernel driver with a specific version you'd
write something like this:

```meson
executable(..., win_subsystem: 'native,6.02')
```

## Added NVidia HPC SDK compilers

Added support for `nvidia_hpc` NVidia HPC SDK compilers, which are currently in public beta testing.

## Project and built-in options can be set in native or cross files

A new set of sections has been added to the cross and native files,
`[project options]` and `[<subproject_name>:project options]`, where
`subproject_name` is the name of a subproject. Any options that are
allowed in the project can be set from this section. They have the
lowest precedent, and will be overwritten by command line arguments.


```meson
option('foo', type : 'string', value : 'foo')
```

```ini
[project options]
foo = 'other val'
```

```console
meson setup builddir/ --native-file my.ini
```

Will result in the option foo having the value `other val`,

```console
meson setup builddir/ --native-file my.ini -Dfoo='different val'
```

Will result in the option foo having the value `different val`,


Subproject options are assigned like this:

```ini
[zlib:project options]
foo = 'some val'
```

Additionally Meson level options can be set in the same way, using the
`[built-in options]` section.

```ini
[built-in options]
c_std = 'c99'
```

These options can also be set on a per-subproject basis, although only
`default_library` and `werror` can currently be set:
```ini
[zlib:built-in options]
default_library = 'static'
```

## `unstable-keyval` is now stable `keyval`

The `unstable-keyval` has been renamed to `keyval` and now promises stability
guarantees.

Meson will print a warning when you load an `unstable-` module that has been
stabilised (so `unstable-keyval` is still accepted for example).

## CMake subproject cross compilation support

Meson now supports cross compilation for CMake subprojects. Meson will
try to automatically guess most of the required CMake toolchain
variables from existing entries in the cross and native files. These
variables will be stored in an automatically generate CMake toolchain
file in the build directory. The remaining variables that can't be
guessed can be added by the user in the new `[cmake]` cross/native
file section.

## Machine file keys are stored case sensitive

Previous the keys were always lowered, which worked fine for the
values that were allowed in the machine files. With the addition of
per-project options we need to make these sensitive to case, as the
options in meson_options.txt are sensitive to case already.

## Consistency between `declare_dependency()` and `pkgconfig.generate()` variables

The `variables` keyword argument in `declare_dependency()` used to
only support dictionary and `pkgconfig.generate()` only list of
strings. They now both support dictionary and list of strings in the
format `'name=value'`. This makes easier to share a common set of
variables for both:

```meson
vars = {'foo': 'bar'}
dep = declare_dependency(..., variables: vars)
pkg.generate(..., variables: vars)
```

## Qt5 compile_translations now supports qresource preprocessing

When using qtmod.preprocess() in combination with
qtmod.compile_translations() to embed translations using rcc, it is no
longer required to do this:

```meson
ts_files = ['list', 'of', 'files']
qtmod.compile_translations(ts_files)
# lang.qrc also contains the duplicated list of files
lang_cpp = qtmod.preprocess(qresources: 'lang.qrc')
```

Instead, use:
```meson
lang_cpp = qtmod.compile_translations(qresource: 'lang.qrc')
```

which will automatically detect and generate the needed
compile_translations targets.

## Controlling subproject dependencies with  `dependency(allow_fallback: ...)`

As an alternative to the `fallback` keyword argument to `dependency`,
you may use `allow_fallback`, which accepts a boolean value. If `true`
and the dependency is not found on the system, Meson will fallback to
a subproject that provides this dependency, even if the dependency is
optional. If `false`, Meson will not fallback even if a subproject
provides this dependency.

## Custom standard library

- It is not limited to cross builds any more, `<lang>_stdlib` property can be
  set in native files.
- The variable name parameter is no longer required as long as the subproject
  calls `meson.override_dependency('c_stdlib', mylibc_dep)`.

## Improvements for the builtin curses dependency

This method has been extended to use config-tools, and a fallback to
find_library for lookup as well as pkg-config.

## HDF5 dependency improvements

HDF5 has been improved so that the internal representations have been
split. This allows selecting pkg-config and config-tool dependencies
separately. Both work as proper dependencies of their type, so
`get_variable` and similar now work correctly.

It has also been fixed to use the selected compiler for the build instead of
the default compiler.

## External projects

A new experimental module `unstable-external_project` has been added
to build code using other build systems than Meson. Currently only
supporting projects with a configure script that generates Makefiles.

```meson
project('My Autotools Project', 'c',
  meson_version : '>=0.56.0',
)

mod = import('unstable-external_project')

p = mod.add_project('configure',
  configure_options : ['--prefix=@PREFIX@',
                       '--libdir=@LIBDIR@',
                       '--incdir=@INCLUDEDIR@',
                       '--enable-foo',
                      ],
)

mylib_dep = p.dependency('mylib')
```


## Per subproject `warning_level` option

`warning_level` can now be defined per subproject, in the same way as
`default_library` and `werror`.

## `meson subprojects` command

A new `--types` argument has been added to all subcommands to run the
command only on wraps with the specified types. For example this
command will only print `Hello` for each git subproject: `meson
subprojects foreach --types git echo "Hello"`. Multiple types can be
set as comma separated list e.g. `--types git,file`.

Subprojects with no wrap file are now taken into account as well. This
happens for example for subprojects configured as git submodule, or
downloaded manually by the user and placed into the `subprojects/`
directory.

The `checkout` subcommand now always stash any pending changes before
switching branch. Note that `update` subcommand was already stashing
changes before updating the branch.

If the command fails on any subproject the execution continues with
other subprojects, but at the end an error code is now returned.

The `update` subcommand has been reworked:
- In the case the URL of `origin` is different as the `url` set in wrap file,
  the subproject will not be updated unless `--reset` is specified (see below).
- In the case a subproject directory exists and is not a git repository but has
  a `[wrap-git]`, Meson used to run git commands that would wrongly apply to the
  main project. It now skip the subproject unless `--reset` is specified (see below).
- The `--rebase` behaviour is now the default for consistency: it was
  already rebasing when current branch and revision are the same, it is
  less confusing to rebase when they are different too.
- Add `--reset` mode that checkout the new branch and hard reset that
  branch to remote commit. This new mode guarantees that every
  subproject are exactly at the wrap's revision. In addition the URL of `origin`
  is updated in case it changed in the wrap file. If the subproject directory is
  not a git repository but has a `[wrap-git]` the directory is deleted and the
  new repository is cloned.
- Local changes are always stashed first to avoid any data loss. In the
  worst case scenario the user can always check reflog and stash list to
  rollback.

## Added CompCert C compiler

Added experimental support for the [CompCert formally-verified C
compiler](https://github.com/AbsInt/CompCert). The current state of
the implementation is good enough to build the [picolibc
project](https://github.com/picolibc/picolibc) with CompCert, but
might still need additional adjustments for other projects.

## Dependencies listed in test and benchmark introspection

The introspection data for tests and benchmarks now includes the
target ids for executables and built files that are needed by the
test. IDEs can use this feature to update the build more quickly
before running a test.

## `include_type` support for the CMake subproject object dependency method

The `dependency()` method of the CMake subproject object now also
supports the `include_type` kwarg which is similar to the sane kwarg
in the `dependency()` function.

## Deprecate Dependency.get_pkgconfig_variable and Dependency.get_configtool_variable

These have been replaced with the more versatile `get_variable()` method
already, and shouldn't be used anymore.
