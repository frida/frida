---
title: Release 0.48
short-description: Release notes for 0.48
...

# New features

## Toggles for build type, optimization and vcrt type

Since the very beginning Meson has provided different project types to
use, such as *debug* and *minsize*. There is also a *plain* type that
adds nothing by default but instead makes it the user's responsibility
to add everything by hand. This works but is a bit tedious.

In this release we have added new options to manually toggle e.g.
optimization levels and debug info so those can be changed
independently of other options. For example by default the debug
buildtype has no optimization enabled at all. If you wish to use GCC's
`-Og` instead, you could set it with the following command:

```
meson configure -Doptimization=g
```

Similarly we have added a toggle option to select the version of
Visual Studio C runtime to use. By default it uses the debug runtime
DLL debug builds and release DLL for release builds but this can be
manually changed with the new base option `b_vscrt`.

## Meson warns if two calls to `configure_file()` write to the same file

If two calls to
[[configure_file]] write to the
same file Meson will print a `WARNING:` message during configuration.
For example:

```meson
project('configure_file', 'cpp')

configure_file(
  input: 'a.in',
  output: 'out',
  command: ['./foo.sh']
)
configure_file(
  input: 'a.in',
  output: 'out',
  command: ['./foo.sh']
)
```

This will output:

```
The Meson build system
Version: 0.47.0.dev1
Source dir: /path/to/srctree
Build dir: /path/to/buildtree
Build type: native build
Project name: configure_file
Project version: undefined
Build machine cpu family: x86_64
Build machine cpu: x86_64
Configuring out with command
WARNING: Output file out for configure_file overwritten. First time written in line 3 now in line 8
Configuring out with command
Build targets in project: 0
Found ninja-1.8.2 at /usr/bin/ninja
```

## New kwarg `console` for `custom_target()`

This keyword argument conflicts with `capture`, and is meant for
commands that are resource-intensive and take a long time to
finish. With the Ninja backend, setting this will add this target to
[Ninja's `console`
pool](https://ninja-build.org/manual.html#_the_literal_console_literal_pool),
which has special properties such as not buffering stdout and
serializing all targets in this pool.

The primary use-case for this is to be able to run external commands
that take a long time to execute. Without setting this, the user does
not receive any feedback about what the program is doing.

## `dependency(version:)` now applies to all dependency types

Previously, version constraints were only enforced for dependencies
found using the pkg-config dependency provider. These constraints now
apply to dependencies found using any dependency provider.

Some combinations of dependency, host and method do not currently
support discovery of the version. In these cases, the dependency will
not be found if a version constraint is applied, otherwise the
`version()` method for the dependency object will return `'unknown'`.

(If discovering the version in one of these combinations is important
to you, and a method exists to determine the version in that case,
please file an issue with as much information as possible.)

## python3 module is deprecated

A generic module `python` has been added in Meson `0.46.0` and has a superset of
the features implemented by the previous `python3` module.

In most cases, it is a simple matter of renaming:
```meson
py3mod = import('python3')
python = py3mod.find_python()
```

becomes

```meson
pymod = import('python')
python = pymod.find_installation()
```

## Dictionary addition

Dictionaries can now be added, values from the second dictionary overrides values
from the first

```meson
d1 = {'a' : 'b'}
d3 = d1 + {'a' : 'c'}
d3 += {'d' : 'e'}
```

## Dist scripts

You can now specify scripts that are run as part of the `dist`
target. An example usage would go like this:

```meson
project('foo', 'c')

# other stuff here

meson.add_dist_script('dist_cleanup.py')
```

## Fatal warnings

A new command line option has been added: `--fatal-meson-warnings`.
When enabled, any warning message printed by Meson will be fatal and
raise an exception. It is intended to be used by developers and CIs to
easily catch deprecation warnings, or any other potential issues.

## Helper methods added for checking GNU style attributes: `__attribute__(...)`

A set of new helpers have been added to the C and C++ compiler objects
for checking GNU style function attributes. These are not just simpler
to use, they may be optimized to return fast on compilers that don't
support these attributes. Currently this is true for MSVC.

```meson
cc = meson.get_compiler('c')
if cc.has_function_attribute('aligned')
   add_project_arguments('-DHAVE_ALIGNED', language : 'c')
endif
```

Would replace code like:

```meson
if cc.compiles('''into foo(void) __attribute__((aligned(32)))''')
   add_project_arguments('-DHAVE_ALIGNED', language : 'c')
endif
```

Additionally, a multi argument version has been added:

```meson
foreach s : cc.get_supported_function_attributes(['hidden', 'alias'])
   add_project_arguments('-DHAVE_@0@'.format(s.to_upper()), language : 'c')
endforeach
```

## `gnome.generate_gir()` now optionally accepts multiple libraries

The GNOME module can now generate a single gir for multiple libraries,
which is something `g-ir-scanner` supported, but had not been exposed
yet.

gnome.generate_gir() will now accept multiple positional arguments, if
none of these arguments are an `Executable` instance.

## Hotdoc module

A new module has been written to ease generation of
[hotdoc](https://hotdoc.github.io/) based documentation. It supports
complex use cases such as hotdoc subprojects (to create documentation
portals) and makes it straight forward to leverage full capabilities
of hotdoc.

Simple usage:

``` meson
hotdoc = import('hotdoc')

hotdoc.generate_doc(
  'foobar',
  c_smart_index: true,
  project_version: '0.1',
  sitemap: 'sitemap.txt',
  index: 'index.md',
  c_sources: ['path/to/file.c'],
  languages: ['c'],
  install: true,
)
```

## `i18n.merge_file()` now fully supports variable substitutions defined in `custom_target()`

Filename substitutions like @BASENAME@ and @PLAINNAME@ were previously
accepted but the name of the build target wasn't altered leading to
colliding target names when using the substitution twice.
i18n.merge_file() now behaves as custom_target() in this regard.

## Projects args can be set separately for cross and native builds (potentially breaking change)

It has been a longstanding bug (or let's call it a "delayed bug fix")
that if you do this:

```meson
add_project_arguments('-DFOO', language : 'c')
```

Then the flag is used both in native and cross compilations. This is
very confusing and almost never what you want. To fix this a new
keyword `native` has been added to all functions that add arguments,
namely `add_global_arguments`, `add_global_link_arguments`,
`add_project_arguments` and `add_project_link_arguments` that behaves
like the following:

```meson
# Added to native builds when compiling natively and to cross
# compilations when doing cross compiles.
add_project_arguments(...)

# Added only to native compilations, not used in cross compilations.
add_project_arguments(..., native : true)

# Added only to cross compilations, not used in native compilations.
add_project_arguments(..., native : false)
```

Also remember that cross compilation is a property of each target.
There can be target that are compiled with the native compiler and
some which are compiled with the cross compiler.

Unfortunately this change is backwards incompatible and may cause some
projects to fail building. However this should be very rare in
practice.

## More flexible `override_find_program()`.

It is now possible to pass an `executable` to
`override_find_program()` if the overridden program is not used during
configure.

This is particularly useful for fallback dependencies like Protobuf
that also provide a tool like protoc.

## `shared_library()` now supports setting dylib compatibility and current version

Now, by default `shared_library()` sets `-compatibility_version` and
`-current_version` of a macOS dylib using the `soversion`.

This can be overridden by using the `darwin_versions:` kwarg to
[[shared_library]]. As usual,
you can also pass this kwarg to `library()` or `build_target()` and it
will be used in the appropriate circumstances.

## Version comparison

`dependency(version:)` and other version constraints now handle
versions containing non-numeric characters better, comparing versions
using the rpmvercmp algorithm (as using the `pkg-config` autoconf
macro `PKG_CHECK_MODULES` does).

This is a breaking change for exact comparison constraints which rely
on the previous comparison behaviour of extending the compared
versions with `'0'` elements, up to the same length of `'.'`-separated
elements.

For example, a version of `'0.11.0'` would previously match a version
constraint of `'==0.11'`, but no longer does, being instead considered
strictly greater.

Instead, use a version constraint which exactly compares with the
precise version required, e.g. `'==0.11.0'`.

## Keyword argument for GNU symbol visibility

Build targets got a new keyword, `gnu_symbol_visibility` that controls
how symbols are exported from shared libraries. This is most commonly
used to hide implementation symbols like this:

```meson
shared_library('mylib', ...
  gnu_symbol_visibility: 'hidden')
```

In this case only symbols explicitly marked as visible in the source
files get exported.

## Git wraps can now clone submodules automatically

To enable this, the following needs to be added to the `.wrap` file:

```ini
clone-recursive=true
```

## `subproject()` function now supports the `required:` kwarg

This allows you to declare an optional subproject. You can now call
`found()` on the return value of the `subproject()` call to see if the
subproject is available before calling `get_variable()` to fetch
information from it.

## `dependency()` objects now support the `.name()` method

You can now fetch the name of the dependency that was searched like
so:

```meson
glib_dep = dependency('glib-2.0')
...
message("dependency name is " + glib_dep.name())
# This outputs `dependency name is glib-2.0`

qt_dep = dependency('qt5')
...
message("dependency name is " + qt_dep.name())
# This outputs `dependency name is qt5`

decl_dep = declare_dependency()
...
message("dependency name is " + decl_dep.name())
# This outputs `dependency name is internal`
```
