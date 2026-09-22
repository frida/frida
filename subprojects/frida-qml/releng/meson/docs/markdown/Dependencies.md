---
short-description: Dependencies for external libraries and frameworks
...

# Dependencies

Very few applications are fully self-contained, but rather they use
external libraries and frameworks to do their work. Meson makes it
very easy to find and use external dependencies. Here is how one would
use the zlib compression library.

```meson
zdep = dependency('zlib', version : '>=1.2.8')
exe = executable('zlibprog', 'prog.c', dependencies : zdep)
```

First Meson is told to find the external library `zlib` and error out
if it is not found. The `version` keyword is optional and specifies a
version requirement for the dependency. Then an executable is built
using the specified dependency. Note how the user does not need to
manually handle compiler or linker flags or deal with any other
minutiae.

If you have multiple dependencies, pass them as an array:

```meson
executable('manydeps', 'file.c', dependencies : [dep1, dep2, dep3, dep4])
```

If the dependency is optional, you can tell Meson not to error out if
the dependency is not found and then do further configuration.

```meson
opt_dep = dependency('somedep', required : false)
if opt_dep.found()
  # Do something.
else
  # Do something else.
endif
```

You can pass the `opt_dep` variable to target construction functions
whether the actual dependency was found or not. Meson will ignore
non-found dependencies.

Meson also allows one to get variables that are defined in a
`pkg-config` file. This can be done by using the
[[dep.get_pkgconfig_variable]] function.

```meson
zdep_prefix = zdep.get_pkgconfig_variable('prefix')
```

These variables can also be redefined by passing the `define_variable`
parameter, which might be useful in certain situations:

```meson
zdep_prefix = zdep.get_pkgconfig_variable('libdir', define_variable: ['prefix', '/tmp'])
```

The dependency detector works with all libraries that provide a
`pkg-config` file. Unfortunately several packages don't provide
pkg-config files. Meson has autodetection support for some of these,
and they are described [later in this
page](#dependencies-with-custom-lookup-functionality).

# Arbitrary variables from dependencies that can be found multiple ways

*Note* new in 0.51.0
*new in 0.54.0, the `internal` keyword*

When you need to get an arbitrary variable from a dependency that can
be found multiple ways and you don't want to constrain the type, you
can use the generic `get_variable` method. This currently supports
cmake, pkg-config, and config-tool based variables.

```meson
foo_dep = dependency('foo')
var = foo_dep.get_variable(cmake : 'CMAKE_VAR', pkgconfig : 'pkg-config-var', configtool : 'get-var', default_value : 'default')
```

It accepts the keywords 'cmake', 'pkgconfig', 'pkgconfig_define',
'configtool', 'internal', and 'default_value'. 'pkgconfig_define'
works just like the 'define_variable' argument to
`get_pkgconfig_variable`. When this method is invoked the keyword
corresponding to the underlying type of the dependency will be used to
look for a variable. If that variable cannot be found or if the caller
does not provide an argument for the type of dependency, one of the
following will happen: If 'default_value' was provided that value will
be returned, if 'default_value' was not provided then an error will be
raised.

## Dependencies that provide resource files

Sometimes a dependency provides installable files which other projects then
need to use. For example, wayland-protocols XML files.

```meson
foo_dep = dependency('foo')
foo_datadir = foo_dep.get_variable('pkgdatadir')
custom_target(
    'foo-generated.c',
    input: foo_datadir / 'prototype.xml',
    output: 'foo-generated.c',
    command: [generator, '@INPUT@', '@OUTPUT@']
)
```

*Since 0.63.0* these actually work as expected, even when they come from a
(well-formed) internal dependency. This only works when treating the files to
be obtained as interchangeable with a system dependency -- e.g. only public
files may be used, and leaving the directory pointed to by the dependency is
not allowed.

# Declaring your own

You can declare your own dependency objects that can be used
interchangeably with dependency objects obtained from the system. The
syntax is straightforward:

```meson
my_inc = include_directories(...)
my_lib = static_library(...)
my_dep = declare_dependency(link_with : my_lib,
  include_directories : my_inc)
```

This declares a dependency that adds the given include directories and
static library to any target you use it in.

# Building dependencies as subprojects

Many platforms do not provide a system package manager. On these
systems dependencies must be compiled from source. Meson's subprojects
make it simple to use system dependencies when they are available and
to build dependencies manually when they are not.

To make this work, the dependency must have Meson build definitions
and it must declare its own dependency like this:

```meson
    foo_dep = declare_dependency(...)
```

Then any project that wants to use it can write out the following
declaration in their main `meson.build` file.

```meson
    foo_dep = dependency('foo', fallback : ['foo', 'foo_dep'])
```

What this declaration means is that first Meson tries to look up the
dependency from the system (such as by using pkg-config). If it is not
available, then it builds subproject named `foo` and from that
extracts a variable `foo_dep`. That means that the return value of
this function is either an external or an internal dependency object.
Since they can be used interchangeably, the rest of the build
definitions do not need to care which one it is. Meson will take care
of all the work behind the scenes to make this work.

# Dependency detection method

You can use the keyword `method` to let Meson know what method to use
when searching for the dependency. The default value is `auto`.
Additional methods are `pkg-config`, `config-tool`, `cmake`,
`builtin`, `system`, `sysconfig`, `qmake`, `extraframework` and `dub`.

```meson
cups_dep = dependency('cups', method : 'pkg-config')
```

For dependencies without [specific detection
logic](#dependencies-with-custom-lookup-functionality), the dependency method
order for `auto` is:

  1. `pkg-config`
  2. `cmake`
  3. `extraframework` (OSX only)

## System

Some dependencies provide no valid methods for discovery, or do so only in
some cases. Some examples of this are Zlib, which provides both pkg-config
and cmake, except when it is part of the base OS image (such as in FreeBSD
and macOS); OpenGL which has pkg-config on Unices from glvnd or mesa, but has
no pkg-config on macOS and Windows.

In these cases Meson provides convenience wrappers in the form of `system`
dependencies. Internally these dependencies do exactly what a user would do
in the build system DSL or with a script, likely calling
[[compiler.find_library]], setting `link_with` and `include_directories`. By
putting these in Meson upstream the barrier of using them is lowered, as
projects using Meson don't have to re-implement the logic.

## Builtin

Some dependencies provide no valid methods for discovery on some systems,
because they are provided internally by the language. One example of this is
intl, which is built into GNU or musl libc but otherwise comes as a `system`
dependency.

In these cases Meson provides convenience wrappers for the `system` dependency,
but first checks if the functionality is usable by default.

## CMake

Meson can use the CMake `find_package()` function to detect
dependencies with the builtin `Find<NAME>.cmake` modules and exported
project configurations (usually in `/usr/lib/cmake`). Meson is able to
use both the old-style `<NAME>_LIBRARIES` variables as well as
imported targets.

It is possible to manually specify a list of CMake targets that should
be used with the `modules` property. However, this step is optional
since Meson tries to automatically guess the correct target based on
the name of the dependency.

Depending on the dependency it may be necessary to explicitly specify
a CMake target with the `modules` property if Meson is unable to guess
it automatically.

```meson
    cmake_dep = dependency('ZLIB', method : 'cmake', modules : ['ZLIB::ZLIB'])
```

Support for adding additional `COMPONENTS` for the CMake
`find_package` lookup is provided with the `components` kwarg
(*introduced in 0.54.0*). All specified components will be passed
directly to `find_package(COMPONENTS)`.

Support for packages which require a specified version for CMake
`find_package` to succeed is provided with the `cmake_package_version`
kwarg (*introduced in 0.57.0*). The specified `cmake_package_version`
will be passed directly as the second parameter to `find_package`.

It is also possible to reuse existing `Find<name>.cmake` files with the
`cmake_module_path` property (*since 0.50.0*). Using this property is
equivalent to setting the `CMAKE_MODULE_PATH` variable in CMake. The
path(s) given to `cmake_module_path` should all be relative to the
project source directory. Absolute paths should only be used if the
CMake files are not stored in the project itself.

Additional CMake parameters can be specified with the `cmake_args`
property (*since 0.50.0*).

## Dub

Please understand that Meson is only able to find dependencies that
exist in the local Dub repository. You need to manually fetch and
build the target dependencies.

For `urld`.
```
dub fetch urld
dub build urld
```

Other thing you need to keep in mind is that both Meson and Dub need
to be using the same compiler. This can be achieved using Dub's
`-compiler` argument and/or manually setting the `DC` environment
variable when running Meson.
```
dub build urld --compiler=dmd
DC="dmd" meson setup builddir
```

## Config tool

[CUPS](#cups), [LLVM](#llvm), [pcap](#pcap), [WxWidgets](#wxwidgets),
[libwmf](#libwmf), [GCrypt](#libgcrypt), [GPGME](#gpgme), and GnuStep either do not provide pkg-config
modules or additionally can be detected via a config tool
(cups-config, llvm-config, libgcrypt-config, etc). Meson has native support for these
tools, and they can be found like other dependencies:

```meson
pcap_dep = dependency('pcap', version : '>=1.0')
cups_dep = dependency('cups', version : '>=1.4')
llvm_dep = dependency('llvm', version : '>=4.0')
libgcrypt_dep = dependency('libgcrypt', version: '>= 1.8')
gpgme_dep = dependency('gpgme', version: '>= 1.0')
```

*Since 0.55.0* Meson won't search $PATH any more for a config tool
binary when cross compiling if the config tool did not have an entry
in the cross file.

# Dependencies with custom lookup functionality

Some dependencies have specific detection logic.

Generic dependency names are case-sensitive<sup>[1](#footnote1)</sup>,
but these dependency names are matched case-insensitively. The
recommended style is to write them in all lower-case.

In some cases, more than one detection method exists, and the `method`
keyword may be used to select a detection method to use. The `auto`
method uses any checking mechanisms in whatever order Meson thinks is
best.

e.g. libwmf and CUPS provide both pkg-config and config-tool support.
You can force one or another via the `method` keyword:

```meson
cups_dep = dependency('cups', method : 'pkg-config')
wmf_dep = dependency('libwmf', method : 'config-tool')
```

## AppleFrameworks

Use the `modules` keyword to list frameworks required, e.g.

```meson
dep = dependency('appleframeworks', modules : 'foundation')
```

These dependencies can never be found for non-OSX hosts.

## Blocks

Enable support for Clang's blocks extension.

```meson
dep = dependency('blocks')
```

*(added 0.52.0)*

## Boost

Boost is not a single dependency but rather a group of different
libraries. To use Boost headers-only libraries, simply add Boost as a
dependency.

```meson
boost_dep = dependency('boost')
exe = executable('myprog', 'file.cc', dependencies : boost_dep)
```

To link against boost with Meson, simply list which libraries you
would like to use.

```meson
boost_dep = dependency('boost', modules : ['thread', 'utility'])
exe = executable('myprog', 'file.cc', dependencies : boost_dep)
```

You can call [[dependency]] multiple times with different modules and
use those to link against your targets.

If your boost headers or libraries are in non-standard locations you
can set the `BOOST_ROOT`, or the `BOOST_INCLUDEDIR` and
`BOOST_LIBRARYDIR` environment variables. *(added in 0.56.0)* You can
also set these parameters as `boost_root`, `boost_includedir`, and
`boost_librarydir` in your native or cross machine file. Note that
machine file variables are preferred to environment variables, and
that specifying any of these disables system-wide search for boost.

You can set the argument `threading` to `single` to use boost
libraries that have been compiled for single-threaded use instead.

## CUDA

*(added 0.53.0)*

Enables compiling and linking against the CUDA Toolkit. The `version`
and `modules` keywords may be passed to request the use of a specific
CUDA Toolkit version and/or additional CUDA libraries, correspondingly:

```meson
dep = dependency('cuda', version : '>=10', modules : ['cublas'])
```

Note that explicitly adding this dependency is only necessary if you are
using CUDA Toolkit from a C/C++ file or project, or if you are utilizing
additional toolkit libraries that need to be explicitly linked to. If the
CUDA Toolkit cannot be found in the default paths on your system, you can
set the path using `CUDA_PATH` explicitly.

## CUPS

`method` may be `auto`, `config-tool`, `pkg-config`, `cmake` or `extraframework`.

## Curses

*(Since 0.54.0)*

Curses (and ncurses) are a cross platform pain in the butt. Meson
wraps up these dependencies in the `curses` dependency. This covers
both `ncurses` (preferred) and other curses implementations.

`method` may be `auto`, `pkg-config`, `config-tool`, or `system`.

*New in 0.56.0* The `config-tool` and `system` methods.

To define some of the preprocessor symbols mentioned in the
[curses autoconf documentation](http://git.savannah.gnu.org/gitweb/?p=autoconf-archive.git;a=blob_plain;f=m4/ax_with_curses.m4):

```meson
conf = configuration_data()
check_headers = [
  ['ncursesw/menu.h', 'HAVE_NCURSESW_MENU_H'],
  ['ncurses/menu.h', 'HAVE_NCURSES_MENU_H'],
  ['menu.h', 'HAVE_MENU_H'],
  ['ncursesw/curses.h', 'HAVE_NCURSESW_CURSES_H'],
  ['ncursesw.h', 'HAVE_NCURSESW_H'],
  ['ncurses/curses.h', 'HAVE_NCURSES_CURSES_H'],
  ['ncurses.h', 'HAVE_NCURSES_H'],
  ['curses.h', 'HAVE_CURSES_H'],
]

foreach h : check_headers
  if compiler.has_header(h.get(0))
    conf.set(h.get(1), 1)
  endif
endforeach
```

## dl (libdl)

*(added 0.62.0)*

Provides access to the dynamic link interface (functions: dlopen,
dlclose, dlsym and others). On systems where this is not built
into libc (mostly glibc < 2.34), tries to find an external library
providing them instead.

`method` may be `auto`, `builtin` or `system`.

## Fortran Coarrays

*(added 0.50.0)*

 Coarrays are a Fortran language intrinsic feature, enabled by
`dependency('coarray')`.

GCC will use OpenCoarrays if present to implement coarrays, while Intel and NAG
use internal coarray support.

## GPGME

*(added 0.51.0)*

`method` may be `auto`, `config-tool` or `pkg-config`.

## GL

This finds the OpenGL library in a way appropriate to the platform.

`method` may be `auto`, `pkg-config` or `system`.

## GTest and GMock

GTest and GMock come as sources that must be compiled as part of your
project. With Meson you don't have to care about the details, just
pass `gtest` or `gmock` to `dependency` and it will do everything for
you. If you want to use GMock, it is recommended to use GTest as well,
as getting it to work standalone is tricky.

You can set the `main` keyword argument to `true` to use the `main()`
function provided by GTest:

```meson
gtest_dep = dependency('gtest', main : true, required : false)
e = executable('testprog', 'test.cc', dependencies : gtest_dep)
test('gtest test', e)
```

## HDF5

*(added 0.50.0)*

HDF5 is supported for C, C++ and Fortran. Because dependencies are
language-specific, you must specify the requested language using the
`language` keyword argument, i.e.,
 * `dependency('hdf5', language: 'c')` for the C HDF5 headers and libraries
 * `dependency('hdf5', language: 'cpp')` for the C++ HDF5 headers and libraries
 * `dependency('hdf5', language: 'fortran')` for the Fortran HDF5 headers and libraries

The standard low-level HDF5 function and the `HL` high-level HDF5
functions are linked for each language.

`method` may be `auto`, `config-tool` or `pkg-config`.

*New in 0.56.0* the `config-tool` method.
*New in 0.56.0* the dependencies now return proper dependency types
 and `get_variable` and similar methods should work as expected.

## iconv

*(added 0.60.0)*

Provides access to the `iconv` family of C functions. On systems where this is
not built into libc, tries to find an external library providing them instead.

`method` may be `auto`, `builtin` or `system`.

## intl

*(added 0.59.0)*

Provides access to the `*gettext` family of C functions. On systems where this
is not built into libc, tries to find an external library providing them
instead.

`method` may be `auto`, `builtin` or `system`.

## JDK

*(added 0.58.0)*
*(deprecated 0.62.0)*

Deprecated name for JNI. `dependency('jdk')` instead of `dependency('jni')`.

## JNI

*(added 0.62.0)*

`modules` is an optional list of strings containing any of `jvm` and `awt`.

Provides access to compiling with the Java Native Interface (JNI). The lookup
will first check if `JAVA_HOME` is set in the environment, and if not will use
the resolved path of `javac`. Systems will usually link your preferred JDK to
well known paths like `/usr/bin/javac` on Linux for instance. Using the path
from `JAVA_HOME` or the resolved `javac`, this dependency will place the JDK
installation's `include` directory and its platform-dependent subdirectory on
the compiler's include path. If `modules` is non-empty, then the proper linker
arguments will also be added.

```meson
dep = dependency('jni', version: '>= 1.8.0', modules: ['jvm'])
```

**Note**: Due to usage of a resolved path, upgrading the JDK may cause the
various paths to not be found. In that case, please reconfigure the build
directory. One workaround is to explicitly set `JAVA_HOME` instead of relying on
the fallback `javac` resolved path behavior.

**Note**: Include paths might be broken on platforms other than `linux`,
`windows`, `darwin`, and `sunos`. Please submit a PR or open an issue in this
case.

**Note**: Use of the `modules` argument on a JDK `<= 1.8` may be broken if your
system is anything other than `x86_64`. Please submit a PR or open an issue in
this case.

## libgcrypt

*(added 0.49.0)*

`method` may be `auto`, `config-tool` or `pkg-config`.

## libwmf

*(added 0.44.0)*

`method` may be `auto`, `config-tool` or `pkg-config`.

## LLVM

Meson has native support for LLVM going back to version LLVM version
3.5. It supports a few additional features compared to other
config-tool based dependencies.

As of 0.44.0 Meson supports the `static` keyword argument for LLVM.
Before this LLVM >= 3.9 would always dynamically link, while older
versions would statically link, due to a quirk in `llvm-config`.

`method` may be `auto`, `config-tool`, or `cmake`.

### Modules, a.k.a. Components

Meson wraps LLVM's concept of components in its own modules concept.
When you need specific components you add them as modules as Meson
will do the right thing:

```meson
llvm_dep = dependency('llvm', version : '>= 4.0', modules : ['amdgpu'])
```

As of 0.44.0 it can also take optional modules (these will affect the arguments
generated for a static link):

```meson
llvm_dep = dependency(
  'llvm', version : '>= 4.0', modules : ['amdgpu'], optional_modules : ['inteljitevents'],
)
```

### Using LLVM tools

When using LLVM as library but also needing its tools, it is often
beneficial to use the same version. This can partially be achieved
with the `version` argument of `find_program()`. However,
distributions tend to package different LLVM versions in rather
different ways. Therefore, it is often better to use the llvm
dependency directly to retrieve the tools:

```meson
llvm_dep = dependency('llvm', version : ['>= 8', '< 9'])
llvm_link = find_program(llvm_dep.get_variable(configtool: 'bindir') / 'llvm-link')
```

## MPI

*(added 0.42.0)*

MPI is supported for C, C++ and Fortran. Because dependencies are
language-specific, you must specify the requested language using the
`language` keyword argument, i.e.,
 * `dependency('mpi', language: 'c')` for the C MPI headers and libraries
 * `dependency('mpi', language: 'cpp')` for the C++ MPI headers and libraries
 * `dependency('mpi', language: 'fortran')` for the Fortran MPI headers and libraries

Meson prefers pkg-config for MPI, but if your MPI implementation does
not provide them, it will search for the standard wrapper executables,
`mpic`, `mpicxx`, `mpic++`, `mpifort`, `mpif90`, `mpif77`. If these
are not in your path, they can be specified by setting the standard
environment variables `MPICC`, `MPICXX`, `MPIFC`, `MPIF90`, or
`MPIF77`, during configuration. It will also try to use the Microsoft
implementation on windows via the `system` method.

`method` may be `auto`, `config-tool`, `pkg-config` or `system`.

*New in 0.54.0* The `config-tool` and `system` method values. Previous
versions would always try `pkg-config`, then `config-tool`, then `system`.

## NetCDF

*(added 0.50.0)*

NetCDF is supported for C, C++ and Fortran. Because NetCDF dependencies are
language-specific, you must specify the requested language using the
`language` keyword argument, i.e.,
 * `dependency('netcdf', language: 'c')` for the C NetCDF headers and libraries
 * `dependency('netcdf', language: 'cpp')` for the C++ NetCDF headers and libraries
 * `dependency('netcdf', language: 'fortran')` for the Fortran NetCDF headers and libraries

Meson uses pkg-config to find NetCDF.

## OpenMP

*(added 0.46.0)*

This dependency selects the appropriate compiler flags and/or libraries to use
for OpenMP support.

The `language` keyword may used.

## OpenSSL

*(added 0.62.0)*

`method` may be `auto`, `pkg-config`, `system` or `cmake`.

## NumPy

*(added 1.4.0)*

`method` may be `auto`, `pkg-config`, or `config-tool`.
`dependency('numpy')` supports regular use of the NumPy C API.
Use of `numpy.f2py` for binding Fortran code isn't yet supported.

## pcap

*(added 0.42.0)*

`method` may be `auto`, `config-tool` or `pkg-config`.

## Pybind11

*(added 1.1.0)*

`method` may be `auto`, `pkg-config`, `config-tool`, or `cmake`.

## Python3

Python3 is handled specially by Meson:
1. Meson tries to use `pkg-config`.
2. If `pkg-config` fails Meson uses a fallback:
    - On Windows the fallback is the current `python3` interpreter.
    - On OSX the fallback is a framework dependency from `/Library/Frameworks`.

Note that `python3` found by this dependency might differ from the one
used in `python3` module because modules uses the current interpreter,
but dependency tries `pkg-config` first.

`method` may be `auto`, `extraframework`, `pkg-config` or `sysconfig`

## Qt

Meson has native Qt support. Its usage is best demonstrated with an
example.

```meson
qt5_mod = import('qt5')
qt5widgets = dependency('qt5', modules : 'Widgets')

processed = qt5_mod.preprocess(
  moc_headers : 'mainWindow.h',   # Only headers that need moc should be put here
  moc_sources : 'helperFile.cpp', # must have #include"moc_helperFile.cpp"
  ui_files    : 'mainWindow.ui',
  qresources  : 'resources.qrc',
)

q5exe = executable('qt5test',
  sources     : ['main.cpp',
                 'mainWindow.cpp',
                 processed],
  dependencies: qt5widgets)
```

Here we have an UI file created with Qt Designer and one source and
header file each that require preprocessing with the `moc` tool. We
also define a resource file to be compiled with `rcc`. We just have to
tell Meson which files are which and it will take care of invoking all
the necessary tools in the correct order, which is done with the
`preprocess` method of the `qt5` module. Its output is simply put in
the list of sources for the target. The `modules` keyword of
`dependency` works just like it does with Boost. It tells which
subparts of Qt the program uses.

You can set the `main` keyword argument to `true` to use the
`WinMain()` function provided by qtmain static library (this argument
does nothing on platforms other than Windows).

Setting the optional `private_headers` keyword to true adds the
private header include path of the given module(s) to the compiler
flags. (since v0.47.0)

**Note** using private headers in your project is a bad idea, do so at
your own risk.

`method` may be `auto`, `pkg-config` or `qmake`.

## SDL2

SDL2 can be located using `pkg-confg`, the `sdl2-config` config tool,
as an OSX framework, or `cmake`.

`method` may be `auto`, `config-tool`, `extraframework`,
`pkg-config` or `cmake`.

## Shaderc

*(added 0.51.0)*

Meson will first attempt to find shaderc using `pkg-config`. Upstream
currently ships three different `pkg-config` files and by default will
check them in this order: `shaderc`, `shaderc_combined`, and
`shaderc_static`. If the `static` keyword argument is `true`, then
Meson instead checks in this order: `shaderc_combined`, `shaderc_static`,
and `shaderc`.

If no `pkg-config` file is found, then Meson will try to detect the
library manually. In this case, it will try to link against either
`-lshaderc_shared` or `-lshaderc_combined`, preferring the latter
if the static keyword argument is true. Note that it is not possible
to obtain the shaderc version using this method.

`method` may be `auto`, `pkg-config` or `system`.

## Threads

This dependency selects the appropriate compiler flags and/or
libraries to use for thread support.

See [threads](Threads.md).

## Valgrind

Meson will find valgrind using `pkg-config`, but only uses the
compilation flags and avoids trying to link with its non-PIC static
libs.

## Vulkan

*(added 0.42.0)*

Vulkan can be located using `pkg-config`, or the `VULKAN_SDK`
environment variable.

`method` may be `auto`, `pkg-config` or `system`.

## WxWidgets

Similar to [Boost](#boost), WxWidgets is not a single library but rather
a collection of modules. WxWidgets is supported via `wx-config`.
Meson substitutes `modules` to `wx-config` invocation, it generates
- `compile_args` using `wx-config --cxxflags $modules...`
- `link_args` using `wx-config --libs $modules...`

### Example

```meson
wx_dep = dependency(
  'wxwidgets', version : '>=3.0.0', modules : ['std', 'stc'],
)
```

```shell
# compile_args:
$ wx-config --cxxflags std stc

# link_args:
$ wx-config --libs std stc
```

## Zlib

Zlib ships with pkg-config and cmake support, but on some operating
systems (windows, macOs, FreeBSD, dragonflybsd, android), it is provided as
part of the base operating system without pkg-config support. The new
System finder can be used on these OSes to link with the bundled
version.

`method` may be `auto`, `pkg-config`, `cmake`, or `system`.

*New in 0.54.0* the `system` method.

<hr>
<a name="footnote1">1</a>: They may appear to be case-insensitive, if the
    underlying file system happens to be case-insensitive.
