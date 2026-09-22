---
title: Release 0.64.0
short-description: Release notes for 0.64.0
...

# New features

## Add `optimization` `plain` option

The `optimization` built-in option now accepts `plain` value,
which will not set any optimization flags. This is now the default
value of the flag for `buildtype=plain`, which is useful for distros,
that set the optimization and hardening flags by other means.

If you are using the value of `get_option('optimization')` in your
Meson scripts, make sure you are not making assumptions about it,
such as that the value can be passed to a compiler in `-O` flag.

## New languages: `nasm` and `masm`

When the `nasm` language is added to the project, `.asm` files are
automatically compiled with NASM. This is only supported for x86 and x86_64 CPU
family. `yasm` is used as fallback if `nasm` command is not found.

When the `masm` language is added to the project, `.masm` files are
automatically compiled with Microsoft's Macro Assembler. This is only supported
for x86, x86_64, ARM and AARCH64 CPU families.

Note that GNU Assembly files usually have `.s` or `.S` extension and were already
built using C compiler such as GCC or CLANG.

```meson
project('test', 'nasm')

exe = executable('hello', 'hello.asm')
test('hello', exe)
```

## Pager and colors for `meson configure` output

The output of `meson configure`, printing all options, is now more readable by
automatically using a pager (`less` by default) and colors. The pager used can
be controlled by setting `PAGER` environment variable, or `--no-pager` command
line option.

## various `install_*` functions no longer handle the sticky bit

It is not possible to portably grant the sticky bit to a file, and where
possible, it doesn't do anything. It is not expected that any users are using
this functionality.

Variously:
- on Linux, it has no meaningful effect
- on Solaris, attempting to set the permission bit is silently ignored by the OS
- on FreeBSD, attempting to set the permission bit is an error

Attempting to set this permission bit in the `install_mode:` kwarg to any
function other than [[install_emptydir]] will now result in a warning, and the
permission bit being ignored.

## `fs.copyfile` to replace `configure_file(copy : true)`

A new method has been added to the `fs` module, `copyfile`. This method replaces
`configure_file(copy : true)`, but only copies files. Unlike `configure_file()`
it runs at build time, and the output name is optional defaulting to the
filename without paths of the input if unset:

```meson
fs.copyfile('src/file.txt')
```
Will create a file in the current build directory called `file.txt`


```meson
fs.copyfile('file.txt', 'outfile.txt')
```
Will create a copy renamed to `outfile.txt`

## Added `update_mime_database` to `gnome.post_install()`

Applications that install a `.xml` file containing a `mime-type` need to update
the cache upon installation. Most applications do that using a custom script,
but it can now be done by Meson directly.

## Added preserve_path arg to install_data

The [[install_data]] function now has an optional argument `preserve_path`
that allows installing multi-directory data file structures that live
alongside source code with a single command.

This is also available in the specialized `py_installation.install_sources`
method.

## BSD support for the `jni` dependency

This system dependency now supports all BSD systems that Meson currently
supports, including FreeBSD, NetBSD, OpenBSD, and DragonflyBSD.

## Credentials from `~/.netrc` for `https` URLs

When a subproject is downloaded using an `https://` URL, credentials from
`~/.netrc` are now used. This avoids hardcoding login and password in plain
text in the URL itself.

## Basic support for oneAPI compilers on Linux and Windows

To use on Linux:

```
source /opt/intel/oneapi/setvars.sh
CC=icx CXX=icpx FC=ifx meson setup builddir
```

## New method to preprocess source files

Compiler object has a new `preprocess()` method. It is supported by all C/C++
compilers. It preprocess sources without compiling them.

The preprocessor will receive the same arguments (include directories, defines,
etc) as with normal compilation. That includes for example args added with
`add_project_arguments()`, or on the command line with `-Dc_args=-DFOO`.

```meson
cc = meson.get_compiler('c')
pp_files = cc.preprocess('foo.c', 'bar.c', output: '@PLAINNAME@')
exe = executable('app', pp_files)
```

## python.find_installation() now accepts pure argument

The default value of `pure:` for `py.install_sources()` and
`py.get_install_dir()` can now be changed by explicitly passing a `pure:` kwarg
to `find_installation()`.

This can be used to ensure that multiple `install_sources()` invocations do not
forget to specify the kwarg each time.

## Generates rust-project.json when there are Rust targets

This is a format similar to compile_commands.json, but specifically used by the
official rust LSP, rust-analyzer. It is generated automatically if there are
Rust targets, and is placed in the build directory.

## `summary()` accepts disablers

Disabler options can be passed to `summary()` as the value to be printed.

## Option to allow meson test to fail fast after the first failing testcase

`meson test --maxfail=1` will now cause all pending or in-progress tests to be
canceled or interrupted after 1 test is marked as failing. This can be used for
example to quit a CI run and avoid burning additional time as soon as it is
known that the overall return status will be failing.

## Incremental ThinLTO with `b_thinlto_cache`

[Incremental ThinLTO](https://clang.llvm.org/docs/ThinLTO.html#incremental) can now be enabled by passing
`-Db_thinlto_cache=true` during setup. The use of caching speeds up incremental builds significantly while retaining all
the runtime performance benefits of ThinLTO.

The cache location defaults to a Meson-managed directory inside the build folder, but can be customized with
`b_thinlto_cache_dir`.

## Update all wraps from WrapDB with `meson wrap update` command

The command `meson wrap update`, with no extra argument, will now update all wraps
that comes from WrapDB to the latest version. The extra `--force` argument will
also replace wraps that do not come from WrapDB if one is available.

The command `meson subprojects update` will not download new wrap files from
WrapDB any more.

## Added `include_core_only` arg to wayland.scan_xml.

The `scan_xml` function from the wayland module now has an optional bool
argument `include_core_only`, so that headers generated by wayland-scanner now
only include `wayland-client-core.h` instead of `wayland-client.h`.

## Automatic fallback using WrapDB

A new command has been added: `meson wrap update-db`. It downloads the list of
wraps available in [WrapDB](https://wrapdb.mesonbuild.com) and stores it locally in
`subprojects/wrapdb.json`. When that file exists and a dependency is not found
on the system but is available in WrapDB, Meson will automatically download it.

