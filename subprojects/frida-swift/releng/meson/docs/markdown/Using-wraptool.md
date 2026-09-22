# Using wraptool

Wraptool is a subcommand of Meson that allows you to manage your
source dependencies using the WrapDB database. It gives you all things
you would expect, such as installing and updating dependencies. The
wrap tool works on all platforms, the only limitation is that the wrap
definition works on your target platform. If you find some Wraps that
don't work, please file bugs or, even better, patches.

All code examples here assume that you are running the commands in
your top level source directory. Lines that start with the `$` mark
are commands to type.

## Simple querying

The simplest operation to do is to query the list of packages
available. To list them all issue the following command:

    $ meson wrap list
    box2d
    enet
    gtest
    libjpeg
    liblzma
    libpng
    libxml2
    lua
    ogg
    sqlite
    vorbis
    zlib

Usually you want to search for a specific package. This can be done
with the `search` command:

    $ meson wrap search jpeg
    libjpeg

If a package is not found in the list of wraps, the `search` command
will look in all the wrap dependencies:

    $ meson wrap search glib-2.0
    Dependency glib-2.0 found in wrap glib

To determine which versions of libjpeg are available to install, issue
the `info` command:

    $ meson wrap info libjpeg
    Available versions of libjpeg:
      9a 2

The first number is the upstream release version, in this case
`9a`. The second number is the Wrap revision number. They don't relate
to anything in particular, but larger numbers imply newer
releases. You should always use the newest available release.

## Installing dependencies

Installing dependencies is just as straightforward. First just create
the `subprojects` directory at the top of your source tree and issue
the install command.

    $ meson wrap install libjpeg
    Installed libjpeg branch 9a revision 2

Now you can issue a `subproject('libjpeg')` in your `meson.build` file
to use it.

To check if your projects are up to date you can issue the `status` command.

    $ meson wrap status
    Subproject status
     libjpeg up to date. Branch 9a, revision 2.
     zlib not up to date. Have 1.2.8 2, but 1.2.8 4 is available.

In this case `zlib` has a newer release available. Updating it is
straightforward:

    $ meson wrap update zlib
    Updated zlib to branch 1.2.8 revision 4

Wraptool can do other things besides these. Documentation for these
can be found in the command line help, which can be accessed by
`meson wrap --help`.

## Automatic dependency fallback

Since *0.64.0* Meson can use WrapDB to automatically find missing dependencies.

The user simply needs to download latest database, the following command stores
it in `subprojects/wrapdb.json`:
    $ meson wrap update-db

Once the database is available locally, any dependency not found on the system
but available in WrapDB will automatically be downloaded.

Automatic fetch of WrapDB subprojects can be disabled by removing the file
`subprojects/wrapdb.json`, or by using `--wrap-mode=nodownload`.
