# An in-depth tutorial

In this tutorial we set up a project with multiple targets, unit tests
and dependencies between targets. Our main product is a shared library
called *foo* that is written in `C++11`. We are going to ignore the
contents of the source files, as they are not really important from a
build definition point of view. The library makes use of the `GLib`
library so we need to detect and link it properly. We also make the
resulting library installable.

The source tree contains three subdirectories `src`, `include` and
`test` that contain, respectively, the source code, headers and unit
tests of our project.

To start things up, here is the top level `meson.build` file.

```meson
project('c++ foolib', 'cpp',
  version : '1.0.0',
  license : 'MIT')
add_global_arguments('-DSOME_TOKEN=value', language : 'cpp')
glib_dep = dependency('glib-2.0')

inc = include_directories('include')

subdir('include')
subdir('src')
subdir('test')

pkg_mod = import('pkgconfig')
pkg_mod.generate(libraries : foolib,
                 version : '1.0',
                 name : 'libfoobar',
                 filebase : 'foobar',
                 description : 'A Library to barnicate your foos.')
```

The definition always starts with a call to the `project` function. In
it you must specify the project's name and programming languages to
use, in this case only `C++`. We also specify two additional
arguments, the project's version and the license it is under. Our
project is version `1.0.0` and is specified to be under the MIT
license.

Then we find GLib, which is an *external dependency*. The `dependency`
function tells Meson to find the library (by default using
`pkg-config`). If the library is not found, Meson will raise an error
and stop processing the build definition.

Then we add a global compiler argument `-DSOME_TOKEN=value`. This flag
is used for *all* C++ source file compilations. It is not possible to
unset it for some targets. The reason for this is that it is hard to
keep track of what compiler flags are in use if global settings change
per target.

Since `include` directory contains the header files, we need a way to
tell compilations to add that directory to the compiler command line.
This is done with the `include_directories` command that takes a
directory and returns an object representing this directory. It is
stored in variable `inc` which makes it accessible later on.

After this are three `subdir` commands. These instruct Meson to go to
the specified subdirectory, open the `meson.build` file that's in
there and execute it. The last few lines are a stanza to generate a
`pkg-config` file. We'll skip that for now and come back to it at the
end of this document.

The first subdirectory we go into is `include`. In it we have a
header file for the library that we want to install. This requires one
line.

```meson
install_headers('foolib.h')
```

This installs the given header file to the system's header directory.
This is by default `/[install prefix]/include`, but it can be changed
with a command line argument.

The Meson definition of `src` subdir is simple.

```meson
foo_sources = ['source1.cpp', 'source2.cpp']
foolib = shared_library('foo',
                        foo_sources,
                        include_directories : inc,
                        dependencies : glib_dep,
                        install : true)
```

Here we just tell Meson to build the library with the given sources.
We also tell it to use the include directories we stored to variable
`inc` earlier. Since this library uses GLib, we tell Meson to add all
necessary compiler and linker flags with the `dependencies` keyword
argument. Its value is `glib_dep` which we set at the top level
`meson.build` file. The `install` argument tells Meson to install the
result. As with the headers, the shared library is installed to the
system's default location (usually `/[install prefix]/lib`) but is
again overridable.

The resulting library is stored in variable `foolib` just like the
include directory was stored in the previous file.

Once Meson has processed the `src` subdir it returns to the main Meson
file and executes the next line that moves it into the `test` subdir.
Its contents look like this.

```meson
testexe = executable('testexe', 'footest.cpp',
                     include_directories : inc,
                     link_with : foolib)
test('foolib test', testexe)
```

First we build a test executable that has the same include directory
as the main library and which also links against the freshly built
shared library. Note that you don't need to specify `glib_dep` here
just to be able to use the built library `foolib`. If the executable
used GLib functionality itself, then we would of course need to add it
as a keyword argument here.

Finally we define a test with the name `foolib test`. It consists of
running the binary we just built. If the executable exits with a zero
return value, the test is considered passed. Nonzero return values
mark the test as failed.

At this point we can return to the pkg-config generator line. All
shared libraries should provide a pkg-config file, which explains how
that library is used. Meson provides this simple generator that should
be sufficient for most simple projects. All you need to do is list a
few basic pieces of information and Meson takes care of generating an
appropriate file. More advanced users might want to create their own
pkg-config files using Meson's [configuration file generator
system](Configuration.md).

With these four files we are done. To configure, build and run the
test suite, we just need to execute the following commands (starting
at source tree root directory).

```console
$ meson setup builddir && cd builddir
$ meson compile
$ meson test
```

To then install the project you only need one command.

```console
$ meson install
```
