# Meson's policy on mixing multiple build systems in one build directory

Meson has been designed with the principle that all dependencies are
either provided by "the platform" via a mechanism such as Pkg-Config
or that they are built as Meson subprojects under the main project.
There are several projects that would like to mix build systems, that
is, build dependencies in the same build directory as the other build
system by having one build system call the other. The build
directories do not necessarily need to be inside each other, but that
is the common case.

This page lists the Meson project's stance on mixing build systems.
The tl/dr version is that while we do provide some functionality for
this use case, it only works for simple cases. Anything more complex
cannot be made reliable and trying to do that would burden Meson
developers with an effectively infinite maintenance burden. Thus these
use cases are not guaranteed to work, and even if a project using them
works today there are no guarantees that it will work in any future
version.

## The definition of "build system mixing"

For the purposes of this page, mixing build systems means any and all
mechanisms where one build system uses build artifacts from a
different build system's build directory in any way.

Note that this definition does not specify what the dependencies are
and how they are built, only how they are consumed. For example
suppose you have a standalone dependency library that builds with
build system X. In this case having Meson call the build system to
build the dependency at build time would be interpreted as mixing
build systems. On the other hand a "Flatpak-like" approach of building
and installing the library with an external mechanism and consuming it
via a standard build-system agnostic method such as Pkg-Config would
not be considered build system mixing. Use of uninstalled-pkgconfig
files is considered mixing, though.

## What does this mean for support and compatibility?

The Meson project will not take on any maintenance burden to ensure
anything other than the simple builds setups as discussed above will
work. Nor will we make changes to support these use cases that would
worsen the user experience of users of plain Meson. This includes, but
is not limited to, the following:

- Any changes in other build systems that cause mixed project breakage
  will not be considered a bug in Meson.

- Breakages in mixed build projects will not be considered regressions
  and such problems will never be considered release blockers,
  regardless of what the underlying issue is.

- Any use case that would require major changes in Meson to work
  around missing or broken functionality in the other build system is
  not supported. These issues must be fixed upstream.
