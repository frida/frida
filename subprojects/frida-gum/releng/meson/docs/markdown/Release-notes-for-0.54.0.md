---
title: Release 0.54.0
short-description: Release notes for 0.54.0
...

# New features

## Emscripten (emcc) now supports threads

In addition to properly setting the compile and linker arguments, a
new Meson builtin has been added to control the PTHREAD_POOL_SIZE
option, `-D<lang>_thread_count`, which may be set to any integer value
greater than 0. If it set to 0 then the PTHREAD_POOL_SIZE option will
not be passed.

## Introduce dataonly for the pkgconfig module

This allows users to disable writing out the inbuilt variables to the
pkg-config file as they might actually not be required.

One reason to have this is for architecture-independent pkg-config
files in projects which also have architecture-dependent outputs.

```
pkgg.generate(
  name : 'libhello_nolib',
  description : 'A minimalistic pkgconfig file.',
  version : libver,
  dataonly: true
)
```

## Consistently report file locations relative to cwd

The paths for filenames in error and warning locations are now
consistently reported relative to the current working directory (when
possible), or as absolute paths (when a relative path does not exist,
e.g. a Windows path starting with a different drive letter to the
current working directory).

(The previous behaviour was to report a path relative to the source
root for all warnings and most errors, and relative to cwd for certain
parser errors)

## `dependency()` consistency

The first time a dependency is found, using `dependency('foo', ...)`,
the return value is now cached. Any subsequent call will return the
same value as long as version requested match, otherwise not-found
dependency is returned. This means that if a system dependency is
first found, it won't fallback to a subproject in a subsequent call
any more and will rather return not-found instead if the system
version does not match. Similarly, if the first call returns the
subproject fallback dependency, it will also return the subproject
dependency in a subsequent call even if no fallback is provided.

For example, if the system has `foo` version 1.0:
```meson
# d2 is set to foo_dep and not the system dependency, even without fallback argument.
d1 = dependency('foo', version : '>=2.0', required : false,
                fallback : ['foo', 'foo_dep'])
d2 = dependency('foo', version : '>=1.0', required : false)
```
```meson
# d2 is not-found because the first call returned the system dependency, but its version is too old for 2nd call.
d1 = dependency('foo', version : '>=1.0', required : false)
d2 = dependency('foo', version : '>=2.0', required : false,
                fallback : ['foo', 'foo_dep'])
```

## Override `dependency()`

It is now possible to override the result of `dependency()` to point
to any dependency object you want. The overriding is global and
applies to every subproject from there on.

For example, this subproject provides 2 libraries with version 2.0:

```meson
project(..., version : '2.0')

libfoo = library('foo', ...)
foo_dep = declare_dependency(link_with : libfoo)
meson.override_dependency('foo', foo_dep)

libbar = library('bar', ...)
bar_dep = declare_dependency(link_with : libbar)
meson.override_dependency('bar', bar_dep)
```

Assuming the system has `foo` and `bar` 1.0 installed, and master project does this:
```meson
foo_dep = dependency('foo', version : '>=2.0', fallback : ['foo', 'foo_dep'])
bar_dep = dependency('bar')
```

This used to mix system 1.0 version and subproject 2.0 dependencies,
but thanks to the override `bar_dep` is now set to the subproject's
version instead.

Another case this can be useful is to force a subproject to use a
specific dependency. If the subproject does `dependency('foo')` but
the main project wants to provide its own implementation of `foo`, it
can for example call `meson.override_dependency('foo',
declare_dependency(...))` before configuring the subproject.

## Simplified `dependency()` fallback

In the case a subproject `foo` calls
`meson.override_dependency('foo-2.0', foo_dep)`, the parent project
can omit the dependency variable name in fallback keyword argument:
`dependency('foo-2.0', fallback : 'foo')`.

## Backend agnostic compile command

A new `meson compile` command has been added to support backend
agnostic compilation. It accepts two arguments, `-j` and `-l`, which
are used if possible (`-l` does nothing with msbuild). A `-j` or `-l`
value < 1 lets the backend decide how many threads to use. For msbuild
this means `-m`, for ninja it means passing no arguments.

```console
meson setup builddir --backend vs
meson compile -C builddir -j0  # this is the same as `msbuild builddir/my.sln -m`
```

```console
meson setup builddir
meson compile -C builddir -j3  # this is the same as `ninja -C builddir -j3`
```

Additionally `meson compile` provides a `--clean` switch to clean the
project.

A complete list of arguments is always documented via `meson compile --help`

## Native (build machine) compilers not always required

`add_languages()` gained a `native:` keyword, indicating if a native or cross
compiler is to be used.

For the benefit of existing simple build definitions which don't
contain any `native: true` targets, without breaking backwards
compatibility for build definitions which assume that the native
compiler is available after `add_languages()`, if the `native:`
keyword is absent the languages may be used for either the build or
host machine, but are never required for the build machine.

This changes the behaviour of the following Meson fragment (when
cross-compiling but a native compiler is not available) from reporting
an error at `add_language` to reporting an error at `executable`.

```
add_language('c')
executable('main', 'main.c', native: true)
```

## Summary improvements

A new `list_sep` keyword argument has been added to `summary()`
function. If defined and the value is a list, elements will be
separated by the provided string instead of being aligned on a new
line.

The automatic `subprojects` section now also print the number of
warnings encountered during that subproject configuration, or the
error message if the configuration failed.

## Add a system type dependency for zlib

This allows zlib to be detected on macOS and FreeBSD without the use
of pkg-config or cmake, neither of which are part of the base install
on those OSes (but zlib is).

A side effect of this change is that `dependency('zlib')` also works
with cmake instead of requiring `dependency('ZLIB')`.

## Added 'name' method

Build target objects (as returned by executable(), library(), ...) now
have a name() method.

## New option `--quiet` to `meson install`

Now you can run `meson install --quiet` and Meson will not verbosely
print every file as it is being installed. As before, the full log is
always available inside the builddir in `meson-logs/install-log.txt`.

When this option is passed, install scripts will have the environment
variable `MESON_INSTALL_QUIET` set.

Numerous speed-ups were also made for the install step, especially on
Windows where it is now 300% to 1200% faster than before depending on
your workload.

## Property support emscripten's wasm-ld

Before 0.54.0 we treated emscripten as both compiler and linker, which
isn't really true. It does have a linker, called wasm-ld (Meson's name
is ld.wasm). This is a special version of clang's lld. This will now
be detected properly.

## Skip sanity tests when cross compiling

For certain cross compilation environments it is not possible to
compile a sanity check application. This can now be disabled by adding
the following entry to your cross file's `properties` section:

```
skip_sanity_check = true
```

## Support for overriding the linker with ldc and gdc

LDC (the llvm D compiler) and GDC (The Gnu D Compiler) now honor D_LD
linker variable (or d_ld in the cross file) and is able to pick
different linkers.

GDC supports all of the same values as GCC, LDC supports ld.bfd,
ld.gold, ld.lld, ld64, link, and lld-link.

## Native file properties

As of Meson 0.54.0, the `--native-file nativefile.ini` can contain:

* binaries
* paths
* properties

which are defined and used the same way as in cross files. The
`properties` are new for Meson 0.54.0, and are read like:

```meson
x = meson.get_external_property('foobar', 'foo')
```

where `foobar` is the property name, and the optional `foo` is the
fallback string value.

For cross-compiled projects, `get_external_property()` reads the
cross-file unless `native: true` is specified.

## Changed the signal used to terminate a test process (group)

A test process (group) is now terminated via SIGTERM instead of
SIGKILL allowing the signal to be handled. However, it is now the
responsibility of the custom signal handler (if any) to ensure that
any process spawned by the top-level test processes is correctly
killed.

## Dynamic Linker environment variables actually match docs

The docs have always claimed that the Dynamic Linker environment
variable should be `${COMPILER_VAR}_LD`, but that's only the case for
about half of the variables. The other half are different. In 0.54.0
the variables match. The old variables are still supported, but are
deprecated and raise a deprecation warning.

## Per subproject `default_library` and `werror` options

The `default_library` and `werror` built-in options can now be defined
per subproject. This is useful for example when building shared
libraries in the main project, but static link a subproject, or when
the main project must build with no warnings but some subprojects
cannot.

Most of the time this would be used either by the parent project by
setting subproject's default_options (e.g. `subproject('foo',
default_options: 'default_library=static')`), or by the user using the
command line `-Dfoo:default_library=static`.

The value is overridden in this order:
- Value from parent project
- Value from subproject's default_options if set
- Value from subproject() default_options if set
- Value from command line if set

## Environment Variables with Cross Builds

Previously in Meson, variables like `CC` effected both the host and
build platforms for native builds, but the just the build platform for
cross builds. Now `CC_FOR_BUILD` is used for the build platform in
cross builds.

This old behavior is inconsistent with the way Autotools works, which
undermines the purpose of distro-integration that is the only reason
environment variables are supported at all in Meson. The new behavior
is not quite the same, but doesn't conflict: Meson doesn't always
respond to an environment when Autoconf would, but when it does it
interprets it as Autotools would.

## Added 'pkg_config_libdir' property

Allows to define a list of folders used by pkg-config for a cross
build and avoid a system directories use.

## More new sample Meson templates for (`Java`, `Cuda`, and more)

Meson now ships with predefined project templates for `Java`, `Cuda`,
`Objective-C++`, and `C#`, we provided with associated values for
corresponding languages, available for both library, and executable.

## Ninja version requirement bumped to 1.7

Meson now uses the [Implicit
outputs](https://ninja-build.org/manual.html#ref_outputs) feature of
Ninja for some types of targets that have multiple outputs which may
not be listed on the command-line. This feature requires Ninja 1.7+.

Note that the latest version of [Ninja available in Ubuntu
16.04](https://packages.ubuntu.com/search?keywords=ninja-build&searchon=names&suite=xenial-backports&section=all)
(the oldest Ubuntu LTS at the time of writing) is 1.7.1. If your
distro does not ship with a new-enough Ninja, you can download the
latest release from Ninja's GitHub page:
https://github.com/ninja-build/ninja/releases

## Added `-C` argument to `meson init` command

The Meson init assumes that it is run inside the project root
directory. If this isn't the case, you can now use `-C` to specify the
actual project source directory.

## More than one argument to `message()` and `warning()`

Arguments passed to `message()` and `warning()` will be printed
separated by space.

## Added `has_tools` method to qt module

It should be used to compile optional Qt code:
```meson
qt5 = import('qt5')
if qt5.has_tools(required: get_option('qt_feature'))
  moc_files = qt5.preprocess(...)
  ...
endif
```

## The MSI installer is only available in 64 bit version

Microsoft ended support for Windows 7, so only 64 bit Windows OSs are
officially supported. Thus only a 64 bit MSI installer will be
provided going forward. People needing a 32 bit version can build
their own with the `msi/createmsi.py` script in Meson's source
repository.

## Uninstalled pkg-config files

**Note**: the functionality of this module is governed by [Meson's
  rules on mixing build systems](Mixing-build-systems.md).

The `pkgconfig` module now generates uninstalled pc files as well. For
any generated `foo.pc` file, an extra `foo-uninstalled.pc` file is
placed into `<builddir>/meson-uninstalled`. They can be used to build
applications against libraries built by Meson without installing them,
by pointing `PKG_CONFIG_PATH` to that directory. This is an
experimental feature provided on a best-effort basis, it might not
work in all use-cases.

## CMake find_package COMPONENTS support

It is now possible to pass components to the CMake dependency backend
via the new `components` kwarg in the `dependency` function.

## Added Microchip XC16 C compiler support

Make sure compiler executables are setup correctly in your path
Compiler is available from the Microchip website for free


## Added Texas Instruments C2000 C/C++ compiler support

Make sure compiler executables are setup correctly in your path
Compiler is available from Texas Instruments website for free

## Unity file block size is configurable

Traditionally the unity files that Meson autogenerates contain all
source files that belong to a single target. This is the most
efficient setting for full builds but makes incremental builds slow.
This release adds a new option `unity_size` which specifies how many
source files should be put in each unity file.

The default value for block size is 4. This means that if you have a
target that has eight source files, Meson will generate two unity
files each of which includes four source files. The old behaviour can
be replicated by setting `unity_size` to a large value, such as 10000.

## Verbose mode for `meson compile`

The new option `--verbose` has been added to `meson compile` that will
enable more verbose compilation logs. Note that for VS backend it
means that logs will be less verbose by default (without `--verbose`
option).
