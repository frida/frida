---
short-description: Creating universal Linux binaries
...

# Creating Linux binaries

Creating Linux binaries that can be downloaded and run on any distro
(like .dmg packages for OSX or .exe installers for Windows) has
traditionally been difficult. This is even more tricky if you want to
use modern compilers and features, which is especially desired in game
development. There is still no simple turn-key solution for this
problem but with a bit of setup it can be relatively straightforward.

## Installing system and GCC

First you need to do a fresh operating system install. You can use
spare hardware, VirtualBox, cloud or whatever you want. Note that the
distro you install must be *at least as old* as the oldest release you
wish to support. Debian stable is usually a good choice, though
immediately after its release you might want to use Debian oldstable
or the previous Ubuntu LTS. The oldest supported version of CentOS is
also a good choice.

Once you have installed the system, you need to install
build-dependencies for GCC. In Debian-based distros this can be done
with the following commands:

```console
$ apt-get build-dep g++
$ apt-get install pkg-config libgmp-dev libmpfr-dev libmpc-dev
```

Then create a `src` subdirectory in your home directory. Copy-paste
the following into `install_gcc.sh` and execute it.

```bash
#!/bin/sh

wget ftp://ftp.fu-berlin.de/unix/languages/gcc/releases/gcc-4.9.2/gcc-4.9.2.tar.bz2
tar xf gcc-4.9.2.tar.bz2

mkdir objdir
cd objdir
../gcc-4.9.2/configure --disable-bootstrap --prefix=${HOME}/devroot \
                       --disable-multilib --enable-languages=c,c++
make -j 4
make install-strip
ln -s gcc ${HOME}/devroot/bin/cc
```

Then finally add the following lines to your `.bashrc`.

```console
$ export LD_LIBRARY_PATH=${HOME}/devroot/lib
$ export PATH=${HOME}/devroot/bin:$PATH
$ export PKG_CONFIG_PATH=${HOME}/devroot/lib/pkgconfig
```

Log out and back in and now your build environment is ready to use.

## Adding other tools

Old distros might have too old versions of some tools. For Meson this
could include Python 3 and Ninja. If this is the case you need to
download, build and install new versions into `~/devroot` in the usual
way.

## Adding dependencies

You want to embed and statically link every dependency you can
(especially C++ dependencies). Meson's [Wrap package
manager](Wrap-dependency-system-manual.md) might be of use here. This
is equivalent to what you would do on Windows, OSX, Android etc.
Sometimes static linking is not possible. In these cases you need to
copy the .so files inside your package. Let's use SDL2 as an example.
First we download and install it as usual giving it our custom install
prefix (that is, `./configure --prefix=${HOME}/devroot`). This makes
Meson's dependency detector pick it up automatically.

## Building and installing

Building happens in much the same way as normally. There are just two
things to note. First, you must tell GCC to link the C++ standard
library statically. If you don't then your app is guaranteed to break
as different distros have binary-incompatible C++ libraries. The
second thing is that you need to point your install prefix to some
empty staging area. Here's the Meson command to do that:

```console
$ LDFLAGS=-static-libstdc++ meson --prefix=/tmp/myapp <other args>
```

The aim is to put the executable in `/tmp/myapp/bin` and shared
libraries to `/tmp/myapp/lib`. The next thing you need is the
embedder. It takes your dependencies (in this case only
`libSDL2-2.0.so.0`) and copies them in the lib directory. Depending on
your use case you can either copy the files by hand or write a script
that parses the output of `ldd binary_file`. Be sure not to copy
system libraries (`libc`, `libpthread`, `libm` etc). For an example,
see the [sample
project](https://github.com/jpakkane/meson/tree/master/manual%20tests/4%20standalone%20binaries).

Make the script run during install with this:

```meson
[[#meson.add_install_script]]('linux_bundler.sh')
```

## Final steps

If you try to run the program now it will most likely fail to start or
crashes. The reason for this is that the system does not know that the
executable needs libraries from the `lib` directory. The solution for
this is a simple wrapper script. Create a script called `myapp.sh`
with the following content:

```bash
#!/bin/bash

cd "${0%/*}"
export LD_LIBRARY_PATH="$(pwd)/lib"
bin/myapp
```

Install it with this Meson snippet:

```meson
[[#install_data]]('myapp.sh', install_dir : '.')
```

And now you are done. Zip up your `/tmp/myapp` directory and you have
a working binary ready for deployment. To run the program, just unzip
the file and run `myapp.sh`.
