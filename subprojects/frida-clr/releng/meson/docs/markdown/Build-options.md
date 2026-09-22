---
short-description: Build options to configure project properties
...

# Build options

Most non-trivial builds require user-settable options. As an example a
program may have two different data backends that are selectable at
build time. Meson provides for this by having a option definition
file. Its name is `meson.options` and it is placed at the root of
your source tree. For versions of meson before 1.1, this file was called
`meson_options.txt`.

Here is a simple option file.

```meson
option('someoption', type : 'string', value : 'optval', description : 'An option')
option('other_one', type : 'boolean', value : false)
option('combo_opt', type : 'combo', choices : ['one', 'two', 'three'], value : 'three')
option('integer_opt', type : 'integer', min : 0, max : 5, value : 3) # Since 0.45.0
option('free_array_opt', type : 'array', value : ['one', 'two'])  # Since 0.44.0
option('array_opt', type : 'array', choices : ['one', 'two', 'three'], value : ['one', 'two'])
option('some_feature', type : 'feature', value : 'enabled')  # Since 0.47.0
option('long_desc', type : 'string', value : 'optval',
       description : 'An option with a very long description' +
                     'that does something in a specific context') # Since 0.55.0
```

For built-in options, see [Built-in options][builtin_opts].

## Build option types

All types allow a `description` value to be set describing the option,
if no description is set then the name of the option will be used instead.

### Strings

The string type is a free form string. If the default value is not set
then an empty string will be used as the default.

### Booleans

Booleans may have values of either `true` or `false`. If no default
value is supplied then `true` will be used as the default.

### Combos

A combo allows any one of the values in the `choices` parameter to be
selected.  If no default value is set then the first value will be the
default.

### Integers

An integer option contains a single integer with optional upper and
lower values that are specified with the `min` and `max` keyword
arguments.

This type is available since Meson version 0.45.0.

### Arrays

Arrays represent an array of strings. By default the array can contain
arbitrary strings. To limit the possible values that can used set the
`choices` parameter. Meson will then only allow the value array to
contain strings that are in the given list. The array may be
empty. The `value` parameter specifies the default value of the option
and if it is unset then the values of `choices` will be used as the
default.

As of 0.47.0 -Dopt= and -Dopt=[] both pass an empty list, before this
-Dopt= would pass a list with an empty string.

This type is available since version 0.44.0

### Features

A `feature` option has three states: `enabled`, `disabled` or `auto`.
It is intended to be passed as value for the `required` keyword
argument of most functions. Currently supported in
[[add_languages]],
[[compiler.find_library]],
[[compiler.has_header]],
[[dependency]],
[[find_program]],
[[import]] and
[[subproject]]
functions.

- `enabled` is the same as passing `required : true`.
- `auto` is the same as passing `required : false`.
- `disabled` do not look for the dependency and always return 'not-found'.

When getting the value of this type of option using [[get_option]], a
special [[@feature]] object is returned instead
of the string representation of the option's value. This object can be
passed to `required`:

```meson
d = dependency('foo', required : get_option('myfeature'))
if d.found()
  app = executable('myapp', 'main.c', dependencies : [d])
endif
```

To check the value of the feature, the object has three methods
returning a boolean and taking no argument:

- `.enabled()`
- `.disabled()`
- `.auto()`

This is useful for custom code depending on the feature:

```meson
if get_option('myfeature').enabled()
  # ...
endif
```

If the value of a `feature` option is set to `auto`, that value is
overridden by the global `auto_features` option (which defaults to
`auto`). This is intended to be used by packagers who want to have
full control on which dependencies are required and which are
disabled, and not rely on build-deps being installed (at the right
version) to get a feature enabled. They could set
`auto_features=enabled` to enable all features and disable explicitly
only the few they don't want, if any.

This type is available since version 0.47.0

## Deprecated options

Since *0.60.0*

Project options can be marked as deprecated and Meson will warn when user sets a
value to it. It is also possible to deprecate only some of the choices, and map
deprecated values to a new value.

```meson
# Option fully deprecated, it warns when any value is set.
option('o1', type: 'boolean', deprecated: true)

# One of the choices is deprecated, it warns only when 'a' is in the list of values.
option('o2', type: 'array', choices: ['a', 'b'], deprecated: ['a'])

# One of the choices is deprecated, it warns only when 'a' is in the list of values
# and replace it by 'c'.
option('o3', type: 'array', choices: ['a', 'b', 'c'], deprecated: {'a': 'c'})

# A boolean option has been replaced by a feature, old true/false values are remapped.
option('o4', type: 'feature', deprecated: {'true': 'enabled', 'false': 'disabled'})

# A feature option has been replaced by a boolean, enabled/disabled/auto values are remapped.
option('o5', type: 'boolean', deprecated: {'enabled': 'true', 'disabled': 'false', 'auto': 'false'})
```

Since *0.63.0* the `deprecated` keyword argument can take the name of a new option
that replace this option. In that case, setting a value on the deprecated option
will set the value on both the old and new names, assuming they accept the same
values.

```meson
# A boolean option has been replaced by a feature with another name, old true/false values
# are accepted by the new option for backward compatibility.
option('o6', type: 'boolean', value: 'true', deprecated: 'o7')
option('o7', type: 'feature', value: 'enabled', deprecated: {'true': 'enabled', 'false': 'disabled'})

# A project option is replaced by a module option
option('o8', type: 'string', value: '', deprecated: 'python.platlibdir')
```

## Using build options

```meson
optval = get_option('opt_name')
```

This function also allows you to query the value of Meson's built-in
project options. For example, to get the installation prefix you would
issue the following command:

```meson
prefix = get_option('prefix')
```

It should be noted that you cannot set option values in your Meson
scripts. They have to be set externally with the `meson configure`
command line tool. Running `meson configure` without arguments in a
build dir shows you all options you can set.

To change their values use the `-D`
option:

```console
$ meson configure -Doption=newvalue
```

Setting the value of arrays is a bit special. If you only pass a
single string, then it is considered to have all values separated by
commas. Thus invoking the following command:

```console
$ meson configure -Darray_opt=foo,bar
```

would set the value to an array of two elements, `foo` and `bar`.

If you need to have commas in your string values, then you need to
pass the value with proper shell quoting like this:

```console
$ meson configure "-Doption=['a,b', 'c,d']"
```

The inner values must always be single quotes and the outer ones
double quotes.

To change values in subprojects prepend the name of the subproject and
a colon:

```console
$ meson configure -Dsubproject:option=newvalue
```

**NOTE:** If you cannot call `meson configure` you likely have a old
  version of Meson. In that case you can call `mesonconf` instead, but
  that is deprecated in newer versions

## Yielding to superproject option

Suppose you have a master project and a subproject. In some cases it
might be useful to have an option that has the same value in both of
them. This can be achieved with the `yield` keyword. Suppose you have
an option definition like this:

```meson
option('some_option', type : 'string', value : 'value', yield : true)
```

If you build this project on its own, this option behaves like
usual. However if you build this project as a subproject of another
project which also has an option called `some_option`, then calling
`get_option` returns the value of the superproject. If the value of
`yield` is `false`, `get_option` returns the value of the subproject's
option.


## Built-in build options

There are a number of [built-in options][builtin_opts]. To get the
current list execute `meson configure` in the build directory.

[builtin_opts]: https://mesonbuild.com/Builtin-options.html

### Visual Studio

#### Startup project

The `backend_startup_project` option can be set to define the default
project that will be executed with the "Start debugging F5" action in
visual studio. It should be the same name as an executable target
name.

```meson
project('my_project', 'c', default_options: ['backend_startup_project=my_exe'])
executable('my_exe', ...)
```

### Ninja

#### Max links

The `backend_max_links` can be set to limit the number of processes
that ninja will use to link.
