---
title: FAQ
...
# Meson Frequently Asked Questions

See also [How do I do X in Meson](howtox.md).

## Why is it called Meson?

When the name was originally chosen, there were two main limitations:
there must not exist either a Debian package or a Sourceforge project
of the given name. This ruled out tens of potential project names. At
some point the name Gluon was considered. Gluons are elementary
particles that hold protons and neutrons together, much like a build
system's job is to take pieces of source code and a compiler and bind
them to a complete whole.

Unfortunately this name was taken, too. Then the rest of subatomic
particles were examined and Meson was found to be available.

## What is the correct way to use threads (such as pthreads)?

```meson
thread_dep = dependency('threads')
```

This will set up everything on your behalf. People coming from
Autotools or CMake want to do this by looking for `libpthread.so`
manually. Don't do that, it has tricky corner cases especially when
cross compiling.

## How to use Meson on a host where it is not available in system packages?

Starting from version 0.29.0, Meson is available from the [Python
Package Index](https://pypi.python.org/pypi/meson/), so installing it
simply a matter of running this command:

```console
$ pip3 install <your options here> meson
```

If you don't have access to PyPI, that is not a problem either. Meson
has been designed to be easily runnable from an extracted source
tarball or even a git checkout. First you need to download Meson. Then
use this command to set up you build instead of plain `meson`.

```console
$ /path/to/meson.py <options>
```

After this you don't have to care about invoking Meson any more. It
remembers where it was originally invoked from and calls itself
appropriately. As a user the only thing you need to do is to `cd` into
your build directory and invoke `meson compile`.

## Why can't I specify target files with a wildcard?

Instead of specifying files explicitly, people seem to want to do this:

```meson
executable('myprog', sources : '*.cpp') # This does NOT work!
```

Meson does not support this syntax and the reason for this is simple.
This cannot be made both reliable and fast. By reliable we mean that
if the user adds a new source file to the subdirectory, Meson should
detect that and make it part of the build automatically.

One of the main requirements of Meson is that it must be fast. This
means that a no-op build in a tree of 10 000 source files must take no
more than a fraction of a second. This is only possible because Meson
knows the exact list of files to check. If any target is specified as
a wildcard glob, this is no longer possible. Meson would need to
re-evaluate the glob every time and compare the list of files produced
against the previous list. This means inspecting the entire source
tree (because the glob pattern could be `src/\*/\*/\*/\*.cpp` or
something like that). This is impossible to do efficiently.

The main backend of Meson is Ninja, which does not support wildcard
matches either, and for the same reasons.

Because of this, all source files must be specified explicitly.

## But I really want to use wildcards!

If the tradeoff between reliability and convenience is acceptable to
you, then Meson gives you all the tools necessary to do wildcard
globbing. You are allowed to run arbitrary commands during
configuration. First you need to write a script that locates the files
to compile. Here's a simple shell script that writes all `.c` files in
the current directory, one per line.


```bash
#!/bin/sh

for i in *.c; do
  echo $i
done
```

Then you need to run this script in your Meson file, convert the
output into a string array and use the result in a target.

```meson
c = run_command('grabber.sh', check: true)
sources = c.stdout().strip().split('\n')
e = executable('prog', sources)
```

The script can be any executable, so it can be written in shell,
Python, Lua, Perl or whatever you wish.

As mentioned above, the tradeoff is that just adding new files to the
source directory does *not* add them to the build automatically. To
add them you need to tell Meson to reinitialize itself. The simplest
way is to touch the `meson.build` file in your source root. Then Meson
will reconfigure itself next time the build command is run. Advanced
users can even write a small background script that utilizes a
filesystem event queue, such as
[inotify](https://en.wikipedia.org/wiki/Inotify), to do this
automatically.

## Should I use `subdir` or `subproject`?

The answer is almost always `subdir`. Subproject exists for a very
specific use case: embedding external dependencies into your build
process. As an example, suppose we are writing a game and wish to use
SDL. Let us further suppose that SDL comes with a Meson build
definition. Let us suppose even further that we don't want to use
prebuilt binaries but want to compile SDL for ourselves.

In this case you would use `subproject`. The way to do it would be to
grab the source code of SDL and put it inside your own source
tree. Then you would do `sdl = subproject('sdl')`, which would cause
Meson to build SDL as part of your build and would then allow you to
link against it or do whatever else you may prefer.

For every other use you would use `subdir`. As an example, if you
wanted to build a shared library in one dir and link tests against it
in another dir, you would do something like this:

```meson
project('simple', 'c')
subdir('src')   # library is built here
subdir('tests') # test binaries would link against the library here
```

## Why is there not a Make backend?

Because Make is slow. This is not an implementation issue, Make simply
cannot be made fast. For further info we recommend you read [this
post](http://neugierig.org/software/chromium/notes/2011/02/ninja.html)
by Evan Martin, the author of Ninja. Makefiles also have a syntax that
is very unpleasant to write which makes them a big maintenance burden.

The only reason why one would use Make instead of Ninja is working on
a platform that does not have a Ninja port. Even in this case it is an
order of magnitude less work to port Ninja than it is to write a Make
backend for Meson.

Just use Ninja, you'll be happier that way. I guarantee it.

## Why is Meson not just a Python module so I could code my build setup in Python?

A related question to this is *Why is Meson's configuration language
not Turing-complete?*

There are many good reasons for this, most of which are summarized on
this web page: [Against The Use Of Programming Languages in
Configuration Files](https://taint.org/2011/02/18/001527a.html).

In addition to those reasons, not exposing Python or any other "real"
programming language makes it possible to port Meson's implementation
to a different language. This might become necessary if, for example,
Python turns out to be a performance bottleneck. This is an actual
problem that has caused complications for GNU Autotools and SCons.

## How do I do the equivalent of Libtools export-symbol and export-regex?

Either by using [GCC symbol
visibility](https://gcc.gnu.org/wiki/Visibility) or by writing a
[linker
script](https://sourceware.org/binutils/docs/ld.html). This
has the added benefit that your symbol definitions are in a standalone
file instead of being buried inside your build definitions. An example
can be found
[here](https://github.com/jpakkane/meson/tree/master/test%20cases/linuxlike/3%20linker%20script).

## My project works fine on Linux and MinGW but fails to link with MSVC due to a missing .lib file (fatal error LNK1181). Why?

With GCC, all symbols on shared libraries are exported automatically
unless you specify otherwise. With MSVC no symbols are exported by
default. If your shared library exports no symbols, MSVC will silently
not produce an import library file leading to failures. The solution
is to add symbol visibility definitions [as specified in GCC
wiki](https://gcc.gnu.org/wiki/Visibility).

## I added some compiler flags and now the build fails with weird errors. What is happening?

You probably did the equivalent to this:

```meson
executable('foobar', ...
           c_args : '-some_arg -other_arg')
```

Meson is *explicit*. In this particular case it will **not**
automatically split your strings at whitespaces, instead it will take
it as is and work extra hard to pass it to the compiler unchanged,
including quoting it properly over shell invocations. This is
mandatory to make e.g. files with spaces in them work flawlessly. To
pass multiple command line arguments, you need to explicitly put them
in an array like this:

```meson
executable('foobar', ...
           c_args : ['-some_arg', '-other_arg'])
```

## Why are changes to default project options ignored?

You probably had a project that looked something like this:

```meson
project('foobar', 'cpp')
```

This defaults to `c++11` on GCC compilers. Suppose you want to use
`c++14` instead, so you change the definition to this:

```meson
project('foobar', 'cpp', default_options : ['cpp_std=c++14'])
```

But when you recompile, it still uses `c++11`. The reason for this is
that default options are only looked at when you are setting up a
build directory for the very first time. After that the setting is
considered to have a value and thus the default value is ignored. To
change an existing build dir to `c++14`, either reconfigure your build
dir with `meson configure` or delete the build dir and recreate it
from scratch.

The reason we don't automatically change the option value when the
default is changed is that it is impossible to know to do that
reliably. The actual question that we need to solve is "if the
option's value is foo and the default value is bar, should we change
the option value to bar also". There are many choices:

 - if the user has changed the value themselves from the default, then
   we must not change it back

 - if the user has not changed the value, but changes the default
   value, then this section's premise would seem to indicate that the
   value should be changed

 - suppose the user changes the value from the default to foo, then
   back to bar and then changes the default value to bar, the correct
   step to take is ambiguous by itself

In order to solve the latter question we would need to remember not
only the current and old value, but also all the times the user has
changed the value and from which value to which other value. Since
people don't remember their own actions that far back, toggling
between states based on long history would be confusing.

Because of this we do the simple and understandable thing: default
values are only defaults and will never affect the value of an option
once set.

## Does wrap download sources behind my back?

It does not. In order for Meson to download anything from the net
while building, two conditions must be met.

First of all there needs to be a `.wrap` file with a download URL in
the `subprojects` directory. If one does not exist, Meson will not
download anything.

The second requirement is that there needs to be an explicit
subproject invocation in your `meson.build` files. Either
`subproject('foobar')` or `dependency('foobar', fallback : ['foobar',
'foo_dep'])`. If these declarations either are not in any build file
or they are not called (due to e.g. `if/else`) then nothing is
downloaded.

If this is not sufficient for you, starting from release 0.40.0 Meson
has a option called `wrap-mode` which can be used to disable wrap
downloads altogether with `--wrap-mode=nodownload`. You can also
disable dependency fallbacks altogether with `--wrap-mode=nofallback`,
which also implies the `nodownload` option.

If on the other hand, you want Meson to always use the fallback
for dependencies, even when an external dependency exists and could
satisfy the version requirements, for example in order to make
sure your project builds when fallbacks are used, you can use
`--wrap-mode=forcefallback` since 0.46.0.

## Why is Meson implemented in Python rather than [programming language X]?

Because build systems are special in ways normal applications aren't.

Perhaps the biggest limitation is that because Meson is used to build
software at the very lowest levels of the OS, it is part of the core
bootstrap for new systems. Whenever support for a new CPU architecture
is added, Meson must run on the system before software using it can be
compiled natively. This requirement adds two hard limitations.

The first one is that Meson must have the minimal amount of
dependencies, because they must all be built during the bootstrap to
get Meson to work.

The second is that Meson must support all CPU architectures, both
existing and future ones. As an example many new programming languages
have only an LLVM based compiler available. LLVM has limited CPU
support compared to, say, GCC, and thus bootstrapping Meson on such
platforms would first require adding new processor support to
LLVM. This is in most cases unfeasible.

A further limitation is that we want developers on as many platforms
as possible to submit to Meson development using the default tools
provided by their operating system. In practice what this means is
that Windows developers should be able to contribute using nothing but
Visual Studio.

At the time of writing (April 2018) there are only three languages
that could fulfill these requirements:

 - C
 - C++
 - Python

Out of these we have chosen Python because it is the best fit for our
needs.

## But I really want a version of Meson that doesn't use python!

Ecosystem diversity is good. We encourage interested users to write this
competing implementation of Meson themselves. As of September 2021, there are 3
projects attempting to do just this:

 - [muon](https://git.sr.ht/~lattis/muon)
 - [Meson++](https://github.com/dcbaker/meson-plus-plus)
 - [boson](https://git.sr.ht/~bl4ckb0ne/boson)

## I have proprietary compiler toolchain X that does not work with Meson, how can I make it work?

Meson needs to know several details about each compiler in order to
compile code with it. These include things such as which compiler
flags to use for each option and how to detect the compiler from its
output. This information cannot be input via a configuration file,
instead it requires changes to Meson's source code that need to be
submitted to Meson master repository. In theory you can run your own
forked version with custom patches, but that's not good use of your
time. Please submit the code upstream so everyone can use the
toolchain.

The steps for adding a new compiler for an existing language are
roughly the following. For simplicity we're going to assume a C
compiler.

- Create a new class with a proper name in
  `mesonbuild/compilers/c.py`. Look at the methods that other
  compilers for the same language have and duplicate what they do.

- If the compiler can only be used for cross compilation, make sure to
  flag it as such (see existing compiler classes for examples).

- Add detection logic to `mesonbuild/environment.py`, look for a
  method called `detect_c_compiler`.

- Run the test suite and fix issues until the tests pass.

- Submit a pull request, add the result of the test suite to your MR
  (linking an existing page is fine).

- If the compiler is freely available, consider adding it to the CI
  system.

## Why does building my project with MSVC output static libraries called `libfoo.a`?

The naming convention for static libraries on Windows is usually
`foo.lib`.  Unfortunately, import libraries are also called `foo.lib`.

This causes filename collisions with the default library type where we
build both shared and static libraries, and also causes collisions
during installation since all libraries are installed to the same
directory by default.

To resolve this, we decided to default to creating static libraries of
the form `libfoo.a` when building with MSVC. This has the following
advantages:

1. Filename collisions are completely avoided.
1. The format for MSVC static libraries is `ar`, which is the same as the GNU
   static library format, so using this extension is semantically correct.
1. The static library filename format is now the same on all platforms and with
   all toolchains.
1. Both Clang and GNU compilers can search for `libfoo.a` when specifying
   a library as `-lfoo`. This does not work for alternative naming schemes for
   static libraries such as `libfoo.lib`.
1. Since `-lfoo` works out of the box, pkgconfig files will work correctly for
   projects built with both MSVC, GCC, and Clang on Windows.
1. MSVC does not have arguments to search for library filenames, and [it does
   not care what the extension is](https://docs.microsoft.com/en-us/cpp/build/reference/link-input-files?view=vs-2019),
   so specifying `libfoo.a` instead of `foo.lib` does not change the workflow,
   and is an improvement since it's less ambiguous.
1. Projects built with the MinGW compiler are fully compatible with
   MSVC as long as they use the same CRT (e.g. UCRT with MSYS2).
   These projects also name their static libraries `libfoo.a`.

If, for some reason, you really need your project to output static
libraries of the form `foo.lib` when building with MSVC, you can set
the
[`name_prefix:`](https://mesonbuild.com/Reference-manual.html#library)
kwarg to `''` and the
[`name_suffix:`](https://mesonbuild.com/Reference-manual.html#library)
kwarg to `'lib'`. To get the default behaviour for each, you can
either not specify the kwarg, or pass `[]` (an empty array) to it.

## Do I need to add my headers to the sources list like in Autotools?

Autotools requires you to add private and public headers to the sources list so
that it knows what files to include in the tarball generated by `make dist`.
Meson's `dist` command simply gathers everything committed to your git/hg
repository and adds it to the tarball, so adding headers to the sources list is
pointless.

Meson uses Ninja which uses compiler dependency information to automatically
figure out dependencies between C sources and headers, so it will rebuild
things correctly when a header changes.

The only exception to this are generated headers, for which you must [declare
dependencies correctly](#how-do-i-tell-meson-that-my-sources-use-generated-headers).

If, for whatever reason, you do add non-generated headers to the sources list
of a target, Meson will simply ignore them.

## How do I tell Meson that my sources use generated headers?

Let's say you use a [`custom_target()`](https://mesonbuild.com/Reference-manual.html#custom_target)
to generate the headers, and then `#include` them in your C code. Here's how
you ensure that Meson generates the headers before trying to compile any
sources in the build target:

```meson
libfoo_gen_headers = custom_target('gen-headers', ..., output: 'foo-gen.h')
libfoo_sources = files('foo-utils.c', 'foo-lib.c')
# Add generated headers to the list of sources for the build target
libfoo = library('foo', sources: [libfoo_sources + libfoo_gen_headers])
```

Now let's say you have a new target that links to `libfoo`:

```meson
libbar_sources = files('bar-lib.c')
libbar = library('bar', sources: libbar_sources, link_with: libfoo)
```

This adds a **link-time** dependency between the two targets, but note that the
sources of the targets have **no compile-time** dependencies and can be built
in any order; which improves parallelism and speeds up builds.

If the sources in `libbar` *also* use `foo-gen.h`, that's a *compile-time*
dependency, and you'll have to add `libfoo_gen_headers` to `sources:` for
`libbar` too:

```meson
libbar_sources = files('bar-lib.c')
libbar = library('bar', sources: libbar_sources + libfoo_gen_headers, link_with: libfoo)
```

Alternatively, if you have multiple libraries with sources that link to
a library and also use its generated headers, this code is equivalent to above:

```meson
# Add generated headers to the list of sources for the build target
libfoo = library('foo', sources: libfoo_sources + libfoo_gen_headers)

# Declare a dependency that will add the generated headers to sources
libfoo_dep = declare_dependency(link_with: libfoo, sources: libfoo_gen_headers)

...

libbar = library('bar', sources: libbar_sources, dependencies: libfoo_dep)
```

**Note:** You should only add *headers* to `sources:` while declaring
a dependency. If your custom target outputs both sources and headers, you can
use the subscript notation to get only the header(s):

```meson
libfoo_gen_sources = custom_target('gen-headers', ..., output: ['foo-gen.h', 'foo-gen.c'])
libfoo_gen_headers = libfoo_gen_sources[0]

# Add static and generated sources to the target
libfoo = library('foo', sources: libfoo_sources + libfoo_gen_sources)

# Declare a dependency that will add the generated *headers* to sources
libfoo_dep = declare_dependency(link_with: libfoo, sources: libfoo_gen_headers)
...
libbar = library('bar', sources: libbar_sources, dependencies: libfoo_dep)
```

A good example of a generator that outputs both sources and headers is
[`gnome.mkenums()`](https://mesonbuild.com/Gnome-module.html#gnomemkenums).

## How do I disable exceptions and RTTI in my C++ project?

With the `cpp_eh` and `cpp_rtti` options. A typical invocation would
look like this:

```
meson -Dcpp_eh=none -Dcpp_rtti=false <other options>
```

The RTTI option is only available since Meson version 0.53.0.

## Should I check for `buildtype` or individual options like `debug` in my build files?

This depends highly on what you actually need to happen. The
´buildtype` option is meant do describe the current build's
_intent_. That is, what it will be used for. Individual options are
for determining what the exact state is. This becomes clearer with a
few examples.

Suppose you have a source file that is known to miscompile when using
`-O3` and requires a workaround. Then you'd write something like this:

```meson
if get_option('optimization') == '3'
    add_project_arguments('-DOPTIMIZATION_WORKAROUND', ...)
endif
```

On the other hand if your project has extra logging and sanity checks
that you would like to be enabled during the day to day development
work (which uses the `debug` buildtype), you'd do this instead:

```meson
if get_option('buildtype') == 'debug'
    add_project_arguments('-DENABLE_EXTRA_CHECKS', ...)
endif
```

In this way the extra options are automatically used during
development but are not compiled in release builds. Note that (since
Meson 0.57.0) you can set optimization to, say, 2 in your debug builds
if you want to. If you tried to set this flag based on optimization
level, it would fail in this case.

## How do I use a library before declaring it?

This is valid (and good) code:
```
libA = library('libA', 'fileA.cpp', link_with : [])
libB = library('libB', 'fileB.cpp', link_with : [libA])
```
But there is currently no way to get something like this to work:
```
libB = library('libB', 'fileB.cpp', link_with : [libA])
libA = library('libA', 'fileA.cpp', link_with : [])
```
This means that you HAVE to write your `library(...)` calls in the order that the
dependencies flow. While ideas to make arbitrary orders possible exist, they were
rejected because reordering the `library(...)` calls was considered the "proper"
way. See [here](https://github.com/mesonbuild/meson/issues/8178) for the discussion.

## Why doesn't meson have user defined functions/macros?

The tl;dr answer to this is that meson's design is focused on solving specific
problems rather than providing a general purpose language to write complex
code solutions in build files. Build systems should be quick to write and
quick to understand, functions muddle this simplicity.

The long answer is twofold:

First, Meson aims to provide a rich set of tools that solve specific problems
simply out of the box. This is similar to the "batteries included" mentality of
Python. By providing tools that solve common problems in the simplest way
possible *in* Meson we are solving that problem for everyone instead of forcing
everyone to solve that problem for themselves over and over again, often
badly. One example of this are Meson's dependency wrappers around various
config-tool executables (sdl-config, llvm-config, etc). In other build
systems each user of that dependency writes a wrapper and deals with the
corner cases (or doesn't, as is often the case), in Meson we handle them
internally, everyone gets fixes and the corner cases are ironed out for
*everyone*. Providing user defined functions or macros goes directly against
this design goal.

Second, functions and macros makes the build system more difficult to reason
about. When you encounter some function call, you can refer to the reference
manual to see that function and its signature. Instead of spending
frustrating hours trying to interpret some bit of m4 or follow long include
paths to figure out what `function1` (which calls `function2`, which calls
`function3`, ad infinitum), you know what the build system is doing. Unless
you're actively developing Meson itself, it's just a tool to orchestrate
building the thing you actually care about. We want you to spend as little
time worrying about build systems as possible so you can spend more time on
*your code*.

Many times user defined functions are used due to a lack of loops or
because loops are tedious to use in the language. Meson has both arrays/lists
and hashes/dicts natively. Compare the following pseudo code:

```meson
func(name, sources, extra_args)
  executable(
    name,
    sources,
    c_args : extra_args
  )
endfunc

func(exe1, ['1.c', 'common.c'], [])
func(exe2, ['2.c', 'common.c'], [])
func(exe2_a, ['2.c', 'common.c'], ['-arg'])
```

```meson
foreach e : [['1', '1.c', []],
             ['2', '2.c', []],
             ['2', '2.c', ['-arg']]]
  executable(
    'exe' + e[0],
    e[1],
    c_args : e[2],
  )
endforeach
```

The loop is both less code and is much easier to reason about than the function
version is, especially if the function were to live in a separate file, as is
common in other popular build systems.

Build system DSLs also tend to be badly thought out as generic programming
languages, Meson tries to make it easy to use external scripts or programs
for handling complex problems. While one can't always convert build logic
into a scripting language (or compiled language), when it can be done this is
often a better solution. External languages tend to be well-thought-out and
tested, generally don't regress, and users are more likely to have domain
knowledge about them. They also tend to have better tooling (such as
autocompletion, linting, testing solutions), which make them a lower
maintenance burden over time.

## Why don't the arguments passed to `add_project_link_arguments` affect anything?

Given code like this:
```meson
add_project_link_arguments(['-Wl,-foo'], language : ['c'])
executable(
  'main',
  'main.c',
  'helper.cpp',
)
```

One might be surprised to find that `-Wl,-foo` is *not* applied to the linkage
of the `main` executable. In this Meson is working as expected, since meson will
attempt to determine the correct linker to use automatically. This avoids
situations like in autotools where dummy C++ sources have to be added to some
compilation targets to get correct linkage. So in the above case the C++ linker
is used, instead of the C linker, as `helper.cpp` likely cannot be linked using
the C linker.

Generally the best way to resolve this is to add the `cpp` language to the
`add_project_link_arguments` call.
```meson
add_project_link_arguments(['-Wl,-foo'], language : ['c', 'cpp'])
executable(
  'main',
  'main.c',
  'helper.cpp',
)
```

To force the use of the C linker anyway the `link_language` keyword argument can
be used. Note that this can result in a compilation failure if there are symbols
that the C linker cannot resolve.
```meson
add_project_link_arguments(['-Wl,-foo'], language : ['c'])
executable(
  'main',
  'main.c',
  'helper.cpp',
  link_language : 'c',
)
```

## How do I ignore the build directory in my VCS?

You don't need to, assuming you use git or mercurial! Meson >=0.57.0 will
create a `.gitignore` and `.hgignore` file for you, inside each build
directory. It glob ignores ```"*"```, since all generated files should not be
checked into git.

Users of older versions of Meson may need to set up ignore files themselves.
