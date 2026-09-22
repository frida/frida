---
title: Release 0.43
short-description: Release notes for 0.43
...

## Portability improvements to Boost Dependency

The Boost dependency has been improved to better detect the various
ways to install boost on multiple platforms. At the same time the
`modules` semantics for the dependency has been changed. Previously it
was allowed to specify header directories as `modules` but it wasn't
required. Now, modules are only used to specify libraries that require
linking.

This is a breaking change and the fix is to remove all modules that aren't
found.

## Generator learned capture

Generators can now be configured to capture the standard output. See
`test cases/common/98 gen extra/meson.build` for an example.

## Can index CustomTarget objects

The `CustomTarget` object can now be indexed like an array. The
resulting object can be used as a source file for other Targets, this
will create a dependency on the original `CustomTarget`, but will only
insert the generated file corresponding to the index value of the
`CustomTarget`'s `output` keyword.

```meson
c = custom_target(
  ...
  output : ['out.h', 'out.c'],
)
lib1 = static_library(
  'lib1',
  [lib1_sources, c[0]],
  ...
)
exec = executable(
  'executable',
  c[1],
  link_with : lib1,
)
```

## Can override executables in the cross file

The cross file can now be used for overriding the result of
`find_program`. As an example if you want to find the `objdump`
command and have the following definition in your cross file:

```ini
[binaries]
...
objdump = '/usr/bin/arm-linux-gnueabihf-objdump-6'
```

Then issuing the command `find_program('objdump')` will return the
version specified in the cross file. If you need the build machine's
objdump, you can specify the `native` keyword like this:

```meson
native_objdump = find_program('objdump', native : true)
```

## Easier handling of supported compiler arguments

A common pattern for handling multiple desired compiler arguments, was
to test their presence and add them to an array one-by-one, e.g.:

```meson
warning_flags_maybe = [
  '-Wsomething',
  '-Wanother-thing',
  '-Wno-the-other-thing',
]
warning_flags = []
foreach flag : warning_flags_maybe
  if cc.has_argument(flag)
    warning_flags += flag
  endif
endforeach
cc.add_project_argument(warning_flags)
```

A helper has been added for the foreach/has_argument pattern, so you
can now simply do:

```meson
warning_flags = [ ... ]
flags = cc.get_supported_arguments(warning_flags)
```

## Better support for shared libraries in non-system paths

Meson has support for prebuilt object files and static libraries. This
release adds feature parity to shared libraries that are either in
non-standard system paths or shipped as part of your project. On
systems that support rpath, Meson automatically adds rpath entries to
built targets using manually found external libraries.

This means that e.g. supporting prebuilt libraries shipped with your
source or provided by subprojects or wrap definitions by writing a
build file like this:

```meson
project('myprebuiltlibrary', 'c')

cc = meson.get_compiler('c')
prebuilt = cc.find_library('mylib', dirs : meson.current_source_dir())
mydep = declare_dependency(include_directories : include_directories('.'),
                           dependencies : prebuilt)
```

Then you can use the dependency object in the same way as any other.

## wrap-svn

The [Wrap dependency system](Wrap-dependency-system-manual.md) now
supports [Subversion](https://subversion.apache.org/) (svn). This
support is rudimentary. The repository url has to point to a specific
(sub)directory containing the `meson.build` file (typically `trunk/`).
However, providing a `revision` is supported.
