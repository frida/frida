---
title: Design rationale
...

This is the original design rationale for Meson. The syntax it
describes does not match the released version
==

A software developer's most important tool is the editor. If you talk
to coders about the editors they use, you are usually met with massive
enthusiasm and praise. You will hear how Emacs is the greatest thing
ever or how vi is so elegant or how Eclipse's integration features
make you so much more productive. You can sense the enthusiasm and
affection that the people feel towards these programs.

The second most important tool, even more important than the compiler,
is the build system.

Those are pretty much universally despised.

The most positive statement on build systems you can usually get (and
it might require some coaxing) is something along the lines of *well,
it's a terrible system, but all other options are even worse*. It is
easy to see why this is the case. For starters, commonly used free
build systems have obtuse syntaxes. They use for the most part global
variables that are set in random locations so you can never really be
sure what a given line of code does. They do strange and unpredictable
things at every turn.

Let's illustrate this with a simple example. Suppose we want to run a
program built with GNU Autotools under GDB. The instinctive thing to
do is to just run `gdb programname`. The problem is that this may or
may not work. In some cases the executable file is a binary whereas at
other times it is a wrapper shell script that invokes the real binary
which resides in a hidden subdirectory. GDB invocation fails if the
binary is a script but succeeds if it is not. The user has to remember
the type of each one of his executables (which is an implementation
detail of the build system) just to be able to debug them. Several
other such pain points can be found in [this blog
post](http://voices.canonical.com/jussi.pakkanen/2011/09/13/autotools/).

Given these idiosyncrasies it is no wonder that most people don't want
to have anything to do with build systems. They'll just copy-paste
code that works (somewhat) in one place to another and hope for the
best. They actively go out of their way not to understand the system
because the mere thought of it is repulsive. Doing this also provides
a kind of inverse job security. If you don't know tool X, there's less
chance of finding yourself responsible for its use in your
organisation. Instead you get to work on more enjoyable things.

This leads to a vicious circle. Since people avoid the tools and don't
want to deal with them, very few work on improving them. The result is
apathy and stagnation.

Can we do better?
--

At its core, building C and C++ code is not a terribly difficult
task. In fact, writing a text editor is a lot more complicated and
takes more effort. Yet we have lots of very high quality editors but
only few build systems with questionable quality and usability.

So, in the grand tradition of own-itch-scratching, I decided to run a
scientific experiment. The purpose of this experiment was to explore
what would it take to build a "good" build system. What kind of syntax
would suit this problem? What sort of problems would this application
need to solve? What sort of solutions would be the most appropriate?

To get things started, here is a list of requirements any modern
cross-platform build system needs to provide.

### 1. Must be simple to use

One of the great virtues of Python is the fact that it is very
readable. It is easy to see what a given block of code does. It is
concise, clear and easy to understand. The proposed build system must
be syntactically and semantically clean. Side effects, global state
and interrelations must be kept at a minimum or, if possible,
eliminated entirely.

### 2. Must do the right thing by default

Most builds are done by developers working on the code. Therefore the
defaults must be tailored towards that use case. As an example the
system shall build objects without optimization and with debug
information. It shall make binaries that can be run directly from the
build directory without linker tricks, shell scripts or magic
environment variables.

### 3. Must enforce established best practices

There really is no reason to compile source code without the
equivalent of `-Wall`. So enable it by default. A different kind of
best practice is the total separation of source and build
directories. All build artifacts must be stored in the build
directory. Writing stray files in the source directory is not
permitted under any circumstances.

### 4. Must have native support for platforms that are in common use

A lot of free software projects can be used on non-free platforms such
as Windows or OSX. The system must provide native support for the
tools of choice on those platforms. In practice this means native
support for Visual Studio and XCode. Having said IDEs invoke external
builder binaries does not count as native support.

### 5. Must not add complexity due to obsolete platforms

Work on this build system started during the Christmas holidays of 2012.
This provides a natural hard cutoff line of 2012/12/24. Any
platform, tool or library that was not in active use at that time is
explicitly not supported. These include Unixes such as IRIX, SunOS,
OSF-1, Ubuntu versions older than 12/10, GCC versions older than 4.7
and so on. If these old versions happen to work, great. If they don't,
not a single line of code will be added to the system to work around
their bugs.

### 6. Must be fast

Running the configuration step on a moderate sized project must not
take more than five seconds. Running the compile command on a fully up
to date tree of 1000 source files must not take more than 0.1 seconds.

### 7. Must provide easy to use support for modern sw development features

An example is precompiled headers. Currently no free software build
system provides native support for them. Other examples could include
easy integration of Valgrind and unit tests, test coverage reporting
and so on.

### 8. Must allow override of default values

Sometimes you just have to compile files with only given compiler
flags and no others, or install files in weird places. The system must
allow the user to do this if he really wants to.

Overview of the solution
--

Going over these requirements it becomes quite apparent that the only
viable approach is roughly the same as taken by CMake: having a domain
specific language to declare the build system. Out of this declaration
a configuration is generated for the backend build system. This can be
a Makefile, Visual Studio or XCode project or anything else.

The difference between the proposed DSL and existing ones is that the
new one is declarative. It also tries to work on a higher level of
abstraction than existing systems. As an example, using external
libraries in current build systems means manually extracting and
passing around compiler flags and linker flags. In the proposed system
the user just declares that a given build target uses a given external
dependency. The build system then takes care of passing all flags and
settings to their proper locations. This means that the user can focus
on his own code rather than marshalling command line arguments from
one place to another.

A DSL is more work than the approach taken by SCons, which is to
provide the system as a Python library. However it allows us to make
the syntax more expressive and prevent certain types of bugs by
e.g. making certain objects truly immutable. The end result is again
the same: less work for the user.

The backend for Unix requires a bit more thought. The default choice
would be Make. However it is extremely slow. It is not uncommon on
large code bases for Make to take several minutes just to determine
that nothing needs to be done. Instead of Make we use
[Ninja](https://ninja-build.org/), which is extremely fast. The
backend code is abstracted away from the core, so other backends can
be added with relatively little effort.

Sample code
--

Enough design talk, let's get to the code. Before looking at the
examples we would like to emphasize that this is not in any way the
final code. It is proof of concept code that works in the system as it
currently exists (February 2013), but may change at any time.

Let's start simple. Here is the code to compile a single executable
binary.

```meson
project('compile one', 'c')
executable('program', 'prog.c')
```

This is about as simple as one can get. First you declare the project
name and the languages it uses. Then you specify the binary to build
and its sources. The build system will do all the rest. It will add
proper suffixes (e.g. '.exe' on Windows), set the default compiler
flags and so on.

Usually programs have more than one source file. Listing them all in
the function call can become unwieldy. That is why the system supports
keyword arguments. They look like this.

```meson
project('compile several', 'c')
sourcelist = ['main.c', 'file1.c', 'file2.c', 'file3.c']
executable('program', sources : sourcelist)
```

External dependencies are simple to use.

```meson
project('external lib', 'c')
libdep = find_dep('extlibrary', required : true)
sourcelist = ['main.c', 'file1.c', 'file2.c', 'file3.c']
executable('program', sources : sourcelist, dep : libdep)
```

In other build systems you have to manually add the compile and link
flags from external dependencies to targets. In this system you just
declare that extlibrary is mandatory and that the generated program
uses that. The build system does all the plumbing for you.

Here's a slightly more complicated definition. It should still be
understandable.

```meson
project('build library', 'c')
foolib = shared_library('foobar', sources : 'foobar.c',\
install : true)
exe = executable('testfoobar', 'tester.c', link : foolib)
add_test('test library', exe)
```

First we build a shared library named foobar. It is marked
installable, so running `meson install` installs it to the library
directory (the system knows which one so the user does not have to
care). Then we build a test executable which is linked against the
library. It will not be installed, but instead it is added to the list
of unit tests, which can be run with the command `meson test`.

Above we mentioned precompiled headers as a feature not supported by
other build systems. Here's how you would use them.

```meson
project('pch demo', 'cxx')
executable('myapp', 'myapp.cpp', pch : 'pch/myapp.hh')
```

The main reason other build systems cannot provide pch support this
easily is because they don't enforce certain best practices. Due to
the way include paths work, it is impossible to provide pch support
that always works with both in-source and out-of-source
builds. Mandating separate build and source directories makes this and
many other problems a lot easier.

Get the code
--

The code for this experiment can be found at [the Meson
repository](https://github.com/mesonbuild/meson). It should be noted
that (at the time of writing) it is not a build system. It is only
a proposal for one. It does not work reliably yet. You probably
should not use it as the build system of your project.

All that said I hope that this experiment will eventually turn into a
full blown build system. For that I need your help. Comments and
especially patches are more than welcome.
