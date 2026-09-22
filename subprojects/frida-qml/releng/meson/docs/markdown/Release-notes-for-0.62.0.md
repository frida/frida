---
title: Release 0.62.0
short-description: Release notes for 0.62.0
...

# New features

## Bash completion scripts sourced in `meson devenv`

If bash-completion scripts are being installed and the shell is bash, they will
be automatically sourced.

## Setup GDB auto-load for `meson devenv`

When GDB helper scripts (*-gdb.py, *-gdb.gdb, and *-gdb.csm) are installed with
a library name that matches one being built, Meson adds the needed auto-load
commands into `<builddir>/.gdbinit` file. When running gdb from top build
directory, that file is loaded by gdb automatically.

## Print modified environment variables with `meson devenv --dump`

With `--dump` option, all environment variables that have been modified are
printed instead of starting an interactive shell. It can be used by shell
scripts that wish to setup their environment themselves.

## New `method` and `separator` kwargs on `environment()` and `meson.add_devenv()`

It simplifies this common pattern:
```meson
env = environment()
env.prepend('FOO', ['a', 'b'], separator: ',')
meson.add_devenv(env)
```

becomes one line:
```meson
meson.add_devenv({'FOO': ['a', 'b']}, method: 'prepend', separator: ',')
```

or two lines:
```meson
env = environment({'FOO': ['a', 'b']}, method: 'prepend', separator: ',')
meson.add_devenv(env)
```

## New custom dependency for libdl

```
dependency('dl')
```

will now check for the functionality of libdl.so, but first check if it is
provided in the libc (for example in libc on OpenBSD or in musl libc on linux).

## pkgconfig.generate will now include variables for builtin directories when referenced

When using the `variables:` family of kwargs to `pkgconfig.generate` to refer
to installed paths, traditionally only `prefix`, `includedir`, and `libdir`
were available by default, and generating a correct (relocatable) pkg-config
file required manually constructing variables for e.g. `datadir`.

Meson now checks each variable to see if it begins with a reference to a
standard directory, and if so, adds it to the list of directories for which a
builtin variable is created.

For example, before it was necessary to do this:
```meson
pkgconfig.generate(
    name: 'bash-completion',
    description: 'programmable completion for the bash shell',
    dataonly: true,
    variables: {
        'prefix': get_option('prefix'),
        'datadir': join_paths('${prefix}', get_option('datadir')),
        'sysconfdir': join_paths('${prefix}', get_option('sysconfdir')),

        'compatdir': '${sysconfdir}/bash_completion.d',
        'completionsdir': '${datadir}/bash-completion/completions',
        'helpersdir': '${datadir}/bash-completion/helpers',
    },
    install_dir: join_paths(get_option('datadir'), 'pkgconfig'),
)
```

Now the first three variables are not needed.

## New keyword argument `verbose` for tests and benchmarks

The new keyword argument `verbose` can be used to mark tests and benchmarks
that must always be logged verbosely on the console.  This is particularly
useful for long-running tests, or when a single Meson test() is wrapping
an external test harness.

## CMake support for versions <3.17.0 is deprecated

Support for CMake versions below 3.17.0 is now deprecated for *both* CMake
dependencies and subprojects.

## Removal of the RPM module

Due to lack of interest, lack of maintainership, and lack of a clear purpose,
the RPM module has been removed.

Users interested in one-shot tools to generate an RPM spec file template for
distro packaging, are encouraged develop an external tool that reads the
introspection data.

For more details, see https://github.com/mesonbuild/meson/issues/9764

## CMake server API support is removed

Support for the
[deprecated (since CMake 3.15)](https://cmake.org/cmake/help/latest/release/3.15.html#deprecated-and-removed-features)
and now
[removed (since CMake 3.20)](https://cmake.org/cmake/help/latest/release/3.20.html#deprecated-and-removed-features)
CMake server API is dropped from Meson.

The new CMake minimum version for CMake subprojects is now CMake 3.14.

## Rust proc-macro crates

Rust has these handy things called proc-macro crates, which are a bit like a
compiler plugin. We can now support them, simply build a [[shared_library]] with
the `rust_crate_type` set to `proc-macro`.

```meson
proc = shared_library(
  'proc',
  'proc.rs',
  rust_crate_type : 'proc-macro',
  install : false,
)

user = executable('user, 'user.rs', link_with : proc)
```

## found programs now have a version method

The return value of [[find_program]] can now check the exact version of the
found program, independent of the minimum version requirement. This can be used
e.g. to perform different actions depending on the exact version detected.

## Minimum required Python version updated to 3.7

Meson now requires at least Python version 3.7 to run as Python 3.6 reached EOL
on December 2021. In practice this should only affect people developing on
Ubuntu Bionic, who will need to manually install python3.8 from the official
repositories.


## Added support for XML translations using itstool

XML files can now be translated easier by using `itstool` via
`i18n.itstool_join()`. This ensures the XML is translated correctly
based on the defined ITS rules for the specific XML layout.

## JNI System Dependency Modules

The JNI system dependency now supports a `modules` keyword argument which is a
list containing any of the following: `jvm`, `awt`.

```meson
jni_dep = dependency('jni', version: '>= 1.8.0', modules: ['jvm', 'awt'])
```

This will add appropriate linker arguments to your target.

## New unstable wayland module

This module can search for protocol xml files from the wayland-protocols
package, and generate .c and .h files using wayland-scanner.

## Experimental command to convert environments to cross files

Meson has a new command `env2mfile` that can be used to convert
"environment variable based" cross and native compilation environments
to Meson machine files. This is especially convenient for e.g. distro
packagers so they can easily generate unambiguous configuration files
for package building.

As an example here's how you would generate a cross file that takes
its settings from the `CC`, `CXX`, `CFLAGS` etc environment variables.

    meson env2mfile --cross --system=baremetal --cpu=armv7 --cpu-family=arm -o armcross.txt

The command also has support for generating Debian build files using
system introspection:

    meson env2mfile --cross --debarch armhf -o debarmhf_cross.txt

Note how you don't need to specify any system details, the command
gets them transparently via `dpkg-architecture`.

Creating a native file is done in the same way:

    meson env2mfile --native -o current_system.txt

This system will detect if the `_FOR_BUILD` environment variables are
enabled and then uses them as needed.

With this you should be able to convert any envvar-based cross build
setup to cross and native files and then use those. This means, among
other things, that you can then run your compilations from any shell,
not just the special one that has all the environment variables set.

As this functionality is still a bit in flux, the specific behaviour
and command line arguments to use are subject to change. Because of
this the main documentation has not yet been updated.

Please try this for your use cases and report to us if it is working.
Patches to make the autodetection work on other distros and platforms
are also welcome.

## Added optional '--allow-dirty' flag for the 'dist' command

An optional `--allow-dirty` flag has been added to the `dist` command.

Previously, if uncommitted changes were present, Meson would warn about
this but continue with the dist process. It now errors out instead. The
error can be suppressed by using the `--allow-dirty` option.

## ldconfig is no longer run on install

Due to various issues of fragility and concern that it doesn't predictably do
the right thing, meson no longer runs ldconfig during `meson install`, and
users who need it run should run it themselves, instead.

## Added support for Texas Instruments MSP430 and ARM compilers

Meson now supports the TI [MSP430](https://www.ti.com/tool/MSP-CGT) and
[ARM](https://www.ti.com/tool/ARM-CGT) toolchains. The compiler and linker are
identified as `ti` and `ti-ar`, respectively. To maintain backwards
compatibility with existing build definitions, the [C2000
toolchain](https://www.ti.com/tool/C2000-CGT) is still identified as `c2000` and
`ar2000`.

## cmake.configure_package_config_file can now take a dict

The `configuration` kwarg of the `configure_package_config_file()` function
from the `cmake` module can now take a dict object, just like the regular
`configure_file()` function.

## Deprecated `java.generate_native_header()` in favor of the new `java.generate_native_headers()`

`java.generate_native_header()` was only useful for the most basic of
situations. It didn't take into account that in order to generate native
headers, you had to have all the referenced Java files. It also didn't take
into account inner classes. Do not use this function from `0.62.0` onward.

`java.generate_native_headers()` has been added as a replacement which should account for the previous function's shortcomings.

```java
// Outer.java

package com.mesonbuild;

public class Outer {
    private static native void outer();

    public static class Inner {
        private static native void inner();
    }
}
```

With the above file, an invocation would look like the following:

```meson
java = import('java')

native_headers = java.generate_native_headers(
    'Outer.java',
    package: 'com.mesonbuild',
    classes: ['Outer', 'Outer.Inner']
)
```

## New option to choose python installation environment

It is now possible to specify `-Dpython.install_env` and choose how python modules are installed.

- `venv`: assume that a virtualenv is active and install to that
- `system`: install to the global site-packages of the selected interpreter
  (the one that the venv module calls --system-site-packages)
- `prefix`: preserve existing behavior
- `auto`: autodetect whether to use venv or system

## JDK System Dependency Renamed from `jdk` to `jni`

The JDK system dependency is useful for creating native Java modules using the
JNI. Since the purpose is to find the JNI, it has been decided that a better
name is in fact "jni". Use of `dependency('jdk')` should be replaced with
`dependency('jni')`.

## i18n.merge_file no longer arbitrarily leaves your project half-built

The i18n module partially accounts for builds with NLS disabled, by disabling
gettext compiled translation catalogs if it cannot build them. Due to
implementation details, this also disabled important data files created via
merge_file, leading to important desktop files etc. not being installed.

This overreaction has been fixed. It is no longer possible to have NLS-disabled
builds which break the project by not installing important files which have
nothing to do with NLS (other than including some).

If you were depending on not having the Gettext tools installed and
successfully mis-building your project, you may need to make your project
actually work with NLS disabled, for example by providing some version of your
files which is still installed even when merge_file cannot be run.

## All directory options now support paths outside of prefix

Previously, Meson only allowed most directory options to be relative to prefix.
This restriction has been now lifted, bringing us in line with Autotools and
CMake. It is also useful for platforms like Nix, which install projects into
multiple independent prefixes.

As a consequence, `get_option` might return absolute paths for any
directory option, if a directory outside of prefix is passed. This
is technically a backwards incompatible change but its effect
should be minimal, thanks to widespread use of `join_paths`/
`/` operator and pkg-config generator module.

## `meson install --strip`

It is now possible to strip targets using `meson install --strip` even if
`-Dstrip=true` option was not set during configuration. This allows doing
stripped and not stripped installations without reconfiguring the build.

## Support for ARM Ltd. Clang toolchain

Support for the `armltdclang` compiler has been added. This differs from the
existing `armclang` toolchain in that it is a fork of Clang by ARM Ltd. and
supports native compilation. The Keil `armclang` toolchain only supports
cross-compilation to embedded devices.

## structured_sources()

A new function, `structured_sources()` has been added. This function allows
languages like Rust which depend on the filesystem layout at compile time to mix
generated and static sources.

```meson
executable(
  'main',
  structured_sources(
    'main.rs,
    {'mod' : generated_mod_rs},
  )
)
```

Meson will then at build time copy the files into the build directory (if
necessary), so that the desired file structure is laid out, and compile that. In
this case:

```
root/
  main.rs
  mod/
    mod.rs
```

## New custom dependency for OpenSSL

Detecting an OpenSSL installation in a cross-platform manner can be
complicated. Officially, pkg-config is supported by upstream. Unofficially,
cmake includes a FindOpenSSL using a different name and which requires
specifying modules.

Meson will now allow the pkg-config name to work in all cases using the following lookup order:
- prefer pkg-config if at all possible
- attempt to probe the system for the standard library naming, and retrieve the version from the headers
- if all else fails, check if cmake can find it

## D features in `declare_dependency`

`declare_dependency`accepts parameters for D specific features.
Accepted new parameters are `d_module_versions` and `d_import_dirs`.

This can be useful to propagate conditional compilation versions. E.g.:

```meson
my_lua_dep = declare_dependency(
    # ...
    d_module_versions: ['LUA_53'],
    d_import_dirs: include_directories('my_lua_folder'),
)
```

## arch_independent kwarg in cmake.write_basic_package_version_file

The `write_basic_package_version_file()` function from the `cmake` module
now supports an `arch_independent` kwarg, so that architecture checks in
the generated Package Version file are skipped, reproducing the behaviour of
CMake's [ARCH_INDEPENDENT](https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html#command:write_basic_package_version_file)
option.

## `dataonly` Pkgconfig Default Install Path

The default install path for `dataonly` pkgconfig files has changed from
`${libdir}/pkgconfig` to `${datadir}/pkgconfig`.

## JAR default install dir

The previous default for `jar()` was `libdir`. With this release, it has been
changed to `datadir/java`. Please open an issue if this is not a sane default
for your system.

