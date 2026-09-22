---
render-subpages: false
...

# The Meson Build system

## Overview

Meson is an open source build system meant to be both extremely fast,
and, even more importantly, as user friendly as possible.

The main design point of Meson is that every moment a developer spends
writing or debugging build definitions is a second wasted. So is every
second spent waiting for the build system to actually start compiling
code.

## Features

*   multiplatform support for Linux, macOS, Windows, GCC, Clang, Visual Studio and others
*   supported languages include C, C++, D, Fortran, Java, Rust
*   build definitions in a very readable and user friendly non-Turing complete DSL
*   cross compilation for many operating systems as well as bare metal
*   optimized for extremely fast full and incremental builds without sacrificing correctness
*   built-in multiplatform dependency provider that works together with distro packages
*   fun!

## Quickstart for beginners

Are you an absolute beginner when it comes to programming? No worries,
read [this beginner guide](SimpleStart.md) to get started.

## Community

The easiest way for most people to connect to other Meson developers is
a web chat. The channel to use is `#mesonbuild` either via Matrix ([web
interface](https://app.element.io/#/room/#mesonbuild:matrix.org)) or
[OFTC IRC](https://www.oftc.net/).

Other methods of communication include the [mailing
list](https://groups.google.com/forum/#!forum/mesonbuild) (hosted by
Google Groups) and the
[Discussions](https://github.com/mesonbuild/meson/discussions) section
of the Meson GitHub repository.

### [Projects using Meson](Users.md)

Many projects are using Meson and they're
a great resource for learning what to (and what not to!) do when
converting existing projects to Meson.

[A short list of Meson users can be found here](Users.md)
but there are many more. We would love to hear about your success
stories too and how things could be improved too!

## Development

All development on Meson is done on the [GitHub
project](https://github.com/mesonbuild/meson). Instructions for
contributing can be found on the [contribution page](Contributing.md).


You do not need to sign a CLA to contribute to Meson.
