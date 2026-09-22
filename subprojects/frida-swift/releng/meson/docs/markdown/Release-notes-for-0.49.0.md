---
title: Release 0.49
short-description: Release notes for 0.49
...

# New features

## Libgcrypt dependency now supports libgcrypt-config

Earlier, `dependency('libgcrypt')` could only detect the library with
pkg-config files. Now, if pkg-config files are not found, Meson will
look for `libgcrypt-config` and if it's found, will use that to find
the library.

## New `section` key for the buildoptions introspection

Meson now has a new `section` key in each build option. This allows
IDEs to group these options similar to `meson configure`.

The possible values for `section` are:

 - core
 - backend
 - base
 - compiler
 - directory
 - user
 - test

## CC-RX compiler for C and CPP

Cross-compilation is now supported for Renesas RX targets with the
CC-RX compiler.

The environment path should be set properly for the CC-RX compiler
executables. The `-cpu` option with the appropriate value should be
mentioned in the cross-file as shown in the snippet below.

```ini
[properties]
c_args      = ['-cpu=rx600']
cpp_args    = ['-cpu=rx600']
```

The default extension of the executable output is `.abs`. Other target
specific arguments to the compiler and linker will need to be added
explicitly from the
cross-file(`c_args`/`c_link_args`/`cpp_args`/`cpp_link_args`) or some
other way. Refer to the CC-RX User's manual for additional compiler
and linker options.

## CMake `find_package` dependency backend

Meson can now use the CMake `find_package` ecosystem to detect
dependencies. Both the old-style `<NAME>_LIBRARIES` variables as well
as imported targets are supported. Meson can automatically guess the
correct CMake target in most cases but it is also possible to manually
specify a target with the `modules` property.

```meson
# Implicitly uses CMake as a fallback and guesses a target
dep1 = dependency('KF5TextEditor')

# Manually specify one or more CMake targets to use
dep2 = dependency('ZLIB', method : 'cmake', modules : ['ZLIB::ZLIB'])
```

CMake is automatically used after `pkg-config` fails when
no `method` (or `auto`) was provided in the dependency options.

## New compiler method `get_argument_syntax`

The compiler object now has `get_argument_syntax` method, which
returns a string value of `gcc`, `msvc`, or an undefined value string
value. This can be used to determine if a compiler uses gcc syntax
(`-Wfoo`), msvc syntax (`/w1234`), or some other kind of arguments.

```meson
cc = meson.get_compiler('c')

if cc.get_argument_syntax() == 'msvc'
  if cc.has_argument('/w1235')
    add_project_arguments('/w1235', language : ['c'])
  endif
elif cc.get_argument_syntax() == 'gcc'
  if cc.has_argument('-Wfoo')
    add_project_arguments('-Wfoo', language : ['c'])
  endif
elif cc.get_id() == 'some other compiler'
  add_project_arguments('--error-on-foo', language : ['c'])
endif
```

## Return `Disabler()` instead of not-found object

Functions such as `dependency()`, `find_library()`, `find_program()`,
and `python.find_installation()` have a new keyword argument:
`disabler`. When set to `true` those functions return `Disabler()`
objects instead of not-found objects.

## `introspect --projectinfo` can now be used without configured build directory

This allows IDE integration to get information about the project
before the user has configured a build directory.

Before you could use `meson.py introspect --projectinfo
build-directory`. Now you also can use `meson.py introspect
--projectinfo project-dir/meson.build`.

The output is similar to the output with a build directory but
additionally also includes information from `introspect
--buildsystem-files`.

For example `meson.py introspect --projectinfo test\ cases/common/47\
subproject\ options/meson.build` This outputs (pretty printed for
readability):

```
{
    "buildsystem_files": [
        "meson_options.txt",
        "meson.build"
    ],
    "name": "suboptions",
    "version": null,
    "descriptive_name": "suboptions",
    "subprojects": [
        {
            "buildsystem_files": [
                "subprojects/subproject/meson_options.txt",
                "subprojects/subproject/meson.build"
            ],
            "name": "subproject",
            "version": "undefined",
            "descriptive_name": "subproject"
        }
    ]
}
```

Both usages now include a new `descriptive_name` property which always
shows the name set in the project.

## Can specify keyword arguments with a dictionary

You can now specify keyword arguments for any function and method call
with the `kwargs` keyword argument. This is perhaps best described
with an example:

```meson
options = {'include_directories': include_directories('inc')}

...

executable(...
  kwargs: options)
```

The above code is identical to this:

```meson
executable(...
  include_directories: include_directories('inc'))
```

That is, Meson will expand the dictionary given to `kwargs` as if the
entries in it had been given as keyword arguments directly.

Note that any individual argument can be specified either directly or
with the `kwarg` dict but not both. If a key is specified twice, it
is a hard error.

## Manpages are no longer compressed implicitly

Earlier, the `install_man` command has automatically compressed
installed manpages into `.gz` format. This collided with manpage
compression hooks already used by various distributions. Now, manpages
are installed uncompressed and distributors are expected to handle
compressing them according to their own compression preferences.

## Native config files

Native files (`--native-file`) are the counterpart to cross files
(`--cross-file`), and allow specifying information about the build
machine, both when cross compiling and when not.

Currently the native files only allow specifying the names of
binaries, similar to the cross file, for example:

```ini
[binaries]
llvm-config = "/opt/llvm-custom/bin/llvm-config"
```

Will override the llvm-config used for *native* binaries. Targets for
the host machine will continue to use the cross file.

## Foreach `break` and `continue`

`break` and `continue` keywords can be used inside foreach loops.

```meson
items = ['a', 'continue', 'b', 'break', 'c']
result = []
foreach i : items
  if i == 'continue'
    continue
  elif i == 'break'
    break
  endif
  result += i
endforeach
# result is ['a', 'b']
```

You can check if an array contains an element like this:
```meson
my_array = [1, 2]
if 1 in my_array
# This condition is true
endif
if 1 not in my_array
# This condition is false
endif
```

You can check if a dictionary contains a key like this:
```meson
my_dict = {'foo': 42, 'foo': 43}
if 'foo' in my_dict
# This condition is true
endif
if 42 in my_dict
# This condition is false
endif
if 'foo' not in my_dict
# This condition is false
endif
```

## Joining paths with /

For clarity and conciseness, we recommend using the `/` operator to separate
path elements:

```meson
joined = 'foo' / 'bar'
```

Before Meson 0.49, joining path elements was done with the legacy
`join_paths` function, but the `/` syntax above is now recommended.

```meson
joined = join_paths('foo', 'bar')
```

This only works for strings.

## Position-independent executables

When `b_pie` option, or `executable()`'s `pie` keyword argument is set
to `true`, position-independent executables are built. All their
objects are built with `-fPIE` and the executable is linked with
`-pie`. Any static library they link must be built with `pic` set to
`true` (see `b_staticpic` option).

## Deprecation warning in pkg-config generator

All libraries passed to the `libraries` keyword argument of the
`generate()` method used to be associated with that generated
pkg-config file. That means that any subsequent call to `generate()`
where those libraries appear would add the filebase of the
`generate()` that first contained them into `Requires:` or
`Requires.private:` field instead of adding an `-l` to `Libs:` or
`Libs.private:`.

This behaviour is now deprecated. The library that should be
associated with the generated pkg-config file should be passed as
first positional argument instead of in the `libraries` keyword
argument. The previous behaviour is maintained but prints a
deprecation warning and support for this will be removed in a future
Meson release. If you cannot create the needed pkg-config file
without this warning, please file an issue with as much details as
possible about the situation.

For example this sample will write `Requires: liba` into `libb.pc` but
print a deprecation warning:

```meson
liba = library(...)
pkg.generate(libraries : liba)

libb = library(...)
pkg.generate(libraries : [liba, libb])
```

It can be fixed by passing `liba` as first positional argument::
```meson
liba = library(...)
pkg.generate(liba)

libb = library(...)
pkg.generate(libb, libraries : [liba])
```

## Subprojects download, checkout, update command-line

New command-line tool has been added to manage subprojects:

- `meson subprojects download` to download all subprojects that have a wrap file.
- `meson subprojects update` to update all subprojects to latest version.
- `meson subprojects checkout` to checkout or create a branch in all git subprojects.

## New keyword argument `is_default` to `add_test_setup()`

The keyword argument `is_default` may be used to set whether the test
setup should be used by default whenever `meson test` is run without
the `--setup` option.

```meson
add_test_setup('default', is_default: true, env: 'G_SLICE=debug-blocks')
add_test_setup('valgrind', env: 'G_SLICE=always-malloc', ...)
test('mytest', exe)
```

For the example above, running `meson test` and `meson test
--setup=default` is now equivalent.
