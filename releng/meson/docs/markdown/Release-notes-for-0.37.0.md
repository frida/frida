---
title: Release 0.37
short-description: Release notes for 0.37
...

# New features

## Mesontest

Mesontest is a new testing tool that allows you to run your tests in
many different ways. As an example you can run tests multiple times:

    mesontest --repeat=1000 a_test

or with an arbitrary wrapper executable:

    mesontest --wrap='valgrind --tool=helgrind' a_test

or under `gdb`, 1000 times in a row. This is handy for tests that fail
spuriously, as when the crash happens you are given the full GDB
command line:

    mesontest --repeat=1000 --gdb a_test

## Mesonrewriter

Mesonrewriter is an experimental tool to manipulate your build
definitions programmatically. It is not installed by default yet but
those interested can run it from the source repository.

As an example, here is how you would add a source file to a build target:

    mesonrewriter add --target=program --filename=new_source.c

## Shared modules

The new `shared_module` function allows the creation of shared
modules, that is, extension modules such as plugins that are meant to
be used solely with `dlopen` rather than linking them to targets.

## Gnome module

- Detect required programs and print useful errors if missing

### gtkdoc

- Allow passing a list of directories to `src_dir` keyword argument
- Add `namespace` keyword argument
- Add `mode` keyword argument
- Fix `gtkdoc-scangobj` finding local libraries

### compile_resources

- Add `gresource_bundle` keyword argument to output `.gresource` files
- Add `export` and `install_header` keyword arguments
- Use depfile support available in GLib >= 2.52.0

## i18n module

- Add `merge_file()` function for creating translated files
- Add `preset` keyword argument to included common gettext flags
- Read languages from `LINGUAS` file

## LLVM IR compilation

Meson has long had support for compiling assembler (GAS) files. In
this release we add support for compiling LLVM IR files in a similar
way when building with the Clang compiler. Just add it to the list of
files when creating a `library` or `executable` target like any other
source file. No special handling is required:

```meson
executable('some-exe', 'main.c', 'asm-file.S', 'ir-file.ll')
```

As always, you can also mix LLVM IR files with C++, C, and Assembly
(GAS) sources.

## ViM indent and syntax files

We now include filetype, indent, and syntax files for ViM [with the
source
tree](https://github.com/mesonbuild/meson/tree/master/data/syntax-highlighting/vim).
Please file issues (or pull requests!) for enhancements or if you face
any problems using them.

## Push URLs in .wrap files

[.wrap files](Using-the-WrapDB.md) for subprojects can now include a
separate push URL to allow developers to push changes directly from a
subproject git checkout.

## pkg-config dependencies

Meson now supports multiple version restrictions while searching for pkg-config dependencies.

```meson
# Just want a lower limit
dependency('zlib', version : '>1.2.1')
# Want both a lower and an upper limit
dependency('opencv', version : ['>=2.3.0', '<=3.1.0'])
# Want to exclude one specific broken version
dependency('foolite', version : ['>=3.12.1', '!=3.13.99'])
```

## Overriding more binaries with environment variables

You can now specify the binary to be used for the following tools by
setting the corresponding environment variable

| Name | Environment variable |
| ---- | -------------------- |
| pkg-config | PKG_CONFIG     |
| readelf    | READELF        |
| nm         | NM             |

## Support for `localstatedir`

Similar to other options such as `bindir` and `datadir`, you can now
specify the `localstatedir` for a project by passing
`--localstatedir=dir` to `meson` or `-Dlocalstatedir=dir` to
`mesonconf` after configuration. You can also access it from inside
the `meson.build` file with `get_option('localstatedir')`.

## New compiler function `symbols_have_underscore_prefix`

Checks if the compiler prefixes an underscore to C global symbols with
the default calling convention. This is useful when linking to
compiled assembly code, or other code that does not have its C symbol
mangling handled transparently by the compiler.

```meson
cc = meson.get_compiler('c')
conf = configuration_data()
if cc.symbols_have_underscore_prefix()
    conf.set('SYMBOLS_HAVE_UNDERSCORE', true)
endif
```

C symbol mangling is platform and architecture dependent, and a helper
function is needed to detect it. For example, Windows 32-bit prefixes
underscore, but 64-bit does not. Linux does not prefix an underscore
but OS X does.

## Vala

GLib Resources compiled with
[`gnome.compile_resources`](Gnome-module.md#compile_resources) that
are added to the sources of a Vala build target will now cause the
appropriate `--gresources` flag to be passed to the Vala compiler so
you don't need to add that yourself to `vala_args:`.

## Improvements to install scripts

You can now pass arguments to install scripts added with
[[meson.add_install_script]]. All
arguments after the script name will be passed to the script.

The `MESON_INSTALL_DESTDIR_PREFIX` environment variable is now set
when install scripts are called. This contains the values of the
`DESTDIR` environment variable and the `prefix` option passed to Meson
joined together. This is useful because both those are usually
absolute paths, and joining absolute paths in a cross-platform way is
tricky. [`os.path.join` in
Python](https://docs.python.org/3/library/os.path.html#os.path.join)
will discard all previous path segments when it encounters an absolute
path, and simply concatenating them will not work on Windows where
absolute paths begin with the drive letter.

## More install directories

Added new options `sbindir` and `infodir` that can be used for
installation.
