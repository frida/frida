---
title: Release 1.4.0
short-description: Release notes for 1.4.0
...

# New features

Meson 1.4.0 was released on 12 March 2024

## Added support for `[[@build_tgt]]`, `[[@custom_tgt]]`, and `[[@custom_idx]]` to certain FS module functions

Support for `[[@build_tgt]]`, `[[@custom_tgt]]`, and `[[@custom_idx]]` was
added to the following FS module APIs:

- `fs.name`
- `fs.parent`
- `fs.replace_suffix`
- `fs.stem`

## Meson now reads the project version of cmake subprojects

CMake subprojects configured by meson will now have their project
version set to the project version in their CMakeLists.txt. This
allows version constraints to be properly checked when falling back to
a cmake subproject.

## `ndebug` setting now controls C++ stdlib assertions

The `ndebug` setting, if disabled, now passes preprocessor defines to enable
debugging assertions within the C++ standard library.

For GCC, `-D_GLIBCXX_ASSERTIONS=1` is set.

For Clang, `-D_GLIBCXX_ASSERTIONS=1` is set to cover libstdc++ usage,
and `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE` or
`-D_LIBCPP_ENABLE_ASSERTIONS=1` is used depending on the Clang version.

## `stldebug` gains Clang support

For Clang, we now pass `-D_GLIBCXX_DEBUG=1` if `debugstl` is enabled, and
we also pass `-D_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG`.

## New `unset()` method on `environment` objects

[[@env]] now has an [[env.unset]] method to ensure an existing environment
is *not* defined.

## File object now has `full_path()` method

Returns a full path pointing to the file. This is useful for printing the path
with e.g [[message]] function for debugging purpose.

**NOTE:** In most cases using the object itself will do the same job
as this and will also allow Meson to setup dependencies correctly.

## New numpy custom dependency

Support for `dependency('numpy')` was added, via supporting the `numpy-config` tool and
pkg-config support, both of which are available since NumPy 2.0.0.

Config-tool support is useful because it will work out of the box when
``numpy`` is installed, while the pkg-config file is located inside python's
site-packages, which makes it impossible to use in an out of the box manner
without setting `PKG_CONFIG_PATH`.

## `depends` kwarg now supported by compiler.preprocess()

It is now possible to specify the dependent targets with `depends:`
for compiler.preprocess(). These targets should be built before the
preprocessing starts.

## Added `preserve_paths` keyword argument to qt module functions.

In `qt4`, `qt5`, and `qt6` modules, `compile_ui`, `compile_moc`, and
`preprocess` functions now have a `preserve_paths` keyword argument.

If `'true`, it specifies that the output files need to maintain their directory
structure inside the target temporary directory. For instance, when a file
called `subdir/one.input` is processed it generates a file
`{target private directory}/subdir/one.out` when `true`,
and `{target private directory}/one.out` when `false` (default).

## Bindgen will now use Meson's heuristic for what is a C++ header

Bindgen natively assumes that a file with the extension `.hpp` is a C++ header,
but that everything else is a C header. Meson has a whole list of extensions it
considers to be C++, and now will automatically look for those extensions and
set bindgen to treat those as C++

## Overriding bindgen language setting

Even though Meson will now tell bindgen to do the right thing in most cases,
there may still be cases where Meson does not have the intended behavior,
specifically with headers with a `.h` suffix, but are C++ headers.

## Bindgen now uses the same C/C++ std as the project as a whole

Which is very important for C++ bindings.

## Tests now abort on errors by default under more sanitizers

Sanitizers like MemorySanitizer do not abort
by default on detected violations. Meson now exports `MSAN_OPTIONS` (in addition to
`ASAN_OPTIONS` and `UBSAN_OPTIONS` from a previous release) when unset in the
environment to provide sensible abort-by-default behavior.

## Use `custom_target` as test executable

The [[test]] function now accepts [[@custom_tgt]] and [[@custom_idx]] for the
command to execute.

