---
title: Release 0.42
short-description: Release notes for 0.42
...

# New features

## Distribution tarballs from Mercurial repositories

Creating distribution tarballs can now be made out of projects based
on Mercurial. As before, this remains possible only with the Ninja
backend.

## Keyword argument verification

Meson will now check the keyword arguments used when calling any
function and print a warning if any of the keyword arguments is not
known. In the future this will become a hard error.

## Add support for Genie to Vala compiler

The Vala compiler has an alternative syntax, Genie, that uses the
`.gs` file extension. Meson now recognises and uses Genie files.

## Pkgconfig support for additional cflags

The Pkgconfig module object can add arbitrary extra cflags to the Cflags
value in the .pc file, using the "extra_cflags" keyword:
```meson
pkg.generate(libraries : libs,
             subdirs : h,
             version : '1.0',
             name : 'libsimple',
             filebase : 'simple',
             description : 'A simple demo library.',
             extra_cflags : '-Dfoo' )
```

## Base options accessible via get_option()

Base options are now accessible via the get_option() function.
```meson
uses_lto = get_option('b_lto')
```

## Allow crate type configuration for Rust compiler

Rust targets now take an optional `rust_crate_type` keyword, allowing
you to set the crate type of the resulting artifact. Valid crate types
are `dylib` or `cdylib` for shared libraries, and `rlib` or
`staticlib` for static libraries. For more, see Rust's [linkage
reference][rust-linkage].

[rust-linkage]: https://doc.rust-lang.org/reference/linkage.html

## Simultaneous use of Address- and Undefined Behavior Sanitizers

Both the address- and undefined behavior sanitizers can now be used
simultaneously by passing `-Db_sanitize=address,undefined` to Meson.

## Unstable SIMD module

A new experimental module to compile code with many different SIMD
instruction sets and selecting the best one at runtime. This module
is unstable, meaning its API is subject to change in later releases.
It might also be removed altogether.


## Import libraries for executables on Windows

The new keyword `implib` to `executable()` allows generation of an import
library for the executable.

## Added build_rpath keyword argument

You can specify `build_rpath : '/foo/bar'` in build targets and the
given path will get added to the target's rpath in the build tree. It
is removed during the install step.

Meson will print a warning when the user tries to add an rpath linker
flag manually, e.g. via `link_args` to a target. This is not
recommended because having multiple rpath causes them to stomp on each
other. This warning will become a hard error in some future release.

## Vulkan dependency module

Vulkan can now be used as native dependency. The dependency module
will detect the VULKAN_SDK environment variable or otherwise try to
receive the vulkan library and header via pkgconfig or from the
system.

## Limiting the maximum number of linker processes

With the Ninja backend it is now possible to limit the maximum number of
concurrent linker processes. This is usually only needed for projects
that have many large link steps that cause the system to run out of
memory if they are run in parallel. This limit can be set with the
new `backend_max_links` option.

## Disable implicit include directories

By default Meson adds the current source and build directories to the
header search path. On some rare occasions this is not desired. Setting
the `implicit_include_directories` keyword argument to `false` these
directories are not used.

## Support for MPI dependency

MPI is now supported as a dependency. Because dependencies are
language-specific, you must specify the requested language with the
`language` keyword, i.e., `dependency('mpi', language='c')` will
request the C MPI headers and libraries. See [the MPI
dependency](Dependencies.md#mpi) for more information.

## Allow excluding files or directories from `install_subdir`

The [[install_subdir]] command
accepts the new `exclude_files` and `exclude_directories` keyword
arguments that allow specified files or directories to be excluded
from the installed subdirectory.

## Make all Meson functionality invocable via the main executable

Previously Meson had multiple executables such as `mesonintrospect`
and `mesontest`. They are now invocable via the main Meson executable
like this:

    meson configure <arguments> # equivalent to mesonconf <options>
    meson test <arguments> # equivalent to mesontest <arguments>

The old commands are still available but they are deprecated
and will be removed in some future release.

## Pcap dependency detector

Meson will automatically obtain dependency information for pcap
using the `pcap-config` tool. It is used like any other dependency:

```meson
pcap_dep = dependency('pcap', version : '>=1.0')
```

## GNOME module mkenums_simple() addition

Most libraries and applications use the same standard templates for
glib-mkenums. There is now a new `mkenums_simple()` convenience method
that passes those default templates to glib-mkenums and allows some tweaks
such as optional function decorators or leading underscores.
