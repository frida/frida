# How do I do X in Meson?

This page lists code snippets for common tasks. These are written
mostly using the C compiler, but the same approach should work on
almost all other compilers.

## Set compiler

When first running Meson, set it in an environment variable.

```console
$ CC=mycc meson <options>
```

Note that environment variables like `CC` only refer to the host
platform in cross builds.  That is, `CC` refers to the compiler used to
compile programs that run on the machine we will eventually install the
project on. The compiler used to build things that run on the machine we
do the building can be specified with `CC_FOR_BUILD`. You can use it in
cross builds.

Note that environment variables are never the idiomatic way to do
anything with Meson, however. It is better to use the native and cross
files. And the tools for the host platform in cross builds can only be
specified with a cross file.

There is a table of all environment variables supported
[Here](Reference-tables.md#compiler-and-linker-selection-variables)


## Set linker

*New in 0.53.0*

Like the compiler, the linker is selected via the `<compiler
variable>_LD` environment variable, or through the `<compiler
entry>_ld` entry in a native or cross file. You must be aware of
whether you're using a compiler that invokes the linker itself (most
compilers including GCC and Clang) or a linker that is invoked
directly (when using MSVC or compilers that act like it, including
Clang-Cl). With the former `c_ld` or `CC_LD` should be the value to
pass to the compiler's special argument (such as `-fuse-ld` with clang
and gcc), with the latter it should be an executable, such as
`lld-link.exe`.

*NOTE* In Meson 0.53.0 the `ld` entry in the cross/native file and the
`LD` environment variable were used, this resulted in a large number
of regressions and was changed in 0.53.1 to `<lang>_ld` and `<comp
variable>_LD`.

```console
$ CC=clang CC_LD=lld meson <options>
```

or

```console
$ CC=clang-cl CC_LD=link meson <options>
```

or in a cross or native file:

```ini
[binaries]
c = 'clang'
c_ld = 'lld'
```

There is a table of all environment variables supported
[Here](Reference-tables.md#compiler-and-linker-selection-variables)


## Set default C/C++ language version

```meson
project('myproj', 'c', 'cpp',
        default_options : ['c_std=c11', 'cpp_std=c++11'])
```

The language version can also be set on a per-target basis.

```meson
executable(..., override_options : ['c_std=c11'])
```

## Enable threads

Lots of people seem to do this manually with `cc.find_library('pthread')`
or something similar. Do not do that. It is not portable. Instead do
this.

```meson
thread_dep = dependency('threads')
executable(..., dependencies : thread_dep)
```

## Set extra compiler and linker flags from the outside (when e.g. building distro packages)

The behavior is the same as with other build systems, with environment
variables during first invocation. Do not use these when you need to
rebuild the source

```console
$ CFLAGS=-fsomething LDFLAGS=-Wl,--linker-flag meson <options>
```

## Use an argument only with a specific compiler

First check which arguments to use.

```meson
if meson.get_compiler('c').get_id() == 'clang'
  extra_args = ['-fclang-flag']
else
  extra_args = []
endif
```

Then use it in a target.

```meson
executable(..., c_args : extra_args)
```

If you want to use the arguments on all targets, then do this.

```meson
if meson.get_compiler('c').get_id() == 'clang'
  add_global_arguments('-fclang-flag', language : 'c')
endif
```

## Set a command's output to configuration

```meson
txt = run_command('script', 'argument', check: true).stdout().strip()
cdata = configuration_data()
cdata.set('SOMETHING', txt)
configure_file(...)
```

## Generate configuration data from files

The [fs module](#Fs-modules) offers the `read` function which enables adding
the contents of arbitrary files to configuration data (among other uses):

```meson
fs = import('fs')
cdata = configuration_data()
copyright = fs.read('LICENSE')
cdata.set('COPYRIGHT', copyright)
if build_machine.system() == 'linux'
    os_release = fs.read('/etc/os-release')
    cdata.set('LINUX_BUILDER', os_release)
endif
configure_file(...)
```

## Generate a runnable script with `configure_file`

`configure_file` preserves metadata so if your template file has
execute permissions, the generated file will have them too.

## Producing a coverage report

First initialize the build directory with this command.

```console
$ meson setup <other flags> -Db_coverage=true
```

Then issue the following commands.

```console
$ meson compile
$ meson test
$ ninja coverage-html (or coverage-xml)
```

The coverage report can be found in the meson-logs subdirectory.

*New in 0.55.0* llvm-cov support for use with clang

## Add some optimization to debug builds

By default the debug build does not use any optimizations. This is the
desired approach most of the time. However some projects benefit from
having some minor optimizations enabled. GCC even has a specific
compiler flag `-Og` for this. To enable its use, just issue the
following command.

```console
$ meson configure -Dc_args=-Og
```

This causes all subsequent builds to use this command line argument.

## Use address sanitizer

Clang and gcc come with a selection of analysis tools such as the [address
sanitizer](https://clang.llvm.org/docs/AddressSanitizer.html). Meson
has native support for these with the `b_sanitize` option.

```console
$ meson setup <other options> -Db_sanitize=address
```

After this you just compile your code and run the test suite. Address
sanitizer will abort executables which have bugs so they show up as
test failures.

## Use Clang static analyzer

Install scan-build program, then do this:

```console
$ meson setup builddir
$ ninja -C builddir scan-build
```

You can use the `SCANBUILD` environment variable to choose the
scan-build executable.

```console
$ SCANBUILD=<your exe> ninja -C builddir scan-build
```

You can use it for passing arguments to scan-build program by
creating a script, for example:

```sh
#!/bin/sh
scan-build -v --status-bugs "$@"
```

And then pass it through the variable (remember to use absolute path):

```console
$ SCANBUILD=$(pwd)/my-scan-build.sh ninja -C builddir scan-build
```

## Use profile guided optimization

Using profile guided optimization with GCC is a two phase
operation. First we set up the project with profile measurements
enabled and compile it.

```console
$ meson setup <Meson options, such as --buildtype=debugoptimized> -Db_pgo=generate
$ meson compile -C builddir
```

Then we need to run the program with some representative input. This
step depends on your project.

Once that is done we change the compiler flags to use the generated
information and rebuild.

```console
$ meson configure -Db_pgo=use
$ meson compile
```

After these steps the resulting binary is fully optimized.

## Add math library (`-lm`) portably

Some platforms (e.g. Linux) have a standalone math library. Other
platforms (pretty much everyone else) do not. How to specify that `m`
is used only when needed?

```meson
cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)
executable(..., dependencies : m_dep)
```

## Install an executable to `libexecdir`

```meson
executable(..., install : true, install_dir : get_option('libexecdir'))
```

## Use existing `Find<name>.cmake` files

Meson can use the CMake `find_package()` ecosystem if CMake is
installed. To find a dependency with custom `Find<name>.cmake`, set
the `cmake_module_path` property to the path in your project where the
CMake scripts are stored.

Example for a `FindCmakeOnlyDep.cmake` in a `cmake` subdirectory:

```meson
cm_dep = dependency('CmakeOnlyDep', cmake_module_path : 'cmake')
```

The `cmake_module_path` property is only needed for custom CMake scripts. System
wide CMake scripts are found automatically.

More information can be found [here](Dependencies.md#cmake)

## Get a default not-found dependency?

```meson
null_dep = dependency('', required : false)
```

This can be used in cases where you want a default value, but might override it
later.

```meson
# Not needed on Windows!
my_dep = dependency('', required : false)
if host_machine.system() in ['freebsd', 'netbsd', 'openbsd', 'dragonfly']
  my_dep = dependency('some dep', required : false)
elif host_machine.system() == 'linux'
  my_dep = dependency('some other dep', required : false)
endif

executable(
  'myexe',
  my_sources,
  deps : [my_dep]
)
```
