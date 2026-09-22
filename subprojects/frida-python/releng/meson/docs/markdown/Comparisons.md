---
title: Comparisons
...

# Comparing Meson with other build systems

A common question is *Why should I choose Meson over a different build
system X?* There is no one true answer to this as it depends on the
use case. Almost all build systems have all the functionality needed
to build medium-to-large projects so the decision is usually made on
other points. Here we list some pros and cons of various build systems
to help you do the decision yourself.

## GNU Autotools

### Pros

Excellent support for legacy Unix platforms, large selection of
existing modules.

### Cons

Needlessly slow, complicated, hard to use correctly, unreliable,
painful to debug, incomprehensible for most people, poor support for
non-Unix platforms (especially Windows).

## CMake

### Pros

Great support for multiple backends (Visual Studio, XCode, etc).

### Cons

The scripting language is cumbersome to work with. Some simple things
are more complicated than necessary.

## SCons

### Pros

Full power of Python available for defining your build.

### Cons

Slow. Requires you to pass your configuration settings on every
invocation. That is, if you do `scons OPT1 OPT2` and then just
`scons`, it will reconfigure everything without settings `OPT1` and
`OPT2`. Every other build system remembers build options from the
previous invocation.

## Bazel

### Pros

Proven to scale to very large projects.

### Cons

Implemented in Java. Poor Windows support. Heavily focused on Google's
way of doing things (which may be a good or a bad thing). Contributing
code requires [signing a CLA](https://bazel.build/contributing.html).

## Meson

### Pros

The fastest build system [see
measurements](Performance-comparison.md), user friendly, designed to
be as invisible to the developer as possible, native support for
modern tools (precompiled headers, coverage, Valgrind etc). Not Turing
complete so build definition files are easy to read and understand.

### Cons

Relatively new so it does not have a large user base yet, and may thus
contain some unknown bugs. Visual Studio and XCode backends not as
high quality as Ninja one.
