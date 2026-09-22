---
short-description: Setting up native compilation
...

# Persistent native environments

New in 0.49.0

Meson has [cross files for describing cross compilation
environments](Cross-compilation.md), for describing native
environments it has equivalent "native files".

Natives describe the *build machine*, and can be used to override
properties of non-cross builds, as well as properties that are marked
as "native" in a cross build.

There are a couple of reasons you might want to use a native file to
keep a persistent environment:

* To build with a non-default native tool chain (such as clang instead of gcc)
* To use a non-default version of another binary, such as yacc, or llvm-config

## Using a native file

During the `setup` phase, use the native file as such:
```sh
meson setup --native-file my-native-file.ini builddir/
```

## Changing native file settings

All of the rules about cross files and changed settings apply to native files
as well, see [here](Cross-compilation.md#changing-cross-file-settings)

## Defining the environment

See the [config-files section](Machine-files.md), for options shared by cross
and native files.

## Native file locations

Like cross files, native files may be installed to user or system wide
locations, defined as:
  - $XDG_DATA_DIRS/meson/native
    (/usr/local/share/meson/native:/usr/share/meson/native if $XDG_DATA_DIRS is
    undefined)
  - $XDG_DATA_HOME/meson/native ($HOME/.local/share/meson/native if
    $XDG_DATA_HOME is undefined)

The order of locations tried is as follows:
 - A file relative to the local dir
 - The user local location
 - The system wide locations in order

These files are not intended to be shipped by distributions, unless
they are specifically for distribution packaging, they are mainly
intended for developers.
