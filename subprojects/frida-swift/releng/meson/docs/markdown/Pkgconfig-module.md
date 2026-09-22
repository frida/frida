# Pkgconfig module

This module is a simple generator for
[pkg-config](https://pkg-config.freedesktop.org/) files.

## Usage

```meson
pkg = import('pkgconfig')
bar_dep = dependency('bar')
lib = library('foo', dependencies : [bar])
pkg.generate(lib)
```

### pkg.generate()

The generated file's properties are specified with the following
keyword arguments.

- `description` a string describing the library, used to set the `Description:` field
- `extra_cflags` a list of extra compiler flags to be added to the
  `Cflags` field after the header search path
- `filebase` the base name to use for the pkg-config file; as an
  example, the value of `libfoo` would produce a pkg-config file called
  `libfoo.pc`
- `install_dir` the directory to install to, defaults to the value of
  option `libdir` followed by `/pkgconfig`
- `libraries` a list of built libraries (usually results of
  shared_library) that the user needs to link against. Arbitrary strings can
  also be provided and they will be added into the `Libs` field. Since 0.45.0
  dependencies of built libraries will be automatically added, see the
  [Implicit dependencies](#implicit-dependencies) section below for the exact
  rules. Since 0.58.0 custom_target() objects are supported as long as they are
  linkable (has known extension such as `.a`, `.so`, etc).
- `libraries_private` list of built libraries or strings to put in the
  `Libs.private` field. Since 0.45.0 dependencies of built libraries will be
  automatically added, see the [Implicit dependencies](#implicit-dependencies)
  section below for the exact rules. Since 0.58.0 custom_target() objects are
  supported as long as they are linkable (has known extension such as `.a`,
  `.so`, etc).
- `name` the name of this library, used to set the `Name:` field
- `subdirs` which subdirs of `include` should be added to the header
  search path, for example if you install headers into
  `${PREFIX}/include/foobar-1`, the correct value for this argument
  would be `foobar-1`
- `requires` list of strings, pkgconfig-dependencies or libraries that
   `pkgconfig.generate()` was used on to put in the `Requires` field
- `requires_private` the same as `requires` but for the `Requires.private` field
- `url` a string with a url for the library
- `variables` a list of strings with custom variables to add to the
  generated file. The strings must be in the form `name=value` and may
  reference other pkgconfig variables,
  e.g. `datadir=${prefix}/share`. The names `prefix`, `libdir` and
  `includedir` are reserved and may not be used. *Since 0.56.0* it can also be a
  dictionary but ordering of Meson dictionaries are not guaranteed, which could
  cause issues when some variables reference other variables.
  Spaces in values are escaped with `\`, this is required in the case the value is
  a path that and is used in `cflags` or `libs` arguments. *Since 0.59.0* if
  escaping is not desired (e.g. space separate list of values) `unescaped_variables`
  keyword argument should be used instead. *Since 0.62.0* builtin directory variables
  that are referenced are automatically created by default.
- `uninstalled_variables` used instead of the `variables` keyword argument, when
  generating the uninstalled pkg-config file. Since *0.54.0*
  Spaces in values are escaped with `\`, this is required in the case the value is
  a path that and is used in `cflags` or `libs` arguments. *Since 0.59.0* if
  escaping is not desired (e.g. space separate list of values)
  `unescaped_uninstalled_variables` keyword argument should be used instead.
- `version` a string describing the version of this library, used to set the
  `Version:` field. (*since 0.46.0*) Defaults to the project version if unspecified.
- `d_module_versions` a list of module version flags used when compiling
   D sources referred to by this pkg-config file
- `dataonly` field. (*since 0.54.0*) this is used for architecture-independent
   pkg-config files in projects which also have architecture-dependent outputs.
- `conflicts` (*since 0.36.0, incorrectly issued a warning prior to 0.54.0*) list of strings to be put in the `Conflicts` field.

Since 0.46 a `StaticLibrary` or `SharedLibrary` object can optionally
be passed as first positional argument. If one is provided a default
value will be provided for all required fields of the pc file:
- `install_dir` is set to `pkgconfig` folder in the same location than the provided library.
- `description` is set to the project's name followed by the library's name.
- `name` is set to the library's name.

Since 0.54.0 uninstalled pkg-config files are generated as well. They
are located in `<build dir>/meson-uninstalled/`. It is sometimes
useful to build projects against libraries built by Meson without
having to install them into a prefix. In order to do so, just set
`PKG_CONFIG_PATH=<builddir>/meson-uninstalled` before building your
application. That will cause pkg-config to prefer those
`-uninstalled.pc` files and find libraries and headers from the Meson
builddir. This is an experimental feature provided on a best-effort
basis, it might not work in all use-cases.

### Implicit dependencies

The exact rules followed to find dependencies that are implicitly
added into the pkg-config file have evolved over time. Here are the
rules as of Meson *0.49.0*, previous versions might have slightly
different behaviour.

- Not found libraries or dependencies are ignored.
- Libraries and dependencies are private by default (i.e. added into
  `Requires.private:` or `Libs.private:`) unless they are explicitly added in
  `libraries` or `requires` keyword arguments, or is the main library (first
  positional argument).
- Libraries and dependencies will be de-duplicated, if they are added in both
  public and private (e.g `Requires:` and `Requires.private:`) it will be removed
  from the private list.
- Shared libraries (i.e. `shared_library()` and **NOT** `library()`) add only
  `-lfoo` into `Libs:` or `Libs.private:` but their dependencies are not pulled.
  This is because dependencies are only needed for static link.
- Other libraries (i.e. `static_library()` or `library()`) add `-lfoo` into `Libs:`
  or `Libs.private:` and recursively add their dependencies into `Libs.private:` or
  `Requires.private:`.
- Dependencies provided by pkg-config are added into `Requires:` or
  `Requires.private:`. If a version was specified when declaring that dependency
  it will be written into the generated file too.
- The threads dependency (i.e. `dependency('threads')`) adds `-pthread` into
  `Libs:` or `Libs.private:`.
- Internal dependencies (i.e.
  `declare_dependency(compiler_args : '-DFOO', link_args : '-Wl,something', link_with : foo)`)
  add `compiler_args` into `Cflags:` if public, `link_args` and `link_with` into
  `Libs:` if public or `Libs.private:` if private.
- Other dependency types add their compiler arguments into `Cflags:` if public,
  and linker arguments into `Libs:` if public or `Libs.private:` if private.
- Once a pkg-config file is generated for a library using `pkg.generate(mylib)`,
  any subsequent call to `pkg.generate()` where mylib appears, will generate a
  `Requires:` or `Requires.private` instead of a `Libs:` or `Libs.private:`.

### Relocatable pkg-config files

By default, the files generated by `pkg.generate` contain a hardcoded prefix path.
In order to make them relocatable, a `pkgconfig.relocatable` builtin option is provided.
See [Pkgconfig module options](Builtin-options.md#pkgconfig-module).
