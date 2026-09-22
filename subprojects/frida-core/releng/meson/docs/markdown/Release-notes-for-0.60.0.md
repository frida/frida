---
title: Release 0.60.0
short-description: Release notes for 0.60.0
...

# New features

## `run_target` can now be used as a dependency

A `run_target()` can now be saved in a variable and reused as a dependency in
an `alias_target()`. This can be used to create custom alias rules that ensure
multiple other targets are run, even if those targets don't produce output
files.

For example:

```
i18n = import('i18n')

all_pot_targets = []

foo_i18n = i18n.gettext('foo')

all_pot_targets += foo_i18n[1]

alias_target('all-pot', all_pot_targets)
```

## The Python Modules dependency method no longer accepts positional arguments

Previously these were ignored with a warning, now they're a hard error.

## `unset_variable()`

`unset_variable()` can be used to unset a variable. Reading a variable after
calling `unset_variable()` will raise an exception unless the variable is set
again.

```meson
# tests/meson.build
tests = ['test1', 'test2']

# ...

unset_variable('tests')

# tests is no longer usable until it is set again
```

## Override python installation paths

The `python` module now has options to control where modules are installed:
- python.platlibdir: Directory for site-specific, platform-specific files.
- python.purelibdir: Directory for site-specific, non-platform-specific files.

Those options are used by python module methods `python.install_sources()` and
`python.get_install_dir()`. By default Meson tries to detect the correct installation
path, but make them relative to the installation `prefix`, which will often result
in installed python modules to not be found by the interpreter unless `prefix`
is `/usr` on Linux, or for example `C:\Python39` on Windows. These new options
can be absolute paths outside of `prefix`.

## New `subprojects packagefiles` subcommand

It is now possible to re-apply `meson.build` overlays (`patch_filename` or
`patch_directory` in the wrap ini file) after a subproject was downloaded and
set up, via `meson subprojects packagefiles --apply <wrap-name>`.

It is also possible for `patch_directory` overlays in a `[wrap-file]`, to copy
the packagefiles out of the subproject and back into `packagefiles/<patch_directory>/`
via `meson subprojects packagefiles --save <wrap-name>`. This is useful for
testing an edit in the subproject and then saving it back to the overlay which
is checked into git.

## Deprecated project options

Project options declared in `meson_options.txt` can now be marked as deprecated
and Meson will warn when user sets a value to it. It is also possible to deprecate
only some of the choices, and map deprecated values to a new value.

```meson
# Option fully deprecated, it warns when any value is set.
option('o1', type: 'boolean', deprecated: true)

# One of the choices is deprecated, it warns only when 'a' is in the list of values.
option('o2', type: 'array', choices: ['a', 'b'], deprecated: ['a'])

# One of the choices is deprecated, it warns only when 'a' is in the list of values
# and replace it by 'c'.
option('o3', type: 'array', choices: ['a', 'b', 'c'], deprecated: {'a': 'c'})

# A boolean option has been replaced by a feature, old true/false values are remapped.
option('o4', type: 'feature', deprecated: {'true': 'enabled', 'false': 'disabled'})

# A feature option has been replaced by a boolean, enabled/disabled/auto values are remapped.
option('o5', type: 'boolean', deprecated: {'enabled': 'true', 'disabled': 'false', 'auto': 'false'})
```

## More efficient static linking of uninstalled libraries

**Note**: This change had to be reverted in 0.60.1 because it caused regressions.
  New API will likely be introduced in 0.61.0 to have better control whether or
  not to create thin archive.

A somewhat common use case of [[static_library]] is to create uninstalled
internal convenience libraries which are solely meant to be linked to other
targets. Some build systems call these "object libraries". Meson's
implementation does always create a static archive.

This will now check to see if the static linker supports "thin archives"
(archives which do not contain the actual object code, only references to their
location on disk) and if so, use them to minimize space usage and speed up
linking.

## gnome.yelp variadic argument deprecation

`gnome.yelp` previously allowed sources to be passed either as variadic
arguments or as a keyword argument. If the keyword argument was given the
variadic arguments would be silently ignored. This has changed in 0.60.0, the
variadic form has been deprecated, and a warning is printed if both are given.

## `static` keyword argument to `meson.override_dependency()`

It is now possible to override shared and/or static dependencies separately.
When the `static` keyword argument is not specified in `dependency()`, the first
override will be used (`static_dep` in the example below).
```meson
static_lib = static_library()
static_dep = declare_dependency(link_with: static_lib)
meson.override_dependency('foo', static_dep, static: true)

shared_lib = shared_library()
shared_dep = declare_dependency(link_with: shared_lib)
meson.override_dependency('foo', shared_dep, static: false)

# Returns static_dep
dependency('foo')

# Returns static_dep
dependency('foo', static: true)

# Returns shared_dep
dependency('foo', static: false)
```

When the `static` keyword argument is not specified in `meson.override_dependency()`,
the dependency is assumed to follow the value of `default_library` option.
```meson
dep = declare_dependency(...)
meson.override_dependency('foo', dep)

# Always works
dependency('foo')

# Works only if default_library is 'static' or 'both'
dependency('foo', static: true)

# Works only if default_library is 'shared' or 'both'
dependency('foo', static: false)
```

## `dependency()` sets `default_library` on fallback subproject

When the `static` keyword argument is set but `default_library` is missing in
`default_options`, `dependency()` will set it when configuring fallback
subproject. `dependency('foo', static: true)` is now equivalent to
`dependency('foo', static: true, default_options: ['default_library=static'])`.

## install_emptydir function

It is now possible to define a directory which will be created during
installation, without creating it as a side effect of installing files into it.
This replaces custom `meson.add_install_script()` routines. For example:

```meson
meson.add_install_script('sh', '-c', 'mkdir -p "$DESTDIR/@0@"'.format(path))
```

can be replaced by:

```meson
install_emptydir(path)
```

and as a bonus this works reliably on Windows, prints a sensible progress
message, will be uninstalled by `ninja uninstall`, etc.

## Cython can now transpile to C++ as an intermediate language

Built-in cython support currently only allows C as an intermediate language, now
C++ is also allowed. This can be set via the `cython_language` option, either on
the command line, or in the meson.build files.

```meson
project(
  'myproject',
  'cython',
  default_options : ['cython_language=cpp'],
)
```

or on a per target basis with:
```meson
python.extension_module(
  'mod',
  'mod.pyx',
  override_options : ['cython_language=cpp'],
)
```

## New custom dependency for iconv

```
dependency('iconv')
```

will now check for the functionality of libiconv.so, but first check if it is
provided in the libc (for example in glibc or musl libc on Linux).

## Unknown options are now always fatal

Passing unknown options to "meson setup" or "meson configure" is now
always fatal. That is, Meson will exit with an error code if this
happens. Previous Meson versions only showed a warning message.

## Install DESTDIR relative to build directory

When `DESTDIR` environment or `meson install --destdir` option is a relative path,
it is now assumed to be relative to the build directory. An absolute path will be
set into environment when executing scripts. It was undefined behavior in prior
Meson versions but was working as relative to build directory most of the time.

## Java Module

The Java module has been added to Meson. The Java module allows users to
generate native header files without needing to use a `custom_target()`.

```meson
jmod = import('java')

native_header = jmod.generate_native_header('File.java', package: 'com.mesonbuild')
native_header_includes = include_directories('.')

jdkjava = shared_module(
  'jdkjava',
  [native_header_includes, other_sources],
  dependencies : [jdk],
  include_directories : [native_header_includes]
)
```

## Link tests can use sources for a different compiler

Usually, the `links` method of the compiler object uses a single program
invocation to do both compilation and linking.  Starting with this version,
whenever the argument to `links` is a file, Meson will check if the file
suffix matches the compiler object's language.  If they do not match,
as in the following case:

```
cxx = meson.get_compiler('cpp')
cxx.links(files('test.c'))
```

then Meson will separate compilation and linking.  In the above example
`test.c` will be compiled with a C compiler and the resulting object file
will be linked with a C++ compiler.  This makes it possible to detect
misconfigurations of the compilation environment, for example when the
C++ runtime is not compatible with the one expected by the C compiler.

For this reason, passing file arguments with an unrecognized suffix to
`links` will cause a warning.

## Relax restrictions of `str.join()`

Since 0.60.0, the [[str.join]] method can take an arbitrary number of arguments
instead of just one list. Additionally, all lists past to [[str.join]] will now
be flattened.

## Improvements for the Rustc compiler

- Werror now works, this set's `-D warnings`, which will cause rustc to error
  for every warning not explicitly disabled
- warning levels have been implemented
- support for meson's pic has been enabled

## The qt modules now accept generated outputs as inputs for qt.compile_*

This means you can use `custom_target`, custom_target indices
(`custom_target[0]`, for example), or the output of `generator.process` as
inputs to the various `qt.compile_*` methods.

```meson
qt = import('qt5')

ct = custom_target(...)

out = qt.compile_ui(sources : ct)
```

## Waf support in external-project module

If the first argument is `'waf'`, special treatment is done for the
[waf](https://waf.io/) build system. The waf executable must be
found either in the current directory, or in system `PATH`.

## Comparing two objects with different types is now an error

Using the `==` and `!=` operators to compare objects of different (for instance
`[1] == 1`) types was deprecated and undefined behavior since 0.45.0 and is
now a hard error.

## Installation tags

It is now possible to install only a subset of the installable files using
`meson install --tags tag1,tag2` command line.

See [documentation](Installing.md#installation-tags) for more details.

## Compiler.unittest_args has been removed

It's never been documented, and it's been marked deprecated for a long time, so
let's remove it.

## Dependencies with multiple names

More than one name can now be passed to `dependency()`, they will be tried in order
and the first name to be found will be used. The fallback subproject will be
used only if none of the names are found on the system. Once one of the name has
been found, all other names are added into the cache so subsequent calls for any
of those name will return the same value. This is useful in case a dependency
could have different names, such as `png` and `libpng`.

## i18n module now returns gettext targets

`r = i18n.gettext('mydomain')` will now provide access to:
- a list of built .mo files
- the mydomain-pot maintainer target which updates .pot files
- the mydomain-update-po maintainer target which updates .po files

## Added support for CLA sources when cross-compiling with the C2000 toolchain

Support for CLA sources has been added for cross-compilation with the C2000 toolchain.

## Support for clippy-driver as a rustc wrapper

Clippy is a popular linting tool for Rust, and is invoked in place of rustc as a
wrapper. Unfortunately it doesn't proxy rustc's output, so we need to have a
small wrapper around it so that Meson can correctly detect the underlying rustc,
but still display clippy

## Force Visual Studio environment activation

Since `0.59.0`, meson automatically activates Visual Studio environment on Windows
for all its subcommands, but only if no other compilers (e.g. `gcc` or `clang`)
are found, and silently continue if Visual Studio activation fails.

`meson setup --vsenv` command line argument can now be used to force Visual Studio
activation even when other compilers are found. It also make Meson abort with an
error message when activation fails. This is especially useful for GitHub Actions
because their Windows images have gcc in their PATH by default.

`--vsenv` is set by default when using `vs` backend.

Only `setup`, `compile`, `dist` and `devenv` subcommands now activate Visual Studio.

## MSVC compiler now assumes UTF-8 source code by default

Every project that uses UTF-8 source files had to add manually `/utf-8` C/C++
compiler argument for MSVC otherwise they wouldn't work on non-English locale.
Meson now switched the default to UTF-8 to be more consistent with all other
compilers.

This can be overridden but using `/source-charset`:
```meson
if cc.get_id() == 'msvc'
  add_project_arguments('/source-charset:.XYZ', language: ['c', 'cpp'])
endif
```

See Microsoft documentation for details:
https://docs.microsoft.com/en-us/cpp/build/reference/source-charset-set-source-character-set.

## Add support for `find_library` in Emscripten

The `find_library` method can be used to find your own JavaScript
libraries. The limitation is that they must have the file extension
`.js`. Other library lookups will look up "native" libraries from the
system like currently. A typical usage would look like this:

```meson
glue_lib = cc.find_library('gluefuncs.js',
                           dirs: meson.current_source_dir())
executable('prog', 'prog.c',
           dependencies: glue_lib)
```

## Optional `custom_target()` name

The name argument is now optional and defaults to the basename of the first
output.

