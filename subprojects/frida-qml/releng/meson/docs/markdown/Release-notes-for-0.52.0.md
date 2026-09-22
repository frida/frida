---
title: Release 0.52.0
short-description: Release notes for 0.52.0
...

# New features

## Gettext targets are ignored if `gettext` is not installed

Previously the `i18n` module has errored out when `gettext` tools are
not installed on the system. Starting with this version they will
become no-ops instead. This makes it easier to build projects on
minimal environments (such as when bootstrapping) that do not have
translation tools installed.

## Support taking environment values from a dictionary

`environment()` now accepts a dictionary as first argument.  If
provided, each key/value pair is added into the `environment_object`
as if `set()` method was called for each of them.

On the various functions that take an `env:` keyword argument, you may
now give a dictionary.

## alias_target

``` meson
runtarget alias_target(target_name, dep1, ...)
```

This function creates a new top-level target. Like all top-level
targets, this integrates with the selected backend. For instance, with
Ninja you can run it as `ninja target_name`. This is a dummy target
that does not execute any command, but ensures that all dependencies
are built. Dependencies can be any build target (e.g. return value of
executable(), custom_target(), etc)


## Enhancements to the pkg_config_path argument

Setting sys_root in the [properties] section of your cross file will
now set PKG_CONFIG_SYSROOT_DIR automatically for host system
dependencies when cross compiling.

## The Meson test program now accepts an additional "--gdb-path" argument to specify the GDB binary

`meson test --gdb testname` invokes GDB with the specific test case. However, sometimes GDB is not in the path or a GDB replacement is wanted.
Therefore, a `--gdb-path` argument was added to specify which binary is executed (per default `gdb`):

```console
$ meson test --gdb --gdb-path /my/special/location/for/gdb testname
$ meson test --gdb --gdb-path cgdb testname
```

## Better support for illumos and Solaris

illumos (and hopefully Solaris) support has been dramatically
improved, and one can reasonably expect projects to compile.

## Splitting of Compiler.get_function_attribute('visibility')

On macOS there is no `protected` visibility, which results in the
visibility check always failing. 0.52.0 introduces two changes to
improve this situation:

1. the "visibility" check no longer includes "protected"
2. a new set of "split" checks are introduced which check for a single
   attribute instead of all attributes.

These new attributes are:
* visibility:default
* visibility:hidden
* visibility:internal
* visibility:protected

## Clang-tidy target

If `clang-tidy` is installed and the project's source root contains a
`.clang-tidy` (or `_clang-tidy`) file, Meson will automatically define
a `clang-tidy` target that runs Clang-Tidy on all source files.

If you have defined your own `clang-tidy` target, Meson will not
generate its own target.

## Add blocks dependency

Add `dependency('blocks')` to use the Clang blocks extension.

## Meson's builtin b_lundef is now supported on macOS

This has always been possible, but there are some additional
restrictions on macOS (mainly do to Apple only features). With the
linker internal re-architecture this has become possible

## Compiler and dynamic linker representation split

0.52.0 includes a massive refactor of the representations of compilers to
tease apart the representations of compilers and dynamic linkers (ld). This
fixes a number of compiler/linker combinations. In particular this fixes
use GCC and vanilla clang on macOS.

## Add `depth` option to `wrap-git`

To allow shallow cloning, an option `depth` has been added to `wrap-git`.
This applies recursively to submodules when `clone-recursive` is set to `true`.

Note that the git server may have to be configured to support shallow cloning
not only for branches but also for tags.

## Enhancements to the source_set module

`SourceSet` objects now provide the `all_dependencies()` method, that
complement the existing `all_sources()` method.

## added `--only test(s)` option to run_project_tests.py

Individual tests or a list of tests from run_project_tests.py can be selected like:
```
python run_project_tests.py --only fortran

python run_project_tests.py --only fortran python3
```

This assists Meson development by only running the tests for the
portion of Meson being worked on during local development.

## Experimental Webassembly support via Emscripten

Meson now supports compiling code to Webassembly using the Emscripten
compiler. As with most things regarding Webassembly, this support is
subject to change.

## Version check in `find_program()`

A new `version` keyword argument has been added to `find_program` to
specify the required version. See [`dependency()`](#dependency) for
argument format. The version of the program is determined by running
`program_name --version` command. If stdout is empty it fallbacks to
stderr. If the output contains more text than simply a version number,
only the first occurrence of numbers separated by dots is kept. If the
output is more complicated than that, the version checking will have
to be done manually using [`run_command()`](#run_command).

## Added `vs_module_defs` to `shared_module()`

Like `shared_library()`, `shared_module()` now accepts
`vs_module_defs` argument for controlling symbol exports, etc.

## Improved support for static libraries

Static libraries had numerous shortcomings in the past, especially
when using uninstalled static libraries. This release brings many
internal changes in the way they are handled, including:

- `link_whole:` of static libraries. In the example below, lib2 used to miss
  symbols from lib1 and was unusable.
```meson
lib1 = static_library(sources)
lib2 = static_library(other_sources, link_whole : lib1, install : true)
```
- `link_with:` of a static library with an uninstalled static library. In the
example below, lib2 now implicitly promote `link_with:` to `link_whole:` because
the installed lib2 would otherwise be unusable.
```meson
lib1 = static_library(sources, install : false)
lib2 = static_library(sources, link_with : lib1, install : true)
```
- pkg-config generator do not include uninstalled static libraries. In the example
  below, the generated `.pc` file used to be unusable because it contained
  `Libs.private: -llib1` and `lib1.a` is not installed. `lib1` is now omitted
  from the `.pc` file because the `link_with:` has been promoted to
  `link_whole:` (see above) and thus lib1 is not needed to use lib2.
```meson
lib1 = static_library(sources, install : false)
lib2 = both_libraries(sources, link_with : lib1, install : true)
pkg.generate(lib2)
```

Many projects have been using `extract_all_objects()` to work around
those issues, and hopefully those hacks could now be removed. Since
this is a pretty large change, please double check if your static
libraries behave correctly, and report any regression.

## Enhancements to the kconfig module

`kconfig.load()` may now accept a `configure_file()` as input file.

## Added `include_type` kwarg to `dependency`

The `dependency()` function now has a `include_type` kwarg. It can take the
values `'preserve'`, `'system'` and `'non-system'`. If it is set to `'system'`,
all include directories of the dependency are marked as system dependencies.

The default value of `include_type` is `'preserve'`.

Additionally, it is also possible to check and change the
`include_type` state of an existing dependency object with the new
`include_type()` and `as_system()` methods.

## Enhancements to `configure_file()`

`input:` now accepts multiple input file names for `command:`-configured file.

`depfile:` keyword argument is now accepted. The dependency file can
list all the additional files the configure target depends on.

## Projects args can be set separately for build and host machines (potentially breaking change)

Simplify `native` flag behavior in `add_global_arguments`,
`add_global_link_arguments`, `add_project_arguments` and
`add_project_link_arguments`. The rules are now very simple:

 - `native: true` affects `native: true` targets

 - `native: false` affects `native: false` targets

 - No native flag is the same as `native: false`

This further simplifies behavior to match the "build vs host" decision
done in last release with `c_args` vs `build_c_args`. The underlying
motivation in both cases is to execute the same commands whether the
overall build is native or cross.

## Allow checking if a variable is a disabler

Added the function `is_disabler(var)`. Returns true if a variable is a disabler
and false otherwise.


## gtkdoc-check support

`gnome.gtkdoc()` now has a `check` keyword argument. If `true` runs it
will run `gtkdoc-check` when running unit tests. Note that this has
the downside of rebuilding the doc for each build, which is often very
slow. It usually should be enabled only in CI.

## `gnome.gtkdoc()` returns target object

`gnome.gtkdoc()` now returns a target object that can be passed as
dependency to other targets using generated doc files (e.g. in
`content_files` of another doc).

## Dist is now a top level command

Previously creating a source archive could only be done with `ninja
dist`. Starting with this release Meson provides a top level `dist`
that can be invoked directly. It also has a command line option to
determine which kinds of archives to create:

```meson
meson dist --formats=xztar,zip
```
