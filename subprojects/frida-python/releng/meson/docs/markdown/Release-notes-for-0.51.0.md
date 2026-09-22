---
title: Release 0.51.0
short-description: Release notes for 0.51.0
...

# New features

## (C) Preprocessor flag handling

Meson previously stored `CPPFLAGS` and per-language compilation flags
separately. (That latter would come from `CFLAGS`, `CXXFLAGS`, etc.,
along with `<lang>_args` options whether specified no the command-line
interface (`-D..`), `meson.build` (`default_options`), or cross file
(`[properties]`).) This was mostly unobservable, except for certain
preprocessor-only checks like `check_header` would only use the
preprocessor flags, leading to confusion if some `-isystem` was in
`CFLAGS` but not `CPPFLAGS`. Now, they are lumped together, and
`CPPFLAGS`, for the languages which are deemed to care to about, is
just another source of compilation flags along with the others already
listed.

## Sanity checking compilers with user flags

Sanity checks previously only used user-specified flags for cross
compilers, but now do in all cases.

All compilers Meson might decide to use for the build are "sanity
checked" before other tests are run. This usually involves building
simple executable and trying to run it. Previously user flags
(compilation and/or linking flags) were used for sanity checking cross
compilers, but not native compilers. This is because such flags might
be essential for a cross binary to succeed, but usually aren't for a
native compiler.

In recent releases, there has been an effort to minimize the
special-casing of cross or native builds so as to make building more
predictable in less-tested cases. Since this the user flags are
necessary for cross, but not harmful for native, it makes more sense
to use them in all sanity checks than use them in no sanity checks, so
this is what we now do.

## New `sourceset` module

A new module, `sourceset`, was added to help building many binaries
from the same source files. Source sets associate source files and
dependencies to keys in a `configuration_data` object or a dictionary;
they then take multiple `configuration_data` objects or dictionaries,
and compute the set of source files and dependencies for each of those
configurations.

## n_debug=if-release and buildtype=plain means no asserts

Previously if this combination was used then assertions were enabled,
which is fairly surprising behavior.

## `target_type` in `build_targets` accepts the value 'shared_module'

The `target_type` keyword argument in `build_target()` now accepts the
value `'shared_module'`.

The statement

```meson
build_target(..., target_type: 'shared_module')
```

is equivalent to this:

```meson
shared_module(...)
```

## New modules kwarg for python.find_installation

This mirrors the modules argument that some kinds of dependencies
(such as qt, llvm, and cmake based dependencies) take, allowing you to
check that a particular module is available when getting a python
version.

```meson
py = import('python').find_installation('python3', modules : ['numpy'])
```

## Support for the Intel Compiler on Windows (ICL)

Support has been added for ICL.EXE and ifort on windows. The support
should be on part with ICC support on Linux/MacOS. The ICL C/C++
compiler behaves like Microsoft's CL.EXE rather than GCC/Clang like
ICC does, and has a different id, `intel-cl` to differentiate it.

```meson
cc = meson.get_compiler('c')
if cc.get_id == 'intel-cl'
  add_project_argument('/Qfoobar:yes', language : 'c')
endif
```

## Added basic support for the Xtensa CPU toolchain

You can now use `xt-xcc`, `xt-xc++`, `xt-nm`, etc... on your cross
compilation file and Meson won't complain about an unknown toolchain.


## Dependency objects now have a get_variable method

This is a generic replacement for type specific variable getters such as
`ConfigToolDependency.get_configtool_variable` and
`PkgConfigDependency.get_pkgconfig_variable`, and is the only way to query
such variables from cmake dependencies.

This method allows you to get variables without knowing the kind of
dependency you have.

```meson
dep = dependency('could_be_cmake_or_pkgconfig')
# cmake returns 'YES', pkg-config returns 'ON'
if ['YES', 'ON'].contains(dep.get_variable(pkgconfig : 'var-name', cmake : 'COP_VAR_NAME', default_value : 'NO'))
  error('Cannot build your project when dep is built with var-name support')
endif
```

## CMake prefix path overrides

When using pkg-config as a dependency resolver we can pass
`-Dpkg_config_path=$somepath` to extend or overwrite where pkg-config
will search for dependencies. Now cmake can do the same, as long as
the dependency uses a ${Name}Config.cmake file (not a
Find{$Name}.cmake file), by passing
`-Dcmake_prefix_path=list,of,paths`. It is important that point this
at the prefix that the dependency is installed into, not the cmake
path.

If you have installed something to `/tmp/dep`, which has a layout like:
```
/tmp/dep/lib/cmake
/tmp/dep/bin
```

then invoke Meson as `meson setup builddir/ -Dcmake_prefix_path=/tmp/dep`

## Tests that should fail but did not are now errors

You can tag a test as needing to fail like this:

```meson
test('shoulfail', exe, should_fail: true)
```

If the test passes the problem is reported in the error logs but due
to a bug it was not reported in the test runner's exit code. Starting
from this release the unexpected passes are properly reported in the
test runner's exit code. This means that test runs that were passing
in earlier versions of Meson will report failures with the current
version. This is a good thing, though, since it reveals an error in
your test suite that has, until now, gone unnoticed.

## New target keyword argument: `link_language`

There may be situations for which the user wishes to manually specify
the linking language.  For example, a C++ target may link C, Fortran,
etc. and perhaps the automatic detection in Meson does not pick the
desired compiler.  The user can manually choose the linker by language
per-target like this example of a target where one wishes to link with
the Fortran compiler:

```meson
executable(..., link_language : 'fortran')
```

A specific case this option fixes is where for example the main
program is Fortran that calls C and/or C++ code. The automatic
language detection of Meson prioritizes C/C++, and so an compile-time
error results like `undefined reference to main`, because the linker
is C or C++ instead of Fortran, which is fixed by this per-target
override.

## New module to parse kconfig output files

The new module `unstable-kconfig` adds the ability to parse and use
kconfig output files from `meson.build`.


## Add new `meson subprojects foreach` command

`meson subprojects` has learned a new `foreach` command which accepts
a command with arguments and executes it in each subproject directory.

For example this can be useful to check the status of subprojects
(e.g. with `git status` or `git diff`) before performing other actions
on them.


## Added c17 and c18 as c_std values for recent GCC and Clang Versions

For gcc version 8.0 and later, the values c17, c18, gnu17, and gnu18
were added to the accepted values for built-in compiler option c_std.

For Clang version 10.0 and later on Apple OSX (Darwin), and for
version 7.0 and later on other platforms, the values c17 and gnu17
were added as c_std values.

## gpgme dependency now supports gpgme-config

Previously, we could only detect GPGME with custom invocations of
`gpgme-config` or when the GPGME version was recent enough (>=1.13.0)
to install pkg-config files. Now we added support to Meson allowing us
to use `dependency('gpgme')` and fall back on `gpgme-config` parsing.

## Can link against custom targets

The output of `custom_target` and `custom_target[i]` can be used in
`link_with` and `link_whole` keyword arguments. This is useful for
integrating custom code generator steps, but note that there are many
limitations:

 - Meson cannot know about link dependencies of the custom target. If
   the target requires further link libraries, you need to add them manually

 - The user is responsible for ensuring that the code produced by
   different toolchains are compatible.

 - `custom_target` may only be used when it has a single output file.
   Use `custom_target[i]` when dealing with multiple output files.

 - The output file must have the correct file name extension.


## Removed the deprecated `--target-files` API

The `--target-files` introspection API is now no longer available. The same
information can be queried with the `--targets` API introduced in 0.50.0.

## Generators have a new `depends` keyword argument

Generators can now specify extra dependencies with the `depends`
keyword argument. It matches the behaviour of the same argument in
other functions and specifies that the given targets must be built
before the generator can be run. This is used in cases such as this
one where you need to tell a generator to indirectly invoke a
different program.

```meson
exe = executable(...)
cg = generator(program_runner,
    output: ['@BASENAME@.c'],
    arguments: ['--use-tool=' + exe.full_path(), '@INPUT@', '@OUTPUT@'],
    depends: exe)
```

## Specifying options per mer machine

Previously, no cross builds were controllable from the command line.
Machine-specific options like the pkg-config path and compiler options
only affected native targets, that is to say all targets in native
builds, and `native: true` targets in cross builds. Now, prefix the
option with `build.` to affect build machine targets, and leave it
unprefixed to affect host machine targets.

For those trying to ensure native and cross builds to the same
platform produced the same result, the old way was frustrating because
very different invocations were needed to affect the same targets, if
it was possible at all. Now, the same command line arguments affect
the same targets everywhere --- Meson is closer to ignoring whether
the "overall" build is native or cross, and just caring about whether
individual targets are for the build or host machines.


## subproject.get_variable() now accepts a `fallback` argument

Similar to `get_variable`, a fallback argument can now be passed to
`subproject.get_variable()`, it will be returned if the requested
variable name did not exist.

``` meson
var = subproject.get_variable('does-not-exist', 'fallback-value')
```

## Add keyword `static` to `find_library`

`find_library` has learned the `static` keyword. They keyword must be
a boolean, where `true` only searches for static libraries and `false`
only searches for dynamic/shared. Leaving the keyword unset will keep
the old behavior of first searching for dynamic and then falling back
to static.

## Fortran `include` statements recursively parsed

While non-standard and generally not recommended, some legacy Fortran
programs use `include` directives to inject code inline. Since v0.51,
Meson can handle Fortran `include` directives recursively.

DO NOT list `include` files as sources for a target, as in general
their syntax is not correct as a standalone target. In general
`include` files are meant to be injected inline as if they were copy
and pasted into the source file.

`include` was never standard and was superseded by Fortran 90 `module`.

The `include` file is only recognized by Meson if it has a Fortran
file suffix, such as `.f` `.F` `.f90` `.F90` or similar. This is to
avoid deeply nested scanning of large external legacy C libraries that
only interface to Fortran by `include biglib.h` or similar.

## CMake subprojects

Meson can now directly consume CMake based subprojects with the
CMake module.

Using CMake subprojects is similar to using the "normal" Meson
subprojects. They also have to be located in the `subprojects`
directory.

Example:

```cmake
add_library(cm_lib SHARED ${SOURCES})
```

```meson
cmake = import('cmake')

# Configure the CMake project
sub_proj = cmake.subproject('libsimple_cmake')

# Fetch the dependency object
cm_lib = sub_proj.dependency('cm_lib')

executable('exe1', ['sources'], dependencies: [cm_lib])
```

It should be noted that not all projects are guaranteed to work. The
safest approach would still be to create a `meson.build` for the
subprojects in question.

## Multiple cross files can be specified

`--cross-file` can be passed multiple times, with the configuration files overlaying the same way as `--native-file`.
