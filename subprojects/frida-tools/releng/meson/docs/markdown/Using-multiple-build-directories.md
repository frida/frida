# Using multiple build directories

One of the main design goals of Meson has been to build all projects
out-of-source. This means that *all* files generated during the build
are placed in a separate subdirectory. This goes against common Unix
tradition where you build your projects in-source. Building out of
source gives two major advantages.

First of all this makes for very simple VCS "ignore" files. In
classical build systems you may need to have tens of lines of
definitions, most of which contain wildcards. When doing out of source
builds all of this busywork goes away. A common ignore file for a
Meson project only contains a few lines that are the build directory
and IDE project files. (Note that since Meson 0.57.0, build directories
are automatically ignored for git and mercurial by generating an ignore
file inside the build directory.)

Secondly this makes it very easy to clean your projects: just delete
the build subdirectory and you are done. There is no need to guess
whether you need to run `make clean`, `make distclean`, `make
mrproper` or something else. When you delete a build subdirectory
there is no possible way to have any lingering state from your old
builds.

The true benefit comes from somewhere else, though.

## Multiple build directories for the same source tree

Since a build directory is fully self contained and treats the source
tree as a read-only piece of data, it follows that you can have
arbitrarily many build trees for any source tree at the same time.
Since all build trees can have different configuration, this is
extremely powerful. Now you might be wondering why one would want to
have multiple build setups at the same time. Let's examine this by
setting up a hypothetical project.

The first thing to do is to set up the default build, that is, the one
we are going to use over 90% of the time. In this we use the system
compiler and build with debug enabled and no optimizations so it
builds as fast as possible. This is the default project type for
Meson, so setting it up is simple.

    mkdir builddir
    meson setup builddir

Another common setup is to build with debug and optimizations to, for
example, run performance tests. Setting this up is just as simple.

    mkdir buildopt
    meson --buildtype=debugoptimized buildopt

For systems where the default compiler is GCC, we would like to
compile with Clang, too. So let's do that.

    mkdir buildclang
    CC=clang CXX=clang++ meson setup buildclang

You can add cross builds, too. As an example, let's set up a Linux ->
Windows cross compilation build using MinGW.

    mkdir buildwine
    meson --cross-file=mingw-cross.txt buildwine

The cross compilation file sets up Wine so that not only can you
compile your application, you can also run the unit test suite just by
issuing the command `meson test`.

To compile any of these build types, just cd into the corresponding
build directory and run `meson compile` or instruct your IDE to do the
same. Note that once you have set up your build directory once, you
can just run Ninja and Meson will ensure that the resulting build is
fully up to date according to the source. Even if you have not touched
one of the directories in weeks and have done major changes to your
build configuration, Meson will detect this and bring the build
directory up to date (or print an error if it can't do that). This
allows you to do most of your work in the default directory and use
the others every now and then without having to babysit your build
directories.

## Specialized uses

Separate build directories allows easy integration for various
different kinds of tools. As an example, Clang comes with a static
analyzer. It is meant to be run from scratch on a given source tree.
The steps to run it with Meson are very simple.

    rm -rf buildscan
    mkdir buildscan
    scan-build meson setup buildscan
    cd buildscan
    scan-build ninja

These commands are the same for every single Meson project, so they
could even be put in a script turning static analysis into a single
command.
