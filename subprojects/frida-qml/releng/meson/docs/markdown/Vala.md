---
title: Vala
short-description: Compiling Vala and Genie programs
...

# Compiling Vala applications and libraries
Meson supports compiling applications and libraries written in
[Vala](https://vala-project.org/) and
[Genie](https://wiki.gnome.org/Projects/Genie) . A skeleton `meson.build` file:

```meson
project('vala app', 'vala', 'c')

dependencies = [
    dependency('glib-2.0'),
    dependency('gobject-2.0'),
]

sources = files('app.vala')

executable('app_name', sources, dependencies: dependencies)
```

You must always specify the `glib-2.0` and `gobject-2.0` libraries as
dependencies, because all current Vala applications use them.
[GLib](https://developer.gnome.org/glib/stable/) is used for basic data types
and [GObject](https://developer.gnome.org/gobject/stable/) is used for the
runtime type system.


## Using libraries

Meson uses the [[dependency]]
function to find the relevant VAPI, C headers and linker flags when it
encounters a Vala source file in a build target. Vala needs a VAPI
file and a C header or headers to use a library. The VAPI file helps
map Vala code to the library's C programming interface. It is the
[`pkg-config`](https://www.freedesktop.org/wiki/Software/pkg-config/)
tool that makes finding these installed files all work seamlessly
behind the scenes. When a `pkg-config` file doesn't exist for the
library then the `find_library()`
method of the [[@compiler]] object
needs to be used. Examples are given later.

Note Vala uses libraries that follow the C Application Binary Interface (C ABI).
The library, however, could be written in C, Vala, Rust, Go, C++ or any other
language that can generate a binary compatible with the C ABI and so provides C
headers.


### The simplest case
This first example is a simple addition to the `meson.build` file because:

 * the library has a `pkg-config` file, `gtk+-3.0.pc`
 * the VAPI is distributed with Vala and so installed with the Vala compiler
 * the VAPI is installed in Vala's standard search path
 * the VAPI, `gtk+-3.0.vapi`, has the same name as the `pkg-config` file

Everything works seamlessly in the background and only a single extra line is
needed:

```meson
project('vala app', 'vala', 'c')

dependencies = [
    dependency('glib-2.0'),
    dependency('gobject-2.0'),
    dependency('gtk+-3.0'),
]

sources = files('app.vala')

executable('app_name', sources, dependencies: dependencies)
```

GTK+ is the graphical toolkit used by GNOME, elementary OS and other
desktop environments. The binding to the library, the VAPI file, is
distributed with Vala.

Other libraries may have a VAPI that is distributed with the library
itself. Such libraries will have their VAPI file installed along with
their other development files. The VAPI is installed in Vala's
standard search path and so works just as seamlessly using the
`dependency()` function.


### Targeting a version of GLib

Meson's [[dependency]] function
allows a version check of a library. This is often used to check a
minimum version is installed. When setting a minimum version of GLib,
Meson will also pass this to the Vala compiler using the
`--target-glib` option.

This is needed when using GTK+'s user interface definition files with
Vala's `[GtkTemplate]`, `[GtkChild]` and `[GtkCallback]` attributes.
This requires `--target-glib 2.38`, or a newer version, to be passed
to Vala. With Meson this is simply done with:

```meson
project('vala app', 'vala', 'c')

dependencies = [
    dependency('glib-2.0', version: '>=2.38'),
    dependency('gobject-2.0'),
    dependency('gtk+-3.0'),
]

sources = files('app.vala')

executable('app_name', sources, dependencies: dependencies)
```

Using `[GtkTemplate]` also requires the GTK+ user interface definition
files to be built into the binary as GResources. For completeness,
the next example shows this:

```meson
project('vala app', 'vala', 'c')

dependencies = [
    dependency('glib-2.0', version: '>=2.38'),
    dependency('gobject-2.0'),
    dependency('gtk+-3.0'),
]

sources = files('app.vala')

sources += import( 'gnome' ).compile_resources(
    'project-resources',
    'src/resources/resources.gresource.xml',
    source_dir: 'src/resources',
)

executable('app_name', sources, dependencies: dependencies)
```


### Adding to Vala's search path

So far we have covered the cases where the VAPI file is either
distributed with Vala or the library. A VAPI can also be included in
the source files of your project. The convention is to put it in the
`vapi` directory of your project.

This is needed when a library does not have a VAPI or your project
needs to link to another component in the project that uses the C ABI.
For example if part of the project is written in C.

The Vala compiler's `--vapidir` option is used to add the project
directory to the VAPI search path. In Meson this is done with the
`add_project_arguments()` function:

```meson
project('vala app', 'vala', 'c')

vapi_dir = meson.current_source_dir() / 'vapi'

add_project_arguments(['--vapidir', vapi_dir], language: 'vala')

dependencies = [
    dependency('glib-2.0'),
    dependency('gobject-2.0'),
    dependency('foo'), # 'foo.vapi' will be resolved as './vapi/foo.vapi'
]

sources = files('app.vala')

executable('app_name', sources, dependencies: dependencies)
```

If the VAPI is for an external library then make sure that the VAPI
name corresponds to the pkg-config file name.

The [`vala-extra-vapis`
repository](https://gitlab.gnome.org/GNOME/vala-extra-vapis) is a
community maintained repository of VAPIs that are not distributed.
Developers use the repository to share early work on new bindings and
improvements to existing bindings. So the VAPIs can frequently change.
It is recommended VAPIs from this repository are copied into your
project's source files.

This also works well for starting to write new bindings before they
are shared with the `vala-extra-vapis` repository.


### Libraries without pkg-config files

A library that does not have a corresponding pkg-config file may mean
`dependency()` is unsuitable for finding the C and Vala interface
files. In this case it is necessary to use the `find_library()` method
of the compiler object.

The first example uses Vala's POSIX binding. There is no pkg-config
file because POSIX includes the standard C library on Unix systems.
All that is needed is the VAPI file, `posix.vapi`. This is included
with Vala and installed in Vala's standard search path. Meson just
needs to be told to only find the library for the Vala compiler:

```meson
project('vala app', 'vala', 'c')

dependencies = [
    dependency('glib-2.0'),
    dependency('gobject-2.0'),
    meson.get_compiler('vala').find_library('posix'),
]

sources = files('app.vala')

executable('app_name', sources, dependencies: dependencies)
```

The next example shows how to link with a C library where no
additional VAPI is needed. The standard maths functions are already
bound in `glib-2.0.vapi`, but the GNU C library requires linking to
the maths library separately. In this example Meson is told to find
the library only for the C compiler:

```meson
project('vala app', 'vala', 'c')

dependencies = [
    dependency('glib-2.0'),
    dependency('gobject-2.0'),
    meson.get_compiler('c').find_library('m', required: false),
]

sources = files('app.vala')

executable('app_name', sources, dependencies: dependencies)
```

The `required: false` means the build will continue when using another
C library that does not separate the maths library. See [Add math
library (-lm) portably](howtox.md#add-math-library-lm-portably).

The final example shows how to use a library that does not have a
pkg-config file and the VAPI is in the `vapi` directory of your
project source files:

```meson
project('vala app', 'vala', 'c')

vapi_dir = meson.current_source_dir() / 'vapi'

add_project_arguments(['--vapidir', vapi_dir], language: 'vala')

dependencies = [
    dependency('glib-2.0'),
    dependency('gobject-2.0'),
    meson.get_compiler('c').find_library('foo'),
    meson.get_compiler('vala').find_library('foo', dirs: vapi_dir),
]

sources = files('app.vala')

executable('app_name', sources, dependencies: dependencies)
```
The `find_library()` method of the C compiler object will try to find the C
header files and the library to link with.

The `find_library()` method of the Vala compiler object needs to have the `dir`
keyword added to include the project VAPI directory. This is not added
automatically by `add_project_arguments()`.

### Working with the Vala Preprocessor

Passing arguments to [Vala's
preprocessor](https://wiki.gnome.org/Projects/Vala/Manual/Preprocessor)
requires specifying the language as `vala`. For example, the following
statement sets the preprocessor symbol `USE_FUSE`:

```meson
add_project_arguments('-D', 'USE_FUSE', language: 'vala')
```

If you need to pass an argument to the C pre-processor then specify
the language as c. For example to set FUSE_USE_VERSION to 26 use:

```meson
add_project_arguments('-DFUSE_USE_VERSION=26', language: 'c')
```

## Building libraries


### Changing C header and VAPI names

Meson's [[library]] target automatically
outputs the C header and the VAPI. They can be renamed by setting the
`vala_header` and `vala_vapi` arguments respectively:

```meson
foo_lib = shared_library('foo', 'foo.vala',
                  vala_header: 'foo.h',
                  vala_vapi: 'foo-1.0.vapi',
                  dependencies: [glib_dep, gobject_dep],
                  install: true,
                  install_dir: [true, true, true])
```

In this example, the second and third elements of the `install_dir`
array indicate the destination with `true` to use default directories
(i.e. `include` and `share/vala/vapi`).


### GObject Introspection and language bindings

A 'binding' allows another programming language to use a library
written in Vala. Because Vala uses the GObject type system as its
runtime type system it is very easy to use introspection to generate a
binding. A Meson build of a Vala library can generate the GObject
introspection metadata. The metadata is then used in separate projects
with [language specific
tools](https://wiki.gnome.org/Projects/Vala/LibraryWritingBindings) to
generate a binding.

The main form of metadata is a GObject Introspection Repository (GIR)
XML file. GIRs are mostly used by languages that generate bindings at
compile time. Languages that generate bindings at runtime mostly use a
typelib file, which is generated from the GIR.

Meson can generate a GIR as part of the build. For a Vala library the
`vala_gir` option has to be set for the `library`:

```meson
foo_lib = shared_library('foo', 'foo.vala',
                  vala_gir: 'Foo-1.0.gir',
                  dependencies: [glib_dep, gobject_dep],
                  install: true,
                  install_dir: [true, true, true, true])
```

The `true` value in `install_dir` tells Meson to use the default
directory (i.e. `share/gir-1.0` for GIRs). The fourth element in the
`install_dir` array indicates where the GIR file will be installed.

To then generate a typelib file use a custom target with the
`g-ir-compiler` program and a dependency on the library:

```meson
g_ir_compiler = find_program('g-ir-compiler')
custom_target('foo typelib', command: [g_ir_compiler, '--output', '@OUTPUT@', '@INPUT@'],
              input: meson.current_build_dir() / 'Foo-1.0.gir',
              output: 'Foo-1.0.typelib',
              depends: foo_lib,
              install: true,
              install_dir: get_option('libdir') / 'girepository-1.0')
```
