---
short-description: Built-in options to configure project properties
...

# Built-in options

Meson provides two kinds of options: [build options provided by the
build files](Build-options.md) and built-in options that are either
universal options, base options, compiler options.

## Universal options

All these can be set by passing `-Doption=value` to `meson` (aka `meson
setup`), or by setting them inside `default_options` of [[project]] in your
`meson.build`. Some options can also be set by `--option=value`, or `--option
value` -- a list is shown by running `meson setup --help`.

For legacy reasons `--warnlevel` is the cli argument for the
`warning_level` option.

They can also be edited after setup using `meson configure
-Doption=value`.

Installation options are usually relative to the prefix but it should
not be relied on, since they can be absolute paths in the following cases:

* When the prefix is `/usr`: `sysconfdir` defaults to `/etc`,
 `localstatedir` defaults to `/var`, and `sharedstatedir` defaults to
 `/var/lib`
* When the prefix is `/usr/local`: `localstatedir` defaults
 to `/var/local`, and `sharedstatedir` defaults to `/var/local/lib`
* When an absolute path outside of prefix is provided by the user/distributor.

### Directories

| Option                               | Default value | Description |
| ------                               | ------------- | ----------- |
| prefix                               | see below     | Installation prefix |
| bindir                               | bin           | Executable directory |
| datadir                              | share         | Data file directory |
| includedir                           | include       | Header file directory |
| infodir                              | share/info    | Info page directory |
| libdir                               | see below     | Library directory |
| licensedir                           | see below     | Licenses directory (since 1.1.0)|
| libexecdir                           | libexec       | Library executable directory |
| localedir                            | share/locale  | Locale data directory |
| localstatedir                        | var           | Localstate data directory |
| mandir                               | share/man     | Manual page directory |
| sbindir                              | sbin          | System executable directory |
| sharedstatedir                       | com           | Architecture-independent data directory |
| sysconfdir                           | etc           | Sysconf data directory |


`prefix` defaults to `C:/` on Windows, and `/usr/local` otherwise. You
should always override this value.

`libdir` is automatically detected based on your platform, it should
be correct when doing "native" (build machine == host machine)
compilation. For cross compiles Meson will try to guess the correct
libdir, but it may not be accurate, especially on Linux where
different distributions have different defaults. Using a [cross
file](Cross-compilation.md#defining-the-environment), particularly the
paths section may be necessary.

`licensedir` is empty by default. If set, it defines the default location
to install a dependency manifest and project licenses. For more details,
see [[meson.install_dependency_manifest]].

### Core options

Options that are labeled "per machine" in the table are set per
machine. See the [specifying options per
machine](#specifying-options-per-machine) section for details.

| Option                                 | Default value | Description                                                    | Is per machine | Is per subproject |
| -------------------------------------- | ------------- | -----------                                                    | -------------- | ----------------- |
| auto_features {enabled, disabled, auto} | auto         | Override value of all 'auto' features                          | no             | no                |
| backend {ninja, vs,<br>vs2010, vs2012, vs2013, vs2015, vs2017, vs2019, vs2022, xcode, none} | ninja | Backend to use    | no             | no                |
| genvslite {vs2022}                     | vs2022        | Setup multi-builtype ninja build directories and Visual Studio solution | no | no |
| buildtype {plain, debug,<br>debugoptimized, release, minsize, custom} | debug | Build type to use                       | no             | no                |
| debug                                  | true          | Enable debug symbols and other information                     | no             | no                |
| default_library {shared, static, both} | shared        | Default library type                                           | no             | yes               |
| errorlogs                              | true          | Whether to print the logs from failing tests.                  | no             | no                |
| install_umask {preserve, 0000-0777}    | 022           | Default umask to apply on permissions of installed files       | no             | no                |
| layout {mirror,flat}                   | mirror        | Build directory layout                                         | no             | no                |
| optimization {plain, 0, g, 1, 2, 3, s} | 0             | Optimization level                                             | no             | no                |
| pkg_config_path {OS separated path}    | ''            | Additional paths for pkg-config to search before builtin paths | yes            | no                |
| prefer_static                          | false         | Whether to try static linking before shared linking            | no             | no                |
| cmake_prefix_path                      | []            | Additional prefixes for cmake to search before builtin paths   | yes            | no                |
| stdsplit                               | true          | Split stdout and stderr in test logs                           | no             | no                |
| strip                                  | false         | Strip targets on install                                       | no             | no                |
| unity {on, off, subprojects}           | off           | Unity build                                                    | no             | no                |
| unity_size {>=2}                       | 4             | Unity file block size                                          | no             | no                |
| warning_level {0, 1, 2, 3, everything} | 1             | Set the warning level. From 0 = none to everything = highest   | no             | yes               |
| werror                                 | false         | Treat warnings as errors                                       | no             | yes               |
| wrap_mode {default, nofallback,<br>nodownload, forcefallback, nopromote} | default | Wrap mode to use                   | no             | no                |
| force_fallback_for                     | []            | Force fallback for those dependencies                          | no             | no                |
| vsenv                                  | false         | Activate Visual Studio environment                             | no             | no                |

#### Details for `backend`

Several build file formats are supported as command runners to build the
configured project. Meson prefers ninja by default, but platform-specific
backends are also available for better IDE integration with native tooling:
Visual Studio for Windows, and xcode for macOS. It is also possible to
configure with no backend at all, which is an error if you have targets to
build, but for projects that need configuration + testing + installation allows
for a lighter automated build pipeline.

#### Details for `genvslite`

Setup multiple buildtype-suffixed, ninja-backend build directories (e.g.
[builddir]_[debug/release/etc.]) and generate [builddir]_vs containing a Visual
Studio solution with multiple configurations that invoke a meson compile of the
setup build directories, as appropriate for the current configuration (builtype).

This has the effect of a simple setup macro of multiple 'meson setup ...'
invocations with a set of different buildtype values.  E.g.
`meson setup ... --genvslite vs2022 somebuilddir` does the following -
```
meson setup ... --backend ninja --buildtype debug somebuilddir_debug
meson setup ... --backend ninja --buildtype debugoptimized somebuilddir_debugoptimized
meson setup ... --backend ninja --buildtype release somebuilddir_release
```
and additionally creates another 'somebuilddir_vs' directory that contains
a generated multi-configuration visual studio solution and project(s) that are
set to build/compile with the somebuilddir_[...] that's appropriate for the
solution's selected buildtype configuration.

#### Details for `buildtype`

<a name="build-type-options"></a> For setting optimization levels and
toggling debug, you can either set the `buildtype` option, or you can
set the `optimization` and `debug` options which give finer control
over the same. Whichever you decide to use, the other will be deduced
from it. For example, `-Dbuildtype=debugoptimized` is the same as
`-Ddebug=true -Doptimization=2` and vice-versa. This table documents
the two-way mapping:

| buildtype      | debug | optimization |
| ---------      | ----- | ------------ |
| plain          | false | plain        |
| debug          | true  | 0            |
| debugoptimized | true  | 2            |
| release        | false | 3            |
| minsize        | true  | s            |

All other combinations of `debug` and `optimization` set `buildtype` to `'custom'`.

#### Details for `warning_level`

Exact flags per warning level is compiler specific, but there is an approximative
table for most common compilers.

| Warning level | GCC/Clang                | MSVC  |
| ------------- | ---                      | ----  |
| 0             |                          |       |
| 1             | -Wall                    | /W2   |
| 2             | -Wall -Wextra            | /W3   |
| 3             | -Wall -Wextra -Wpedantic | /W4   |
| everything    | -Weverything             | /Wall |

Clang's `-Weverything` is emulated on GCC by passing all known warning flags.

#### Details for `vsenv`

The `--vsenv` argument is supported since `0.60.0`, `-Dvsenv=true` syntax is supported
since `1.1.0`.

Since `0.59.0`, meson automatically activates a Visual Studio environment on Windows
for all its subcommands, but only if no other compilers (e.g. `gcc` or `clang`)
are found, and silently continues if Visual Studio activation fails.

Setting the `vsenv` option to `true` forces Visual Studio activation even when other
compilers are found. It also make Meson abort with an error message when activation
fails.

`vsenv` is `true` by default when using the `vs` backend.

## Base options

These are set in the same way as universal options, either by
`-Doption=value`, or by setting them inside `default_options` of
[[project]] in your `meson.build`. However, they cannot be shown in
the output of `meson setup --help` because they depend on both the current
platform and the compiler that will be selected. The only way to see
them is to setup a builddir and then run `meson configure` on it with
no options.

The following options are available. Note that they may not be
available on all platforms or with all compilers:

| Option              | Default value        | Possible values                                               | Description                                                                    |
|---------------------|----------------------|---------------------------------------------------------------|--------------------------------------------------------------------------------|
| b_asneeded          | true                 | true, false                                                   | Use -Wl,--as-needed when linking                                               |
| b_bitcode           | false                | true, false                                                   | Embed Apple bitcode, see below                                                 |
| b_colorout          | always               | auto, always, never                                           | Use colored output                                                             |
| b_coverage          | false                | true, false                                                   | Enable coverage tracking                                                       |
| b_lundef            | true                 | true, false                                                   | Don't allow undefined symbols when linking                                     |
| b_lto               | false                | true, false                                                   | Use link time optimization                                                     |
| b_lto_threads       | 0                    | Any integer*                                                  | Use multiple threads for lto. *(Added in 0.57.0)*                              |
| b_lto_mode          | default              | default, thin                                                 | Select between lto modes, thin and default. *(Added in 0.57.0)*                |
| b_thinlto_cache     | false                | true, false                                                   | Enable LLVM's ThinLTO cache for faster incremental builds. *(Added in 0.64.0)* |
| b_thinlto_cache_dir | (Internal build dir) | true, false                                                   | Specify where to store ThinLTO cache objects. *(Added in 0.64.0)*              |
| b_ndebug            | false                | true, false, if-release                                       | Disable asserts                                                                |
| b_pch               | true                 | true, false                                                   | Use precompiled headers                                                        |
| b_pgo               | off                  | off, generate, use                                            | Use profile guided optimization                                                |
| b_sanitize          | none                 | see below                                                     | Code sanitizer to use                                                          |
| b_staticpic         | true                 | true, false                                                   | Build static libraries as position independent                                 |
| b_pie               | false                | true, false                                                   | Build position-independent executables (since 0.49.0)                          |
| b_vscrt             | from_buildtype       | none, md, mdd, mt, mtd, from_buildtype, static_from_buildtype | VS runtime library to use (since 0.48.0) (static_from_buildtype since 0.56.0)  |

The value of `b_sanitize` can be one of: `none`, `address`, `thread`,
`undefined`, `memory`, `leak`, `address,undefined`, but note that some
compilers might not support all of them. For example Visual Studio
only supports the address sanitizer.

\* < 0 means disable, == 0 means automatic selection, > 0 sets a specific number to use

LLVM supports `thin` lto, for more discussion see [LLVM's documentation](https://clang.llvm.org/docs/ThinLTO.html)

<a name="b_vscrt-from_buildtype"></a>
The default value of `b_vscrt` is `from_buildtype`. The following table is
used internally to pick the CRT compiler arguments for `from_buildtype` or
`static_from_buildtype` *(since 0.56)* based on the value of the `buildtype`
option:

| buildtype      | from_buildtype | static_from_buildtype |
| --------       | -------------- | --------------------- |
| debug          | `/MDd`         | `/MTd`                |
| debugoptimized | `/MD`          | `/MT`                 |
| release        | `/MD`          | `/MT`                 |
| minsize        | `/MD`          | `/MT`                 |
| custom         | error!         | error!                |

### Notes about Apple Bitcode support

`b_bitcode` will pass `-fembed-bitcode` while compiling and will pass
`-Wl,-bitcode_bundle` while linking. These options are incompatible
with `b_asneeded`, so that option will be silently disabled.

[[shared_module]]s will not have
bitcode embedded because `-Wl,-bitcode_bundle` is incompatible with
both `-bundle` and `-Wl,-undefined,dynamic_lookup` which are necessary
for shared modules to work.

## Compiler options

Same caveats as base options above.

The following options are available. They can be set by passing
`-Doption=value` to `meson`. Note that both the options themselves and
the possible values they can take will depend on the target platform
or compiler being used:

| Option           | Default value | Possible values                          | Description |
| ------           | ------------- | ---------------                          | ----------- |
| c_args           |               | free-form comma-separated list           | C compile arguments to use |
| c_link_args      |               | free-form comma-separated list           | C link arguments to use |
| c_std            | none          | none, c89, c99, c11, c17, c18, c2x, c23, gnu89, gnu99, gnu11, gnu17, gnu18, gnu2x, gnu23 | C language standard to use |
| c_winlibs        | see below     | free-form comma-separated list           | Standard Windows libs to link against |
| c_thread_count   | 4             | integer value ≥ 0                        | Number of threads to use with emcc when using threads |
| cpp_args         |               | free-form comma-separated list           | C++ compile arguments to use |
| cpp_link_args    |               | free-form comma-separated list           | C++ link arguments to use |
| cpp_std          | none          | none, c++98, c++03, c++11, c++14, c++17, c++20 <br/>c++2a, c++1z, gnu++03, gnu++11, gnu++14, gnu++17, gnu++1z, <br/> gnu++2a, gnu++20, vc++14, vc++17, vc++20, vc++latest | C++ language standard to use |
| cpp_debugstl     | false         | true, false                              | C++ STL debug mode |
| cpp_eh           | default       | none, default, a, s, sc                  | C++ exception handling type |
| cpp_rtti         | true          | true, false                              | Whether to enable RTTI (runtime type identification) |
| cpp_thread_count | 4             | integer value ≥ 0                        | Number of threads to use with emcc when using threads |
| cpp_winlibs      | see below     | free-form comma-separated list           | Standard Windows libs to link against |
| fortran_std      | none          | [none, legacy, f95, f2003, f2008, f2018] | Fortran language standard to use |
| cuda_ccbindir    |               | filesystem path                          | CUDA non-default toolchain directory to use (-ccbin) *(Added in 0.57.1)* |

The default values of `c_winlibs` and `cpp_winlibs` are in
compiler-specific argument forms, but the libraries are: kernel32,
user32, gdi32, winspool, shell32, ole32, oleaut32, uuid, comdlg32,
advapi32.

All these `<lang>_*` options are specified per machine. See below in
the [specifying options per machine](#specifying-options-per-machine)
section on how to do this in cross builds.

When using MSVC, `cpp_eh=none` will result in no exception flags being
passed, while the `cpp_eh=[value]` will result in `/EH[value]`. Since
*0.51.0* `cpp_eh=default` will result in `/EHsc` on MSVC. When using
gcc-style compilers, nothing is passed (allowing exceptions to work),
while `cpp_eh=none` passes `-fno-exceptions`.

Since *0.54.0* The `<lang>_thread_count` option can be used to control
the value passed to `-s PTHREAD_POOL_SIZE` when using emcc. No other
c/c++ compiler supports this option.

Since *0.63.0* all compiler options can be set per subproject, see
[here](#specifying-options-per-subproject) for details on how the default value
is inherited from the main project. This is useful, for example, when the main
project requires C++11, but a subproject requires C++14. The `cpp_std` value
from the subproject's `default_options` is now respected.

Since *1.3.0* `c_std` and `cpp_std` options now accept a list of values.
Projects that prefer GNU C, but can fallback to ISO C, can now set, for
example, `default_options: 'c_std=gnu11,c11'`, and it will use `gnu11` when
available, but fallback to c11 otherwise. It is an error only if none of the
values are supported by the current compiler.
Likewise, a project that can take benefit of `c++17` but can still build with
`c++11` can set `default_options: 'cpp_std=c++17,c++11'`.
This allows us to deprecate `gnuXX` values from the MSVC compiler. That means
that `default_options: 'c_std=gnu11'` will now print a warning with MSVC
but fallback to `c11`. No warning is printed if at least one
of the values is valid, i.e. `default_options: 'c_std=gnu11,c11'`.
In the future that deprecation warning will become an hard error because
`c_std=gnu11` should mean GNU is required, for projects that cannot be
built with MSVC for example.

## Specifying options per machine

Since *0.51.0*, some options are specified per machine rather than
globally for all machine configurations. Prefixing the option with
`build.` only affects the build machine configuration, while leaving it
unprefixed only affects the host machine configuration.
For example:

 - `build.pkg_config_path` controls the paths pkg-config will search
   for `native: true` (build machine) dependencies.

 - `pkg_config_path` controls the paths pkg-config will search for
   `native: false` (host machine) dependencies.

This is useful for cross builds. In native builds, the build and host
machines are the same, and the unprefixed option alone will suffice.

Prior to *0.51.0*, these options only affected native builds when
specified on the command line as there was no `build.` prefix.
Similarly named fields in the `[properties]` section of the cross file
would affect cross compilers, but the code paths were fairly different,
allowing differences in behavior to crop out.

## Specifying options per subproject

Since *0.54.0* `default_library` and `werror` built-in options can be
defined per subproject. This is useful, for example, when building
shared libraries in the main project and statically linking a subproject,
or when the main project must build with no warnings but some subprojects
cannot.

Most of the time, this would be used either in the parent project by
setting subproject's default_options (e.g. `subproject('foo',
default_options: 'default_library=static')`), or by the user through the
command line: `-Dfoo:default_library=static`.

The value is overridden in this order:
- Value from parent project
- Value from subproject's default_options if set
- Value from subproject() default_options if set
- Value from command line if set

Since *0.56.0* `warning_level` can also be defined per subproject.

## Module options

Some Meson modules have built-in options. They can be set by prefixing the
option with the module's name:
`-D<module>.<option>=<value>` (e.g. `-Dpython.platlibdir=/foo`).

### Pkgconfig module

| Option      | Default value | Possible values | Description                                                |
|-------------|---------------|-----------------|------------------------------------------------------------|
| relocatable | false         | true, false     | Generate the pkgconfig files as relocatable (Since 0.63.0) |

*Since 0.63.0* The `pkgconfig.relocatable` option is used by the
pkgconfig module–namely [`pkg.generate()`](Pkgconfig-module.md)–and
affects how the `prefix` (not to be confused with the
[install prefix](#directories)) in the generated pkgconfig file is set.
When it is `true`, the `prefix` will be relative to the `install_dir`-this
allows the pkgconfig file to be moved around and still work, as long
as the relative path is not broken. In general, this allows for the whole
installed package to be placed anywhere on the system and still work as a
dependency. When it is set to `false`, the `prefix` will be the same as
the install prefix.

An error will be raised if `pkgconfig.relocatable` is `true` and the
`install_dir` for a generated pkgconfig file points outside the
install prefix. For example: if the install prefix is `/usr` and the
`install_dir` for a pkgconfig file is `/var/lib/pkgconfig`.

### Python module

| Option            | Default value | Possible values             | Description |
| ------            | ------------- | -----------------           | ----------- |
| bytecompile       | 0             | integer from -1 to 2        | What bytecode optimization level to use (Since 1.2.0) |
| install_env       | prefix        | {auto,prefix,system,venv}   | Which python environment to install to (Since 0.62.0) |
| platlibdir        |               | Directory path              | Directory for site-specific, platform-specific files (Since 0.60.0) |
| purelibdir        |               | Directory path              | Directory for site-specific, non-platform-specific files  (Since 0.60.0) |
| allow_limited_api | true          | true, false                 | Disables project-wide use of the Python Limited API (Since 1.3.0) |

*Since 0.60.0* The `python.platlibdir` and `python.purelibdir` options are used
by the python module methods `python.install_sources()` and
`python.get_install_dir()`; Meson tries to detect the correct installation paths
and make them relative to the installation `prefix` by default which will often
result in the interpreter not finding the installed python modules unless
`prefix` is `/usr` on Linux, or, for instance, `C:\Python39` on Windows. These
options can be absolute paths outside of `prefix`.

*Since 0.62.0* The `python.install_env` option is used to detect the correct
installation path. Setting to `system` will avoid making the paths relative to
`prefix` and instead use the global site-packages of the selected python
interpreter directly, even if it is a venv. Setting to `venv` will instead use
the paths for the virtualenv the python found installation comes from (or fail
if it is not a virtualenv).  Setting to `auto` will check if the found
installation is a virtualenv, and use `venv` or `system` as appropriate (but
never `prefix`). This option is mutually exclusive with the `platlibdir`/`purelibdir`.

For backwards compatibility purposes, the default `install_env` is `prefix`.

*Since 1.2.0* The `python.bytecompile` option can be used to enable compiling
python bytecode. Bytecode has 3 optimization levels:

- 0, bytecode without optimizations
- 1, bytecode with some optimizations
- 2, bytecode with some more optimizations

To this, Meson adds level `-1`, which is to not attempt to compile bytecode at
all.

*Since 1.3.0* The `python.allow_limited_api` option affects whether the
`limited_api` keyword argument of the `extension_module` method is respected.
If set to `false`, the effect of the `limited_api` argument is disabled.
