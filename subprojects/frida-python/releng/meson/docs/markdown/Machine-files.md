# Cross and Native File reference

Cross and native files are nearly identical, but not completely. This
is the documentation on the common values used by both, for the
specific values of one or the other see the [cross
compilation](Cross-compilation.md) and [native
environments](Native-environments.md).

*Changed in 0.56.0* Keys within sections are now case sensitive. This
is required to make project options work correctly.

## Data Types

There are four basic data types in a machine file:
- strings
- arrays
- booleans
- integers

A string is specified single quoted:
```ini
[section]
option1 = 'false'
option2 = '2'
```

An array is enclosed in square brackets, and must consist of strings or booleans
```ini
[section]
option = ['value']
```

A boolean must be either `true` or `false`, and unquoted.
```ini
option = false
```

An integer must be an unquoted numeric constant.
```ini
option = 42
```

## Sections

The following sections are allowed:
- constants
- binaries
- paths
- properties
- cmake
- project options
- built-in options

### constants

*Since 0.56.0*

String and list concatenation is supported using the `+` operator,
joining paths is supported using the `/` operator. Entries defined in
the `[constants]` section can be used in any other section (they are
always parsed first), entries in any other section can be used only
within that same section and only after it has been defined.

```ini
[constants]
toolchain = '/toolchain'
common_flags = ['--sysroot=' + toolchain / 'sysroot']

[properties]
c_args = common_flags + ['-DSOMETHING']
cpp_args = c_args + ['-DSOMETHING_ELSE']

[binaries]
c = toolchain / 'gcc'
```

This can be useful with cross file composition as well. A generic
cross file could be composed with a platform specific file where
constants are defined:

```ini
# aarch64.ini
[constants]
arch = 'aarch64-linux-gnu'
```

```ini
# cross.ini
[binaries]
c = arch + '-gcc'
cpp = arch + '-g++'
strip = arch + '-strip'
pkg-config = arch + '-pkg-config'
...
```

This can be used as `meson setup --cross-file aarch64.ini --cross-file
cross.ini builddir`.

Note that file composition happens before the parsing of values. The
example below results in `b` being `'HelloWorld'`:

```ini
# file1.ini:
[constants]
a = 'Foo'
b = a + 'World'
```

```ini
#file2.ini:
[constants]
a = 'Hello'
```

The example below results in an error when file1.ini is included
before file2.ini because `b` would be defined before `a`:

```ini
# file1.ini:
[constants]
b = a + 'World'
```

```ini
#file2.ini:
[constants]
a = 'Hello'
```

*Since 1.3.0* Some tokens are replaced in the machine file before parsing it:
- `@GLOBAL_SOURCE_ROOT@`: the absolute path to the project's source tree
- `@DIRNAME@`: the absolute path to the machine file's parent directory.

It can be used, for example, to have paths relative to the source directory, or
relative to toolchain's installation directory.
```ini
[binaries]
c = '@DIRNAME@/toolchain/gcc'
exe_wrapper = '@GLOBAL_SOURCE_ROOT@' / 'build-aux' / 'my-exe-wrapper.sh'
```

### Binaries

The binaries section contains a list of binaries. These can be used
internally by Meson, or by the `find_program` function.

These values must be either strings or an array of strings

Compilers and linkers are defined here using `<lang>` and `<lang>_ld`.
`<lang>_ld` is special because it is compiler specific. For compilers
like gcc and clang which are used to invoke the linker this is a value
to pass to their "choose the linker" argument (-fuse-ld= in this
case). For compilers like MSVC and Clang-Cl, this is the path to a
linker for Meson to invoke, such as `link.exe` or `lld-link.exe`.
Support for `ld` is *new in 0.53.0*

*changed in 0.53.1* the `ld` variable was replaced by `<lang>_ld`,
because it regressed a large number of projects. in 0.53.0 the `ld`
variable was used instead.

Native example:

```ini
c = '/usr/bin/clang'
c_ld = 'lld'
sed = 'C:\\program files\\gnu\\sed.exe'
llvm-config = '/usr/lib/llvm8/bin/llvm-config'
```

Cross example:

```ini
c = ['ccache', '/usr/bin/i586-mingw32msvc-gcc']
cpp = ['ccache', '/usr/bin/i586-mingw32msvc-g++']
c_ld = 'gold'
cpp_ld = 'gold'
ar = '/usr/i586-mingw32msvc/bin/ar'
strip = '/usr/i586-mingw32msvc/bin/strip'
pkg-config = '/usr/bin/i586-mingw32msvc-pkg-config'
```

An incomplete list of internally used programs that can be overridden
here is:

- cmake
- cups-config
- gnustep-config
- gpgme-config
- libgcrypt-config
- libwmf-config
- llvm-config
- pcap-config
- pkg-config
- sdl2-config
- wx-config (or wx-3.0-config or wx-config-gtk)

### Paths and Directories

*Deprecated in 0.56.0* use the built-in section instead.

As of 0.50.0 paths and directories such as libdir can be defined in
the native and cross files in a paths section. These should be
strings.

```ini
[paths]
libdir = 'mylibdir'
prefix = '/my prefix'
```

These values will only be loaded when not cross compiling. Any
arguments on the command line will override any options in the native
file. For example, passing `--libdir=otherlibdir` would result in a
prefix of `/my prefix` and a libdir of `otherlibdir`.

### Properties

*New in native files in 0.54.0*, always in cross files.

In addition to special data that may be specified in cross files, this
section may contain random key value pairs accessed using the
`meson.get_external_property()`, or `meson.get_cross_property()`.

*Changed in 0.56.0* putting `<lang>_args` and `<lang>_link_args` in
the properties section has been deprecated, and should be put in the
built-in options section.

#### Supported properties

This is a non exhaustive list of supported variables in the `[properties]`
section.

- `cmake_toolchain_file` specifies an absolute path to an already existing
  CMake toolchain file that will be loaded with `include()` as the last
  instruction of the automatically generated CMake toolchain file from Meson.
  (*new in 0.56.0*)
- `cmake_defaults` is a boolean that specifies whether Meson should automatically
  generate default toolchain variables from other sections (`binaries`,
  `host_machine`, etc.) in the machine file. Defaults are always overwritten
  by variables set in the `[cmake]` section. The default is `true`. (*new in 0.56.0*)
- `cmake_skip_compiler_test` is an enum that specifies when Meson should
  automatically generate toolchain variables to skip the CMake compiler
  sanity checks. This only has an effect if `cmake_defaults` is `true`.
  Supported values are `always`, `never`, `dep_only`. The default is `dep_only`.
  (*new in 0.56.0*)
- `cmake_use_exe_wrapper` is a boolean that controls whether to use the
  `exe_wrapper` specified in `[binaries]` to run generated executables in CMake
  subprojects. This setting has no effect if the `exe_wrapper` was not specified.
  The default value is `true`. (*new in 0.56.0*)
- `java_home` is an absolute path pointing to the root of a Java installation.
- `bindgen_clang_arguments` an array of extra arguments to pass to clang when
  calling bindgen

### CMake variables

*New in 0.56.0*

All variables set in the `[cmake]` section will be added to the
generate CMake toolchain file used for both CMake dependencies and
CMake subprojects. The type of each entry must be either a string or a
list of strings.

**Note:** All occurrences of `\` in the value of all keys will be replaced with
          a `/` since CMake has a lot of issues with correctly escaping `\` when
          dealing with variables (even in cases where a path in `CMAKE_C_COMPILER`
          is correctly escaped, CMake will still trip up internally for instance)

          A custom toolchain file should be used (via the `cmake_toolchain_file`
          property) if `\` support is required.

```ini
[cmake]

CMAKE_C_COMPILER    = '/usr/bin/gcc'
CMAKE_CXX_COMPILER  = 'C:\\user\\bin\\g++'
CMAKE_SOME_VARIABLE = ['some', 'value with spaces']
```

For instance, the `[cmake]` section from above will generate the
following code in the CMake toolchain file:

```cmake
set(CMAKE_C_COMPILER    "/usr/bin/gcc")
set(CMAKE_CXX_COMPILER  "C:/usr/bin/g++")
set(CMAKE_SOME_VARIABLE "some" "value with spaces")
```

### Project specific options

*New in 0.56.0*

Path options are not allowed, those must be set in the `[paths]`
section.

Being able to set project specific options in a cross or native file
can be done using the `[project options]` section of the specific file
(if doing a cross build the options from the native file will be
ignored)

For setting options in subprojects use the `[<subproject>:project
options]` section instead.

```ini
[project options]
build-tests = true

[zlib:project options]
build-tests = false
```

### Meson built-in options

*Before 0.56.0, `<lang>_args` and `<lang>_link_args` must be put in the `properties` section instead, else they will be ignored.*

Meson built-in options can be set the same way:

```ini
[built-in options]
c_std = 'c99'
```

You can set some Meson built-in options on a per-subproject basis,
such as `default_library` and `werror`. The order of precedence is:

1) Command line
2) Machine file
3) Build system definitions

```ini
[zlib:built-in options]
default_library = 'static'
werror = false
```

Options set on a per-subproject basis will inherit the option from the
parent if the parent has a setting but the subproject doesn't, even
when there is a default set Meson language.

```ini
[built-in options]
default_library = 'static'
```

will make subprojects use default_library as static.

Some options can be set on a per-machine basis (in other words, the
value of the build machine can be different than the host machine in a
cross compile). In these cases the values from both a cross file and a
native file are used.

An incomplete list of options is:
- pkg_config_path
- cmake_prefix_path

## Loading multiple machine files

Native files allow layering (cross files can be layered since Meson
0.52.0). More than one file can be loaded, with values from a previous
file being overridden by the next. The intention of this is not
overriding, but to allow composing files. This composition is done by
passing the command line argument multiple times:

```console
meson setup builddir/ --cross-file first.ini --cross-file second.ini --cross-file third.ini
```

In this case `first.ini` will be loaded, then `second.ini`, with
values from `second.ini` replacing `first.ini`, and so on.

For example, if there is a project using C and C++, python 3.4-3.7,
and LLVM 5-7, and it needs to build with clang 5, 6, and 7, and gcc
5.x, 6.x, and 7.x; expressing all of these configurations in
monolithic configurations would result in 81 different native files.
By layering them, it can be expressed by just 12 native files.
