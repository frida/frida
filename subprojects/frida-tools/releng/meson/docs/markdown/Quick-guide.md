---
title: Quickstart Guide
short-description: Getting Started using Mesonbuild
...

# Using Meson

Meson has been designed to be as simple to use as possible. This page
outlines the initial steps needed for installation, troubleshooting,
and standard use.

For more advanced configuration please refer to the command line help
`meson --help` or the Meson documentation located at the
[Mesonbuild](https://mesonbuild.com) website.

Table of Contents:
* [Requirements](#requirements)
* [Installation using package manager](#installation-using-package-manager)
* [Installation using Python](#installation-using-python)
* [Installation from source](#installation-from-source)
* [Troubleshooting](#troubleshooting)
* [Compiling a Meson project](#compiling-a-meson-project)
* [Using Meson as a distro packager](#using-meson-as-a-distro-packager)

Requirements
--

* [Python 3](https://python.org)
* [Ninja](https://github.com/ninja-build/ninja/)

*Ninja is only needed if you use the Ninja backend. Meson can also
generate native VS and Xcode project files.*


Installation using package manager
--

Ubuntu:

```console
$ sudo apt-get install python3 python3-pip python3-setuptools \
                       python3-wheel ninja-build
```
*Due to our frequent release cycle and development speed, distro packaged software may quickly become outdated.*

Installation using Python
--
Requirements: **pip3**

The best way to receive the most up-to-date version of Mesonbuild.

Install as a local user (recommended):
```console
$ pip3 install --user meson
```
Install as root:
```console
# pip3 install meson
```

*If you are unsure whether to install as root or a local user, install
 as a local user.*


Installation from source
--
Requirements: **git**

Meson can be run directly from the cloned git repository.

```console
$ git clone https://github.com/mesonbuild/meson.git /path/to/sourcedir
```
Troubleshooting:
--
Common Issues:
```console
$ meson setup builddir
$ bash: /usr/bin/meson: No such file or directory
```

Description: The default installation prefix for the python pip module
installation is not included in your shell environment PATH. The
default prefix for python pip installation modules is located under
``/usr/local``.

**Resolution:
This issue can be resolved by altering the default shell environment
PATH to include ``/usr/local/bin``. **

*Note: There are other ways of fixing this issue such as using
 symlinks or copying the binaries to a default path and these methods
 are not recommended or supported as they may break package management
 interoperability.*


Compiling a Meson project
--

The most common use case of Meson is compiling code on a code base you
are working on. The steps to take are very simple.

```console
$ cd /path/to/source/root
$ meson setup builddir && cd builddir
$ meson compile
$ meson test
```

The only thing to note is that you need to create a separate build
directory. Meson will not allow you to build source code inside your
source tree. All build artifacts are stored in the build directory.
This allows you to have multiple build trees with different
configurations at the same time. This way generated files are not
added into revision control by accident.

To recompile after code changes, just type `meson compile`. The build
command is always the same. You can do arbitrary changes to source
code and build system files and Meson will detect those and will do
the right thing. If you want to build optimized binaries, just use the
argument `--buildtype=debugoptimized` when running Meson. It is
recommended that you keep one build directory for unoptimized builds
and one for optimized ones. To compile any given configuration, just
go into the corresponding build directory and run `meson compile`.

Meson will automatically add compiler flags to enable debug
information and compiler warnings (i.e. `-g` and `-Wall`). This means
the user does not have to deal with them and can instead focus on
coding.

Using Meson as a distro packager
--

Distro packagers usually want total control on the build flags
used. Meson supports this use case natively. The commands needed to
build and install Meson projects are the following.

```console
$ cd /path/to/source/root
$ meson --prefix /usr --buildtype=plain builddir -Dc_args=... -Dcpp_args=... -Dc_link_args=... -Dcpp_link_args=...
$ meson compile -C builddir
$ meson test -C builddir
$ DESTDIR=/path/to/staging/root meson install -C builddir
```

The command line switch `--buildtype=plain` tells Meson not to add its
own flags to the command line. This gives the packager total control
on used flags.

This is very similar to other build systems. The only difference is
that the `DESTDIR` variable is passed as an environment variable
rather than as an argument to `meson install`.

As distro builds happen always from scratch, you might consider
enabling [unity builds](Unity-builds.md) on your packages because they
are faster and produce better code. However there are many projects
that do not build with unity builds enabled so the decision to use
unity builds must be done by the packager on a case by case basis.
