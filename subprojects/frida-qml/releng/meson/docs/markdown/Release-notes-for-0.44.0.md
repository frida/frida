---
title: Release 0.44
short-description: Release notes for 0.44
...

# New features

## Added warning function

This function prints its argument to the console prefixed by
"WARNING:" in yellow color. A simple example:

warning('foo is deprecated, please use bar instead')


## Adds support for additional Qt5-Module keyword `moc_extra_arguments`

When `moc`-ing sources, the `moc` tool does not know about any
preprocessor macros. The generated code might not match the input
files when the linking with the moc input sources happens.

This amendment allows to specify a a list of additional arguments
passed to the `moc` tool. They are called `moc_extra_arguments`.


## Prefix-dependent defaults for sysconfdir, localstatedir and sharedstatedir

These options now default in a way consistent with
[FHS](http://refspecs.linuxfoundation.org/fhs.shtml) and common usage.

If prefix is `/usr`, default sysconfdir to `/etc`, localstatedir to `/var` and
sharedstatedir to `/var/lib`.

If prefix is `/usr/local` (the default), default localstatedir to
`/var/local` and sharedstatedir to `/var/local/lib`.


## An array type for user options

Previously to have an option that took more than one value a string
value would have to be created and split, but validating this was
difficult. A new array type has been added to the `meson_options.txt`
for this case. It works like a 'combo', but allows more than one
option to be passed. The values can optionally be validated against a
list of valid values. When used on the command line (with -D), values
are passed as a comma separated list.

```meson
option('array_opt', type : 'array', choices : ['one', 'two', 'three'], value : ['one'])
```

These can be overwritten on the command line,

```meson
meson _build -Darray_opt=two,three
```

## LLVM dependency supports both dynamic and static linking

The LLVM dependency has been improved to consistently use dynamic
linking. Previously recent version (>= 3.9) would link dynamically
while older versions would link statically.

Now LLVM also accepts the `static` keyword to enable statically
linking to LLVM modules instead of dynamically linking.


## Added `if_found` to subdir

Added a new keyword argument to the `subdir` command. It is given a
list of dependency objects and the function will only recurse in the
subdirectory if they are all found. Typical usage goes like this.

```meson
d1 = dependency('foo') # This is found
d2 = dependency('bar') # This is not found

subdir('somedir', if_found : [d1, d2])
```

In this case the subdirectory would not be entered since `d2` could
not be found.

## `get_unquoted()` method for the `configuration` data object

New convenience method that allows reusing a variable value
defined quoted. Useful in C for config.h strings for example.


## Added disabler object

A disabler object is a new kind of object that has very specific
semantics. If it is used as part of any other operation such as an
argument to a function call, logical operations etc, it will cause the
operation to not be evaluated. Instead the return value of said
operation will also be the disabler object.

For example if you have an setup like this:

```meson
dep = dependency('foo')
lib = shared_library('mylib', 'mylib.c',
  dependencies : dep)
exe = executable('mytest', 'mytest.c',
  link_with : lib)
test('mytest', exe)
```

If you replace the dependency with a disabler object like this:

```meson
dep = disabler()
lib = shared_library('mylib', 'mylib.c',
  dependencies : dep)
exe = executable('mytest', 'mytest.c',
  link_with : lib)
test('mytest', exe)
```

Then the shared library, executable and unit test are not
created. This is a handy mechanism to cut down on the number of `if`
statements.


## Config-Tool based dependencies gained a method to get arbitrary options

A number of dependencies (CUPS, LLVM, pcap, WxWidgets, GnuStep) use a
config tool instead of pkg-config. As of this version they now have a
`get_configtool_variable` method, which is analogous to the
`get_pkgconfig_variable` for pkg config.

```meson
dep_llvm = dependency('LLVM')
llvm_inc_dir = dep_llvm.get_configtool_variable('includedir')
```

## Embedded Python in Windows MSI packages

Meson now ships an internal version of Python in the MSI installer
packages. This means that it can run Python scripts that are part of
your build transparently. That is, if you do the following:

```meson
myprog = find_program('myscript.py')
```

Then Meson will run the script with its internal Python version if necessary.

## Libwmf dependency now supports libwmf-config

Earlier, `dependency('libwmf')` could only detect the library with
pkg-config files. Now, if pkg-config files are not found, Meson will
look for `libwmf-config` and if it's found, will use that to find the
library.
