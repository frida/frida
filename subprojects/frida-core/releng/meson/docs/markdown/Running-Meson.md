---
short-description: Building a project with Meson
...

# Running Meson

There are two different ways of invoking Meson. First, you can run it
directly from the source tree with the command
`/path/to/source/meson.py`. Second, Meson may also be installed in which case
the command is simply `meson`. In this manual we only use the latter
format for simplicity.

At the time of writing only a command line version of Meson is
available. This means that Meson must be invoked using the terminal.
If you wish to use the MSVC compiler, you need to run Meson under
"Visual Studio command prompt".

All available Meson commands are listed on the [commands reference
page](Commands.md).

## Configuring the build directory

Let us assume that we have a source tree that has a Meson build
system. This means that at the topmost directory has a file called
`meson.build`. We run the following commands to get the build started.

```sh
cd /path/to/source/root
meson setup builddir
```

We invoke Meson with the `setup` command, giving it the location of the build
directory. Meson uses [out of source
builds](http://web.archive.org/web/20190715081007/http://voices.canonical.com/jussi.pakkanen/2013/04/16/why-you-should-consider-using-separate-build-directories/).

Hint: The syntax of Meson is `meson [command] [arguments] [options]`.
The `setup` command takes a `builddir` and a `srcdir` argument. If no
`srcdir` is given Meson will deduce the `srcdir` based on `pwd` and
the location of `meson.build`.

Meson then loads the build configuration file and writes the
corresponding build backend in the build directory. By default Meson
generates a *debug build*, which turns on basic warnings and debug
information and disables compiler optimizations.

Additionally, the invocation can pass options to Meson. The list of
options is documented [here](Builtin-options.md).

You can specify a different type of build with the `--buildtype` command line
argument. It can have one of the following values.

| value            | meaning                                                                                                                                                    |
| ------           | --------                                                                                                                                                   |
| `plain`          | no extra build flags are used, even for compiler warnings, useful for distro packagers and other cases where you need to specify all arguments by yourself |
| `debug`          | debug info is generated but the result is not optimized, this is the default                                                                               |
| `debugoptimized` | debug info is generated and the code is optimized (on most compilers this means `-g -O2`)                                                                  |
| `release`        | full optimization, no debug info                                                                                                                           |

The build directory is mandatory. The reason for this is that it
simplifies the build process immensely. Meson will not, under any
circumstances, write files inside the source directory (if it does, it
is a bug and should be fixed). This means that the user does not need
to add a bunch of files to their revision control's ignore list. It
also means that you can create arbitrarily many build directories for
any given source tree.

For example, if we wanted to test building the source code with the
Clang compiler instead of the system default, we could just type the
following commands:

```sh
cd /path/to/source/root
CC=clang CXX=clang++ meson setup buildclang
```

This separation is even more powerful if your code has multiple
configuration options (such as multiple data backends). You can create
a separate subdirectory for each of them. You can also have build
directories for optimized builds, code coverage, static analysis and
so on. They are all neatly separated and use the same source tree.
Changing between different configurations is just a question of
changing to the corresponding directory.

Unless otherwise mentioned, all following command line invocations are
meant to be run in the source directory.

By default, Meson will use the Ninja backend to build your project. If
you wish to use any of the other backends, you need to pass the
corresponding argument during configuration time. As an example, here
is how you would use Meson to generate a Visual Studio solution.

```sh
meson setup <build dir> --backend=vs
```

You can then open the generated solution with Visual Studio and
compile it in the usual way. A list of backends can be obtained with
`meson setup --help`.

## Environment variables

Sometimes you want to add extra compiler flags, this can be done by
passing them in environment variables when calling Meson. See [the
reference
tables](Reference-tables.md#compiler-and-linker-flag-environment-variables)
for a list of all the environment variables. Be aware however these
environment variables are only used for the native compiler and will
not affect the compiler used for cross-compiling, where the flags
specified in the cross file will be used.

Furthermore it is possible to stop Meson from adding flags itself by
using the `--buildtype=plain` option, in this case you must provide
the full compiler and linker arguments needed.

## Building from the source

To start the build, simply type the following command.

```sh
meson compile -C builddir
```

See [`meson compile` description](Commands.md#compile) for more info.

### Building directly with ninja

By default Meson uses the [Ninja build
system](https://ninja-build.org/) to actually build the code. To start
the build, simply type the following command.

```sh
ninja -C builddir
```

The main usability difference between Ninja and Make is that Ninja
will automatically detect the number of CPUs in your computer and
parallelize itself accordingly. You can override the amount of
parallel processes used with the command line argument `-j <num
processes>`.

It should be noted that after the initial configure step `ninja` is
the only command you ever need to type to compile. No matter how you
alter your source tree (short of moving it to a completely new
location), Meson will detect the changes and regenerate itself
accordingly. This is especially handy if you have multiple build
directories. Often one of them is used for development (the "debug"
build) and others only every now and then (such as a "static analysis"
build). Any configuration can be built just by `cd`'ing to the
corresponding directory and running Ninja.

## Running tests

Meson provides native support for running tests. The command to do
that is simple.

```sh
meson test -C builddir
```

See [`meson test` description](Commands.md#test) for more info.

Meson does not force the use of any particular testing framework. You
are free to use GTest, Boost Test, Check or even custom executables.

Note: it can be also invoked directly with ninja with the following command:
```sh
ninja -C builddir test
```

## Installing

Installing the built software is just as simple.

```sh
meson install -C builddir
```

See [`meson install` description](Commands.md#install) for more info.

Note that Meson will only install build targets explicitly tagged as
installable, as detailed in the [installing targets
documentation](Installing.md).

By default Meson installs to `/usr/local`. This can be changed by
passing the command line argument `--prefix /your/prefix` to Meson
during configure time. Meson also supports the `DESTDIR` variable used
in e.g. building packages. It is used like this:

```sh
DESTDIR=/path/to/staging meson install -C builddir
```

Note: it can be also invoked directly with ninja with the following
command:

```sh
ninja -C builddir install
```

## Command line help

Meson has a standard command line help feature. It can be accessed
with the following command.

    meson --help

## Exit status

Meson exits with status 0 if successful, 1 for problems with the
command line or meson.build file, and 2 for internal errors.
