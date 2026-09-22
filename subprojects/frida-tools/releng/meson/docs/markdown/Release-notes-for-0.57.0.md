---
title: Release 0.57.0
short-description: Release notes for 0.57.0
...

# New features

## Project version can be specified with a file

Meson can be instructed to load a project's version string from an
external file like this:

```meson
project('foo', 'c', version: files('VERSION'))
```

The version file must contain exactly one line of text which will
be used as the project's version. If the line ends in a newline
character, it is removed.

## Support for reading files at configuration time with the `fs` module

Reading text files during configuration is now supported. This can be done at
any time after `project` has been called

```meson
project('myproject', 'c')
license_text = run_command(
    find_program('python3'), '-c', 'print(open("COPYING").read())'
).stdout().strip()
about_header = configuration_data()
about_header.add('COPYRIGHT', license_text)
about_header.add('ABOUT_STRING', meson.project_name())
...
```

There are several problems with the above approach:
1. It's ugly and confusing
2. If `COPYING` changes after configuration, Meson won't correctly rebuild when
   configuration data is based on the data in COPYING
3. It has extra overhead

`fs.read` replaces the above idiom thus:
```meson
project('myproject', 'c')
fs = import('fs')
license_text = fs.read('COPYING').strip()
about_header = configuration_data()
about_header.add('COPYRIGHT', license_text)
about_header.add('ABOUT_STRING', meson.project_name())
...
```

They are not equivalent, though. Files read with `fs.read` create a
configuration dependency on the file, and so if the `COPYING` file is modified,
Meson will automatically reconfigure, guaranteeing the build is consistent. It
can be used for any properly encoded text files. It supports specification of
non utf-8 encodings too, so if you're stuck with text files in a different
encoding, it can be passed as an argument. See the [[@meson]]
documentation for details.

## meson install --dry-run

New option to meson install command that does not actually install files, but
only prints messages.

## Experimental support for C++ modules in Visual Studio

Modules are a new C++ 20 feature for organising source code aiming to
increase compilation speed and reliability. This support is
experimental and may change in future releases. It only works with the
latest preview release of Visual Studio.

## Qt6 module

A module for Qt6 is now available with the same functionality as the Qt5
module.

Currently finding Qt6 is only available via `qmake` as pkg-config files aren't
generated (see [QTBUG-86080](https://bugreports.qt.io/browse/QTBUG-86080)) and
CMake support is not available for this module yet.

## Unstable Rust module

A new unstable module has been added to make using Rust with Meson easier.
Currently, it adds a single function to ease defining Rust tests, as well as a
wrapper around bindgen, making it easier to use.

## Meson test() now accepts `protocol : 'rust'`

This allows native Rust tests to be run and parsed by Meson; simply set the
protocol to `rust` and Meson takes care of the rest.

## MSVC/Clang-Cl Argument Changes/Cleanup

* "Disable Debug" (`/Od`) is no longer manually specified for optimization levels {`0`,`g`} (it is already the default for MSVC).
* "Run Time Checking" (`/RTC1`) removed from `debug` buildtype by default
* Clang-CL `debug` buildtype arguments now match MSVC arguments
* There is now no difference between `buildtype` flags and `debug` + `optimization` flags

The /Od flag has been removed, as it is already the default in the MSVC compilers, and conflicts with other user options.

/RTC1 conflicts with other RTC argument types as there are many different options, and has been removed by default.
Run Time Checking can be enabled by manually adding `/RTC1` or other RTC flags of your choice.

The `debug` buildtype for clang-cl added additional arguments compared to MSVC, which had more to do with optimization than debug. The arguments removed are `/Ob0`, `/Od`, `/RTC1`. (`/Zi` was also removed, but it is already added by default when debug is enabled.)

If these are important issues for you and would like builtin toggle options,
please file an issue in the Meson bug tracker.

## Buildtype remains even if dependent options are changed

Setting the `buildtype` option to a value sets the `debug` and
`optimization` options to predefined values. Traditionally setting the
options to other values would then change the buildtype to `custom`.
This is confusing and means that you can't use, for example, debug
level `g` in `debug` buildtype even though it would make sense under
many circumstances.

Starting with this release, the buildtype is only changed when the user
explicitly sets it; setting the build type modifies the `debug` and
`optimization` options as before.

## Passing internal dependencies to the compiler object

Methods on the compiler object (such as `compiles`, `links`, `has_header`)
can be passed dependencies returned by `declare_dependency`, as long as they
only specify compiler/linker arguments or other dependencies that satisfy
the same requirements.

## `unstable-external_project` improvements

- Default arguments are added to `add_project()` in case some tags are not found
  in `configure_options`: `'--prefix=@PREFIX@'`, `'--libdir=@PREFIX@/@LIBDIR@'`,
  and `'--includedir=@PREFIX@/@INCLUDEDIR@'`. It was previously considered a fatal
  error to not specify them.

- When the `verbose` keyword argument is not specified, or is false, command outputs
  are written on file in `<builddir>/meson-logs/`.

- The `LD` environment variable is not passed any more when running the configure
  script. It caused issues because Meson sets `LD` to the `CC` linker wrapper but
  autotools expects it to be a real linker (e.g. `/usr/bin/ld`).

## `gnome.post_install()`

Post-install update of various system wide caches. Each script will be executed
only once even if `gnome.post_install()` is called multiple times from multiple
subprojects. If `DESTDIR` is specified during installation all scripts will be
skipped.

Currently supports `glib-compile-schemas`, `gio-querymodules`, and
`gtk-update-icon-cache`.

## "Edit and continue" (/ZI) is no longer used by default for Visual Studio

Meson was adding the `/ZI` compiler argument as an argument for Visual Studio
in debug mode. This enables the `edit-and-continue` debugging in
Visual Studio IDE's.

Unfortunately, it is also extremely expensive and breaks certain use cases such
as link time code generation. Edit and continue can be enabled by manually by
adding `/ZI` to compiler arguments.

The `/ZI` argument has now been replaced by the `/Zi` argument for debug builds.

If this is an important issue for you and would like a builtin toggle option,
please file an issue in the Meson bug tracker.

## Minimum required Python version updated to 3.6

Meson now requires at least Python version 3.6 to run as Python 3.5
reaches EOL on September 2020. In practice this should only affect
people developing on Ubuntu Xenial, which will similarly reach EOL in
April 2021.

## Packaging a subproject

The `meson dist` command can now create a distribution tarball for a subproject
in the same git repository as the main project. This can be useful if parts of
the project (e.g. libraries) can be built and distributed separately. In that
case they can be moved into `subprojects/mysub` and running `meson dist` in that
directory will now create a tarball containing only the source code from that
subdir and not the rest of the main project or other subprojects.

For example:
```sh
git clone https://github.com/myproject
cd myproject/subprojects/mysubproject
meson setup builddir
meson dist -C builddir
```

## `custom_target()` and `run_target()` now accepts an `env` keyword argument

Environment variables can now be passed to the `custom_target()` command.

```meson
env = environment()
env.append('PATH', '/foo')
custom_target(..., env: env)
custom_target(..., env: {'MY_ENV': 'value'})
custom_target(..., env: ['MY_ENV=value'])
```

## `summary()` accepts external programs or dependencies

External program objects and dependency objects can be passed to
`summary()` as the value to be printed.

## CMake `find_package` version support

It is now possible to specify a requested package version for the CMake
dependency backend via the new `cmake_package_version` kwarg in the
`dependency` function.

## `meson test` only rebuilds test dependencies

Until now, `meson test` rebuilt the whole project independent of the
requested tests and their dependencies.  With this release, `meson test`
will only rebuild what is needed for the tests or suites that will be run.
This feature can be used, for example, to speed up bisecting regressions
using commands like the following:

    git bisect start <broken commit> <working commit>
    git bisect run meson test <failing test name>

This would find the broken commit automatically while at each step
rebuilding only those pieces of code needed to run the test.

However, this change could cause failures when upgrading to 0.57, if the
dependencies are not specified correctly in `meson.build`.

## The `add_*_script` methods now accept a File as the first argument

Meson now accepts `file` objects, including those produced by
`configure_file`, as the first parameter of the various
`add_*_script` methods

```meson
install_script = configure_file(
  configuration : conf,
  input : 'myscript.py.in',
  output : 'myscript.py',
)

meson.add_install_script(install_script, other, params)
```

## Unity build with Vala disabled

The approach that meson has used for Vala unity builds is incorrect, we
combine the generated C files like we would any other C file. This is very
fragile however, as the Vala compiler generates helper functions and macros
which work fine when each file is a separate translation unit, but fail when
they are combined.

## New logging format for `meson test`

The console output format for `meson test` has changed in several ways.
The major changes are:

* if stdout is a tty, `meson` includes a progress report.

* if `--print-errorlogs` is specified, the logs are printed as tests run
rather than afterwards.  All the error logs are printed rather than only
the first ten.

* if `--verbose` is specified and `--num-processes` specifies more than
one concurrent test, test output is buffered and printed after the
test finishes.

* the console logs include a reproducer command.  If `--verbose` is
specified, the command is printed for all tests at the time they start;
otherwise, it is printed for failing tests at the time the test finishes.

* for TAP and Rust tests, Meson is able to report individual subtests.  If
`--verbose` is specified, all tests are reported.  If `--print-errorlogs`
is specified, only failures are.

In addition, if `--verbose` was specified, Meson used not to generate
logs.  This limitation has now been removed.

These changes make the default `ninja test` output more readable, while
`--verbose` output provides detailed, human-readable logs that
are well suited to CI environments.

## Specify DESTDIR on command line

`meson install` command now has a `--destdir` argument that overrides `DESTDIR`
from environment.

## Skip install scripts if DESTDIR is set

`meson.add_install_script()` now has `skip_if_destdir` keyword argument. If set
to `true` the script won't be run if `DESTDIR` is set during installation. This is
useful in the case the script updates system wide caches, or performs other tasks
that are only needed when copying files into final destination.

## Add support for prelinked static libraries

The static library gains a new `prelink` keyword argument that can be
used to prelink object files in that target. This is currently only
supported for the GNU toolchain, patches to add it to other compilers
are most welcome.

## Rust now has an `std` option

Rust calls these `editions`, however, Meson generally refers to such language
versions as "standards", or `std` for short.  Therefore, Meson's Rust support
uses `std` for consistency with other languages.

## Ctrl-C behavior in `meson test`

Starting from this version, sending a `SIGINT` signal (or pressing `Ctrl-C`)
to `meson test` will interrupt the longest running test.  Pressing `Ctrl-C`
three times within a second will exit `meson test`.

## Support added for LLVM's thinLTO

A new `b_lto_mode` option has been added, which may be set to `default` or
`thin`. Thin only works for clang, and only with gnu gold, lld variants, or
ld64.

## `test()` timeout and timeout_multiplier value <= 0

`test(..., timeout: 0)`, or negative value, used to abort the test immediately
but now instead allow infinite duration. Note that omitting the `timeout`
keyword argument still defaults to 30s timeout.

Likewise, `add_test_setup(..., timeout_multiplier: 0)`, or
`meson test --timeout-multiplier 0`, or negative value, disable tests timeout.


## Knob to control LTO thread

Both the gnu linker and lld support using threads for speeding up LTO, meson
now provides a knob for this: `-Db_lto_threads`. Currently this is only
supported for clang and gcc. Any positive integer is supported, `0` means
`auto`. If the compiler or linker implements its own `auto` we use that,
otherwise the number of threads on the machine is used.

## `summary()` now uses left alignment for both keys and values

Previously it aligned keys toward the center, but this was deemed harder
to read than having everything left aligned.

## `//` is now allowed as a function id for `meson rewrite`.

msys bash may expand `/` to a path, breaking
`meson rewrite kwargs set project / ...`. Passing `//` will be converted to
`/` by msys bash but in order to keep usage shell-agnostic, this release
also allows `//` as the id.  This way, `meson rewrite kwargs set project
// ...` will work in both msys bash and other shells.

## Get keys of configuration data object

All keys of the `configuration_data` object can be obtained with the `keys()`
method as an alphabetically sorted array.
