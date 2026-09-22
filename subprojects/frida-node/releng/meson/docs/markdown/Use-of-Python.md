# Use of Python

Meson is implemented in Python. This has both positive and negative
sides. The main thing people seem to be mindful about is the
dependency on Python to build source code. This page discusses various
aspects of this problem.

# Dependency hell

There have been many Python programs that are difficult to maintain on
multiple platforms. The reasons come mostly from dependencies. The
program may use dependencies that are hard to compile on certain
platforms, are outdated, conflict with other dependencies, not
available on a given Python version and so on.

Meson avoids dependency problems with one simple rule: Meson is not
allowed to have any dependencies outside the Python basic library. The
only thing you need is Python 3 (and possibly Ninja).

## Reimplementability

Meson has been designed in such a way that the implementation language
is never exposed in the build definitions. This makes it possible (and
maybe even easy) to reimplement Meson in any other programming
language. There are currently no plans to reimplement Meson, but we
will make sure that Python is not exposed inside the build
definitions.

## Cross platform tooling

There is no one technical solution or programming language that works
natively on all operating systems currently in use. When Autotools was
designed in the late 80s, Unix shell was available pretty much
anywhere. This is no longer the case.

It is also the case that as any project gets larger, sooner or later
it requires code generation, scripting or other tooling. This seems to
be inevitable. Because there is no scripting language that would be
available everywhere, these tools either need to be rewritten for each
platform (which is a lot of work and is prone to errors) or the
project needs to take a dependency on _something_.

Any project that uses Meson (at least the current version) can rely on
the fact that Python 3 will always be available, because you can't
compile the project without it. All tooling can then be done in Python
3 with the knowledge that it will run on any platform without any
extra dependencies (modulo the usual porting work). This reduces
maintenance effort on multiplatform projects by a fair margin.
