---
title: Release 1.3.0
short-description: Release notes for 1.3.0
...

# New features

Meson 1.3.0 was released on 19 November 2023
## Clarify of implicitly-included headers in C-like compiler checks

Compiler check methods `compiler.compute_int()`, `compiler.alignment()`
and `compiler.sizeof()` now have their implicitly-included headers
corrected and documented.

`<stdio.h>` was included unintentionally when cross-compiling, which
is less than ideal because there is no guarantee that a standard library
is available for the target platform. Only `<stddef.h>` is included instead.

For projects that depend on the old behavior, the compiler check methods
have an optional argument `prefix`, which can be used to specify additional
`#include` directives.

## Treat warnings as error in compiler checks

Compiler check methods `compiler.compiles()`, `compiler.links()` and `compiler.run()`
now have a new `werror: true` keyword argument to treat compiler warnings as error.
This can be used to check if code compiles without warnings.

## Compilers now have a `has_define` method

This method returns true if the given preprocessor symbol is
defined, else false is returned. This is useful is cases where
an empty define has to be distinguished from a non-set one, which
is not possible using `get_define`.

Additionally it makes intent clearer for code that only needs
to check if a specific define is set at all and does not care
about its value.

## [[configure_file]] now has a `macro_name` parameter.

This new paramater, `macro_name` allows C macro-style include guards to be added
to [[configure_file]]'s output when a template file is not given. This change
simplifies the creation of configure files that define macros with dynamic names
and want the C-style include guards.

## `c_std` and `cpp_std` options now accepts a list of values

Projects that prefer GNU C, but can fallback to ISO C, can now set, for
example, `default_options: 'c_std=gnu11,c11'`, and it will use `gnu11` when
available, but fallback to `c11` otherwise. It is an error only if none of the
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

## More meaningful description of many generative tasks

When a module uses a `CustomTarget` to process files, it now has the possibility
to customize the message displayed by ninja.

Many modules were updated to take advantage of this new feature.

## Deprecate 'jar' as a build_target type

The point of `build_target()` is that what is produced can be conditionally
changed. However, `jar()` has a significant number of non-overlapping arguments
from other build_targets, including the kinds of sources it can include. Because
of this crafting a `build_target` that can be used as a Jar and as something
else is incredibly hard to do. As such, it has been deprecated, and using
`jar()` directly is recommended.

## generator.process() gains 'env' keyword argument

Like the kwarg of the same name in `custom_target()`, `env` allows
you to set the environment in which the generator will process inputs.

## Target names for executables now take into account suffixes.

In previous versions of meson, a `meson.build` file like this:

```
exectuable('foo', 'main.c')
exectuable('foo', 'main.c', name_suffix: 'bar')
```

would result in a configure error because meson internally used
the same id for both executables. This build file is now allowed
since meson takes into account the `bar` suffix when generating the
second executable. This allows for executables with the same basename
but different suffixes to be built in the same subdirectory.

## Executable gains vs_module_defs keyword

This allows using a .def file to control which functions an [[executable]] will
expose to a [[shared_module]].

## find_program() now supports the 'default_options' argument

In a similar fashion as dependency(), find_program() now also allows you to set default 
options for the subproject that gets built in case of a fallback.

## `fs.relative_to()`

The `fs` module now has a `relative_to` method. The method will return the
relative path from argument one to argument two, if one exists. Otherwise, the
absolute path to argument one is returned.

```meson
assert(fs.relative_to('c:\\prefix\\lib', 'c:\\prefix\\bin') == '..\\lib')
assert(fs.relative_to('c:\\proj1\\foo', 'd:\\proj1\\bar') == 'c:\\proj1\\foo')
assert(fs.relative_to('prefix\\lib\\foo', 'prefix') == 'lib\\foo')

assert(fs.relative_to('/prefix/lib', '/prefix/bin') == '../lib')
assert(fs.relative_to('prefix/lib/foo', 'prefix') == 'lib/foo')
```

In addition to strings, it can handle files, custom targets, custom target
indices, and build targets.

## Added follow_symlinks arg to install_data, install_header, and install_subdir

The [[install_data]], [[install_headers]], [[install_subdir]] functions now
have an optional argument `follow_symlinks` that, if set to `true`, makes it so
symbolic links in the source are followed, rather than copied into the
destination tree, to match the old behavior.  The default, which is currently
to follow links, is subject to change in the future.

## Added 'fill' kwarg to int.to_string()

int.to_string() now accepts a `fill` argument. This allows you to pad the
string representation of the integer with leading zeroes:

```meson
n = 4
message(n.to_string())
message(n.to_string(fill: 3))

n = -4
message(n.to_string(fill: 3))
```

OUTPUT:
```meson
4
004
-04
```

## Added 'json' output_format to configure_file()

When no input file is specified, [[configure_file]] can now
generate a `json` file from given [[@cfg_data]]. 
Field descriptions are not preserved in the json file.

## `@GLOBAL_SOURCE_ROOT@` and `@DIRNAME@` in machine files

Some tokens are now replaced in the machine file before parsing it:
- `@GLOBAL_SOURCE_ROOT@`: the absolute path to the project's source tree
- `@DIRNAME@`: the absolute path to the machine file's parent directory.

It can be used, for example, to have paths relative to the source directory, or
relative to toolchain's installation directory.
```ini
[binaries]
c = '@DIRNAME@/toolchain/gcc'
exe_wrapper = '@GLOBAL_SOURCE_ROOT@' / 'build-aux' / 'my-exe-wrapper.sh'
```

## clang-tidy-fix target

If `clang-tidy` is installed and the project's source root contains a
`.clang-tidy` (or `_clang-tidy`) file, Meson will automatically define
a `clang-tidy-fix` target that runs `run-clang-tidy` tool with `-fix`
option to apply the changes found by clang-tidy to the source code.

If you have defined your own `clang-tidy-fix` target, Meson will not
generate its own target.

## Meson compile command now accepts suffixes for TARGET

The syntax for specifying a target for meson compile is now
`[PATH_TO_TARGET/]TARGET_NAME.TARGET_SUFFIX[:TARGET_TYPE]` where
`TARGET_SUFFIX` is the suffix argument given in the build target
within meson.build. It is optional and `TARGET_NAME` remains
sufficient if it uniquely resolves to one single target.

## New environment variable `MESON_PACKAGE_CACHE_DIR`

If the `MESON_PACKAGE_CACHE_DIR` environment variable is set, it is used instead of the
project's `subprojects/packagecache`. This allows sharing the cache across multiple
projects. In addition it can contain an already extracted source tree as long as it
has the same directory name as the `directory` field in the wrap file. In that
case, the directory will be copied into `subprojects/` before applying patches.

## Update options with `meson setup <builddir> -Dopt=value`

If the build directory already exists, options are updated with their new value
given on the command line (`-Dopt=value`). Unless `--reconfigure` is also specified,
this won't reconfigure immediately. This has the same behaviour as
`meson configure <builddir> -Dopt=value`.

Previous Meson versions were simply a no-op.

## Clear persistent cache with `meson setup --clearcache`

Just like `meson configure --clearcache`, it is now possible to clear the cache
and reconfigure in a single command with `meson setup --clearcache --reconfigure <builddir>`.

## pkg-config dependencies can now get a variable with multiple replacements

When using [[dep.get_variable]] and defining a `pkgconfig_define`, it is
sometimes useful to remap multiple dependency variables. For example, if the
upstream project changed the variable name that is interpolated and it is
desirable to support both versions.

It is now possible to pass multiple pairs of variable/value.

The same applies to the compatibility [[dep.get_pkgconfig_variable]] method.

## Machine files: `pkgconfig` field deprecated and replaced by `pkg-config`

Meson used to allow both `pkgconfig` and `pkg-config` entries in machine files,
the former was used for `dependency()` lookup and the latter was used as return
value for `find_program('pkg-config')`.

This inconsistency is now fixed by deprecating `pkgconfig` in favor of
`pkg-config` which matches the name of the binary. For backward compatibility
it is still allowed to define both with the same value, in that case no
deprecation warning is printed.

## Support targeting Python's limited C API

The Python module's `extension_module` function has gained the ability
to build extensions which target Python's limited C API via a new keyword
argument: `limited_api`.

## All compiler `has_*` methods support the `required` keyword

Now instead of

```meson
assert(cc.has_function('some_function'))
assert(cc.has_type('some_type'))
assert(cc.has_member('struct some_type', 'x'))
assert(cc.has_members('struct some_type', ['x', 'y']))
```

we can use

```meson
cc.has_function('some_function', required: true)
cc.has_type('some_type', required: true)
cc.has_member('struct some_type', 'x', required: true)
cc.has_members('struct some_type', ['x', 'y'], required: true)
```

## Deprecated `rust_crate_type` and replaced by `rust_abi`

The new `rust_abi` keyword argument is accepted by [[shared_library]],
[[static_library]], [[library]] and [[shared_module]] functions. It can be either
`'rust'` (the default) or `'c'` strings.

`rust_crate_type` is now deprecated because Meson already knows if it's a shared
or static library, user only need to specify the ABI (Rust or C).

`proc_macro` crates are now handled by the [`rust.proc_macro()`](Rust-module.md#proc_macro)
method.

## Tests now abort on errors by default under sanitizers

Sanitizers like AddressSanitizer and UndefinedBehaviorSanitizer do not abort
by default on detected violations. Meson now exports `ASAN_OPTIONS` and `UBSAN_OPTIONS`
when unset in the environment to provide sensible abort-by-default behavior.

## `<lang>_(shared|static)_args` for both_library, library, and build_target

We now allow passing arguments like `c_static_args` and `c_shared_args`. This
allows a [[both_libraries]] to have arguments specific to either the shared or
static library, as well as common arguments to both.

There is a drawback to this, since Meson now cannot re-use object files between
the static and shared targets. This could lead to much higher compilation time
when using a [[both_libraries]] if there are many sources.

## `-j` shorthand for `--num-processes`

`-j` now means the same thing as `--num-processes`. It was inconsistently
supported only in some subcommands. Now you may use it everywhere

## Unified message(), str.format() and f-string formatting

They now all support the same set of values: strings, integers, bools, options,
dictionaries and lists thereof.

- Feature options (i.e. enabled, disabled, auto) were not previously supported
  by any of those functions.
- Lists and dictionaries were not previously supported by f-string.
- str.format() allowed any type and often resulted in printing the internal
  representation which is now deprecated.

## Subprojects excluded from scan-build reports

The `scan-build` target, created when using the `ninja` backend with `scan-build`
present, now excludes bugs found in subprojects from its final report.

## vs_module_defs keyword now supports indexes of custom_target

This means you can do something like:
```meson
defs = custom_target('generate_module_defs', ...)
shared_library('lib1', vs_module_defs : defs[0])
shared_library('lib2', vs_module_defs : defs[2])
```

## Automatic fallback to `cmake` and `cargo` subproject

CMake subprojects have been supported for a while using the `cmake.subproject()`
module method. However until now it was not possible to use a CMake subproject
as fallback in a `dependency()` call.

A wrap file can now specify the method used to build it by setting the `method`
key in the wrap file's first section. The method defaults to `meson`.

Supported methods:
- `meson` requires `meson.build` file.
- `cmake` requires `CMakeLists.txt` file. [See details](Wrap-dependency-system-manual.md#cmake-wraps).
- `cargo` requires `Cargo.toml` file. [See details](Wrap-dependency-system-manual.md#cargo-wraps).

