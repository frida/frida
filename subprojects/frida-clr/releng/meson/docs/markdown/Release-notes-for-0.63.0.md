---
title: Release 0.63.0
short-description: Release notes for 0.63.0
...

# New features

## `add_project_dependencies()` function

Dependencies can now be added to all build products using
`add_project_dependencies()`.  This can be useful in several
cases:

* with special dependencies such as `dependency('threads')`
* with system libraries such as `find_library('m')`
* with the `include_directories` keyword argument of
`declare_dependency()`, to add both source and build
directories to the include search path

## Coverage targets now respect tool config files

gcovr >= 4.2 supports `gcovr.cfg` in the project source root to configure how
coverage is generated. If Meson detects that gcovr will load this file, it no
longer excludes the `subprojects/` directory from coverage. It's a good default
for Meson to guess that projects want to ignore it, but not all projects prefer
that and it is assumed that if a gcovr.cfg exists then it will manually
include/exclude desired paths.

lcov supports `.lcovrc`, but only as a systemwide or user setting. This is
non-ideal for projects, so Meson will now detect one in the project source root
and, if present, manually tell lcov to use it.

## D compiler checks

Some compiler checks are implemented for D:
 - `run`
 - `sizeof`
 - `has_header` (to check if a module is present)
 - `alignment`

Example:

```meson
ptr_size = meson.get_compiler('d').sizeof('void*')
```

## Deprecate an option and replace it with a new one

The `deprecated` keyword argument can now take the name of a new option
that replace this option. In that case, setting a value on the deprecated option
will set the value on both the old and new names, assuming they accept the same
values.

```meson
# A boolean option has been replaced by a feature with another name, old true/false values
# are accepted by the new option for backward compatibility.
option('o1', type: 'boolean', value: 'true', deprecated: 'o2')
option('o2', type: 'feature', value: 'enabled', deprecated: {'true': 'enabled', 'false': 'disabled'})

# A project option is replaced by a module option
option('o3', type: 'string', value: '', deprecated: 'python.platlibdir')
```

## Running Windows executables with Wine in `meson devenv`

When cross compiling for Windows, `meson devenv` now sets `WINEPATH` pointing to
all directories containing needed DLLs and executables.

## Diff files for wraps

Wrap files can now define `diff_files`, a list of local patch files in `diff`
format. Meson will apply the diff files after extracting or cloning the project,
and after applying the overlay archive (`patch_*`). For this feature, the
`patch` or `git` command-line tool must be available.

## Added preserve_path arg to install_headers

The [[install_headers]] function now has an optional argument `preserve_path`
that allows installing multi-directory headerfile structures that live
alongside sourcecode with a single command.

For example, the headerfile structure

```meson
headers = [
  'one.h',
  'two.h',
  'alpha/one.h',
  'alpha/two.h',
  'alpha/three.h'
  'beta/one.h'
]
```

can now be passed to `install_headers(headers, subdir: 'mylib', preserve_path: true)`
and the resulting directory tree will look like

```
{prefix}
└── include
    └── mylib
        ├── alpha
        │   ├── one.h
        │   ├── two.h
        │   └── three.h
        ├── beta
        │   └── one.h
        ├── one.h
        └── two.h
```

## JAR Resources

The ability to add resources to a JAR has been added. Use the `java_resources`
keyword argument. It takes a `structured_src` object.

```meson
jar(
  meson.project_name(),
  sources,
  main_class: 'com.mesonbuild.Resources',
  java_resources: structured_sources(
    files('resources/resource1.txt'),
    {
      'subdir': files('resources/subdir/resource2.txt'),
    }
  )
)
```

To access these resources in your Java application:

```java
try (InputStreamReader reader = new InputStreamReader(
        Resources.class.getResourceAsStream("/resource1.txt"),
        StandardCharsets.UTF_8)) {
    // ...
}

try (InputStreamReader reader = new InputStreamReader(
        Resources.class.getResourceAsStream("/subdir/resource2.txt"),
        StandardCharsets.UTF_8)) {
    // ...
}
```

## Support for mold linker added

The high performance linker mold can be selected via `CC_LD` or `CXX_LD` for
Clang and GCC >= 12.0.1.

## MSVC now sets the __cplusplus #define accurately

MSVC will always return `199711L` for `__cplusplus`, even when a newer c++
standard is explicitly requested, unless you pass a specific option to the
compiler for MSVC 2017 15.7 and newer. Older versions are unaffected by this.

Microsoft's stated rationale is that "a lot of existing code appears to depend
on the value of this macro matching 199711L", therefore for compatibility with
such (MSVC-only) code they will require opting in to the standards-conformant
value.

Meson now always sets the option if it is available, as it is unlikely that
users want the default behavior, and *impossible* to use the default behavior
in cross-platform code (which frequently breaks as soon as the first person
tries to compile using MSVC).

## Added `debug` function

In addition to the `message()`, `warning()` and `error()` functions there is now the
`debug()` function to log messages that only end up in the `meson-log.txt` logfile
and are not printed to stdout at configure time.

## Compiler options can be set per subproject

All compiler options can now be set per subproject. See
[here](Build-options.md#specifying-options-per-subproject) for details on how
the default value is inherited from main project.

This is useful for example when the main project requires C++11 but a subproject
requires C++14. The `cpp_std` value from subproject's `default_options` is now
respected.

## Per-subproject languages

Subprojects does not inherit languages added by main project or other subprojects
any more. This could break subprojects that wants to compile e.g. `.c` files but
did not add `c` language, either in `project()` or `add_languages()`, and were
relying on the main project to do it for them.

## Installed pkgconfig files can now be relocatable

The pkgconfig module now has a module option `pkgconfig.relocatable`.
When set to `true`, the pkgconfig files generated will have their
`prefix` variable set to be relative to their `install_dir`.

For example to enable it from the command line run:

```sh
meson setup builddir -Dpkgconfig.relocatable=true …
```

It will only work if the `install_dir` for the generated pkgconfig
files are located inside the install prefix of the package. Not doing
so will cause an error.

This should be useful on Windows or any other platform where
relocatable packages are desired.

## New prefer_static built-in option

Users can now set a boolean, `prefer_static`, that controls whether or not
static linking should be tried before shared linking. This option acts as
strictly a preference. If the preferred linking method is not successful,
then Meson will fallback and try the other linking method. Specifically
setting the `static` kwarg in the meson.build will take precedence over
the value of `prefer_static` for that specific `dependency` or
`find_library` call.

## Python extension modules now depend on the python library by default

Python extension modules are usually expected to link to the python library
and/or its headers in order to build correctly (via the default `embed: false`,
which may not actually link to the library itself). This means that every
single use of `.extension_module()` needed to include the `dependencies:
py_installation.dependency()` kwarg explicitly.

In the interest of doing the right thing out of the box, this is now the
default for extension modules that don't already include a dependency on
python. This is not expected to break anything, because it should always be
needed. Nevertheless, `py_installation.dependency().partial_dependency()` will
be detected as already included while providing no compile/link args.

## Python extension modules now build with hidden visibility

Python extension modules are usually expected to only export a single symbol,
decorated with the `PyMODINIT_FUNC` macro and providing the module entry point.
On versions of python >= 3.9, the python headers contain GNU symbol visibility
attributes to mark the init function with default visibility; it is then safe
to set the [[shared_module]] inherited kwarg `gnu_symbol_visibility: 'hidden'`.

In the interest of doing the right thing out of the box, this is now the
default for extension modules for found installations that are new enough to
have this set, which is not expected to break anything, but remains possible to
set explicitly (in which case that will take precedence).

## Added support for multiline fstrings

Added support for multiline f-strings which use the same syntax as f-strings
for string substitution.

```meson
x = 'hello'
y = 'world'

msg = f'''Sending a message...
"@x@ @y@"
'''
```

which produces:

```
Sending a message....

"hello world"

```

