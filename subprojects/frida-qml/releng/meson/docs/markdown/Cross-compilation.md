---
short-description: Setting up cross-compilation
...

# Cross compilation

Meson has full support for cross compilation through the use of
a cross build definition file.  An minimal example of one such
file `x86_64-w64-mingw32.txt` for a GCC/MinGW cross compiler
targeting 64-bit Windows could be:

```ini
[binaries]
c = 'x86_64-w64-mingw32-gcc'
cpp = 'x86_64-w64-mingw32-g++'
ar = 'x86_64-w64-mingw32-ar'
strip = 'x86_64-w64-mingw32-strip'
exe_wrapper = 'wine64'

[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
```

Which is then used during the `setup` phase.

```sh
meson setup --cross-file x86_64-w64-mingw32.txt build-mingw
meson compile -C build-mingw
```

Since cross compiling is
more complicated than native building, let's first go over some
nomenclature. The three most important definitions are traditionally
called *build*, *host* and *target*. This is confusing because those
terms are used for quite many different things. To simplify the issue,
we are going to call these the *build machine*, *host machine* and
*target machine*. Their definitions are the following:

* *build machine* is the computer that is doing the actual compiling.
* *host machine* is the machine on which the compiled binary will run.
* *target machine* is the machine on which the compiled binary's
  output will run, *only meaningful* if the program produces
  machine-specific output.

The `tl/dr` summary is the following: if you are doing regular cross
compilation, you only care about [[@build_machine]] and
[[@host_machine]]. Just ignore [[@target_machine]] altogether and you will
be correct 99% of the time. Only compilers and similar tools care
about the target machine. In fact, for so-called "multi-target" tools
the target machine need not be fixed at build-time like the others but
chosen at runtime, so `target_machine` *still* doesn't matter. If your
needs are more complex or you are interested in the actual details, do
read on.

This might be easier to understand through examples. Let's start with
the regular, not cross-compiling case. In these cases all of these
three machines are the same. Simple so far.

Let's next look at the most common cross-compilation setup. Let's
suppose you are on a 64 bit OSX machine and you are cross compiling a
binary that will run on a 32 bit ARM Linux board. In this case your
*build machine* is 64 bit OSX, your *host machine* is 32 bit ARM Linux
and your *target machine* is irrelevant (but defaults to the same
value as the *host machine*). This should be quite understandable as
well.

The usual mistake in this case is to call the OSX system the *host*
and the ARM Linux board the *target*. That's because these were their
actual names when the cross-compiler itself was compiled! Let's assume
the cross-compiler was created on OSX too. When that happened the
*build* and *host machines* were the same OSX and different from the
ARM Linux *target machine*.

In a nutshell, the typical mistake assumes that the terms *build*,
*host* and *target* refer to some fixed positions whereas they're
actually relative to where the current compiler is running. Think of
*host* as a *child* of the current compiler and *target* as an
optional *grand-child*. Compilers don't change their terminology when
they're creating another compiler, that would at the very least make
their user interface much more complex.

The most complicated case is when you cross-compile a cross compiler.
As an example you can, on a Linux machine, generate a cross compiler
that runs on Windows but produces binaries on MIPS Linux. In this case
*build machine* is x86 Linux, *host machine* is x86 Windows and
*target machine* is MIPS Linux. This setup is known as the [Canadian
Cross](https://en.wikipedia.org/wiki/Cross_compiler#Canadian_Cross).
As a side note, be careful when reading cross compilation articles on
Wikipedia or the net in general. It is very common for them to get
build, host and target mixed up, even in consecutive sentences, which
can leave you puzzled until you figure it out.

Again note that when you cross-compile something, the 3 systems
(*build*, *host*, and *target*) used when building the cross compiler
don't align with the ones used when building something with that
newly-built cross compiler. To take our Canadian Cross scenario from
above (for full generality), since its *host machine* is x86 Windows,
the *build machine* of anything we build with it is *x86 Windows*. And
since its *target machine* is MIPS Linux, the *host machine* of
anything we build with it is *MIPS Linux*. Only the *target machine*
of whatever we build with it can be freely chosen by us, say if we
want to build another cross compiler that runs on MIPS Linux and
targets Aarch64 iOS. As this example hopefully makes clear to you, the
machine names are relative and shifted over to the left by one
position.

If you did not understand all of the details, don't worry. For most
people it takes a while to wrap their head around these concepts.
Don't panic, it might take a while to click, but you will get the hang
of it eventually.

## Defining the environment

Meson requires you to write a cross build definition file. It defines
various properties of the cross build environment. The cross file
consists of different sections.

There are a number of options shared by cross and native files,
[here](Machine-files.md). It is assumed that you have read that
section already, as this documentation will only call out options
specific to cross files.

### Binaries

```ini
[binaries]
exe_wrapper = 'wine' # A command used to run generated executables.
```

The `exe_wrapper` option defines a *wrapper command* that can be used
to run executables for this host. In this case we can use Wine, which
runs Windows applications on Linux. Other choices include running the
application with qemu or a hardware simulator. If you have this kind
of a wrapper, these lines are all you need to write. Meson will
automatically use the given wrapper when it needs to run host
binaries. This happens e.g. when running the project's test suite.

### Properties

In addition to the properties allowed in [all machine
files](Machine-files.md#properties), the cross file may contain
specific information about the cross compiler or the host machine. It
looks like this:

```ini
[properties]
sizeof_int = 4
sizeof_wchar_t = 4
sizeof_void* = 4

alignment_char = 1
alignment_void* = 4
alignment_double = 4

has_function_printf = true

sys_root = '/some/path'
pkg_config_libdir = '/some/path/lib/pkgconfig'
```

In most cases you don't need the size and alignment settings, Meson
will detect all these by compiling and running some sample programs.
If your build requires some piece of data that is not listed here,
Meson will stop and write an error message describing how to fix the
issue. If you need extra compiler arguments to be used during cross
compilation you can set them with `[langname]_args = [args]`. Just
remember to specify the args as an array and not as a single string
(i.e. not as `'-DCROSS=1 -DSOMETHING=3'`).

*Since 0.52.0* The `sys_root` property may point to the root of the
host system path (the system that will run the compiled binaries).
This is used internally by Meson to set the PKG_CONFIG_SYSROOT_DIR
environment variable for pkg-config. If this is unset the host system
is assumed to share a root with the build system.

*Since 0.54.0* The pkg_config_libdir property may point to a list of
path used internally by Meson to set the PKG_CONFIG_LIBDIR environment
variable for pkg-config. This prevents pkg-config from searching cross
dependencies in system directories.

One important thing to note, if you did not define an `exe_wrapper` in
the previous section, is that Meson will make a best-effort guess at
whether it can run the generated binaries on the build machine. It
determines whether this is possible by looking at the `system` and
`cpu_family` of build vs host. There will however be cases where they
do match up, but the build machine is actually not compatible with the
host machine. Typically this will happen if the libc used by the build
and host machines are incompatible, or the code relies on kernel
features not available on the build machine. One concrete example is a
macOS build machine producing binaries for an iOS Simulator x86-64
host. They're both `darwin` and the same architecture, but their
binaries are not actually compatible. In such cases you may use the
`needs_exe_wrapper` property to override the auto-detection:

```ini
[properties]
needs_exe_wrapper = true
```

### Machine Entries

The next bit is the definition of host and target machines. Every
cross build definition must have one or both of them. If it had
neither, the build would not be a cross build but a native build. You
do not need to define the build machine, as all necessary information
about it is extracted automatically. The definitions for host and
target machines look the same. Here is a sample for host machine.

```ini
[host_machine]
system = 'windows'
subsystem = 'windows'
kernel = 'nt'
cpu_family = 'x86'
cpu = 'i686'
endian = 'little'
```

These values define the machines sufficiently for cross compilation
purposes. The corresponding target definition would look the same but
have `target_machine` in the header. These values are available in
your Meson scripts. There are three predefined variables called,
surprisingly, [[@build_machine]], [[@host_machine]] and
[[@target_machine]]. Determining the operating system of your host
machine is simply a matter of calling `host_machine.system()`.
Starting from version 1.2.0 you can get more fine grained information
using the `.subsystem()` and `.kernel()` methods. The return values of
these functions are documented in [the reference table
page](Reference-tables.md).

There are two different values for the CPU. The first one is
`cpu_family`. It is a general type of the CPU. This should have a
value from [the CPU Family table](Reference-tables.md#cpu-families).
*Note* that Meson does not add `el` to end cpu_family value for little
endian systems. Big endian and little endian mips are both just
`mips`, with the `endian` field set appropriately.

The second value is `cpu` which is a more specific subtype for the
CPU. Typical values for a `x86` CPU family might include `i386` or
`i586` and for `arm` family `armv5` or `armv7hl`. Note that CPU type
strings are very system dependent. You might get a different value if
you check its value on the same machine but with different operating
systems.

If you do not define your host machine, it is assumed to be the build
machine. Similarly if you do not specify target machine, it is assumed
to be the host machine.


## Starting a cross build


Once you have the cross file, starting a build is simple

```console
$ meson setup builddir --cross-file cross_file.txt
```

Once configuration is done, compilation is started by invoking `meson compile`
in the usual way.

## Introspection and system checks

The main *meson* object provides two functions to determine cross
compilation status.

```meson
[[#meson.is_cross_build]]        # returns true when cross compiling
[[#meson.can_run_host_binaries]] # returns true if the host binaries can be run, either with a wrapper or natively
```

You can run system checks on both the system compiler or the cross
compiler. You just have to specify which one to use.

```meson
build_compiler = [[#meson.get_compiler]]('c', native : true)
host_compiler = [[#meson.get_compiler]]('c', native : false)

build_int_size = build_compiler.sizeof('int')
host_int_size  = host_compiler.sizeof('int')
```

## Mixing host and build targets

Sometimes you need to build a tool which is used to generate source
files. These are then compiled for the actual target. For this you
would want to build some targets with the system's native compiler.
This requires only one extra keyword argument.

```meson
native_exe = executable('mygen', 'mygen.c', native : true)
```

You can then take `native_exe` and use it as part of a generator rule or anything else you might want.

## Using a custom standard library

Sometimes in cross compilation you need to build your own standard
library instead of using the one provided by the compiler. Meson has
built-in support for switching standard libraries transparently. The
invocation to use in your cross file is the following:

```ini
[properties]
c_stdlib = ['mylibc', 'mylibc_dep'] # Subproject name, variable name
```

This specifies that C standard library is provided in the Meson
subproject `mylibc` in internal dependency variable `mylibc_dep`. It
is used on every cross built C target in the entire source tree
(including subprojects) and the standard library is disabled. The
build definitions of these targets do not need any modification.

Note that it is supported for any language, not only `c`, using `<lang>_stdlib`
property.

Since *0.56.0* the variable name parameter is no longer required as long as the
subproject calls `meson.override_dependency('c_stdlib', mylibc_dep)`.
The above example becomes:

```ini
[properties]
c_stdlib = 'mylibc'
```

## Changing cross file settings

Cross file settings are only read when the build directory is set up
the first time. Any changes to them after the fact will be ignored.
This is the same as regular compiles where you can't change the
compiler once a build tree has been set up. If you need to edit your
cross file, then you need to wipe your build tree and recreate it from
scratch.

## Custom data

You can store arbitrary data in `properties` and access them from your
Meson files. As an example if your cross file has this:

```ini
[properties]
somekey = 'somevalue'
```

then you can access that using the `meson` object like this:

```meson
myvar = meson.get_external_property('somekey')
# myvar now has the value 'somevalue'
```

## Cross file locations

As of version 0.44.0 Meson supports loading cross files from system
locations (except on Windows). This will be
$XDG_DATA_DIRS/meson/cross, or if XDG_DATA_DIRS is undefined, then
/usr/local/share/meson/cross and /usr/share/meson/cross will be tried
in that order, for system wide cross files. User local files can be
put in $XDG_DATA_HOME/meson/cross, or ~/.local/share/meson/cross if
that is undefined.

The order of locations tried is as follows:
 - A file relative to the local dir
 - The user local location
 - The system wide locations in order

Distributions are encouraged to ship cross files either with their
cross compiler toolchain packages or as a standalone package, and put
them in one of the system paths referenced above.

These files can be loaded automatically without adding a path to the
cross file. For example, if a ~/.local/share/meson/cross contains a
file called x86-linux, then the following command would start a cross
build using that cross files:

```sh
meson setup builddir/ --cross-file x86-linux
```
