# Pkg config files

[Pkg-config](https://en.wikipedia.org/wiki/Pkg-config) is a way for
shared libraries to declare the compiler flags needed to use them.
There are two different ways of generating Pkg-config files in Meson.
The first way is to build them manually with the `configure_file`
command. The second way is to use Meson's built in Pkg-config file
generator. The difference between the two is that the latter is very
simple and meant for basic use cases. The former should be used when
you need to provide a more customized solution.

In this document we describe the simple generator approach. It is used in the following way.

```meson
pkg = import('pkgconfig')
libs = ...     # the library/libraries users need to link against
h = ['.', ...] # subdirectories of ${prefix}/${includedir} to add to header path
pkg.generate(libraries : libs,
             subdirs : h,
             version : '1.0',
             name : 'libsimple',
             filebase : 'simple',
             description : 'A simple demo library.')
```

This causes a file called `simple.pc` to be created and placed into
the install directory during the install phase.

More information on the pkg-config module and the parameters can be
found on the [pkgconfig-module](Pkgconfig-module.md) page.
