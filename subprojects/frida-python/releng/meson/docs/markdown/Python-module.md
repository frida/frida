---
short-description: Generic python module
authors:
    - name: Mathieu Duponchelle
      email: mathieu@centricular.com
      years: [2018]
      has-copyright: false
...

# Python module

This module provides support for finding and building extensions against
python installations, be they python 2 or 3.

*Added 0.46.0*

## Functions

### `find_installation()`

``` meson
pymod.find_installation(name_or_path, ...)
```

Find a python installation matching `name_or_path`.

That argument is optional, if not provided then the returned python
installation will be the one used to run Meson.

If provided, it can be:

- A simple name, eg `python-2.7`, Meson will look for an external program
  named that way, using [[find_program]]

- A path, eg `/usr/local/bin/python3.4m`

- One of `python2` or `python3`: in either case, the module will try
  some alternative names: `py -2` or `py -3` on Windows, and `python`
  everywhere. In the latter case, it will check whether the version
  provided by the sysconfig module matches the required major version.

  *Since 1.2.0*, searching for minor version (e.g. `python3.11`) also
  works on Windows.

Keyword arguments are the following:

- `required`: by default, `required` is set to `true` and Meson will
  abort if no python installation can be found. If `required` is set to `false`,
  Meson will continue even if no python installation was found. You can
  then use the `.found()` method on the returned object to check
  whether it was found or not. Since *0.48.0*  the value of a
  [`feature`](Build-options.md#features) option can also be passed to the
  `required` keyword argument.
- `disabler`: if `true` and no python installation can be found, return a
  [[@disabler]] object instead of a not-found object.
  *Since 0.49.0*
- `modules`: a list of module names that this python installation must have.
  *Since 0.51.0*
- `pure`: On some platforms, architecture independent files are
  expected to be placed in a separate directory. However, if the
  python sources should be installed alongside an extension module
  built with this module, this keyword argument can be used to
  override the default behavior of `.install_sources()`.
  *since 0.64.0*

**Returns**: a [python installation][`python_installation` object]

## `python_installation` object

The `python_installation` object is an [[@external_program]], with several
added methods.

### Methods

#### `path()`

```meson
str py_installation.path()
```

*Added 0.50.0*

Works like the path method of other `ExternalProgram` objects. Was not
provided prior to 0.50.0 due to a bug.

#### `extension_module()`

``` meson
shared_module py_installation.extension_module(module_name, list_of_sources, ...)
```

Create a [[shared_module]] target that is named according to the naming
conventions of the target platform.

All positional and keyword arguments are the same as for
[[shared_module]], excluding `name_suffix` and `name_prefix`, and with
the addition of the following:

- `subdir`: By default, Meson will install the extension module in
  the relevant top-level location for the python installation, eg
  `/usr/lib/site-packages`. When subdir is passed to this method,
  it will be appended to that location. This keyword argument is
  mutually exclusive with `install_dir`
- `limited_api`: *since 1.3.0* A string containing the Python version
  of the [Py_LIMITED_API](https://docs.python.org/3/c-api/stable.html) that
  the extension targets. For example, '3.7' to target Python 3.7's version of
  the limited API. This behavior can be disabled by setting the value of
  `python.allow_limited_api`. See [Python module options](Builtin-options.md#python-module).

Additionally, the following diverge from [[shared_module]]'s default behavior:

- `gnu_symbol_visibility`: if unset, it will default to `'hidden'` on versions
  of Python that support this (the python headers define `PyMODINIT_FUNC` has
  default visibility).

*since 0.63.0* `extension_module` automatically adds a dependency to the library
if one is not explicitly provided. To support older versions, the user may need to
add `dependencies : py_installation.dependency()`, see [[dependency]].

**Returns**: a [[@build_tgt]] object

#### `dependency()`

``` meson
python_dependency py_installation.dependency(...)
```

*since 0.53.0*

This method accepts no positional arguments, and the same keyword
arguments as the standard [[dependency]] function. It also supports the
following keyword argument:

- `embed`: *(since 0.53.0)* If true, Meson will try to find a python
  dependency that can be used for embedding python into an
  application.
- `disabler` *(since 0.60.0)*: if `true` and the dependency couldn't be found,
  returns a [disabler object](#disabler-object) instead of a not-found dependency.

**Returns**: a [python dependency][`python_dependency` object]

#### `install_sources()`

``` meson
void py_installation.install_sources(list_of_files, ...)
```

Install actual python sources (`.py`).

All positional and keyword arguments are the same as for
[[install_data]], with the addition of the following:

*Since 0.60.0* `python.platlibdir` and `python.purelibdir` options can be used
to control the default installation path. See [Python module options](Builtin-options.md#python-module).

- `pure`: On some platforms, architecture independent files are
  expected to be placed in a separate directory. However, if the
  python sources should be installed alongside an extension module
  built with this module, this keyword argument can be used to
  override that behaviour. Defaults to the value specified in
  `find_installation()`, or else `true`

- `subdir`: See documentation for the argument of the same name to
  [][`extension_module()`]

- `install_tag` *(since 0.60.0)*: A string used by `meson install --tags` command
  to install only a subset of the files. By default it has the tag `python-runtime`.

#### `get_install_dir()`

``` meson
string py_installation.get_install_dir(...)
```

Retrieve the directory [][`install_sources()`] will install to.

It can be useful in cases where `install_sources` cannot be used
directly, for example when using [[configure_file]].

This function accepts no arguments, its keyword arguments are the same
as [][`install_sources()`].

*Since 0.60.0* `python.platlibdir` and `python.purelibdir` options can be used
to control the default installation path. See [Python module options](Builtin-options.md#python-module).

**Returns**: A string

#### `language_version()`

``` meson
string py_installation.language_version()
```

Get the major.minor python version, eg `2.7`.

The version is obtained through the `sysconfig` module.

This function expects no arguments or keyword arguments.

**Returns**: A string

#### `get_path()`

``` meson
string py_installation.get_path(path_name, fallback)
```

Get a path as defined by the `sysconfig` module.

For example:

``` meson
purelib = py_installation.get_path('purelib')
```

This function requires at least one argument, `path_name`,
which is expected to be a non-empty string.

If `fallback` is specified, it will be returned if no path
with the given name exists. Otherwise, attempting to read
a non-existing path will cause a fatal error.

**Returns**: A string

#### `has_path()`

``` meson
    bool py_installation.has_path(path_name)
```

**Returns**: true if a path named `path_name` can be retrieved with
[][`get_path()`], false otherwise.

#### `get_variable()`

``` meson
string py_installation.get_variable(variable_name, fallback)
```

Get a variable as defined by the `sysconfig` module.

For example:

``` meson
py_bindir = py_installation.get_variable('BINDIR', '')
```

This function requires at least one argument, `variable_name`,
which is expected to be a non-empty string.

If `fallback` is specified, it will be returned if no variable
with the given name exists. Otherwise, attempting to read
a non-existing variable will cause a fatal error.

**Returns**: A string

#### `has_variable()`

``` meson
    bool py_installation.has_variable(variable_name)
```

**Returns**: true if a variable named `variable_name` can be retrieved
with [][`get_variable()`], false otherwise.

## `python_dependency` object

This [[@dep]] object subclass will try various methods to obtain
the compiler and linker arguments, starting with pkg-config then
potentially using information obtained from python's `sysconfig`
module.

It exposes the same methods as its parent class.
