# Wrap best practices and tips

There are several things you need to take into consideration when
writing a Meson build definition for a project. This is especially
true when the project will be used as a subproject. This page lists a
few things to consider when writing your definitions.

## Do not put config.h in external search path

Many projects use a `config.h` header file that they use for
configuring their project internally. These files are never installed
to the system header files so there are no inclusion collisions. This
is not the case with subprojects, your project tree may have an
arbitrary number of configuration files, so we need to ensure they
don't clash.

The basic problem is that the users of the subproject must be able to
include subproject headers without seeing its `config.h` file. The
most correct solution is to rename the `config.h` file into something
unique, such as `foobar-config.h`. This is usually not feasible unless
you are the maintainer of the subproject in question.

The pragmatic solution is to put the config header in a directory that
has no other header files and then hide that from everyone else. One
way is to create a top level subdirectory called `internal` and use
that to build your own sources, like this:

```meson
subdir('internal') # create config.h in this subdir
internal_inc = include_directories('internal')
shared_library('foo', 'foo.c', include_directories : internal_inc)
```

Many projects keep their `config.h` in the top level directory that
has no other source files in it. In that case you don't need to move
it but can just do this instead:

```meson
internal_inc = include_directories('.') # At top level meson.build
```

## Make libraries buildable both as static and shared

Some platforms (e.g. iOS) requires linking everything in your main app
statically. In other cases you might want shared libraries. They are
also faster during development due to Meson's relinking optimization.
However building both library types on all builds is slow and
wasteful.

Your project should use the `library` method that can be toggled
between shared and static with the `default_library` builtin option.


```meson
mylib = library('foo', 'foo.c')
```

## Declare generated headers explicitly

Meson's Ninja backend works differently from Make and other
systems. Rather than processing things directory per directory, it
looks at the entire build definition at once and runs the individual
compile jobs in what might look to the outside as a random order.

The reason for this is that this is much more efficient so your builds
finish faster. The downside is that you have to be careful with your
dependencies. The most common problem here is headers that are
generated at compile time with e.g. code generators. If these headers
are needed when building code that uses these libraries, the compile
job might be run before the code generation step. The fix is to make
the dependency explicit like this:

```meson
myheader = custom_target(...)
mylibrary = shared_library(...)
mydep = declare_dependency(link_with : mylibrary,
  include_directories : include_directories(...),
  sources : myheader)
```

And then you can use the dependency in the usual way:

```meson
executable('dep_using_exe', 'main.c',
  dependencies : mydep)
```

Meson will ensure that the header file has been built before compiling `main.c`.

## Avoid exposing compilable source files in declare_dependency

The main use for the `sources` argument in `declare_dependency` is to
construct the correct dependency graph for the backends, as
demonstrated in the previous section. It is extremely important to
note that it should *not* be used to directly expose compilable
sources (`.c`, `.cpp`, etc.) of dependencies, and should rather only
be used for header/config files. The following example will illustrate
what can go wrong if you accidentally expose compilable source files.

So you've read about unity builds and how Meson natively supports
them. You decide to expose the sources of dependencies in order to
have unity builds that include their dependencies. For your support
library you do

```meson
my_support_sources = files(...)

mysupportlib = shared_library(
  ...
  sources : my_support_sources,
  ...)
mysupportlib_dep = declare_dependency(
  ...
  link_with : mylibrary,
  sources : my_support_sources,
  ...)
```

And for your main project you do:

```meson
mylibrary = shared_library(
  ...
  dependencies : mysupportlib_dep,
  ...)
myexe = executable(
  ...
  link_with : mylibrary,
  dependencies : mysupportlib_dep,
  ...)
```

This is extremely dangerous. When building, `mylibrary` will build and
link the support sources `my_support_sources` into the resulting
shared library. Then, for `myexe`, these same support sources will be
compiled again, will be linked into the resulting executable, in
addition to them being already present in `mylibrary`. This can
quickly run afoul of the [One Definition Rule
(ODR)](https://en.wikipedia.org/wiki/One_Definition_Rule) in C++, as
you have more than one definition of a symbol, yielding undefined
behavior. While C does not have a strict ODR rule, there is no
language in the standard which guarantees such behavior to work.
Violations of the ODR can lead to weird idiosyncratic failures such as
segfaults. In the overwhelming number of cases, exposing library
sources via the `sources` argument in `declare_dependency` is thus
incorrect. If you wish to get full cross-library performance, consider
building `mysupportlib` as a static library instead and employing LTO.

There are exceptions to this rule. If there are some natural
constraints on how your library is to be used, you can expose sources.
For instance, the WrapDB module for GoogleTest directly exposes the
sources of GTest and GMock. This is valid, as GTest and GMock will
only ever be used in *terminal* link targets. A terminal target is the
final target in a dependency link chain, for instance `myexe` in the
last example, whereas `mylibrary` is an intermediate link target. For
most libraries this rule is not applicable though, as you cannot in
general control how others consume your library, and as such should
not expose sources.
