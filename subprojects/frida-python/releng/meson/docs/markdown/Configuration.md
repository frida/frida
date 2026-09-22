---
short-description: Build-time configuration options
...

# Configuration

If there are multiple configuration options, passing them through
compiler flags becomes very burdensome. It also makes the
configuration settings hard to inspect. To make things easier, Meson
supports the generation of configure files. This feature is similar to
one found in other build systems such as CMake.

Suppose we have the following Meson snippet:

```meson
conf_data = [[#configuration_data]]
conf_data.set('version', '1.2.3')
[[#configure_file]](input : 'config.h.in',
               output : 'config.h',
               configuration : conf_data)
```

and that the contents of `config.h.in` are

```c
#define VERSION_STR "@version@"
```

Meson will then create a file called `config.h` in the corresponding
build directory whose contents are the following.

```c
#define VERSION_STR "1.2.3"
```

More specifically, Meson will find all strings of the type `@varname@`
and replace them with respective values set in `conf_data`. You can
use a single `configuration_data` object as many times as you like,
but it becomes immutable after being passed to the `configure_file`
function. That is, after it has been used once to generate output the
`set` function becomes unusable and trying to call it causes an error.
Copy of immutable `configuration_data` is still immutable.

For more complex configuration file generation Meson provides a second
form. To use it, put a line like this in your configuration file.

    #mesondefine TOKEN

The replacement that happens depends on what the value and type of TOKEN is:

```c
#define TOKEN     // If TOKEN is set to boolean true.
#undef TOKEN      // If TOKEN is set to boolean false.
#define TOKEN 4   // If TOKEN is set to an integer or string value.
/* undef TOKEN */ // If TOKEN has not been set to any value.
```

Note that if you want to define a C string, you need to do the quoting
yourself like this:

```meson
conf_data.set('TOKEN', '"value"')
```

Since this is such a common operation, Meson provides a convenience
method:

```meson
plain_var = 'value'
conf_data.set_quoted('TOKEN', plain_var) # becomes #define TOKEN "value"
```

Often you have a boolean value in Meson but need to define the C/C++
token as 0 or 1. Meson provides a convenience function for this use
case.

```meson
conf_data.set10(token, boolean_value)
# The line above is equivalent to this:
if boolean_value
  conf_data.set(token, 1)
else
  conf_data.set(token, 0)
endif
```

## Configuring without an input file

If the input file is not defined then Meson will generate a header
file all the entries in the configuration data object. The
replacements are the same as when generating `#mesondefine` entries:

```meson
conf_data.set('FOO', '"string"') => #define FOO "string"
conf_data.set('FOO', 'a_token')  => #define FOO a_token
conf_data.set('FOO', true)       => #define FOO
conf_data.set('FOO', false)      => #undef FOO
conf_data.set('FOO', 1)          => #define FOO 1
conf_data.set('FOO', 0)          => #define FOO 0
```

In this mode, you can also specify a comment which will be placed
before the value so that your generated files are self-documenting.

```meson
conf_data.set('BAR', true, description : 'Set BAR if it is available')
```

Will produce:

```c
/* Set BAR if it is available */
#define BAR
```

## Dealing with file encodings

The default Meson file encoding to configure files is utf-8. If you
need to configure a file that is not utf-8 encoded the encoding
keyword will allow you to specify which file encoding to use. It is
however strongly advised to convert your non utf-8 file to utf-8
whenever possible. Supported file encodings are those of python3, see
[standard-encodings](https://docs.python.org/3/library/codecs.html#standard-encodings).

## Using dictionaries

Since *0.49.0* [[configuration_data]] takes an optional dictionary as
first argument. If provided, each key/value pair is added into the
`configuration_data` as if `set()` method was called for each of them.
[[configure_file]]'s `configuration` kwarg also accepts a dictionary
instead of a configuration_data object.

Example:
```meson
cdata = configuration_data({
  'STRING' : '"foo"',
  'INT' : 42,
  'DEFINED' : true,
  'UNDEFINED' : false,
})

configure_file(output : 'config1.h',
  configuration : cdata,
)

configure_file(output : 'config2.h',
  configuration : {
    'STRING' : '"foo"',
    'INT' : 42,
    'DEFINED' : true,
    'UNDEFINED' : false,
  }
)

```

# A full example

Generating and using a configuration file requires the following steps:

 - generate the file
 - create an include directory object for the directory that holds the file
 - use it in a target

We are going to use the traditional approach of generating a header
file in the top directory. The common name is `config.h` but we're
going to use an unique name. This avoids the problem of accidentally
including the wrong header file when building a project with many
subprojects.

At the top level we generate the file:

```meson
conf_data = configuration_data()
# Set data
configure_file(input : 'projconfig.h.in',
  output : 'projconfig.h',
  configuration : conf_data)
```

Immediately afterwards we generate the include object.

```meson
configuration_inc = include_directories('.')
```

Finally we specify this in a target that can be in any subdirectory.

```meson
executable(..., include_directories : configuration_inc)
```

Now any source file in this target can include the configuration
header like this:

```c
#include<projconfig.h>
```
